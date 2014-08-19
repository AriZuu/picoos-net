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

/**
 * @file    picoos-net.h
 * @brief   Include file of pico]OS network library.
 * @author  Ari Suutari <ari@stonepile.fi>
 */

/**
 * @mainpage picoos-net - network libarary for pico]OS
 * <b> Table Of Contents </b>
 * - @ref driver-api
 * - @ref socket-api
 * - @ref net-config
 * - @ref uip-config
 * @section overview Overview
 * This library contains IPv4 and IPv6 network stack for pico]OS. Library
 * is based on uIP network stack which has low resource usage, making
 * it suitable for all kinds of microcontrollers. uIP project
 * lives under Contiki OS nowadays.
 *
 * @subsection features Features
 * <b>Socket layer:</b>
 *
 * Simple socket layer, which makes writing of applications much
 * easier than using standard uIP style.
 *
 * <b>Telnet layer:</b>
 *
 * Simple telnet protocol layer based on socket layer to 
 * help writing of CLI applications.
 *
 * <b>Device drivers:</b>
 *
 * Some device drivers suitable for use with socket layer.
 * Currently drivers for ENC28J60, CS8900A and unix tap are included.
 *
 * Typical packet flow when using socket layer is:
 *
 * in:  ::netInterfacePoll -> ::netEthernetInput -> arp -> uip:tcpip_input
 *
 * out: uip:tcpip_output -> ::netInterfaceOutput -> ::netEthernetOutput -> arp -> ::netInterfaceXmit
 */

/** @defgroup api   Network API */
/** @defgroup config   Configuration */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <net/uip.h>
#include <net/uip_arp.h>

/**
 * @defgroup driver-api Driver API
 * @ingroup api
 * @{
 */

/**
 * Device driver function: Transmit current packet to network.
 */
void netInterfaceXmit(void);

/**
 * Device driver function: Poll network adapter for packet. Function should
 * return true if packet is available. It must also deliver the packet for
 * network stack by calling ::netEthernetInput.
 */
bool netInterfacePoll(void);

/**
 * Device driver function: Initialize network interface. Called by socket layer
 * main loop during startup.
 */
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

#if NETCFG_SOCKETS == 1 || DOX == 1

/**
 * Called from network driver interrupt code to wake up 
 * main loop, causing immediate processing of packet.
 */
void netInterrupt(void);

/**
 * Called by network driver init code to
 * enable device polling (instead of using interrupts).
 */
void netEnableDevicePolling(UINT_t ticks);

/** @} */

/**
 * @defgroup socket-api Socket API
 * @ingroup api
 * @{
*/

/** 
 * Initialize socket layer. Before calling this Ethernet and IP address setup
 * must be performed using uip_setethaddr and uip_sethostaddr. 
 *
 * After initializing network interface function starts a thread for main loop,
 * which performs necessary uIP services for socket layer.
 */
void netInit(void);

/**
 * Get socket table slot number for given socket. Used
 * by BSD socket layer to convert socket pointer to 
 * socket descriptor number.
 */
int netSockSlot(NetSock* sock);

/**
 * Get NetSock* pointer for given connection slot.
 */
NetSock* netSockConnection(int slot);

/**
 * Allocate a new socket descriptor by searching
 * for a free one in socket table.
 */
NetSock* netSockAlloc(NetSockState initialState);

/**
 * Free a new socket descriptor.
 */
void netSockFree(NetSock* sock);

/**
 * Create new UDP socket. This is the same as
 * netSockUdpCreate() in previous versions.
 */
NetSock* netSockCreateUDP(uip_ipaddr_t* ip, int port);

/**
 * Create a server socket, for listening incoming connections.
 * This is same as netSockServerCreate in previous versions.
 */
NetSock* netSockCreateTCPServer(int port);

/**
 * Bind server socket to given port before listening on it.
 */
int netSockBind(NetSock* sock, int port);

/**
 * Start listening for incoming connections.
 * Port is set during netSockServerCreate().
 */
void netSockListen(NetSock* sock);

/**
 * Accept new incoming connection.
 */
NetSock* netSockAccept(NetSock* listenSocket, uip_ipaddr_t* peer);

#if UIP_ACTIVE_OPEN == 1 || DOX == 1

/**
 * Create new client connection to given IP address and port.
 * This replaces netSockConnect call in previous library versions.
 */
NetSock* netSockCreateTCP(uip_ipaddr_t* ip, int port);

#endif

/**
 * Connect socket (udp or tcp) to given IP and port.
 * This works like BSD connect() now. To get functionality
 * similar to previous library versions, use netSockCrateTCP().
 */
int netSockConnect(NetSock* sock, uip_ipaddr_t* ip, int port);
/**
 * Set **accept hook** function to handling incoming connections.
 * After accept hook is called by main loop it is responsible
 * for handling the connection. Usually a new pico]OS thread
 * is created to process data.
 * 
 * Accept hook should not block.
 * 
 * Accept hook provides functionality that is similar to unix *accept()*.
 */
void netSockAcceptHookSet(NetSockAcceptHook hook);

/**
 * Read data from socket. Similar to unix *read()*.
 * Function blocks until data is available.
 */
int netSockRead(NetSock* sock, void* data, uint16_t max, uint16_t timeout);

/**
 * Read a line (terminated by CR or NL) from socket.
 */
int netSockReadLine(NetSock* sock, void* data, uint16_t max, uint16_t timeout);

/**
 * Write data to socket.
 */
int netSockWrite(NetSock* sock, const void* data, uint16_t len);

/**
 * Close a connection.
 */
void netSockClose(NetSock* sock);

/*
 * Main thread.
 */
 
void netMainThread(void* arg);

#endif

#if NETCFG_TELNETD == 1 || DOX == 1

#if !NETCFG_BSD_SOCKETS || !NETCFG_COMPAT_SOCKETS
#error BSD sockets required for telnet.
#endif

typedef struct {

  int   state;
  char  inBuf[80];
  char* inPtr;
  int   inLen;
  char  outBuf[80];
  char* outPtr;
  int   sock;
} NetTelnet;

/**
 * Initialize telnet protocol state machine for given socket connection.
 */
void telnetInit(NetTelnet* state, int sock);

/**
 * Write data using telnet protocol.
 */
void telnetWrite(NetTelnet* conn, char* data);

/** 
 * Flush data. Causes immediate sending of data that has been buffered.
 */
void telnetFlush(NetTelnet* conn);

/**
 * Read a line from socket using telnet protocol.
 * Handly when writing CLI servers.
 */
int telnetReadLine(NetTelnet* conn, char* data, int max, int timeout);

#endif

/** @} */

extern POSSEMA_t uipGiant;

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */
