#include "ip.h"
#include "icmp.h"
#include "packet.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"
#include "nat.h"

#include <stdlib.h>

void ip_forward_packet(u32 dst, char *packet, int len)
{
	struct iphdr *iph = packet_to_ip_hdr(packet);
	--iph->ttl;
	if(iph->ttl <= 0)
	{
		icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
		free(packet);
		return ;
	}

	iph->checksum = ip_checksum(iph);
	
	rt_entry_t *rt_entry = longest_prefix_match(dst);
	if(rt_entry)
	{
		u32 next_hop = rt_entry->gw;
		if(!next_hop) next_hop = dst;
		
		iface_send_packet_by_arp(rt_entry->iface, next_hop, packet, len);
	}
	else
	{
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
		free(packet);
	}
}

void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	if (daddr == iface->ip && ip->protocol == IPPROTO_ICMP) {
		struct icmphdr *icmp = (struct icmphdr *)IP_DATA(ip);
		if (icmp->type == ICMP_ECHOREQUEST) {
			icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		}

		free(packet);
	}
	else {
		nat_translate_packet(iface, packet, len);
	}
}

