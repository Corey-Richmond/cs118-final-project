/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "steve_macros.h"
#include "eth_macros.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"
#include "icmp_error.h"
#include "checksum_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
	/* REQUIRES */
	assert(sr);

	/* Initialize cache and cache cleanup thread */
	sr_arpcache_init(&(sr->cache));

	pthread_attr_init(&(sr->attr));
	pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
	pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
	pthread_t thread;

	pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

	/* Add initialization code here! */

} /* -- sr_init -- */

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
	struct sr_if *iface = sr_get_interface(sr, interface);

	/* REQUIRES */
	assert(sr);
	assert(packet);
	assert(interface);
	assert(iface);

	/* Debug */
	printf("*** -> Received packet of length %d \n",len);

	/* Allow easy access to the ethernet headers */
	sr_ethernet_hdr_t *eth_header_in = (sr_ethernet_hdr_t*) packet;

	/* Ensure the packet is actually meant for us */
	char bcast_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	if (memcmp(eth_header_in->ether_dhost, iface->addr, ETHER_ADDR_LEN))
		if (memcmp(eth_header_in->ether_dhost, bcast_addr, ETHER_ADDR_LEN))
		;

	/* Route the packet to the appropriate handler (IP/ARP) */
	if (eth_header_in->ether_type == htons(ethertype_ip))
		handle_ip(sr, packet, len, interface);
	else if (eth_header_in->ether_type == htons(ethertype_arp))
		handle_arp(sr, packet, len, interface);

	return;
}/* end sr_handlepacket */


/*---------------------------------------------------------------------
 * Method: handle_ip
 * Scope:  Global
 *
 * This method is called each time the router receives an IP packet on 
 * the interface.  The packet buffer, the packet length and receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 *---------------------------------------------------------------------*/

void handle_ip(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
	printf("handle_ip() called\n");
	struct sr_if* iface = sr_get_interface(sr, interface);

	sr_ip_hdr_t *ip_header_in = (sr_ip_hdr_t*) (packet + IP_HEAD_OFF);
	
	uint32_t dest = ip_header_in->ip_dst;

	/* send error if ttl is 0 */
	if(ip_header_in->ip_ttl <= 1){
		send_icmp_error(sr, packet, len, interface, 11, 0);
		return;
	}

	/* If it's for us, remind sender not to bother us */
	if (dest == iface->ip) {
		/* Pretend we can't be reached for most */
		if (ip_header_in->ip_p > 0x1)
			send_icmp_error(sr, packet, len, interface, 3, 3);
		/* ... but echo all echo packets */
		else {
			sr_icmp_hdr_t *icmp_header_in = 
					(sr_icmp_hdr_t*) (packet + ICMP_HEAD_OFF);
			if (icmp_header_in->icmp_type == 0x8 
					&& icmp_header_in->icmp_code == 0x0) {
				send_icmp_error(sr, packet, len, interface, 0, 0);
			}
		}
		return;
	}


	struct sr_rt* rt = sr->routing_table;

	int num_matching = 0;
	uint32_t best_match_gw = 0;
	char best_match_iface[sr_IFACE_NAMELEN];

	while(rt){
	
		if(!((( rt->dest.s_addr) ^ dest) & (rt->mask.s_addr))){
			uint32_t a = !(rt->mask.s_addr);
			a++;
			int match = 0;
			while(a != 1){
				a /= 2;
				match++;
			}
			match = 32 - match;
			if(match > num_matching){
				num_matching = match;
				best_match_gw =  rt->gw.s_addr;
				memcpy(best_match_iface, rt->interface, sr_IFACE_NAMELEN);
			}
		}
		rt = rt->next;
	}
	
	if(num_matching == 0){
		send_icmp_error(sr, packet, len, interface, 3, 0);
		return;
	}

	/* Get the iface of the best match */
	struct sr_if *best_iface = sr_get_interface(sr, best_match_iface);

	sr_ethernet_hdr_t *eth_header_out = (sr_ethernet_hdr_t*) packet;
	memcpy(eth_header_out->ether_shost, best_iface->addr, ETHER_ADDR_LEN);
	
	/* DECREMENT TTL */
	ip_header_in->ip_ttl--;

	/* Fill in the IP checksum */
	ip_header_in->ip_sum = 0x0;
	ip_header_in->ip_sum = 
			get_checksum_16(packet+IP_HEAD_OFF, IP_HEAD_SIZE);

	struct sr_arpentry *addr;
	if((addr = sr_arpcache_lookup(&(sr->cache), dest))){
		memcpy(eth_header_out->ether_dhost, addr->mac, ETHER_ADDR_LEN);
		memcpy(eth_header_out->ether_shost, best_iface->addr, ETHER_ADDR_LEN);
		sr_send_packet(sr, packet, len, best_match_iface);
		printf("Sent packet on interface %s\n", best_match_iface);
	} else {
		sr_arpcache_queuereq(&(sr->cache), best_match_gw, packet, len, best_match_iface);
	}

	return;

}


/*---------------------------------------------------------------------
 * Method: handle_arp
 * Scope:  Global
 *
 * This method is called each time the router receives an ARP packet on 
 * the interface.  The packet buffer, the packet length and receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 *---------------------------------------------------------------------*/

void handle_arp(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
	printf("handle_arp() called\n");
	
	struct sr_if* iface = sr_get_interface(sr, interface);

	/* Allow easy access to the headers */
	sr_arp_hdr_t *arp_header_in = (sr_arp_hdr_t*) (packet + ARP_HEAD_OFF);

	if(arp_header_in->ar_tip != iface->ip){
		return;
	}

	/* Route the packet to the appropriate handler (Req/Rep) */
	if (arp_header_in->ar_op == htons(arp_op_reply))
		handle_arp_reply(sr, packet, len, interface);
	else if (arp_header_in->ar_op == htons(arp_op_request))
		handle_arp_request(sr, packet, len, interface);

	return;
}


/*---------------------------------------------------------------------
 * Method: handle_arp_reply
 * Scope:  Global
 * 
 * This method is called each time the router receives an ARP reply on 
 * the interface.  The packet buffer, the packet length and receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 *---------------------------------------------------------------------*/

void handle_arp_reply(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
	printf("handle_arp_reply() called\n");
	
	struct sr_if* iface = sr_get_interface(sr, interface);

	/* ====== Headers ====== */
	/* Allow easy access to the headers */
	sr_arp_hdr_t *arp_header_in = (sr_arp_hdr_t*) (packet + ARP_HEAD_OFF);

	uint32_t ip = arp_header_in->ar_sip;
	unsigned char mac[ETHER_ADDR_LEN];
	
	memcpy(mac, arp_header_in->ar_sha, ETHER_ADDR_LEN);

	struct sr_arpreq* req = sr_arpcache_insert(&(sr->cache), mac, ip);

	if(!req){
		return;
		/* this IP was not in the request queue */
	} else {
		struct sr_packet* pack = req->packets;
		while(pack){
			sr_ethernet_hdr_t *packet_eth_header = (sr_ethernet_hdr_t*) pack->buf;
			memcpy(packet_eth_header->ether_dhost, mac, ETHER_ADDR_LEN);
			
			sr_send_packet(sr, pack->buf, pack->len, pack->iface);

			pack = pack->next;
		}

		sr_arpreq_destroy(&(sr->cache), req);
	}

	return;
}

/*---------------------------------------------------------------------
 * Method: handle_arp_request
 * Scope:  Global
 *
 * This method is called each time the router receives an ARP request on 
 * the interface.  The packet buffer, the packet length and receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 *---------------------------------------------------------------------*/

void handle_arp_request(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
	printf("handle_arp_request() called\n");
	struct sr_if* iface = sr_get_interface(sr, interface);

	/* Allocate a new packet */
	size_t out_len = ARP_REPLY_SIZE;
	uint8_t *packet_out = malloc(out_len);

	/* ====== Headers ====== */
	/* Allow easy access to the headers */
	sr_ethernet_hdr_t *eth_header_out = (sr_ethernet_hdr_t*) packet_out;
	sr_ethernet_hdr_t *eth_header_in = (sr_ethernet_hdr_t*) packet;
	sr_arp_hdr_t *arp_header_out = (sr_arp_hdr_t*) (packet_out + ARP_HEAD_OFF);
	sr_arp_hdr_t *arp_header_in = (sr_arp_hdr_t*) (packet + ARP_HEAD_OFF);


	/* Create the ethernet header */
	memcpy(eth_header_out->ether_dhost, eth_header_in->ether_shost, 
		ETHER_ADDR_LEN);
	memcpy(eth_header_out->ether_shost, iface->addr, 
		ETHER_ADDR_LEN);
	eth_header_out->ether_type = htons(ethertype_arp);

	/* ====== Body ====== */
	/* Create the ARP packet */
	arp_header_out->ar_hrd = htons(0x1);
	arp_header_out->ar_pro = htons(ethertype_ip);
	arp_header_out->ar_hln = ETHER_ADDR_LEN;
	arp_header_out->ar_pln = IP_ADDR_LEN;
	arp_header_out->ar_op = htons(arp_op_reply);
	memcpy(arp_header_out->ar_sha, iface->addr, ETHER_ADDR_LEN);
	arp_header_out->ar_sip = iface->ip;
	memcpy(arp_header_out->ar_tha, arp_header_in->ar_sha, ETHER_ADDR_LEN);
	arp_header_out->ar_tip = arp_header_in->ar_sip;

	/* Send the packet */
	sr_send_packet(sr, packet_out, out_len, interface);

	return;
}
