#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	//fprintf(stdout, "TODO: determine the direction of this packet.\n");
	struct iphdr * iph = packet_to_ip_hdr(packet);
	u32 saddr = ntohl(iph->saddr);
	u32 daddr = ntohl(iph->daddr);

	rt_entry_t *sentry = longest_prefix_match(saddr);
	rt_entry_t *dentry = longest_prefix_match(daddr);

	if((sentry->iface->ip & sentry->iface->mask == nat.internal_iface->ip & nat.internal_iface->mask) && \
		(dentry->iface->ip & dentry->iface->mask == nat.external_iface->ip & nat.external_iface->mask))
		return DIR_OUT;
	else if((sentry->iface->ip & sentry->iface->mask == nat.external_iface->ip & nat.external_iface->mask) && \
		(dentry->iface->ip & dentry->iface->mask == nat.internal_iface->ip & nat.internal_iface->mask))
		return DIR_IN;
	else
		return DIR_INVALID;
}

int new_external_port()
{
	for(int i = NAT_PORT_MIN; i < NAT_PORT_MAX; i++)
	{
		if(nat.assigned_ports[i] == 0)
		{
			nat.assigned_ports[i] = 1;
			return i;
		}
	}
	return -1;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	//fprintf(stdout, "TODO: do translation for this packet.\n");
	struct iphdr *iph = packet_to_ip_hdr(packet);
	struct tcphdr *tcph = packet_to_tcp_hdr(packet);

	u32 saddr = ntohl(iph->saddr);
	u32 daddr = ntohl(iph->daddr);
	u16 sport = ntohs(tcph->sport);
	u16 dport = ntohs(tcph->dport);
	struct nat_mapping *map_entry = NULL;

	pthread_mutex_lock(&nat.lock);
	int dir = get_packet_direction(packet);
	u8 ind = hash8((char *)&daddr, 4) ^ hash8((char *)&dport, 2), find = 0;
	if(dir == DIR_OUT)
	{
		list_for_each_entry(map_entry, &nat.nat_mapping_list[ind], list)
		{
			if(map_entry->internal_ip == saddr && map_entry->internal_port == sport)
			{
				find = 1;
				break;
			}
		}

		if(!find)
		{
			if((tcph->flags & TCP_SYN == 1) && (tcph->flags & TCP_ACK == 0))
			{
				map_entry = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
				memset(map_entry, 0, sizeof(struct nat_mapping));
				init_list_head(&map_entry->list);
				map_entry->remote_ip = daddr;
				map_entry->remote_port = dport;
				map_entry->internal_ip = saddr;
				map_entry->internal_port = sport;
				map_entry->external_ip = nat.external_iface->ip;
				map_entry->external_port = new_external_port();
				list_add_tail(&map_entry->list, &nat.nat_mapping_list[ind]->list);
			}
			else
			{
				icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
				free(packet);
				return ;
			}
		}
		
		if(tcph->flags & TCP_FIN)
			map_entry->conn.internal_fin = 1;
		if((tcph->flags & TCP_ACK) && ntohl(tcph->ack) > map_entry->conn.internal_ack)
			map_entry->conn.internal_ack = ntohl(tcph->ack);
		if(ntohl(tcph->seq) > map_entry->conn.internal_seq_end)
			map_entry->conn.internal_seq_end = ntohl(tcph->seq);
		if(tcph->flags & TCP_RST)
		{
			map_entry->conn.internal_fin = 2;
			map_entry->conn.external_fin = 2;
		}

		iph->saddr = htonl(map_entry->external_ip);
		tcph->sport = htons(map_entry->external_port);
		tcp->checksum = tcp_checksum(iph, tcph);
		iph->checksum = ip_checksum(pih);
		map_entry->update_time = time(NULL);
	}
	else if(dir == DIR_IN)
	{
		list_for_each_entry(map_entry, &nat.nat_mapping_list[ind], list)
		{
			if(map_entry->external_ip == daddr && map_entry->external_port == dport)
			{
				find = 1;
				break;
			}
		}

		if(!find)
		{
			struct dnat_rule *dnat_entry = NULL;
			u8 rule_exist = 0;
			list_for_each_entry(dnat_entry, &nat.rules, list)
			{
				if(dnat_entry->external_ip == daddr && dnat_entry->external_port == dport)
				{
					rule_exist = 1;
					break;
				}
			}
			if((tcph->flags & TCP_SYN == 1) && (tcph->flags & TCP_ACK == 0) && rule_exist)
			{
				map_entry = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
				memset(map_entry, 0, sizeof(struct nat_mapping));
				init_list_head(&map_entry->list);
				map_entry->remote_ip = saddr;
				map_entry->remote_port = sport;
				map_entry->internal_ip = dnat_entry->internal_ip;
				map_entry->internal_port = dnat_entry->internal_port;
				map_entry->external_ip = dnat_entry->external_ip;
				map_entry->external_port = dnat_entry->external_port;
				list_add_tail(&map_entry->list, &nat.nat_mapping_list[ind]->list);
			}
			else
			{
				icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
				free(packet);
				return ;
			}
		}

		if(tcph->flags & TCP_FIN)
			map_entry->conn.external_fin = 1;
		if((tcph->flags & TCP_ACK) && ntohl(tcph->ack) > map_entry->conn.external_ack)
			map_entry->conn.external_ack = ntohl(tcph->ack);
		if(ntohl(tcph->seq) > map_entry->conn.external_seq_end)
			map_entry->conn.external_seq_end = ntohl(tcph->seq);
		if(tcph->flags & TCP_RST)
		{
			map_entry->conn.internal_fin = 2;
			map_entry->conn.external_fin = 2;
		}

		iph->daddr = htonl(map_entry->internal_ip);
		tcph->dport = htons(map_entry->internal_port);
		tcp->checksum = tcp_checksum(iph, tcph);
		iph->checksum = ip_checksum(pih);
		map_entry->update_time = time(NULL);
	}
	else
	{
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}
	
	pthread_mutex_unlock(&nat.lock);
	ip_send_packet(packet, len);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		//fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		sleep(1);
		pthread_mutex_lock(&nat.lock);
		time_t now = time(NULL);
		for(int i = 0; i < HASH_8BITS; i++)
		{
			struct nat_mapping *map_entry = NULL, *q = NULL;
			list_for_each_entry_safe(map_entry, q, &nat.nat_mapping_list[i], list)
			{
				if(is_flow_finished(&map_entry->conn) || now - map_entry->update_time > TCP_ESTABLISHED_TIMEOUT || \
					(map_entry->conn.external_fin == 2 && map_entry->conn.internal_fin == 2))
				{
					nat.assigned_ports[map_entry->external_port] = 0;
					list_delete_entry(&map_entry->list);
					free(map_entry);
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
	}

	return NULL;
}

#define MAX_STR_SIZE 100
int get_next_line(FILE *input, char (*strs)[MAX_STR_SIZE], int *num_strings)
{
	const char *delim = " \t\n";
	char buffer[120];
	int ret = 0;
	if (fgets(buffer, sizeof(buffer), input)) {
		char *token = strtok(buffer, delim);
		*num_strings = 0;
		while (token) {
			strcpy(strs[(*num_strings)++], token);
			token = strtok(NULL, delim);
		}

		ret = 1;
	}

	return ret;
}

int read_ip_port(const char *str, u32 *ip, u16 *port)
{
	int i1, i2, i3, i4;
	int ret = sscanf(str, "%d.%d.%d.%d:%hu", &i1, &i2, &i3, &i4, port);
	if (ret != 5) {
		log(ERROR, "parse ip-port string error: %s.", str);
		exit(1);
	}

	*ip = (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;

	return 0;
}

int parse_config(const char *filename)
{
	FILE *input;
	char strings[10][MAX_STR_SIZE];
	int num_strings;

	input = fopen(filename, "r");
	if (input) {
		while (get_next_line(input, strings, &num_strings)) {
			if (num_strings == 0)
				continue;

			if (strcmp(strings[0], "internal-iface:") == 0)
				nat.internal_iface = if_name_to_iface("n1-eth0");
			else if (strcmp(strings[0], "external-iface:") == 0)
				nat.external_iface = if_name_to_iface("n1-eth1");
			else if (strcmp(strings[0], "dnat-rules:") == 0) {
				struct dnat_rule *rule = (struct dnat_rule*)malloc(sizeof(struct dnat_rule));
				read_ip_port(strings[1], &rule->external_ip, &rule->external_port);
				read_ip_port(strings[3], &rule->internal_ip, &rule->internal_port);
				
				list_add_tail(&rule->list, &nat.rules);
			}
			else {
				log(ERROR, "incorrect config file, exit.");
				exit(1);
			}
		}

		fclose(input);
	}
	else {
		log(ERROR, "could not find config file '%s', exit.", filename);
		exit(1);
	}
	
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	parse_config(config_file);

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit()
{
	//fprintf(stdout, "TODO: release all resources allocated.\n");
	pthread_mutex_lock(&nat.lock);
		time_t now = time(NULL);
		for(int i = 0; i < HASH_8BITS; i++)
		{
			struct nat_mapping *map_entry = NULL, *q = NULL;
			list_for_each_entry_safe(map_entry, q, &nat.nat_mapping_list[i], list)
			{
				list_delete_entry(&map_entry->list);
				free(map_entry);
			}
		}

		pthread_kill(nat.thread, SIGTERM);
		pthread_mutex_unlock(&nat.lock);
}
