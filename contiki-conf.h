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

#ifndef _CONTIKI_CONF_H
#define _CONTIKI_CONF_H

#include <picoos.h>
#include <stdbool.h>
#include <inttypes.h>

typedef unsigned short uip_stats_t;

#define CCIF

#include "netcfg.h"

typedef JIF_t clock_time_t;

#define CLOCK_CONF_SECOND HZ

#include <sys/timer.h>
#include <sys/etimer.h>

#ifndef NETCFG_UIP_SPLIT
#define NETCFG_UIP_SPLIT 1
#endif

#ifndef UIP_CONF_BYTE_ORDER
#define UIP_CONF_BYTE_ORDER       LITTLE_ENDIAN
#endif

#if NETCFG_SOCKETS == 1

typedef enum {

  NET_SOCK_NULL,
  NET_SOCK_BUSY,
  NET_SOCK_READING,
  NET_SOCK_READING_LINE,
  NET_SOCK_READ_OK,
  NET_SOCK_WRITING,
  NET_SOCK_WRITE_OK,
  NET_SOCK_CONNECT,
  NET_SOCK_CONNECT_OK,
  NET_SOCK_CLOSE,
  NET_SOCK_CLOSE_OK,
  NET_SOCK_PEER_CLOSED,
  NET_SOCK_PEER_ABORTED,
  NET_SOCK_DESTROY

} NetSockState;

#define NET_SOCK_EOF	 0
#define NET_SOCK_ABORT	 -1
#define	NET_SOCK_TIMEOUT -2

struct netSockState {

  POSFLAG_t sockChange;
  POSFLAG_t uipChange;
  POSMUTEX_t mutex;

  NetSockState state;
  uint16_t len;
  uint16_t max;
  char* buf;
};

typedef struct netSockState volatile NetSock;
typedef NetSock uip_tcp_appstate_t;
typedef NetSock uip_udp_appstate_t;
typedef int (*NetSockAcceptHook)(NetSock* sock, int port);

#define UIP_APPCALL netTcpAppcall
void netTcpAppcall(void);

#if UIP_CONF_UDP == 1

#define UIP_UDP_APPCALL netUdpAppcall
void netUdpAppcall(void);

#endif
#endif

#endif
