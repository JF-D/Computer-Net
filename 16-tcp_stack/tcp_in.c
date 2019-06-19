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
	//printf("seq:%u, end:%u, nxt:%u, seqend:%u\n", cb->seq, rcv_end, tsk->rcv_nxt, cb->seq_end);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

void tcp_send_FIN_ACK(struct tcp_sock *tsk)
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

void tcp_rcv_ofo_pkt(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	struct tbd_data_block *tmp, *q, *dblk;
	tmp = list_entry(tsk->rcv_ofo_buf.list.next, struct tbd_data_block, list);
	dblk = new_tbd_data_block(cb->flags, cb->seq, cb->pl_len, cb->payload);
	list_for_each_entry_safe(tmp, q, &tsk->rcv_ofo_buf.list, list)
	{
		if(tmp->seq > cb->seq)
			break;
	}
	q = list_entry(tmp->list.prev, struct tbd_data_block, list);
	list_insert(&dblk->list, &q->list, &tmp->list);
}

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	
	if(cb->flags & TCP_ACK)
	{
		//tsk->rcv_nxt = cb->seq_end;
		switch(tsk->state)
		{
		case TCP_ESTABLISHED:
			if(cb->pl_len > 0)
			{
				if(!is_tcp_seq_valid(tsk, cb))
					break;
				if(tsk->rcv_nxt != cb->seq)
				{
					tcp_rcv_ofo_pkt(tsk, cb);
					tcp_send_control_packet(tsk, TCP_ACK);
					break;
				}
				
				pthread_mutex_lock(&rcv_buf_lock);
				write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
				tsk->rcv_wnd -= cb->pl_len;
				tsk->rcv_nxt = cb->seq_end;
				struct tbd_data_block *tmp, *q;
				list_for_each_entry_safe(tmp, q, &tsk->rcv_ofo_buf.list, list)
				{
					//printf("***** %u %u %u\n", tsk->rcv_nxt, tmp->len, tmp->seq);
					if(tmp->seq == tsk->rcv_nxt)
					{
						/* while(ring_buffer_free(tsk->rcv_buf) < tmp->len)
						{
							pthread_mutex_unlock(&rcv_buf_lock);
							wake_up(tsk->wait_recv);
							sleep_on(tsk->wait_send);
							pthread_mutex_lock(&rcv_buf_lock);
						}*/
						write_ring_buffer(tsk->rcv_buf, tmp->packet, tmp->len);
						tsk->rcv_wnd -= tmp->len;
						tsk->rcv_nxt = tmp->seq_end;
						u8 flags = tmp->flags;
						list_delete_entry(&tmp->list);
						free(tmp->packet);
						free(tmp);
						if(flags & TCP_FIN)
						{
							tcp_send_FIN_ACK(tsk);
							break;
						}
					}
					else
						break;
				}
				pthread_mutex_unlock(&rcv_buf_lock);
				wake_up(tsk->wait_recv);
				tcp_send_control_packet(tsk, TCP_ACK);
			}
			else
			{
				tsk->snd_wnd = cb->rwnd;
				wake_up(tsk->wait_send);
			}
			struct tbd_data_block *tmp, *q;
			pthread_mutex_lock(&send_buf_lock);
			list_for_each_entry_safe(tmp, q, &tsk->send_buf.list, list)
			{
				if(tmp->seq_end <= cb->ack)
				{
					list_delete_entry(&tmp->list);
					free(tmp->packet);
					free(tmp);
					tcp_unset_retrans_timer(tsk);
					if(!list_empty(&tsk->send_buf.list))
						tcp_set_retrans_timer(tsk);
				}
			}
			pthread_mutex_unlock(&send_buf_lock);
			break;
		case TCP_SYN_RECV:
			tsk->rcv_nxt = cb->seq_end;
			tcp_set_state(tsk, TCP_ESTABLISHED);
			tcp_sock_accept_enqueue(tsk);
			wake_up(tsk->parent->wait_accept);
			break;
		case TCP_FIN_WAIT_1:
			tsk->rcv_nxt = cb->seq_end;
			tcp_set_state(tsk, TCP_FIN_WAIT_2);
			break;
		case TCP_LAST_ACK:
			tsk->rcv_nxt = cb->seq_end;
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
			tsk->rcv_nxt = cb->seq_end;
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
			tcp_hash(child_sock);
			struct tbd_data_block *dblk = new_tbd_data_block(TCP_SYN | TCP_ACK, child_sock->snd_nxt, 0, NULL);
			tcp_send_control_packet(child_sock, TCP_SYN | TCP_ACK);
			pthread_mutex_lock(&send_buf_lock);
			list_add_tail(&dblk->list, &child_sock->send_buf.list);
			pthread_mutex_unlock(&send_buf_lock);
			if(!child_sock->retrans_timer.enable)
				tcp_set_retrans_timer(child_sock);
		}
		
	}
	else if(cb->flags & TCP_FIN)
	{
		if(cb->seq != tsk->rcv_nxt)
			return ;
		tcp_send_FIN_ACK(tsk);
	}
}
