/* Author(s): Bu Fanchen
 * COS 318, Fall 2013: Project 3 Pre-emptive Scheduler
 * Implementation of the process scheduler for the kernel.
 */

#include "common.h"
#include "interrupt.h"
#include "queue.h"
#include "printf.h"
#include "scheduler.h"
#include "util.h"
#include "syslib.h"

pcb_t *current_running;
node_t ready_queue;
node_t sleep_wait_queue;
// more variables...
volatile uint32_t time_elapsed;

priority_t raw_priority[MAX_PID];
priority_t act_priority[MAX_PID];

/* TODO:wake up sleeping processes whose deadlines have passed */
void check_sleeping(){
  uint32_t current_time = do_gettimeofday();
  node_t* temp;
  while(!is_empty(&sleep_wait_queue) && ((pcb_t*) peek(&sleep_wait_queue))-> deadline <= current_time) //existing at least one task need to be waked up
  {
      temp = dequeue(&sleep_wait_queue);
      enqueue(&ready_queue, temp);
      ((pcb_t *)temp)->status = READY;
  }
}

/* Round-robin scheduling: Save current_running before preempting */
void put_current_running(){
  enqueue(&ready_queue, (node_t *) current_running);
  current_running->status = READY;
}

/* Change current_running to the next task */
void scheduler(){
     ASSERT(disable_count);
     check_sleeping();
     while (is_empty(&ready_queue)){
          leave_critical();
          enter_critical();
          check_sleeping();
     }
     current_running = (pcb_t *) dequeue(&ready_queue);
     if(act_priority[current_running->pid]<=0) //priority used out 
	 {
          pcb_t *temp = current_running;
          do {
               enqueue(&ready_queue, (node_t *) current_running);
               current_running = (pcb_t *) dequeue(&ready_queue);   //run the next task
          } while(act_priority[current_running->pid]<=0 && temp != current_running);	//the next task also pr used out and exist at least one having pr					
          if(act_priority[current_running->pid]<=0) //all tasks pr used out
               do {
                    enqueue(&ready_queue, (node_t *) current_running);
                    current_running = (pcb_t *) dequeue(&ready_queue);
                    act_priority[current_running->pid] = raw_priority[current_running->pid];
               } while(temp != current_running);					//restore the prs
     }
     act_priority[current_running->pid]--; //run once, pr -1
     ASSERT(NULL != current_running);
     current_running->entry_count++;	//run counting +1

}

int lte_deadline(node_t *a, node_t *b) {
     pcb_t *x = (pcb_t *)a;
     pcb_t *y = (pcb_t *)b;

     if (x->deadline > y->deadline) {
          return 1;
     } else {
          return 0;
     }
}

void do_sleep(int milliseconds){
     ASSERT(!disable_count);

     enter_critical();
     // TODO
     current_running->status = SLEEPING;
     current_running->deadline = do_gettimeofday() + milliseconds; //deadline update
     enqueue_sort(&sleep_wait_queue, (node_t *) current_running, lte_deadline);
     scheduler_entry();

     leave_critical();
}

void do_yield(){
     enter_critical();
     put_current_running();
     scheduler_entry();
     leave_critical();
}

void do_exit(){
     enter_critical();
     current_running->status = EXITED;
     scheduler_entry();
     /* No need for leave_critical() since scheduler_entry() never returns */
}

void block(node_t * wait_queue){
     ASSERT(disable_count);
     current_running->status = BLOCKED;
     enqueue(wait_queue, (node_t *) current_running);
     enter_critical();
     scheduler_entry();
     leave_critical();
}

void unblock(pcb_t * task){
     ASSERT(disable_count);
     task->status = READY;
     enqueue(&ready_queue, (node_t *) task);
}

pid_t do_getpid(){
     pid_t pid;
     enter_critical();
     pid = current_running->pid;
     leave_critical();
     return pid;
}

uint32_t do_gettimeofday(void){
     return time_elapsed;
}

priority_t do_getpriority(){
	/* TODO */
     return raw_priority[current_running->pid];
}


void do_setpriority(priority_t priority){
	/* TODO */
     if(priority < 0 || priority >= MAX_PRIORITY)
          return;
     raw_priority[current_running->pid] = priority;
     act_priority[current_running->pid] = priority;
}

uint32_t get_timer(void) {
     return do_gettimeofday();
}

