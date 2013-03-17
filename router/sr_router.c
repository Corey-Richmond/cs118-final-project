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

#include "steve_macros.h"
#include "eth_macros.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"
#include "icmp_error.h"

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
	if (memcmp(&(eth_header_in->ether_dhost), &(iface->addr), ETHER_ADDR_LEN))
		if (memcmp(&(eth_header_in->ether_dhost), bcast_addr, ETHER_ADDR_LEN))
		return;

	/* Route the packet to the appropriate handler (IP/ARP) */
	if (eth_header_in->ether_type == htons(ethertype_ip))
		send_icmp_error(sr, packet, len, interface, 3, 0);
		/*handle_ip(sr, packet, len, interface);*/
	else if (eth_header_in->ether_type == htons(ethertype_arp))
		handle_arp(sr, packet, len, interface);

	return;
}/* end sr_handlepacket */


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
	
	struct sr_if* iface = sr_get_interface(sr, interface);

	/* Allow easy access to the headers */
	sr_ethernet_hdr_t *eth_header_in = (sr_ethernet_hdr_t*) packet;
	sr_arp_hdr_t *arp_header_in = (sr_arp_hdr_t*) (packet + ARP_HEAD_OFF);

	/* Route the packet to the appropriate handler (Req/Rep) */
	if (arp_header_in->ar_op == htons(arp_op_reply))
		/*handle_arp_reply(sr, packet, len, interface);*/printf("implement arp_reply\n"); 
	else if (arp_header_in->ar_op == htons(arp_op_request))
		handle_arp_request(sr, packet, len, interface);

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
	memcpy(&(eth_header_out->ether_dhost), &(eth_header_in->ether_shost), 
		ETHER_ADDR_LEN);
	memcpy(&(eth_header_out->ether_shost), &(iface->addr), 
		ETHER_ADDR_LEN);
	eth_header_out->ether_type = htons(ethertype_arp);

	/* ====== Body ====== */
	/* Create the ARP packet */
	arp_header_out->ar_hrd = htons(0x1);
	arp_header_out->ar_pro = htons(ethertype_ip);
	arp_header_out->ar_hln = ETHER_ADDR_LEN;
	arp_header_out->ar_pln = IP_ADDR_LEN;
	arp_header_out->ar_op = htons(arp_op_reply);
	memcpy(&(arp_header_out->ar_sha), &(iface->addr), ETHER_ADDR_LEN);
	arp_header_out->ar_sip = iface->ip;
	memcpy(&(arp_header_out->ar_tha), &(arp_header_in->ar_sha), ETHER_ADDR_LEN);
	arp_header_out->ar_tip = arp_header_in->ar_sip;

	/* Send the packet */
	sr_send_packet(sr, packet_out, out_len, interface);

	return;
}