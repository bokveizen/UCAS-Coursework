/* kernel.h: definitions used by kernel code */

#ifndef KERNEL_H
#define KERNEL_H

#define NUM_REGISTERS 8

#include "common.h"

/* ENTRY_POINT points to a location that holds a pointer to kernel_entry */
#define ENTRY_POINT ((void (**)(int)) 0xa080fff8)

/* System call numbers */
enum {
    SYSCALL_YIELD,
    SYSCALL_EXIT,
};

/* All stacks should be STACK_SIZE bytes large
 * The first stack should be placed at location STACK_MIN
 * Only memory below STACK_MAX should be used for stacks
 */
enum {
    STACK_MIN = 0xa1000000,
    STACK_SIZE = 0x10000,
    STACK_MAX = 0xafffffff,
};

typedef enum {
	PROCESS_BLOCKED,
	PROCESS_READY,
	PROCESS_RUNNING,
	PROCESS_EXITED,
} process_state;

typedef struct pcb {
	uint32_t reg[32];//Regs
	int pid;
	process_state state;
	/* need student add */
} pcb_t;
/* The task currently running.  Accessed by scheduler.c and by entry.s assembly methods */
extern volatile pcb_t *current_running;

void kernel_entry(int fn);

#endif                          /* KERNEL_H */
