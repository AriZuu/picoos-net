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

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <net/uip.h>
#include <net/uip_arp.h>

/*
 * Packet flow
 * in:  netInterfacePoll -> netEthernetInput -> arp -> uip:tcpip_input
 * out: uip:tcpip_output -> netInterfaceOutput -> netEthernetOutput -> arp -> netInterfaceXmit
 */

void netInterfaceXmit(void);
bool netInterfacePoll(void);
void netInterfaceInit(void);

/**
 * Pass outgoing packet to interface layer for sending.
 * Before transmitting packet ARP processing is done
 * if needed by device driver (ethernet).
 */
#if UIP_CONF_IPV6
void netInterfaceOutput(uip_lladdr_t* lla);
#else
void netInterfaceOutput(void);
#endif

/** 
 * Pass packet has been received to  ethernet layer.
 * Typically called from device driver for
 * ethernet-type devices. Function performs
 * either arp processing or passes packet to upper layer.
 */
void netEthernetInput(void);

/**
 * Pass outgoing packet to ethernet layer. Performs
 * arp lookup before passing packet to interface
 * xmit function. Called by netIntefaceOutput
 * for ethernet-type devices.
 */
#if UIP_CONF_IPV6
void netEthernetOutput(uip_lladdr_t* lla);
#else
void netEthernetOutput(void);
#endif

#if NETCFG_SOCKETS == 1
void netInit(void);
NetSock* netSockUdpCreate(uip_ipaddr_t* ip, int port);

#if UIP_ACTIVE_OPEN == 1
NetSock* netSockConnect(uip_ipaddr_t* ip, int port);
#endif

void netSockAcceptHookSet(NetSockAcceptHook hook);
int netSockRead(NetSock* sock, void* data, uint16_t max, uint16_t timeout);
int netSockReadLine(NetSock* sock, void* data, uint16_t max, uint16_t timeout);
int netSockWrite(NetSock* sock, const void* data, uint16_t len);
void netSockClose(NetSock* sock);
void netMainThread(void* arg);
void netInterrupt(void);
void netEnableDevicePolling(UINT_t ticks);

#endif

#if NETCFG_TELNETD == 1

typedef struct {

  int   state;
  char  inBuf[80];
  char* inPtr;
  int   inLen;
  char  outBuf[80];
  char* outPtr;
  NetSock* sock;
} NetTelnet;

void telnetInit(NetTelnet* state, NetSock* sock);
void telnetWrite(NetTelnet* conn, char* data);
void telnetFlush(NetTelnet* conn);
int telnetReadLine(NetTelnet* conn, char* data, int max, int timeout);

#endif

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */
