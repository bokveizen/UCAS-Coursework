/* scheduler.c */

#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "util.h"
#include "queue.h"

int scheduler_count;
// process or thread runs time
uint64_t cpu_time;

void printstr(char *s);
void printnum(unsigned long long n);
void scheduler(void)
{
	++scheduler_count;

	current_running = queue_pop(ready_queue);
	current_running->state=PROCESS_RUNNING;
}

void do_yield(void)
{
	save_pcb();

	current_running->state = PROCESS_READY;
	queue_push(ready_queue,current_running);

	scheduler_entry();

	ASSERT(0);
}

void do_exit(void)
{
	current_running->state = PROCESS_EXITED;
	scheduler_entry();
}

void block(void)
{
	save_pcb();
	current_running->state = PROCESS_BLOCKED;
	queue_push(blocked_queue, current_running);
	scheduler_entry();
	ASSERT(0);
}

int unblock(void)
{
	pcb_t *pcb_temp;
	int i;
	int block_count = queue_size(blocked_queue);
	ready_queue->head = (ready_queue->head - block_count + ready_queue->capacity) % ready_queue->capacity;
	i = ready_queue->head;
	while(block_count)
	{
		pcb_temp = queue_pop(blocked_queue);
		ready_queue->pcbs[i] = pcb_temp;
		if(ready_queue->isEmpty == TRUE) ready_queue->isEmpty = FALSE;
		i = (i+1)%ready_queue->capacity;
		block_count--;
	}
	return blocked_tasks;
}

bool_t blocked_tasks(void)
{
	return !blocked_queue->isEmpty;
}

