#include "common.h"
#include "syslib.h"
#include "util.h"

unsigned int task2_timer;

void _start(void)
{
	yield();
	task2_timer = get_timer();
	yield();
	task2_timer = get_timer () - task2_timer;
	print_location(1, 3);
	printstr("Process to process time (ms):");
	printint(34,3,((unsigned int) task2_timer)/MHZ);
	while(1)yield();
}

