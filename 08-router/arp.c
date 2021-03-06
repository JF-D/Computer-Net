#include "ip.h"
#include "log.h"
#include "arp.h"
#include "base.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	int len = ETHER_HDR_SIZE + sizeof(struct ether_arp);
	char *packet = malloc(len);

	struct ether_header *eh = (struct ether_header*)packet;
	struct ether_arp *arp = (struct ether_arp*)(packet + ETHER_HDR_SIZE);

	memset(eh->ether_dhost, 0xff, ETH_ALEN);
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	
	eh->ether_type = htons(ETH_P_ARP);

	arp->arp_hrd = htons(ARPHRD_ETHER);
	arp->arp_pro = htons(0x0800);
	arp->arp_hln = 6;
	arp->arp_pln = 4;
	arp->arp_op = htons(ARPOP_REQUEST);
	
	memcpy(arp->arp_sha, iface->mac, ETH_ALEN);
	memset(arp->arp_tha, 0, ETH_ALEN);

	arp->arp_spa = htonl(iface->ip);
	arp->arp_tpa = htonl(dst_ip);
	
	//send
	iface_send_packet(iface, packet, len);
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	int len = ETHER_HDR_SIZE + sizeof(struct ether_arp);
	char *packet = malloc(len);

	struct ether_header *eh = (struct ether_header*)packet;
	struct ether_arp *arp = (struct ether_arp*)(packet + ETHER_HDR_SIZE);

	memcpy(eh->ether_dhost, req_hdr->arp_sha, ETH_ALEN);
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);

	eh->ether_type = htons(ETH_P_ARP);

	arp->arp_hrd = htons(ARPHRD_ETHER);
	arp->arp_pro = htons(0x0800);
	arp->arp_hln = 6;
	arp->arp_pln = 4;
	arp->arp_op = htons(ARPOP_REPLY);
	
	memcpy(arp->arp_sha, iface->mac, ETH_ALEN);
	memcpy(arp->arp_tha, req_hdr->arp_sha, ETH_ALEN);

	arp->arp_spa = htonl(iface->ip);
	arp->arp_tpa = req_hdr->arp_spa;

	//send
	iface_send_packet(iface, packet, len);
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	//fprintf(stderr, "TODO: process arp packet: arp request & arp reply.\n");
	struct ether_arp *arp = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	
	int arp_type = ntohs(arp->arp_op);
	
	if(arp_type == ARPOP_REQUEST)
	{
		if(ntohl(arp->arp_tpa) != iface->ip)
			return ;
		
		arp_send_reply(iface, arp);
	}
	else if(arp_type == ARPOP_REPLY)
	{
		if(ntohl(arp->arp_tpa) != iface->ip)
			return ;		
	}

	arpcache_insert(ntohl(arp->arp_spa), arp->arp_sha);
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		//log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		//log(DEBUG, "lookup "IP_FMT" failed, pend this packet", HOST_IP_FMT_STR(dst_ip));
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
