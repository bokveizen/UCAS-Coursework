#include "common.h"
#include "scheduler.h"
#include "util.h"

unsigned int task2_timer, tp_sw_timer;

void thread4(void)
{
	task2_timer = get_timer();
	do_yield();
	task2_timer = (get_timer() - task2_timer)/2;
	print_location(1, 2);
	printstr("Thread-process switching time (ms):");
	printint(44,2,((unsigned int) task2_timer)/MHZ); 
	do_exit();
}

void thread5(void)
{
	task2_timer = get_timer() - task2_timer;
	print_location(1, 1);
	printstr("Thread to thread time (ms):");
	printint(34,1,((unsigned int) task2_timer)/MHZ);
	task2_timer = get_timer();
	do_yield();
	do_exit();
}

