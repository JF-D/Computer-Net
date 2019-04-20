#include "ip.h"
#include "icmp.h"
#include "packet.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"

// #include "log.h"

#include <stdio.h>
#include <stdlib.h>

// initialize ip header 
void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = htons(len);
	ip->id = rand();
	ip->frag_off = htons(IP_DF);
	ip->ttl = DEFAULT_TTL;
	ip->protocol = proto;
	ip->saddr = htonl(saddr);
	ip->daddr = htonl(daddr);
	ip->checksum = ip_checksum(ip);
}

// lookup in the routing table, to find the entry with the same and longest prefix.
// the input address is in host byte order
rt_entry_t *longest_prefix_match(u32 dst)
{
	//fprintf(stderr, "TODO: longest prefix match for the packet.\n");
	rt_entry_t *rt_entry = NULL, *max_rt_entry = NULL;
	list_for_each_entry(rt_entry, &rtable, list)
	{
		u32 ip = dst & rt_entry->mask;
		if((ip == (rt_entry->dest & rt_entry->mask)) &&
			(max_rt_entry == NULL || max_rt_entry->mask < rt_entry->mask))
		{
			max_rt_entry = rt_entry;
		}
	}

	return max_rt_entry;
}

// send IP packet
//
// Different from forwarding packet, ip_send_packet sends packet generated by
// router itself. This function is used to send ICMP packets.
void ip_send_packet(char *packet, int len)
{
	//fprintf(stderr, "TODO: send ip packet.\n");
	struct iphdr *iph = packet_to_ip_hdr(packet);

	u32 dst = ntohl(iph->daddr);
	rt_entry_t *rt_entry = longest_prefix_match(dst);
	
	if(!rt_entry)
	{
		//log(ERROR, "Could not find next hop for IP (dst:" IP_FMT ")", HOST_IP_FMT_STR(dst));
		free(packet);
		return ;
	}
	
	//IP
	ip_init_hdr(iph, rt_entry->iface->ip, ntohl(iph->daddr), len - ETHER_HDR_SIZE, IPPROTO_ICMP);

	u32 next_hop = rt_entry->gw;
	if(!next_hop) next_hop = dst;

	iface_send_packet_by_arp(rt_entry->iface, next_hop, packet, len);
}
