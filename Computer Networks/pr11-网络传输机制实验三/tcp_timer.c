#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"
#include "ip.h"

#include <unistd.h>
#include <stdio.h>
static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	struct tcp_sock *tsk;
	struct tcp_timer *t, *q;
	list_for_each_entry_safe(t, q, &timer_list, list)
	{
		if(t->type == 0)
		{
			t->timeout -= TCP_TIMER_SCAN_INTERVAL;
			if (t->timeout <= 0)
			{
				list_delete_entry(&t->list);
				tsk = timewait_to_tcp_sock(t);
				if (!tsk->parent) tcp_bind_unhash(tsk);
				tcp_set_state(tsk, TCP_CLOSED);
				free_tcp_sock(tsk);
			}
		}
		else
		{
			tsk = timer_to_tcp_sock(t);
			pthread_mutex_lock(&(tsk->timer_lock));
			t->timeout -= TCP_TIMER_SCAN_INTERVAL;
			if(t->timeout <= 0)
			{
				pthread_mutex_lock(&(tsk->send_lock));
				struct send_buffer* buf;
				list_for_each_entry(buf, &(tsk->send_list), list)
				{
					if(buf->num >= 10)
					{
						exit(1);
					}
					else
					{
						char* temp;
						temp = (char *)malloc((buf->tot_size));
						memcpy(temp, buf->pkt, buf->tot_size);
						ip_send_packet(temp, buf->tot_size);
						t->timeout = TCP_TIMER_SEND;
						buf->num++;
						for(int i = 0; i < buf->num; i++) t->timeout*=2;
					}
					break;
				}
				pthread_mutex_unlock(&(tsk->send_lock));
			}
			pthread_mutex_unlock(&(tsk->timer_lock));
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->timewait;

	timer->type = 0;
	timer->timeout = TCP_TIMEWAIT_TIMEOUT;
	list_add_tail(&timer->list, &timer_list);

	tcp_sock_inc_ref_cnt(tsk);
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}

void init_timer(struct tcp_sock* tsk)
{
	struct tcp_timer *timer = &tsk->timer_send;

	timer->type = 1;
	timer->timeout = TCP_TIMER_SEND;
	list_add_tail(&timer->list, &timer_list);

	tcp_sock_inc_ref_cnt(tsk);	
}

void reset_timer(struct tcp_sock* tsk)
{
	pthread_mutex_lock(&(tsk->timer_lock));
	struct tcp_timer *timer = &tsk->timer_send;
	timer->timeout = TCP_TIMER_SEND;
	pthread_mutex_unlock(&(tsk->timer_lock));
}
