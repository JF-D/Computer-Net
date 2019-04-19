#include "ip.h"

#include <stdio.h>
#include <stdlib.h>

void ip_forward_packet(u32 dst, char *packet, int len)
{
	struct iphdr *iph = packet_to_ip_hdr(packet);
	--iph->ttl;
	if(iph->ttl <= 0)
	{
		icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
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
	}
}

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	//fprintf(stderr, "TODO: handle ip packet.\n");
	struct ether_header *eh = (struct ether_header *)packet;
	struct iphdr *iph = packet_to_ip_hdr(packet);

	if(iph->protocol == IPPROTO_ICMP)
	{
		struct icmphdr *icmp = (struct icmphdr*)((char *)iph + IP_HDR_SIZE(iph));
		if(icmp->type == ICMP_ECHOREQUEST && ntohl(iph->daddr) == iface->ip)
		{
			ip_send_packet(packet, len);
			return ;
		}
	}
	ip_forward_packet(ntohl(iph->daddr), packet, len);
}
