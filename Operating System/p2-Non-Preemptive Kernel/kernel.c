/*
   kernel.c
   the start of kernel
   */

#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "th.h"
#include "util.h"
#include "queue.h"

#include "tasks.c"

volatile pcb_t *current_running;
queue_t ready_queue, blocked_queue;
pcb_t *ready_arr[NUM_TASKS], *blocked_arr[NUM_TASKS];
struct queue R_Queue, B_Queue;
struct pcb proc_table[NUM_TASKS];
void _stat(void){
  ready_queue = &R_Queue;
  ready_queue->pcbs=ready_arr;
  queue_init(ready_queue);
  ready_queue->capacity = NUM_TASKS;

  blocked_queue = &B_Queue;
  blocked_queue->pcbs=blocked_arr;
  queue_init(blocked_queue);
  blocked_queue->capacity = NUM_TASKS;


  int i = 0;
  while(i < NUM_TASKS) {
    int j;
    for(j=0;j<32;j++)
    {
      proc_table[i].reg[j] = 0;
    }
    proc_table[i].reg[29] = STACK_MIN + (STACK_SIZE * i);
    proc_table[i].reg[31] = task[i]->entry_point;
    proc_table[i].state = PROCESS_READY;
    proc_table[i].pid = i;
    queue_push(ready_queue, proc_table + i);
    i++;
  }

  clear_screen(0, 0, 30, 24);

  /*Schedule the first task */
  scheduler_count = 0;
  scheduler_entry();

  /*We shouldn't ever get here */
  ASSERT(0);
  
}

