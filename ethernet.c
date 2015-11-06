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

#include <picoos.h>
#include <picoos-u.h>
#include <picoos-net.h>
#include <string.h>

#if NETSTACK_CONF_WITH_IPV6
struct uip_eth_addr uip_ethaddr = {{0,0,0,0,0,0}};
#endif

#if NETCFG_DRIVER_CS8900A == 2 || \
    NETCFG_DRIVER_ENC28J60 == 2 || \
    NETCFG_DRIVER_HDLC_BRIDGE == 2 || \
    NETCFG_DRIVER_TM4C1294 == 2 || \
    NETCFG_DRIVER_TAP == 2

#if NETSTACK_CONF_WITH_IPV6
void netInterfaceOutput(const uip_lladdr_t* lla)
{
  netEthernetOutput(lla);
}
#else
void netInterfaceOutput(void)
{
  netEthernetOutput();
}
#endif

#endif

#if NETCFG_DRIVER_CS8900A == 2

#include "drivers/cs8900a.h"

void netInterfaceInit(void)
{
  cs8900aInit();
}

bool netInterfacePoll(void)
{
  uip_len = cs8900aPoll();
  if (uip_len) {

    netEthernetInput();
    return true;
  }

  return false;
}

void netInterfaceXmit(void)
{
  cs8900aSend();
}

#endif

#if NETCFG_DRIVER_ENC28J60 == 2

#include "drivers/enc28j60.h"

void netInterfaceInit(void)
{
  enc28j60_spi_init();
  enc28j60_Init();
#ifndef ENC28J60_USE_INTERRUPTS
  netEnableDevicePolling(MS(5));
#endif
}

bool netInterfacePoll(void)
{
#ifdef ENC28J60_USE_INTERRUPTS
  enc28j60_InterruptPin_Disable();
#endif
  uip_len = enc28j60_Frame_Recv(uip_buf, UIP_BUFSIZE);
#ifdef ENC28J60_USE_INTERRUPTS
  enc28j60_Enable_Global_Interrupts();
  enc28j60_InterruptPin_Enable();
#endif
  if (uip_len) {

    netEthernetInput();
    return true;
  }

  return false;
}

void netInterfaceXmit(void)
{
#ifdef ENC28J60_USE_INTERRUPTS
  enc28j60_InterruptPin_Disable();
#endif
  enc28j60_Frame_Send(uip_buf, uip_len);
#ifdef ENC28J60_USE_INTERRUPTS
  enc28j60_InterruptPin_Enable();
#endif
}

#endif

#if NETCFG_DRIVER_TAP == 2

#include "drivers/unixtap.h"

void netInterfaceInit(void)
{
  tapInit();
}

bool netInterfacePoll(void)
{
  uip_len = tapPoll();
  if (uip_len) {

    netEthernetInput();
    return true;
  }

  return false;
}

void netInterfaceXmit(void)
{
  tapSend();
}

#endif

#if NETCFG_DRIVER_HDLC_BRIDGE == 2

#include "drivers/stm32_hdlc_bridge.h"

void netInterfaceInit(void)
{
  hdlcInit();
}

bool netInterfacePoll(void)
{
  uip_len = hdlcPoll();
  if (uip_len) {

    netEthernetInput();
    return true;
  }

  return false;
}

void netInterfaceXmit(void)
{
  hdlcSend();
}

#endif

#if NETCFG_DRIVER_TM4C1294 == 2

#include "drivers/tm4c_emac.h"

void netInterfaceInit(void)
{
  tivaEmacInit();
}

bool netInterfacePoll(void)
{
  uip_len = tivaEmacPoll(uip_buf, UIP_BUFSIZE);
  if (uip_len) {

    netEthernetInput();
    return true;
  }

  return false;
}

void netInterfaceXmit(void)
{
  tivaEmacSend(uip_buf, uip_len);
}

#endif

#define BUF ((struct uip_eth_hdr *)&uip_buf16(0))
#define IPBUF ((struct uip_tcpip_hdr *)&uip_buf32(UIP_LLH_LEN))

void netEthernetInput()
{
#if NETSTACK_CONF_WITH_IPV6

  if(BUF->type == uip_htons(UIP_ETHTYPE_IPV6)) {

    tcpip_input();
    return;
  }

#else

  if(BUF->type == uip_htons(UIP_ETHTYPE_ARP)) {

    uip_arp_arpin();
    if(uip_len > 0) {

      netInterfaceXmit();
    }

    return;
  }

  if(BUF->type == uip_htons(UIP_ETHTYPE_IP)) {

    uip_len -= sizeof(struct uip_eth_hdr);
    tcpip_input();
  }

#endif
}

#if NETSTACK_CONF_WITH_IPV6
void netEthernetOutput(const uip_lladdr_t* lladdr)
{
  /*
   * If L3 dest is multicast, build L2 multicast address
   * as per RFC 2464 section 7
   * else fill with th eaddrsess in argument
   */
  if(lladdr == NULL) {
    /* the dest must be multicast */
    (&BUF->dest)->addr[0] = 0x33;
    (&BUF->dest)->addr[1] = 0x33;
    (&BUF->dest)->addr[2] = IPBUF->destipaddr.u8[12];
    (&BUF->dest)->addr[3] = IPBUF->destipaddr.u8[13];
    (&BUF->dest)->addr[4] = IPBUF->destipaddr.u8[14];
    (&BUF->dest)->addr[5] = IPBUF->destipaddr.u8[15];
  } else {
    memcpy(&BUF->dest, lladdr, UIP_LLADDR_LEN);
  }
  memcpy(&BUF->src, &uip_lladdr, UIP_LLADDR_LEN);
  BUF->type = UIP_HTONS(UIP_ETHTYPE_IPV6); //math tmp
   
  uip_len += sizeof(struct uip_eth_hdr);
  netInterfaceXmit();
}
#else
void netEthernetOutput()
{
  uip_arp_out();
  netInterfaceXmit();
}
#endif
