// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(err & FEC_WR)){
		panic("pgfault: Page fault addr[%08x] NOT caused by a write\n",addr);
	}
	if(!((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
			&& (uvpt[PGNUM(addr)] & PTE_COW))){
		panic("pgfault: Page fault NOT caused by COW");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr,PGSIZE);
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0) {
		panic("pgfault: sys_page_alloc PFTEMP (%e)", r);
	}
	memcpy(PFTEMP,addr,PGSIZE);
	if((r=sys_page_map(0,PFTEMP,0,addr,PTE_P | PTE_U | PTE_W))<0){
		panic("pgfault: sys_page_map (%e)", r);
	}
	if((r=sys_page_unmap(0,PFTEMP))<0){
		panic("pgfault: sys_page_unmap (%e)", r);
	}
	return;
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	uintptr_t addr = pn*PGSIZE;
	void* p_addr = (void*)addr;

	//Lab 5:
	// If the page table entry has the PTE_SHARE bit set, just copy the mapping directly
	if(uvpt[pn] & PTE_SHARE){
		if((r=sys_page_map(0,p_addr,envid,p_addr,uvpt[pn] & PTE_SYSCALL))<0){
			//failed!
			return r;
		}
		return 0;
	}

	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		// Map page COW, U and P in child
		if((r=sys_page_map(0,p_addr,envid,p_addr,PTE_P|PTE_U|PTE_COW))){
			panic("duppage:sys_page_map fail! pn=%08x (%e)",pn,r);
		}
		// Map page COW, U and P in parent
		if ((r = sys_page_map(0, p_addr, 0, p_addr, PTE_P | PTE_U | PTE_COW)) < 0){
			panic("duppage:sys_page_map fail! pn=%08x (%e)",pn,r);
		}
	}
	//只读页
	else if((r = sys_page_map(0, p_addr, envid, p_addr, PTE_P | PTE_U)) < 0){
		panic("duppage:sys_page_map fail! read-only pn=%08x (%e)",pn,r);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	envid_t envid;
	int r;

	set_pgfault_handler(pgfault);

	if((envid = sys_exofork())==0){
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	} else if(envid<0){
		panic("fork:sys_exfork:%e\n",envid);
	}

	// We're in the parent

	//拷贝父进程的页映射关系到子进程
	uintptr_t addr;
	for(addr=0;addr<USTACKTOP;addr+=PGSIZE){
		if((uvpd[PDX(addr)] & PTE_P)
		   && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U)){
			duppage(envid,PGNUM(addr));
		}
	}
	//allocate a new page for the child's user exception stack
	//The child can't do this themselves
	// because the mechanism by which it would is to run the pgfault handler, which
	// needs to run on the exception stack (catch 22).
	if((r=sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE),
						 PTE_W | PTE_U | PTE_P)<0)){
		panic("fork:sys_page_alloc fail! (%e)\n",r);
	}

	extern void _pgfault_upcall();
	// Set page fault handler for the child
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall: %e\n", r);

	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("fork:sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
// 1、parent和child内存空间相互共享。
// 2、将parent的内存拷贝到child中（栈使用COW，异常栈相互独立），
// 3、将其他部分直接将parent的页映射到child
int
sfork(void)
{
	//panic("sfork not implemented");
	//return -E_INVAL;

    envid_t envid, thisenvid = sys_getenvid();
    int perm;
    int r;
    uint32_t i, j, pn;

    set_pgfault_handler(pgfault);

    if ((envid = sys_exofork()) == 0) {
        // child
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    for (i = PDX(UXSTACKTOP-PGSIZE); i >= PDX(UTEXT) ; i--) {
        if (uvpd[i] & PTE_P) {
            for (j = 0; j < NPTENTRIES; j++) {
                pn = PGNUM(PGADDR(i, j, 0));
                if (pn == PGNUM(UXSTACKTOP-PGSIZE))
                    break;

                if (pn == PGNUM(USTACKTOP-PGSIZE))
                    duppage(envid, pn);
                else if (uvpt[pn] & PTE_P) {

                    perm = uvpt[pn] & ~(uvpt[pn] & ~(PTE_P |PTE_U | PTE_W | PTE_AVAIL));

                    if ((r = sys_page_map(thisenvid, (void *)(PGADDR(i, j, 0)),
                                          envid, (void *)(PGADDR(i, j, 0)), perm)) < 0)
                        return r;
                }
            }
        }
    }

    if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W)) < 0)
        return r;

    if ((r = sys_page_map(envid, (void *)(UXSTACKTOP - PGSIZE), thisenvid, PFTEMP, PTE_U | PTE_P | PTE_W)) < 0)
        return r;

    memmove((void *)(UXSTACKTOP - PGSIZE), PFTEMP, PGSIZE);

    if ((r = sys_page_unmap(thisenvid, PFTEMP)) < 0)
        return r;

    extern void _pgfault_upcall(void);
    sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        return r;

    return envid;
}
