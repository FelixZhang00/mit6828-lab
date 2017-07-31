// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include "pmap.h"

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display stack backtrace", mon_backtrace},
    { "shutdown", "Shutdown JOS", mon_shutdown },
    { "showmappings", "Display page mappings that start within [va1, va2]", mon_showmappings},
    { "setperm", "Set/clear/change permission of the given mapping", mon_setperm },
    { "dump", "Dump memory in the given virtual address range", mon_dump },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	// execrise-10
	int i = 0;
	uint32_t ebp = read_ebp();

	cprintf("Stack backtrace:\n");
	while((int)ebp != 0){ //in entry.S movl $0x0,%ebp
	  uint32_t eip = *((uint32_t *)ebp + 1);
	  cprintf("  ebp %08x  eip %08x  args",ebp,eip);
	  uint32_t *args = (uint32_t *)ebp + 2;
	  for(i=0;i<5;i++){
	    cprintf(" %08x ",args[i]);	
	  }
	  cprintf("\n");
	  //exercise-12
	  struct Eipdebuginfo eipinfo;	
	  debuginfo_eip(eip,&eipinfo);	
	  cprintf("         %s:%d: %.*s+%d\n",
		  eipinfo.eip_file,eipinfo.eip_line,
		  eipinfo.eip_fn_namelen,eipinfo.eip_fn_name,
                  eip-eipinfo.eip_fn_addr);
	  ebp = *((uint32_t *)ebp);
	}

	return 0;
}

//int
//mon_showmappings(int argc, char **argv, struct Trapframe *tf)
//{
//    if(argc != 3){
//        cprintf("Usage: showmappings begin_addr end_addr\n");
//        return 0;
//    }
//    if(argc > 3){
//        cprintf("showmappings:Too many arguments:%d\n",argc);
//        return 0;
//    }
//
//    extern pde_t *kern_pgdir;
//    extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
//
//    long begin_addr = strtol(argv[1],NULL,16);
//    long end_addr = strtol(argv[2],NULL,16);
//    if(begin_addr>end_addr){
//        cprintf("begin_addr(0x%x) must smaller than end_addr(0x%x)\n",begin_addr,end_addr);
//        return 0;
//    }
//    if(end_addr > 0xffffffff){
//        cprintf("end_addr(0x%x) is too large.",end_addr);
//        return 0;
//    }
//
//    uintptr_t begin = ROUNDUP(begin_addr,PGSIZE);
//    uintptr_t end = ROUNDUP(end_addr,PGSIZE);
//
//    pde_t *pdep = &kern_pgdir[PDX(begin)];
//
//    for(;begin < end;begin+=PGSIZE){
//        cprintf("0x%08x--0x%08x: ",begin,begin+PGSIZE);
//        pte_t *pte = pgdir_walk(kern_pgdir,(void *)begin,0);
//        if(!pte){
//            cprintf("not mapped addr=0x%08x\n",begin);
//            return 0;
//        }
//        cprintf("page %08x ",PTE_ADDR(*pte));
//        cprintf("PTE_P: %x, PTE_W: %x, PTE_U: %x\n",*pte&PTE_P,*pte&PTE_W,*pte&PTE_U);
//    }
//
//
//    return 0;
//}


void
printheader()
{
    cprintf("      Virtual            Page         Physical         Permissions\n");
    cprintf("            Address        Size             Address      kernel/user\n");
    cprintf(" --------------------------------------------------------------------\n");
}

void
printfootter()
{
    cprintf("---Type <return> to continue, or q to quit---");
}

void
printpermission(pte_t *ptep)
{
    // print permission bits
    cprintf("     ");
    if (ptep && (*ptep & PTE_P)) {
        cprintf("R");
        if (*ptep & PTE_W)
            cprintf("W");
        else
            cprintf("-");
        cprintf("/");
        if (*ptep & PTE_U) {
            cprintf("R");
            if (*ptep & PTE_W)
                cprintf("W");
            else
                cprintf("-");
        } else
            cprintf("--");
    } else
        cprintf("--/--");
    cprintf("\n");
}

bool
flushscreen(int cnt, int lim, int header)
{
    char ch;
    if (cnt > 0 && cnt % lim == 0) {
        printfootter();
        ch = getchar();
        cprintf("\n");
        if (ch == 'q')
            return 1;
        if (header)
            printheader();
    }
    return 0;
}

void
printmap(pte_t *ptep, uintptr_t va, int size)
{
    char sizestr[16];
    physaddr_t pa;

    if (size == PGSIZE) {
        strcpy(sizestr, "4-KByte");
        pa = PTE_ADDR(va);
    }
    else if (size == PTSIZE) {
        strcpy(sizestr, "4-MByte");
        pa = PTE_ADDR_EX(va);
    } else
        pa = 0;

    if (size < 0)
        cprintf("  0x%08x-0x%08x     /           no mapping     ",
                va, va-size-1);
    else
        cprintf("  0x%08x-0x%08x  %s  0x%08x-0x%08x",
                va, va+size-1, sizestr, pa, (pa+size-1<0xFFFFFFFF)?pa+size-1:0xFFFFFFFF);

    printpermission(ptep);
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
    extern pde_t *kern_pgdir;
    extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

    uintptr_t va1, va2, i, pttop;
    pde_t *pdep;
    pte_t *ptep;
    int count, over;

    count = 0;
    over = 0;

    if (argc < 2) {
        cprintf("Usage: showmappings start_addr [end_addr]\n");
        cprintf("       all mappings start within [sa, ea] will be printed\n");
    }
    else if (argc > 3)
        cprintf("mon_showmappings: Too many arguments: %d\n", argc);
    else {
        va1 = ROUNDUP(strtol(argv[1], NULL, 16), PGSIZE);
        if (argc == 3)
            va2 = ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);
        else
            va2 = va1;

        // start with a 4-MByte page, va1 align with upper page
        pdep = &kern_pgdir[PDX(va1)];
        if (pdep && (*pdep & PTE_P) && (*pdep & PTE_PS))
            va1 = ROUNDUP(va1, PTSIZE);

        printheader();

        // the boundary conditions are used to prevent overflow
        if (va1 > va2)
            cprintf("                                 EMPTY                               \n");

        for (i = va1; va1 <= i && i <= va2; i = pttop) {
            if (over || flushscreen(count, 20, 1))
                break;
            pttop = ROUNDUP(i+1, PTSIZE);
            pdep = &kern_pgdir[PDX(i)];
            if (pdep && (*pdep & PTE_P)) {
                if (!(*pdep & PTE_PS)) {
                    // 4-KByte
                    for (i; i < pttop; i += PGSIZE) {
                        if ((over = (flushscreen(count, 20, 1) == 1 ||
                                     i > va2 || i < va1)))
                            break;
                        ptep = pgdir_walk(kern_pgdir, (const void *)i, 0);
                        if (ptep &&(*ptep & PTE_P))
                            printmap(ptep, i, PGSIZE);
                        else // pte unmapped
                            printmap(ptep, i, -PGSIZE);
                        count++;
                    }
                } else {
                    // 4-MByte
                    printmap(pdep, ROUNDDOWN(i, PTSIZE), PTSIZE);
                    count++;
                }
            } else {
                // pde unmapped
                printmap(pdep, i, -PTSIZE);
                count++;
            }
        }
    }


    cprintf(" --------------------------------END---------------------------------\n");

    return 0;
}

void
setperm(pte_t *ptep, int perms[])
{
    if (perms[0] == 1)
        *ptep |= PTE_P;
    else if (perms[0] == -1)
        *ptep &= ~PTE_P;
    if (perms[1] == 1)
        *ptep |= PTE_U;
    else if (perms[1] == -1)
        *ptep &= ~PTE_U;
    if (perms[2] == 1)
        *ptep |= PTE_W;
    else if (perms[2] == -1)
        *ptep &= ~PTE_W;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
    extern pde_t *kern_pgdir;
    extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

    pde_t *pdep;
    pte_t *ptep;
    uintptr_t addr;
    int perms[3], add;
    int i;

    // check arguments
    if (argc != 3 || (argc == 3 && strlen(argv[2]) > 4)) {
        cprintf("Usage: setperm addr [+|-]perm\n");
        cprintf("       perm = {P, U, W}\n");
        return 0;
    }

    if (argv[2][0] == '+') {
        i = 1;
        add = 1;
    } else if (argv[2][0] == '-') {
        i = 1;
        add = -1;
    } else {
        i = 0;
        add = 1;
    }

    perms[0] = perms[1] = perms[2] = 0;
    for (; i < strlen(argv[2]); i++) {
        if (!(argv[2][i]=='P' || argv[2][i]=='U' || argv[2][i]=='W'
              ||argv[2][i]=='p' || argv[2][i]=='u' || argv[2][i]=='w')) {
            cprintf("Usage: setperm addr [+/-]perm\n");
            cprintf("       perm = {P, U, W}\n");
            return 0;
        }
        if (argv[2][i] == 'P' || argv[2][i] == 'p' )
            perms[0] = 1 * add;
        if (argv[2][i] == 'U' || argv[2][i] == 'u' )
            perms[1] = 1 * add;
        if (argv[2][i] == 'W' || argv[2][i] == 'w' )
            perms[2] = 1 * add;
    }

    addr = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);

    pdep = &kern_pgdir[PDX(addr)];
    if (pdep) { // no need to judge whether PTE_P stands
        if (*pdep & PTE_PS) {
            // 4-MByte
            addr = ROUNDDOWN(addr, PTSIZE);
            printmap(pdep, addr, PTSIZE);
            setperm(pdep, perms);
            cprintf(" ---->\n");
            printmap(pdep, addr, PTSIZE);
        } else {
            // 4-KByte
            ptep = pgdir_walk(kern_pgdir, (const void *) addr, 0);
            if (ptep) {
                printmap(ptep, addr, PGSIZE);
                setperm(ptep, perms);
                cprintf(" ---->\n");
                printmap(ptep, addr, PGSIZE);
            } else
                printmap(ptep, addr, -PGSIZE);
        }
    } else {
        // pde unmapped
        addr = ROUNDDOWN(addr, PTSIZE);
        printmap(pdep, addr, -PTSIZE);
    }
    return 0;
}

int
checkmapping(uintptr_t va)
{
    extern pde_t *kern_pgdir;
    extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

    pte_t *ptep;

    ptep = pgdir_walk(kern_pgdir, (const void *)va, 0);

    if (ptep && (*ptep & PTE_P))
        return 1;
    return 0;
}

int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
    uintptr_t va1, va2, i;

    if (argc != 3)
        cprintf("Usage: dump start_addr len\n");
    else {
        va1 = strtol(argv[1], NULL, 16);
        va2 = (uintptr_t)((uint32_t *)va1 + strtol(argv[2], NULL, 10));

        for (i = 0; va1 < va2; va1 = (uintptr_t)((uint32_t *)va1 + 1), i++){
            if (flushscreen(i, 23*4, 0))
                break;
            if (!(i % 4))
                cprintf("0x%08x:", va1);

            if (checkmapping(va1))
                cprintf("\t0x%08x", *(uint32_t *)va1);
            else
                cprintf("\tpgunmapped");

            if (i % 4 == 3)
                cprintf("\n");
        }
        if (i % 4)
            cprintf("\n");
    }
    return 0;
}

int
mon_shutdown(int argc, char **argv, struct Trapframe *tf)
{

    // referenced chaOS from github
    // [https://github.com/Kijewski/chaOS/]

    const char *s = "Shutdown";

    __asm __volatile ("cli");

    for (;;) {
        // (phony) ACPI shutdown (http://forum.osdev.org/viewtopic.php?t=16990)
        // Works for qemu and bochs.
        outw (0xB004, 0x2000);

        // Magic shutdown code for bochs and qemu.
        while(*s) {
            outb (0x8900, *s);
            ++s;
        }

        // Magic code for VMWare. Also a hard lock.
        __asm __volatile ("cli; hlt");
    }
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	//cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("\033[31mWelcome \033[32mto \033[33mthe \033[34mJOS \033[35mkernel \033[36mmonitor!\033[0m\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
