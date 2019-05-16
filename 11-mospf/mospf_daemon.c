#include <time.h>
#include "packet.h"
#include "rtable.h"
#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

//#include "ether.h"
#include "ip.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);
void *spf_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, db, spf;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
	pthread_create(&spf, NULL, spf_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	//fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	while(1)
	{
		sleep(MOSPF_DEFAULT_HELLOINT);
		pthread_mutex_lock(&mospf_lock);
		iface_info_t *iface;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
			char *packet = malloc(len);
			
			struct ether_header *eth = (struct ether_header*)packet;
			memcpy(eth->ether_shost, iface->mac, ETH_ALEN);

			u8 dhost[ETH_ALEN] = {0x01, 0, 0x5e, 0, 0, 0x05};
			memcpy(eth->ether_dhost, dhost, ETH_ALEN);
			eth->ether_type = htons(ETH_P_IP);
			
			struct mospf_hello *hello = (struct mospf_hello*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
			mospf_init_hello(hello, iface->mask);

			struct mospf_hdr *mhdr = (struct mospf_hdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
			mospf_init_hdr(mhdr, 1, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, instance->router_id, 0);
			mhdr->checksum = mospf_checksum(mhdr);

			struct iphdr *iph = packet_to_ip_hdr(packet);
			ip_init_hdr(iph, iface->ip, MOSPF_ALLSPFRouters, len - ETHER_HDR_SIZE, 90);

			//printf("rid:"IP_FMT" IP:"IP_FMT" mask:"IP_FMT"\n", HOST_IP_FMT_STR(instance->router_id), HOST_IP_FMT_STR(iface->ip), HOST_IP_FMT_STR(iface->mask));
			iface_send_packet(iface, packet, len);
		}
		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

void *checking_nbr_thread(void *param)
{
	//fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	while(1)
	{
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		iface_info_t *iface;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			mospf_nbr_t *pos = NULL, *q = NULL;
			list_for_each_entry_safe(pos, q, &iface->nbr_list, list)
			{
				if(pos->alive > 3 * MOSPF_DEFAULT_HELLOINT)
				{
					list_delete_entry(&pos->list);
					free(pos);
					iface->num_nbr --;
				}
				else
				{
					pos->alive ++;
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

void *checking_database_thread(void *param)
{
	//fprintf(stdout, "TODO: link state database timeout operation.\n");
	while(1)
	{
		sleep(1);
		time_t now = time(NULL);
		pthread_mutex_lock(&mospf_lock);
		mospf_db_entry_t *db_entry, *p;
		list_for_each_entry_safe(db_entry, p, &mospf_db, list)
		{
			if(now - db_entry->upd > MOSPF_DATABASE_TIMEOUT)
			{
				list_delete_entry(&db_entry->list);
				free(db_entry->array);
				free(db_entry);
			}
		}

		printf("\n\nmospf database:\n");
		list_for_each_entry_safe(db_entry, p, &mospf_db, list)
		{
			for(int i = 0; i < db_entry->nadv; i++)
				printf("rid:"IP_FMT" subnet:"IP_FMT" mask:"IP_FMT" nbr:"IP_FMT"\n", HOST_IP_FMT_STR(db_entry->rid), \
					HOST_IP_FMT_STR(db_entry->array[i].subnet), HOST_IP_FMT_STR(db_entry->array[i].mask), \
					HOST_IP_FMT_STR(db_entry->array[i].rid));
		}
		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	//fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	pthread_mutex_lock(&mospf_lock);
	struct iphdr *iph = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhdr = (struct mospf_hdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct mospf_hello *hello = (struct mospf_hello*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);

	int find = 0;
	//printf("iface:"IP_FMT"\n", HOST_IP_FMT_STR(iface->ip));
	mospf_nbr_t *nbr;
	list_for_each_entry(nbr, &iface->nbr_list, list)
	{
		//printf("nbr-- rid:"IP_FMT" IP:"IP_FMT" mask:"IP_FMT"\n", HOST_IP_FMT_STR(nbr->nbr_id), HOST_IP_FMT_STR(nbr->nbr_ip), HOST_IP_FMT_STR(nbr->nbr_mask));
		if(nbr->nbr_ip == ntohl(iph->saddr))
		{
			nbr->alive = 0;
			find = 1;
		}
	}

	if(!find)
	{
		nbr = (mospf_nbr_t *)malloc(sizeof(mospf_nbr_t));
		init_list_head(&nbr->list);
		nbr->nbr_id = ntohl(mhdr->rid);
		nbr->nbr_ip = ntohl(iph->saddr);
		nbr->nbr_mask = ntohl(hello->mask);
		nbr->alive = 0;
		//printf("nbr-- rid:"IP_FMT" IP:"IP_FMT" mask:"IP_FMT"\n", HOST_IP_FMT_STR(nbr->nbr_id), HOST_IP_FMT_STR(nbr->nbr_ip), HOST_IP_FMT_STR(nbr->nbr_mask));
		list_add_tail(&nbr->list, &iface->nbr_list);
		iface->num_nbr ++;
	}
	pthread_mutex_unlock(&mospf_lock);
}

void *sending_mospf_lsu_thread(void *param)
{
	//fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	while(1)
	{
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		if(--instance->lsuint <= 0)
		{
			int nadv = 0;
			iface_info_t *iface;
			list_for_each_entry(iface, &instance->iface_list, list)
			{
				if(iface->num_nbr == 0)
					nadv++;
				else
					nadv += iface->num_nbr;
			}

			int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nadv * MOSPF_LSA_SIZE;
			char *packet = malloc(len);
			struct mospf_hdr *hdr = (struct mospf_hdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
			struct mospf_lsu *lsu = (struct mospf_lsu*)((char *)hdr + MOSPF_HDR_SIZE);
			struct mospf_lsa *lsa = (struct mospf_lsa*)((char *)lsu + MOSPF_LSU_SIZE);

			int i = 0;
			list_for_each_entry(iface, &instance->iface_list, list)
			{
				if(iface->num_nbr == 0)
				{
					lsa[i].rid = htonl(0);
					lsa[i].subnet = htonl(iface->ip & iface->mask);
					lsa[i].mask = htonl(iface->mask);
					++i;
					continue;
				}
				mospf_nbr_t *nbr;
				list_for_each_entry(nbr, &iface->nbr_list, list)
				{
					lsa[i].rid = htonl(nbr->nbr_id);
					lsa[i].subnet = htonl(nbr->nbr_ip & nbr->nbr_mask);
					lsa[i].mask = htonl(nbr->nbr_mask);
					++i;
				}
			}

			mospf_init_lsu(lsu, nadv);
			mospf_init_hdr(hdr, MOSPF_TYPE_LSU, len-ETHER_HDR_SIZE-IP_BASE_HDR_SIZE, instance->router_id, instance->area_id);
			hdr->checksum = mospf_checksum(hdr);

			list_for_each_entry(iface, &instance->iface_list, list)
			{
				mospf_nbr_t *nbr;
				list_for_each_entry(nbr, &iface->nbr_list, list)
				{
					char *pkt = malloc(len);
					memcpy(pkt, packet, len);
					struct iphdr *iph = packet_to_ip_hdr(pkt);
					ip_init_hdr(iph, iface->ip, nbr->nbr_ip, len-ETH_ALEN, 90);
					struct ether_header *eth = (struct ether_header *)pkt;
					memcpy(eth->ether_shost, iface->mac, ETH_ALEN);
					eth->ether_type = htons(ETH_P_IP);
					ip_send_packet(pkt, len);
				}
			}
			free(packet);
			++instance->sequence_num;
			instance->lsuint = MOSPF_DEFAULT_LSUINT;
		}
		pthread_mutex_unlock(&mospf_lock);
	}

	return NULL;
}

int G[10][10];
int d[10], pre[10], vis[10];
u32 gate_way[10];
iface_info_t *fwd_iface[10];

//return: number of nodes
int build_graph()
{
	int n = 1;
	
	mospf_db_entry_t *db_entry;
	list_for_each_entry(db_entry, &mospf_db, list)
		n++;
	
	d[0] = 0;
	iface_info_t *iface;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		mospf_nbr_t *nbr;
		list_for_each_entry(nbr, &iface->nbr_list, list)
		{
			int k = 1;
			list_for_each_entry(db_entry, &mospf_db, list)
			{
				if(db_entry->rid == nbr->nbr_id)
				{
					G[0][k] = G[k][0] = 1;
					d[k] = 1;
					gate_way[k] = nbr->nbr_ip;
					fwd_iface[k] = iface;
					break;
				}
				else
					k++;
			}
		}
	}
	
	int x = 1;
	list_for_each_entry(db_entry, &mospf_db, list)
	{
		for(int i = 0; i < db_entry->nadv; i++)
		{
			if(db_entry->array[i].rid == 0)
				continue;
			
			if(db_entry->array[i].rid == instance->router_id)
			{
				G[0][x] = G[x][0] = 1;
				pre[x] = 0;
				continue;
			}

			int k = 1;
			mospf_db_entry_t *db_entry_p;
			list_for_each_entry(db_entry_p, &mospf_db, list)
			{
				if(db_entry_p->rid == db_entry->array[i].rid)
					G[x][k] = G[k][x] = 1;
				else
					k++;
			}
		}

		x++;
	}

	return n;
}

void spf(int n)
{
	int t = n;
	while(--t)
	{
		int v = 1, dis = 0x7f7f7f7f;
		for(int i = 1; i < n; i++)
		{
			if(d[i] < dis && !vis[i])
			{
				dis = d[i];
				v = i;
			}
		}
		vis[v] = 1;

		for(int i = 1; i < n; i++)
		{
			if(!vis[i] && G[v][i] > 0 && d[v] + G[v][i] < d[i])
			{
				d[i] = d[v] + G[v][i];
				pre[i] = v;
			}
		}
	}
}

void *spf_thread(void *param)
{
	while(1)
	{
		sleep(MOSPF_DATABASE_TIMEOUT);
		pthread_mutex_lock(&mospf_lock);
		memset(G, 0, sizeof(G));
		memset(d, 0x7f, sizeof(d));
		memset(vis, 0, sizeof(vis));
		memset(pre, 0xff, sizeof(pre));
		memset(gate_way, 0, sizeof(gate_way));
		memset(fwd_iface, 0, sizeof(fwd_iface));

		int n = build_graph();
		spf(n);

		int t = n;
		memset(vis, 0, sizeof(vis));
		while(--t)
		{
			int v = 1, dis = 0x7f7f7f7f;
			for(int i = 1; i < n; i++)
			{
				if(d[i] < dis && !vis[i])
				{
					dis = d[i];
					v = i;
				}
			}
			vis[v] = 1;

			int k = 1;
			mospf_db_entry_t *db_entry;
			list_for_each_entry(db_entry, &mospf_db, list)
			{
				if(k != v)
				{
					k++;
					continue;
				}

				while(pre[k] != 0)
					k = pre[k];

				for(int i = 0; i < db_entry->nadv; i++)
				{
					if(db_entry->array[i].rid == 0)
					{
						rt_entry_t *rt_entry_old = longest_prefix_match(db_entry->array[i].subnet);
						rt_entry_t *rt_entry = new_rt_entry(db_entry->array[i].subnet, db_entry->array[i].mask, \
							gate_way[k], fwd_iface[k]);
						add_rt_entry(rt_entry);
						if(rt_entry_old != NULL)
							remove_rt_entry(rt_entry_old);
						continue;
					}

					if(db_entry->array[i].rid == instance->router_id)
						continue;
					
					rt_entry_t *rt_entry = longest_prefix_match(db_entry->array[i].subnet);
					if(rt_entry == NULL)
					{
						rt_entry = new_rt_entry(db_entry->array[i].subnet, db_entry->array[i].mask, \
							gate_way[k], fwd_iface[k]);
						add_rt_entry(rt_entry);
					}
					else if(rt_entry->mask < db_entry->array[i].mask)
					{
						rt_entry_t *rt_entry_old = rt_entry;
						rt_entry = new_rt_entry(db_entry->array[i].subnet, db_entry->array[i].mask, \
							gate_way[k], fwd_iface[k]);
						add_rt_entry(rt_entry);
						remove_rt_entry(rt_entry_old);
					}
				}
			}
		}

		//print rtable
		printf("\n\nrtable:\n");
		rt_entry_t *rt_entry;
		printf("dest\tmask\tgw\n");
		list_for_each_entry(rt_entry, &rtable, list)
		{
			printf(IP_FMT"\t"IP_FMT"\t"IP_FMT"\n", HOST_IP_FMT_STR(rt_entry->dest), \
				HOST_IP_FMT_STR(rt_entry->mask), HOST_IP_FMT_STR(rt_entry->gw));
		}

		pthread_mutex_unlock(&mospf_lock);
	}
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	//fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct mospf_hdr *hdr = (struct mospf_hdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	struct mospf_lsu *lsu = (struct mospf_lsu*)((char *)hdr + MOSPF_HDR_SIZE);
	struct mospf_lsa *lsa = (struct mospf_lsa*)((char *)lsu + MOSPF_LSU_SIZE);

	if(ntohl(hdr->rid) == instance->router_id)
		return ;
	
	int received = 0, nadv = ntohl(lsu->nadv);
	
	pthread_mutex_lock(&mospf_lock);
	mospf_db_entry_t *db_entry;
	list_for_each_entry(db_entry, &mospf_db, list)
	{
		if(ntohl(hdr->rid) == db_entry->rid)
		{
			received = 1;
			if(ntohs(lsu->seq) > db_entry->seq)
			{
				free(db_entry->array);
				db_entry->array = (struct mospf_lsa *)malloc(nadv * MOSPF_LSA_SIZE);
				db_entry->seq = ntohs(lsu->seq);
				db_entry->nadv = nadv;
				db_entry->upd = time(NULL);
				for(int i = 0; i < nadv; i++)
				{
					db_entry->array[i].rid = ntohl(lsa[i].rid);
					db_entry->array[i].subnet = ntohl(lsa[i].subnet);
					db_entry->array[i].mask = ntohl(lsa[i].mask);
				}
			}
		}
	}

	if(!received)
	{
		db_entry = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
		init_list_head(&db_entry->list);
		db_entry->rid = ntohl(hdr->rid);
		db_entry->seq = ntohs(lsu->seq);
		db_entry->nadv = nadv;
		db_entry->upd = time(NULL);
		list_add_tail(&db_entry->list, &mospf_db);

		db_entry->array = (struct mospf_lsa *)malloc(nadv * MOSPF_LSA_SIZE);
		for(int i = 0; i < nadv; i++)
		{
			db_entry->array[i].rid = ntohl(lsa[i].rid);
			db_entry->array[i].subnet = ntohl(lsa[i].subnet);
			db_entry->array[i].mask = ntohl(lsa[i].mask);
		}
	}

	if(--lsu->ttl > 0)
	{
		iface_info_t *iface_f;
		list_for_each_entry(iface_f, &instance->iface_list, list)
		{
			if(iface_f == iface)
				continue;
			mospf_nbr_t *nbr;
			list_for_each_entry(nbr, &iface_f->nbr_list, list)
			{
				char *pkt = malloc(len);
				memcpy(pkt, packet, len);
				struct mospf_hdr *hdr_p = (struct mospf_hdr*)(pkt + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
				hdr_p->checksum = mospf_checksum(hdr_p);
				struct iphdr *iph = packet_to_ip_hdr(pkt);
				ip_init_hdr(iph, iface_f->ip, nbr->nbr_ip, len-ETH_ALEN, 90);
				struct ether_header *eth = (struct ether_header *)pkt;
				memcpy(eth->ether_shost, iface_f->mac, ETH_ALEN);
				ip_send_packet(pkt, len);
			}
		}
	}
	pthread_mutex_unlock(&mospf_lock);
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}
