#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>

// handling incoming packet for TCP_LISTEN state
//
// 1. malloc a child tcp sock to serve this connection request; 
// 2. send TCP_SYN | TCP_ACK by child tcp sock;
// 3. hash the child tcp sock into established_table (because the 4-tuple 
//    is determined).
void tcp_state_listen(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if(cb->flags & TCP_SYN)
	{
		struct tcp_sock* new_tcp_sock = alloc_tcp_sock();
	
		new_tcp_sock->sk_sip	= cb->daddr;
		new_tcp_sock->sk_sport	= cb->dport;
		new_tcp_sock->sk_dip	= cb->saddr;
		new_tcp_sock->sk_dport	= cb->sport;
		new_tcp_sock->parent	= tsk;
	
		list_add_tail(&(new_tcp_sock->list), &(tsk->listen_queue));
	
		new_tcp_sock->backlog	= tsk->backlog;
		new_tcp_sock->iss		= tcp_new_iss();
		new_tcp_sock->snd_nxt	= new_tcp_sock->iss;
		new_tcp_sock->rcv_nxt	= cb->seq + 1;
		new_tcp_sock->snd_wnd	= cb->rwnd;
		new_tcp_sock->snd_una	= new_tcp_sock->iss - 1;
		new_tcp_sock->state		= TCP_SYN_RECV;
	
		tcp_hash(new_tcp_sock);
		tcp_send_control_packet(new_tcp_sock, TCP_SYN | TCP_ACK);
	}

	else if(!(cb->flags & TCP_RST))
	{
		tcp_send_reset(cb);
	}
}

// handling incoming packet for TCP_CLOSED state, by replying TCP_RST
void tcp_state_closed(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	tcp_send_reset(cb);
}

// handling incoming packet for TCP_SYN_SENT state
//
// If everything goes well (the incoming packet is TCP_SYN|TCP_ACK), reply with 
// TCP_ACK, and enter TCP_ESTABLISHED state, notify tcp_sock_connect; otherwise, 
// reply with TCP_RST.
void tcp_state_syn_sent(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if(cb->flags == (TCP_SYN|TCP_ACK))
	{
		if(cb->ack == tsk->snd_nxt)
		{
			tcp_set_state(tsk, TCP_ESTABLISHED);
			tsk->snd_una = cb->ack - 1;
			tsk->rcv_nxt = cb->seq + 1;
			tsk->snd_wnd = cb->rwnd;
			
			tcp_send_control_packet(tsk, TCP_ACK);
			
			if(tsk->wait_connect->sleep)
				wake_up(tsk->wait_connect);
			return;
		}
	}
	tcp_send_control_packet(tsk, TCP_RST);
	tcp_bind_unhash(tsk);
	tcp_unhash(tsk);
}

// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (!old_snd_wnd && tsk->wait_send->sleep)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

// handling incoming ack packet for tcp sock in TCP_SYN_RECV state
//
// 1. remove itself from parent's listen queue;
// 2. add itself to parent's accept queue;
// 3. wake up parent (wait_accept) since there is established connection in the
//    queue.
void tcp_state_syn_recv(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if(cb->ack == tsk->snd_nxt)
	{
		struct tcp_sock *tmp;
		list_for_each_entry(tmp, &(tsk->parent->listen_queue), list) {
			if(tmp == tsk) list_delete_entry(&(tmp->list));
		}
	
		tcp_sock_accept_enqueue(tsk);
		tcp_set_state(tsk, TCP_ESTABLISHED);
		tsk->snd_una = cb->ack - 1;
		tsk->rcv_wnd = cb->rwnd;
		if(tsk->parent->wait_accept->sleep)
			wake_up(tsk->parent->wait_accept);
	}
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

void tcp_recv_data(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	pthread_mutex_lock(&(tsk->rcv_buf->lock));
	write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
	pthread_mutex_unlock(&(tsk->rcv_buf->lock));	
	tsk->rcv_nxt += cb->pl_len;
	tsk->snd_una = cb->ack - 1;
	if(tsk->wait_recv->sleep)
		wake_up(tsk->wait_recv);
	tcp_send_control_packet(tsk, TCP_ACK);
}
// Process an incoming packet as follows:
// 	 1. if the state is TCP_CLOSED, hand the packet over to tcp_state_closed;
// 	 2. if the state is TCP_LISTEN, hand it over to tcp_state_listen;
// 	 3. if the state is TCP_SYN_SENT, hand it to tcp_state_syn_sent;
// 	 4. check whether the sequence number of the packet is valid, if not, drop
// 	    it;
// 	 5. if the TCP_RST bit of the packet is set, close this connection, and
// 	    release the resources of this tcp sock;
// 	 6. if the TCP_SYN bit is set, reply with TCP_RST and close this connection,
// 	    as valid TCP_SYN has been processed in step 2 & 3;
// 	 7. check if the TCP_ACK bit is set, since every packet (except the first 
//      SYN) should set this bit;
//   8. process the ack of the packet: if it ACKs the outgoing SYN packet, 
//      establish the connection; (if it ACKs new data, update the window;)
//      if it ACKs the outgoing FIN packet, switch to correpsonding state;
//   9. (process the payload of the packet: call tcp_recv_data to receive data;)
//  10. if the TCP_FIN bit is set, update the TCP_STATE accordingly;
//  11. at last, do not forget to reply with TCP_ACK if the connection is alive.
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	if(tsk->state == TCP_CLOSED){
		tcp_state_closed(tsk, cb, packet);
	}
	
	if(tsk->state == TCP_LISTEN){
		tcp_state_listen(tsk, cb, packet);
		return;
	}
	
	if(tsk->state == TCP_SYN_SENT){
		tcp_state_syn_sent(tsk, cb, packet);
		return;
	}
	
	if(!is_tcp_seq_valid(tsk, cb)){
		return;
	}
	
	if(cb->flags & TCP_RST){
		tcp_set_state(tsk, TCP_CLOSED);
		tcp_bind_unhash(tsk);
		if(tsk) tcp_unhash(tsk);
		return;
	}
	
	if(cb->flags & TCP_SYN){
		tcp_send_reset(cb);
		tcp_set_state(tsk, TCP_CLOSED);
		tcp_bind_unhash(tsk);
		tcp_unhash(tsk);
		return;
	}
	
	if(!(cb->flags & TCP_ACK)){
		tcp_send_reset(cb);
		tcp_set_state(tsk, TCP_CLOSED);
		tcp_bind_unhash(tsk);
		if(tsk) tcp_unhash(tsk);
		return;
	}
	
	if(tsk->state == TCP_SYN_RECV){
		tcp_state_syn_recv(tsk, cb, packet);
		return;
	}

	if(less_or_equal_32b(cb->ack, tsk->snd_nxt) && less_than_32b(tsk->snd_una, cb->ack)){
		tcp_update_window_safe(tsk, cb);
	}
	
	if(tsk->state == TCP_FIN_WAIT_1 && cb->ack == tsk->snd_nxt){
		tcp_set_state(tsk, TCP_FIN_WAIT_2);
	}
	
	if(tsk->state == TCP_LAST_ACK && cb->ack == tsk->snd_nxt){
		tcp_set_state(tsk, TCP_CLOSED);
	}
	
	if(cb->flags & TCP_FIN){
		if(tsk->state == TCP_ESTABLISHED){
			if(tsk->wait_recv->sleep) wake_up(tsk->wait_recv);
			tcp_set_state(tsk, TCP_CLOSE_WAIT);
			tsk->rcv_nxt = cb->seq + 1;
			if(less_than_32b(tsk->snd_una, cb->ack - 1)){
				tsk->snd_una = cb->ack - 1;
			}
			tcp_send_control_packet(tsk, TCP_ACK);
		}
		if(tsk->state == TCP_FIN_WAIT_2){
			tcp_set_state(tsk, TCP_TIME_WAIT);
			tsk->rcv_nxt = cb->seq + 1;
			if(less_than_32b(tsk->snd_una, cb->ack - 1)){
				tsk->snd_una = cb->ack - 1;
			}
			tcp_send_control_packet(tsk, TCP_ACK);
			tcp_set_timewait_timer(tsk);
		}
		return;
	}
	if(cb->flags == TCP_ACK){
		tsk->snd_una = cb->ack - 1;
		return;
	}
	tcp_recv_data(tsk, cb, packet);
}

