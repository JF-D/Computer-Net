#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	//fprintf(stderr, "TODO: malloc and send icmp packet.\n");
	struct ether_header *eh_pkt = (struct ether_header *)in_pkt;
	struct iphdr *iph_pkt = packet_to_ip_hdr(in_pkt);
	struct icmphdr *icmp_pkt = (struct icmphdr*)((char *)iph_pkt + IP_HDR_SIZE(iph_pkt));

	int packet_sz;
	if(icmp_pkt->type == ICMP_ECHOREQUEST)
		packet_sz = len;
	else
		packet_sz = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE + 4 + \
						IP_HDR_SIZE(iph_pkt) + ICMP_COPIED_DATA_LEN;
	

	char *packet = malloc(packet_sz);
	
	struct iphdr *iph = packet_to_ip_hdr(packet);
	struct icmphdr *icmp = (struct icmphdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	char *rest_icmp = packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE;

	iph->daddr = iph_pkt->saddr;

	icmp->type = type;
	icmp->code = code;
	
	if(icmp_pkt->type == ICMP_ECHOREQUEST)
		memcpy(rest_icmp, (char *)icmp_pkt + ICMP_HDR_SIZE, len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - ICMP_HDR_SIZE);
	else
	{
		memset(rest_icmp, 0, 4);
		memcpy(rest_icmp+4, (char *)iph_pkt, IP_HDR_SIZE(iph) + ICMP_COPIED_DATA_LEN);
	}

	icmp->checksum = icmp_checksum(icmp, packet_sz - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);

	ip_send_packet(packet, len);

	free(in_pkt);
}
