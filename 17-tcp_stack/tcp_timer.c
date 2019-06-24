#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_sock *tsk;
	struct tcp_timer *p, *q;
	list_for_each_entry_safe(p, q, &timer_list, list)
	{
		p->timeout -= TCP_TIMER_SCAN_INTERVAL;
		if(p->timeout > 0)
			continue;
		if(p->type == 0)
		{
			tsk = timewait_to_tcp_sock(p);
			tcp_bind_unhash(tsk);
			tcp_set_state(tsk, TCP_CLOSED);
			list_delete_entry(&p->list);
			free_tcp_sock(tsk);
		}
		else if(p->type == 1)
		{
			tsk = retranstimer_to_tcp_sock(p);
			struct tbd_data_block *dblk = list_entry(tsk->send_buf.list.next, struct tbd_data_block, list);
			if(dblk->times >= 3)
			{
				tcp_send_control_packet(tsk, TCP_RST | TCP_ACK);
				continue;
			}
			dblk->times += 1;
			u32 cur_seq = tsk->snd_nxt;
			tsk->snd_nxt = dblk->seq;
			if(dblk->flags & (TCP_SYN | TCP_FIN))
			{
				tcp_send_control_packet(tsk, dblk->flags);
			}
			else
			{
				int pkt_len  = dblk->len + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE;
				char *packet = malloc(pkt_len);
				memcpy(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE, dblk->packet, dblk->len);
				tcp_send_packet(tsk, packet, pkt_len);
			}
			tsk->snd_nxt = cur_seq;
			p->timeout = TCP_RETRANS_INTERVAL_INITIAL * (1 << (dblk->times + 1));
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	tsk->timewait.type = 0;
	tsk->timewait.timeout = TCP_TIMEWAIT_TIMEOUT;
	tsk->ref_cnt += 1;
	list_add_tail(&tsk->timewait.list, &timer_list);
}

void tcp_set_retrans_timer(struct tcp_sock *tsk)
{
	tsk->retrans_timer.type = 1;
	tsk->retrans_timer.enable = 1;
	tsk->retrans_timer.timeout = TCP_RETRANS_INTERVAL_INITIAL;
	tsk->ref_cnt += 1;
	list_add_tail(&tsk->retrans_timer.list, &timer_list);
}

void tcp_unset_retrans_timer(struct tcp_sock *tsk)
{
	tsk->retrans_timer.enable = 0;
	tsk->retrans_timer.timeout = 0;
	tsk->ref_cnt -= 1;
	list_delete_entry(&tsk->retrans_timer.list);
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		pthread_mutex_lock(&send_buf_lock);
		tcp_scan_timer_list();
		pthread_mutex_unlock(&send_buf_lock);
	}

	return NULL;
}
