#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);

//关机
int mon_shutdown(int argc, char **argv, struct Trapframe *tf);

// 显示页映射
// Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof)
// that apply to a particular range of virtual/linear addresses in the currently active address space.
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);


#endif	// !JOS_KERN_MONITOR_H
