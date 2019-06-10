#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	printf("seq:%d, end:%d, nxt:%d, seqend:%d\n", cb->seq, rcv_end, tsk->rcv_nxt, cb->seq_end);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	
	if(cb->flags & TCP_ACK)
	{
		tsk->rcv_nxt = cb->seq_end;
		switch(tsk->state)
		{
		case TCP_ESTABLISHED:
			if(cb->pl_len > 0)
			{
				pthread_mutex_lock(&rcv_buf_lock);
				write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
				tsk->rcv_wnd -= cb->pl_len;
				pthread_mutex_unlock(&rcv_buf_lock);
				wake_up(tsk->wait_recv);
				tcp_send_control_packet(tsk, TCP_ACK);
			}
			else
			{
				tcp_update_window(tsk, cb);
			}
			break;
		case TCP_SYN_RECV:
			tcp_set_state(tsk, TCP_ESTABLISHED);
			tcp_sock_accept_enqueue(tsk);
			wake_up(tsk->parent->wait_accept);
			break;
		case TCP_FIN_WAIT_1:
			tcp_set_state(tsk, TCP_FIN_WAIT_2);
			break;
		case TCP_LAST_ACK:
			tcp_set_state(tsk, TCP_CLOSED);
			break;
		default:
			break;
		}
	}

	if(cb->flags & TCP_RST)
	{
		//RST: reset the tcp sock
		tcp_sock_close(tsk);
		return ;
	}
	else if(cb->flags & TCP_SYN)
	{
		//SYN & ACK: 
		if((cb->flags & TCP_ACK) && tsk->state == TCP_SYN_SENT)
		{
			wake_up(tsk->wait_connect);
			tcp_send_control_packet(tsk, TCP_ACK);
			tcp_set_state(tsk, TCP_ESTABLISHED);
		}
		else if(tsk->state == TCP_LISTEN) //SYN: 
		{
			struct tcp_sock *child_sock = alloc_tcp_sock();
			child_sock->local.ip = cb->daddr;
			child_sock->local.port = cb->dport;
			child_sock->peer.ip = cb->saddr;
			child_sock->peer.port = cb->sport;
			child_sock->parent = tsk;
			child_sock->iss = tcp_new_iss();
			child_sock->snd_una = child_sock->iss;
			child_sock->snd_nxt = child_sock->iss;
			child_sock->rcv_nxt = cb->seq_end;
			
			tcp_set_state(child_sock, TCP_SYN_RECV);
			tcp_sock_listen_enqueue(child_sock);
			tcp_send_control_packet(child_sock, TCP_SYN | TCP_ACK);
			tcp_hash(child_sock);
		}
		
	} 
	else if(cb->flags & TCP_FIN)
	{
		switch (tsk->state)
		{
		case TCP_ESTABLISHED:
			tcp_send_control_packet(tsk, TCP_ACK);
			tcp_set_state(tsk, TCP_CLOSE_WAIT);
			wake_up(tsk->wait_recv);
			break;
		
		case TCP_FIN_WAIT_2:
			tcp_send_control_packet(tsk, TCP_ACK);
			tcp_set_state(tsk, TCP_TIME_WAIT);
			tcp_set_timewait_timer(tsk);
			break;

		default:
			break;
		}
	}
}
