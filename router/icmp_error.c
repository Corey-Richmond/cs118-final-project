/*
 * =============================================================================
 *
 *       Filename:  icmp_error.c
 *
 *    Description:  Handles sending ICMP error packets to remote hosts
 *
 *        Version:  1.0
 *        Created:  03/16/2013 10:54:57 PM
 *       Revision:  1
 *       Compiler:  gcc
 *
 *         Author:  Steven Weiss, Michael Jennings
 *   Organization:  UCLA Computer Science
 *
 * =============================================================================
 */

#include <string.h>
#include <stdlib.h>
#include "sr_router.h"
#include "sr_utils.h"
#include "checksum_utils.h"
#include "steve_macros.h"
#include "eth_macros.h"

void send_icmp_error(struct sr_instance* sr,
		uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */,
		uint8_t type,
		uint8_t code )
{
	struct sr_if* iface = sr_get_interface(sr, interface);

	/* ====== Begin ICMP Packet Construction ====== */
	/* Create an empty packet */
	uint8_t *packet_out = malloc(100);
	size_t out_len = ICMP_T3_SIZE;
	memset(packet_out, 0, out_len);

	/* ====== Headers ====== */
	/* Set up easy access to the headers */
	sr_ethernet_hdr_t *eth_header_out = (sr_ethernet_hdr_t*) packet_out;
	sr_ethernet_hdr_t *eth_header_in = (sr_ethernet_hdr_t*) packet;
	sr_ip_hdr_t *ip_header_out = (sr_ip_hdr_t*) (packet_out + IP_HEAD_OFF);
	sr_ip_hdr_t *ip_header_in = (sr_ip_hdr_t*) (packet + IP_HEAD_OFF);
	sr_icmp_hdr_t *icmp_header_out = 
			(sr_icmp_hdr_t*) (packet_out + ICMP_HEAD_OFF);
	/* sr_icmp_hdr_t *icmp_header_in = 
			(sr_icmp_hdr_t*) (packet + ICMP_HEAD_OFF); */

	/* Create the ethernet header */
	memcpy(&(eth_header_out->ether_dhost), &(eth_header_in->ether_shost), 
		ETHER_ADDR_LEN);
	memcpy(&(eth_header_out->ether_shost), &(iface->addr), 
		ETHER_ADDR_LEN);
	eth_header_out->ether_type = htons(ethertype_ip);

	/* Create the IP header */
	ip_header_out->ip_v = 0x4;
	ip_header_out->ip_hl = 0x5;
	ip_header_out->ip_tos = 0x00;
	ip_header_out->ip_off = htons(0x0000 | IP_DF);
	ip_header_out->ip_ttl = 0x7f;
	ip_header_out->ip_p = ip_protocol_icmp;
	ip_header_out->ip_sum = 0x0;
	ip_header_out->ip_src = iface->ip;
	ip_header_out->ip_dst = ip_header_in->ip_src;

	/* Create the ICMP Packet */
	icmp_header_out->icmp_type = type;
	icmp_header_out->icmp_code = code;
	icmp_header_out->icmp_sum = 0;

	/* ====== Data ====== */
	/* For echo type, copy over the data. */
	/* TODO: Test echo */
	if (type == 0x0) { 
		memcpy(packet_out+ICMP_HEAD_OFF+4, 
				packet+ICMP_HEAD_OFF+4, len-ICMP_HEAD_OFF-4);
		out_len = len;
	}

	/* For the other two types, copy the headers */
	else if (type == icmp_type_unreachable || type == icmp_type_timeout) {
		sr_icmp_t3_hdr_t *icmp_t3_header_out = 
				(sr_icmp_t3_hdr_t*) (packet_out + IP_DATA_OFF);
		icmp_t3_header_out->unused = 0x0;
		icmp_t3_header_out->next_mtu = 0;
		memcpy(&(icmp_t3_header_out->data), 
				packet+IP_HEAD_OFF, ICMP_DATA_SIZE);
	}

	/* Finish filling out IP header */
	ip_header_out->ip_len = htons(out_len-IP_HEAD_OFF);
	ip_header_out->ip_id = htons(out_len-IP_HEAD_OFF); /* TODO */

	/* Fill in the ICMP checksum */
	icmp_header_out->icmp_sum = 
			get_checksum_16(packet_out+IP_DATA_OFF, out_len-IP_DATA_OFF);

	/* Fill in the IP checksum */
	ip_header_out->ip_sum = 
			get_checksum_16(packet_out+IP_HEAD_OFF, out_len-IP_HEAD_OFF);

	/* ====== End Packet Construction ====== */

	/* Send the packet */
	/* forward_pkt(sr, packet_out, */
	sr_send_packet(sr, packet_out, out_len, interface);
		packet_out[out_len] = 0;
		int i;
		for (i = 0; i < out_len; i++)
			printf("%x ",*(packet_out+i));
		printf("\n");

	/* Free the packet */
	free(packet_out);

	return;
} /* end send_icmp_eror */
