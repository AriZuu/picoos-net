/*
 * Copyright (c) 2006-2013, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#warning license check !
#include <picoos.h>
#include <picoos-net.h>
#include <string.h>
#include <net/uip-split.h>

#if UIP_CONF_IPV6
#include "net/uip-nd6.h"
#include "net/uip-ds6.h"
#endif

#define DEBUG DEBUG_NONE
#include "net/uip-debug.h"

#if UIP_LOGGING
void uip_log(char *msg);
#define UIP_LOG(m) uip_log(m)
#else
#define UIP_LOG(m)
#endif

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf32(UIP_LLH_LEN))

void tcpip_input()
{
  uip_input();
  if(uip_len > 0) {

#if NETCFG_UIP_SPLIT == 1
    uip_split_output();
#else
    tcpip_output();
#endif
  }

  uip_len = 0;
#if UIP_CONF_IPV6
  uip_ext_len = 0;
#endif /*UIP_CONF_IPV6*/
}

#if UIP_CONF_IPV6
#if 0
void
tcpip_ipv6_output(void)
{
  uip_ds6_nbr_t *nbr = NULL;
  uip_ipaddr_t *nexthop;

  if(uip_len == 0) {
    return;
  }

  if(uip_len > UIP_LINK_MTU) {
    UIP_LOG("tcpip_ipv6_output: Packet to big");
    uip_len = 0;
    return;
  }

  if(uip_is_addr_unspecified(&UIP_IP_BUF->destipaddr)){
    UIP_LOG("tcpip_ipv6_output: Destination address unspecified");
    uip_len = 0;
    return;
  }

  if(!uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
    /* Next hop determination */
    nbr = NULL;
    if(uip_ds6_is_addr_onlink(&UIP_IP_BUF->destipaddr)){
      nexthop = &UIP_IP_BUF->destipaddr;
    } else {
      uip_ds6_route_t* locrt;
      locrt = uip_ds6_route_lookup(&UIP_IP_BUF->destipaddr);
      if(locrt == NULL) {
        if((nexthop = uip_ds6_defrt_choose()) == NULL) {
#ifdef UIP_FALLBACK_INTERFACE
	  PRINTF("FALLBACK: removing ext hdrs & setting proto %d %d\n", 
		 uip_ext_len, *((uint8_t *)UIP_IP_BUF + 40));
	  if(uip_ext_len > 0) {
	    extern void remove_ext_hdr(void);
	    uint8_t proto = *((uint8_t *)UIP_IP_BUF + 40);
	    remove_ext_hdr();
	    /* This should be copied from the ext header... */
	    UIP_IP_BUF->proto = proto;
	  }
	  UIP_FALLBACK_INTERFACE.output();
#else
          PRINTF("tcpip_ipv6_output: Destination off-link but no route\n");
#endif /* !UIP_FALLBACK_INTERFACE */
          uip_len = 0;
          return;
        }
      } else {
	nexthop = &locrt->nexthop;
      }
    }
    /* End of next hop determination */
#if UIP_CONF_IPV6_RPL
    if(rpl_update_header_final(nexthop)) {
      uip_len = 0;
      return;
    }
#endif /* UIP_CONF_IPV6_RPL */
    if((nbr = uip_ds6_nbr_lookup(nexthop)) == NULL) {
      if((nbr = uip_ds6_nbr_add(nexthop, NULL, 0, NBR_INCOMPLETE)) == NULL) {
        uip_len = 0;
        return;
      } else {
#if UIP_CONF_IPV6_QUEUE_PKT
        /* Copy outgoing pkt in the queuing buffer for later transmit. */
        if(uip_packetqueue_alloc(&nbr->packethandle, UIP_DS6_NBR_PACKET_LIFETIME) != NULL) {
          memcpy(uip_packetqueue_buf(&nbr->packethandle), UIP_IP_BUF, uip_len);
          uip_packetqueue_set_buflen(&nbr->packethandle, uip_len);
        }
#endif
      /* RFC4861, 7.2.2:
       * "If the source address of the packet prompting the solicitation is the
       * same as one of the addresses assigned to the outgoing interface, that
       * address SHOULD be placed in the IP Source Address of the outgoing
       * solicitation.  Otherwise, any one of the addresses assigned to the
       * interface should be used."*/
       if(uip_ds6_is_my_addr(&UIP_IP_BUF->srcipaddr)){
          uip_nd6_ns_output(&UIP_IP_BUF->srcipaddr, NULL, &nbr->ipaddr);
        } else {
          uip_nd6_ns_output(NULL, NULL, &nbr->ipaddr);
        }

        stimer_set(&nbr->sendns, uip_ds6_if.retrans_timer / 1000);
        nbr->nscount = 1;
      }
    } else {
      if(nbr->state == NBR_INCOMPLETE) {
        PRINTF("tcpip_ipv6_output: nbr cache entry incomplete\n");
#if UIP_CONF_IPV6_QUEUE_PKT
        /* Copy outgoing pkt in the queuing buffer for later transmit and set
           the destination nbr to nbr. */
        if(uip_packetqueue_alloc(&nbr->packethandle, UIP_DS6_NBR_PACKET_LIFETIME) != NULL) {
          memcpy(uip_packetqueue_buf(&nbr->packethandle), UIP_IP_BUF, uip_len);
          uip_packetqueue_set_buflen(&nbr->packethandle, uip_len);
        }
#endif /*UIP_CONF_IPV6_QUEUE_PKT*/
        uip_len = 0;
        return;
      }
      /* Send in parallel if we are running NUD (nbc state is either STALE,
         DELAY, or PROBE). See RFC 4861, section 7.7.3 on node behavior. */
      if(nbr->state == NBR_STALE) {
        nbr->state = NBR_DELAY;
        stimer_set(&nbr->reachable, UIP_ND6_DELAY_FIRST_PROBE_TIME);
        nbr->nscount = 0;
        PRINTF("tcpip_ipv6_output: nbr cache entry stale moving to delay\n");
      }

      tcpip_output(&nbr->lladdr);

#if UIP_CONF_IPV6_QUEUE_PKT
      /*
       * Send the queued packets from here, may not be 100% perfect though.
       * This happens in a few cases, for example when instead of receiving a
       * NA after sendiong a NS, you receive a NS with SLLAO: the entry moves
       * to STALE, and you must both send a NA and the queued packet.
       */
      if(uip_packetqueue_buflen(&nbr->packethandle) != 0) {
        uip_len = uip_packetqueue_buflen(&nbr->packethandle);
        memcpy(UIP_IP_BUF, uip_packetqueue_buf(&nbr->packethandle), uip_len);
        uip_packetqueue_free(&nbr->packethandle);
        tcpip_output(&nbr->lladdr);
      }
#endif /*UIP_CONF_IPV6_QUEUE_PKT*/

      uip_len = 0;
      return;
    }
  }

  /* Multicast IP destination address. */
  tcpip_output(NULL);
  uip_len = 0;
  uip_ext_len = 0;
}
#endif
uint8_t
tcpip_output(uip_lladdr_t *a)
{
  netInterfaceOutput(a);
  return 0;
}

#else

uint8_t tcpip_output(void)
{
  netInterfaceOutput();
  return 0;
}

#endif

void uip_log(char* m)
{
  nosPrintf("uip: %s", m);
  nosPrint("\n");
}

