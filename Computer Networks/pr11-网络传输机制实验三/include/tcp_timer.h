#ifndef __TCP_TIMER_H__
#define __TCP_TIMER_H__

#include "list.h"

#include <stddef.h>

struct tcp_timer {
	int type;	// now only support time-wait
	int timeout;	// in micro second
	struct list_head list;
};

struct tcp_sock;
#define timewait_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, timewait))

#define timer_to_tcp_sock(t)\
	(struct tcp_sock *)((char *)(t) - offsetof(struct tcp_sock, timer_send))

#define TCP_TIMER_SCAN_INTERVAL 10000
#define TCP_MSL			1000000
#define TCP_TIMEWAIT_TIMEOUT	(2 * TCP_MSL)
#define TCP_TIMER_SEND 200000

// the thread that scans timer_list periodically
void *tcp_timer_thread(void *arg);
// add the timer of tcp sock to timer_list
void tcp_set_timewait_timer(struct tcp_sock *);
void init_timer(struct tcp_sock* tsk);
void reset_timer(struct tcp_sock* tsk);

#endif
