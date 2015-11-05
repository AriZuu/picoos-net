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
#include <picoos-net.h>
#include <string.h>
#include <net/ip/uip-split.h>

#ifndef NETCFG_STACK_SIZE
#define NETCFG_STACK_SIZE 500
#endif

#ifndef NETCFG_TASK_PRIORITY
#define NETCFG_TASK_PRIORITY 3
#endif

#if NETSTACK_CONF_WITH_IPV6
#include "net/ipv6/uip-ds6.h"
#endif

#if NETCFG_SOCKETS == 1

extern void srvTask(void* arg);

POSSEMA_t uipGiant;
static POSMUTEX_t uipMutex;
static volatile int dataToSend = 0;
static NetSockAcceptHook acceptHook = NULL;
static volatile UINT_t pollTicks;

#define SOCK_TABSIZE (UIP_CONF_MAX_CONNECTIONS + UIP_CONF_UDP_CONNS + UIP_LISTENPORTS)
NetSock netSocketTable[SOCK_TABSIZE];

int netSockSlot(NetSock* sock)
{
  if (sock == NULL)
    return -1;

  return sock - netSocketTable;
}

NetSock* netSockConnection(int s)
{
  NetSock* sock;

  sock = netSocketTable + s;
  if (sock == NET_SOCK_NULL)
    sock = NULL;

  return sock;
}

NetSock* netSockAlloc(NetSockState initialState)
{
  int      i;
  NetSock* sock;

  // Ensure that this is the only task which manipulates socket table.
  posMutexLock(uipMutex);

  // Find free socket descriptor
  sock = netSocketTable;
  for (i = 0; i < SOCK_TABSIZE; i++, sock++)
    if (sock->state == NET_SOCK_NULL)
      break;

  if (i >= SOCK_TABSIZE) {

    posMutexUnlock(uipMutex);
    return NULL; // No free sockets
  }

  sock->state = initialState;
  sock->mutex = posMutexCreate();
  sock->sockChange = posFlagCreate();
  sock->uipChange = posFlagCreate();
  sock->timeout = INFINITE;
  sock->buf = NULL;
  sock->len = 0;
  sock->max = 0;

  P_ASSERT("netSockInit", sock->mutex != NULL && sock->sockChange != NULL && sock->uipChange != NULL);

  POS_SETEVENTNAME(sock->mutex, "sock:mutex");
  POS_SETEVENTNAME(sock->sockChange, "sock:api");
  POS_SETEVENTNAME(sock->uipChange, "sock:uip");

  posMutexUnlock(uipMutex);
  return sock;
}

#if UIP_ACTIVE_OPEN == 1
NetSock* netSockCreateTCP(uip_ipaddr_t* ip, int port)
{
  NetSock* sock;

  sock = netSockAlloc(NET_SOCK_UNDEF_TCP);
  if (sock == NULL)
    return NULL;

  if (netSockConnect(sock, ip, port) == -1) {

    netSockFree(sock);
    return NULL;
  }

  return sock;
}
#endif

#if UIP_CONF_UDP == 1
NetSock* netSockCreateUDP(uip_ipaddr_t* ip, int port)
{
  NetSock* sock;

  sock = netSockAlloc(NET_SOCK_UNDEF_UDP);
  if (sock == NULL)
    return NULL;

  if (netSockConnect(sock, ip, port) == -1) {

    netSockFree(sock);
    return NULL;
  }

  return sock;
}
#endif

int netSockConnect(NetSock* sock, uip_ipaddr_t* ip, int port)
{
  struct uip_conn* tcp;
  struct uip_udp_conn* udp;

#if UIP_ACTIVE_OPEN == 1
  P_ASSERT("sockConnect", (sock->state == NET_SOCK_UNDEF_TCP || sock->state == NET_SOCK_UNDEF_UDP ||
                           sock->state == NET_SOCK_BOUND || sock->state == NET_SOCK_BOUND_UDP));
#else
  P_ASSERT("sockConnect", (sock->state == NET_SOCK_UNDEF_UDP || NET_SOCK_BOUND_UDP));
#endif

  posMutexLock(uipMutex);

  if (sock->state == NET_SOCK_UNDEF_TCP)  {

#if UIP_ACTIVE_OPEN == 1
    tcp = uip_connect(ip, uip_htons(port));
    if (tcp == NULL) {

      posMutexUnlock(uipMutex);
      return -1;
    }

    tcp->appstate.sock = sock;

    posMutexLock(sock->mutex);
    sock->state = NET_SOCK_CONNECT;
    posMutexUnlock(uipMutex);

    while (sock->state == NET_SOCK_CONNECT) {

      posMutexUnlock(sock->mutex);
      posFlagGet(sock->uipChange, POSFLAG_MODE_GETMASK);
      posMutexLock(sock->mutex);
    }

    if (sock->state == NET_SOCK_PEER_CLOSED || sock->state == NET_SOCK_PEER_ABORTED) {
  
      posMutexUnlock(sock->mutex);
      netSockClose(sock);
      return -1;
    }

    P_ASSERT("sockConnect", sock->state == NET_SOCK_CONNECT_OK);
    sock->state = NET_SOCK_BUSY;
#endif
  }
  else {

#if UIP_CONF_UDP == 1
    udp = uip_udp_new(ip, uip_htons(port));
    if (udp == NULL) {

      posMutexUnlock(uipMutex);
      return -1;
    }

    udp->appstate.sock = sock;
    if (sock->state == NET_SOCK_BOUND_UDP)
      uip_udp_bind(udp, sock->port);

    sock->state = NET_SOCK_BUSY;
#endif
  }

  posMutexUnlock(uipMutex);
  return 0;
}

void netSockAcceptHookSet(NetSockAcceptHook hook)
{
  acceptHook = hook;
}

NetSock* netSockCreateTCPServer(int port)
{
  NetSock* sock;

  sock = netSockAlloc(NET_SOCK_UNDEF_TCP);
  if (sock == NULL)
    return NULL;

  if (netSockBind(sock, port) == -1) {
 
    netSockFree(sock);
    return NULL;
  }

  return sock;
}

int netSockBind(NetSock* sock, int port)
{
#if UIP_ACTIVE_OPEN == 1
  P_ASSERT("sockConnect", (sock->state == NET_SOCK_UNDEF_TCP || sock->state == NET_SOCK_UNDEF_UDP));
#else
  P_ASSERT("sockConnect", (sock->state == NET_SOCK_UNDEF_UDP));
#endif

  sock->port = uip_htons(port);
  sock->state = sock->state == NET_SOCK_UNDEF_TCP ? NET_SOCK_BOUND : NET_SOCK_BOUND_UDP;

  return 0;
}

void netSockListen(NetSock* sock)
{
  posMutexLock(sock->mutex);
  sock->state = NET_SOCK_LISTENING;
  posMutexUnlock(sock->mutex);

  posMutexLock(uipMutex);
  uip_listen(sock->port);
  posMutexUnlock(uipMutex);
}

NetSock* netSockAccept(NetSock* listenSock, uip_ipaddr_t* peer)
{
  NetSock* sock;

  posMutexLock(listenSock->mutex);

  P_ASSERT("sockAccept", listenSock->state == NET_SOCK_LISTENING);

  listenSock->state = NET_SOCK_ACCEPTING;
  posFlagSet(listenSock->sockChange, 0);

  while (listenSock->state == NET_SOCK_ACCEPTING) {

    posMutexUnlock(listenSock->mutex);
    posFlagGet(listenSock->uipChange, POSFLAG_MODE_GETMASK);
    posMutexLock(listenSock->mutex);
  }

  P_ASSERT("sockAccept", listenSock->state == NET_SOCK_ACCEPTED);

  uip_ipaddr_copy(peer, &listenSock->newConnection->ripaddr);
  sock = listenSock->newConnection->appstate.sock;
  listenSock->newConnection = NULL;
  listenSock->state = NET_SOCK_LISTENING;

  posMutexUnlock(listenSock->mutex);

  return sock;
}

static int sockRead(NetSock* sock, NetSockState state, void* data, uint16_t max, uint16_t timeout)
{
  int len;
  bool timedOut = false;

  posMutexLock(sock->mutex);

  if (sock->state == NET_SOCK_PEER_CLOSED) {

    posMutexUnlock(sock->mutex);
    return NET_SOCK_EOF;
  }

  if (sock->state == NET_SOCK_PEER_ABORTED) {

    posMutexUnlock(sock->mutex);
    return NET_SOCK_ABORT;
  }

  P_ASSERT("sockRead", sock->state == NET_SOCK_BUSY);

  sock->state = state;
  sock->buf = data;
  sock->max = max;
  sock->len = 0;

  posFlagSet(sock->sockChange, 0);

  while (sock->state == state && !timedOut) {

    posMutexUnlock(sock->mutex);
    timedOut = posFlagWait(sock->uipChange, timeout) == 0;
    posMutexLock(sock->mutex);
  }

  if (sock->state == NET_SOCK_PEER_CLOSED)
    len = NET_SOCK_EOF;
  else if (sock->state == NET_SOCK_PEER_ABORTED)
    len = NET_SOCK_ABORT;
  else {

    P_ASSERT("sockRead", (timedOut && sock->state == state) || sock->state == NET_SOCK_READ_OK);

    if (timedOut && sock->state == state)
      len = NET_SOCK_TIMEOUT;
    else
      len = sock->len;

    sock->state = NET_SOCK_BUSY;
  }

  posMutexUnlock(sock->mutex);
  return len;
}

int netSockRead(NetSock* sock, void* data, uint16_t max, uint16_t timeout)
{
  return sockRead(sock, NET_SOCK_READING, data, max, timeout);
}

int netSockReadLine(NetSock* sock, void* data, uint16_t max, uint16_t timeout)
{
  return sockRead(sock, NET_SOCK_READING_LINE, data, max, timeout);
}

int netSockWrite(NetSock* sock, const void* data, uint16_t len)
{
  posMutexLock(sock->mutex);

  if (sock->state == NET_SOCK_PEER_CLOSED) {

    posMutexUnlock(sock->mutex);
    return NET_SOCK_EOF;
  }

  if (sock->state == NET_SOCK_PEER_ABORTED) {

    posMutexUnlock(sock->mutex);
    return NET_SOCK_ABORT;
  }

  P_ASSERT("sockWrite", sock->state == NET_SOCK_BUSY);

  sock->state = NET_SOCK_WRITING;
  sock->buf = (void*)data;
  sock->len = len;

  dataToSend = 1;
  posSemaSignal(uipGiant);

  while (sock->state == NET_SOCK_WRITING) {

    posMutexUnlock(sock->mutex);
    posFlagGet(sock->uipChange, POSFLAG_MODE_GETMASK);
    posMutexLock(sock->mutex);
  }

  if (sock->state == NET_SOCK_PEER_CLOSED)
    len = NET_SOCK_EOF;
  else if (sock->state == NET_SOCK_PEER_ABORTED)
    len = NET_SOCK_ABORT;
  else {

    P_ASSERT("sockWrite", sock->state == NET_SOCK_WRITE_OK);

    sock->state = NET_SOCK_BUSY;
  }

  posMutexUnlock(sock->mutex);

  return len;
}

void netSockFree(NetSock* sock)
{
  posMutexDestroy(sock->mutex);
  posFlagDestroy(sock->sockChange);
  posFlagDestroy(sock->uipChange);
  sock->mutex = NULL;
  sock->sockChange = NULL;
  sock->uipChange = NULL;

  sock->state = NET_SOCK_NULL;
}

void netSockClose(NetSock* sock)
{
  posMutexLock(sock->mutex);

  if (sock->state == NET_SOCK_BUSY) {

    sock->state = NET_SOCK_CLOSE;

    dataToSend = 1;
    posSemaSignal(uipGiant);

    while (sock->state == NET_SOCK_CLOSE) {

      posMutexUnlock(sock->mutex);
      posFlagGet(sock->uipChange, POSFLAG_MODE_GETMASK);
      posMutexLock(sock->mutex);
    }
  }

  if (sock->state == NET_SOCK_LISTENING) {

    posMutexLock(uipMutex);
    uip_unlisten(sock->port);
    posMutexUnlock(uipMutex);

    sock->port = 0;
    sock->state = NET_SOCK_CLOSE_OK;
  }

  P_ASSERT("CloseState", (sock->state == NET_SOCK_PEER_CLOSED ||
                          sock->state == NET_SOCK_PEER_ABORTED || 
                          sock->state == NET_SOCK_CLOSE_OK));

  netSockFree(sock);
}

static void netTcpAppcallMutex(NetSock* sock);

void netTcpAppcall()
{
  NetSock *sock = NULL;

  if (uip_connected()) {
    
    if (uip_conn->appstate.sock == NULL) {

      if (acceptHook != NULL) {

        sock = netSockAlloc(NET_SOCK_BUSY);
        if (sock == NULL) {

          uip_abort();
          return;
        }

        uip_conn->appstate.sock = sock;

        if ((*acceptHook)(sock, uip_ntohs(uip_conn->lport)) == -1) {

          netSockFree(sock);
          uip_conn->appstate.sock = NULL;
          uip_abort();
          return;
        }

      }
      else {

        int i;
        NetSock* listenSock;

        listenSock = netSocketTable;
        for (i = 0; i < SOCK_TABSIZE; i++, listenSock++)
          if ((listenSock->state == NET_SOCK_LISTENING ||
              listenSock->state == NET_SOCK_ACCEPTING ||
              listenSock->state == NET_SOCK_ACCEPTED) &&
              listenSock->port == uip_conn->lport)
            break;

        if (i >= SOCK_TABSIZE) {

          uip_abort();
          return;
        }

        bool timeout = false;
 
        posMutexLock(listenSock->mutex);
        while (listenSock->state != NET_SOCK_ACCEPTING && !timeout) {
 
          posMutexUnlock(listenSock->mutex);
          timeout = posFlagWait(listenSock->sockChange, MS(200)) == 0;
          posMutexLock(listenSock->mutex);
        }

        if (timeout) {
      
          uip_abort();
          posMutexUnlock(listenSock->mutex);
          return;
        }

        sock = netSockAlloc(NET_SOCK_BUSY);
        if (sock == NULL) {
      
          uip_abort();
          posMutexUnlock(listenSock->mutex);
          return;
        }

        uip_conn->appstate.sock = sock;
        listenSock->newConnection = uip_conn;
        listenSock->state = NET_SOCK_ACCEPTED;

        posFlagSet(listenSock->uipChange, 0);
        posMutexUnlock(listenSock->mutex);
      }
    }
    else {

      sock = uip_conn->appstate.sock;
      if (sock->state == NET_SOCK_CONNECT) {

        posMutexLock(sock->mutex);
        sock->state = NET_SOCK_CONNECT_OK;
        posFlagSet(sock->uipChange, 1);
        posMutexUnlock(sock->mutex);
      }
    }
  }

  sock = uip_conn->appstate.sock;

  // Check if connection is related to socket.
  // If not, the socket has already been closed and
  // there is nothing more to do.
  if (sock == NULL)
     return;

  posMutexLock(sock->mutex);
  netTcpAppcallMutex(sock);
  if (sock->mutex != NULL)
    posMutexUnlock(sock->mutex);
}

static void netAppcallClose(NetSock* sock, NetSockState nextState)
{
  uip_conn->appstate.sock = NULL;
  sock->state = nextState;
  posFlagSet(sock->uipChange, 0);
}

static void netTcpAppcallMutex(NetSock* sock)
{
  if (uip_aborted()) {

    netAppcallClose(sock, NET_SOCK_PEER_ABORTED);
  }

  if (uip_timedout()) {

    netAppcallClose(sock, NET_SOCK_PEER_ABORTED);
  }

  if(uip_acked()) {

    if (sock->state == NET_SOCK_WRITING) {

      if (sock->len <= uip_mss()) {

        sock->len = 0;
        sock->state = NET_SOCK_WRITE_OK;
        posFlagSet(sock->uipChange, 0);
      }
      else {

        sock->buf = sock->buf + uip_mss();
        sock->len -= uip_mss();
        uip_send(sock->buf, sock->len);
      }
    }
  }

  if (uip_newdata()) {

    bool timeout = false;
    uint16_t dataLeft = uip_datalen();
    char* dataPtr = uip_appdata;

    while (dataLeft > 0 && !timeout) {

      while (sock->state != NET_SOCK_READING &&
             sock->state != NET_SOCK_READING_LINE && 
             !timeout) {

        posMutexUnlock(sock->mutex);
        timeout = posFlagWait(sock->sockChange, MS(500)) == 0;
        posMutexLock(sock->mutex);
      }

      if (timeout) {

        // Timeout or bad state
        uip_abort();
        netAppcallClose(sock, NET_SOCK_PEER_ABORTED);
      }
      else if (sock->state == NET_SOCK_READING_LINE) {

        char ch;

        while (dataLeft && sock->len < sock->max) {
          
          ch = *dataPtr;

          if (ch == '\r') {
       
            ++dataPtr;
            --dataLeft;
            continue;
          }

          sock->buf[sock->len] = ch;
          ++dataPtr;
          --dataLeft;
          ++sock->len;
          if (ch == '\n')
            break;
        }

        if (sock->len && (sock->len == sock->max || sock->buf[sock->len - 1] == '\n')) {

          sock->state = NET_SOCK_READ_OK;
          posFlagSet(sock->uipChange, 0);
        }
      }
      else if (sock->state == NET_SOCK_READING) {

        if (dataLeft > sock->max)
          sock->len = sock->max;
        else
          sock->len = dataLeft;

        memcpy(sock->buf, dataPtr, sock->len);
        dataLeft -= sock->len;
        dataPtr += sock->len;

        sock->state = NET_SOCK_READ_OK;
        posFlagSet(sock->uipChange, 0);
      }
    }
  }

  if (uip_rexmit()) {

    uip_send(sock->buf, sock->len);
  }

  if (uip_closed()) {

    netAppcallClose(sock, NET_SOCK_PEER_CLOSED);
  }

  if (uip_poll()) {

    if (sock->state == NET_SOCK_CLOSE) {

      uip_close();
      netAppcallClose(sock, NET_SOCK_CLOSE_OK);
    }
    else if (sock->state == NET_SOCK_WRITING) {

      uip_send(sock->buf, sock->len);
    }
  }
}

#if UIP_CONF_UDP == 1
static void netUdpAppcallMutex(NetSock* sock);

void netUdpAppcall()
{
  NetSock *sock;
  sock = uip_udp_conn->appstate.sock;

  if (sock->mutex == NULL) {

    return;
  }

  posMutexLock(sock->mutex);
  netUdpAppcallMutex(sock);
  if (sock->mutex != NULL)
    posMutexUnlock(sock->mutex);
}

static void netUdpAppcallMutex(NetSock* sock)
{
  if (uip_newdata()) {

    bool timeout = false;

    while (sock->state != NET_SOCK_READING && !timeout) {

      posMutexUnlock(sock->mutex);
      timeout = posFlagWait(sock->sockChange, MS(500)) == 0;
      posMutexLock(sock->mutex);
    }

    if (!timeout) {

      if (uip_datalen() > sock->max)
        sock->len = sock->max;
      else
        sock->len = uip_datalen();

      memcpy(sock->buf, uip_appdata, sock->len);

      sock->state = NET_SOCK_READ_OK;
      posFlagSet(sock->uipChange, 0);
    }
  }

  if (uip_poll()) {

    if (sock->state == NET_SOCK_CLOSE) {

      uip_udp_remove(uip_udp_conn);
      netAppcallClose(sock, NET_SOCK_CLOSE_OK);
    }
    else if (sock->state == NET_SOCK_WRITING) {

      memcpy(uip_appdata, sock->buf, sock->len);
      uip_udp_send(sock->len);
      sock->state = NET_SOCK_WRITE_OK;
      posFlagSet(sock->uipChange, 0);
    }
  }
}

#endif

void netInit()
{
  POSTASK_t t;
  int i;

  uipGiant = posSemaCreate(0);
  uipMutex = posMutexCreate();

  pollTicks = INFINITE;
  P_ASSERT("netInit", uipGiant != NULL && uipMutex != NULL);

  POS_SETEVENTNAME(uipGiant, "uip:giant");
  POS_SETEVENTNAME(uipMutex, "uip:mutex");

// Initialize contiki-style timers (used by uip code)

  etimer_init();

  dataToSend = 0;
  memset((void*)netSocketTable, '\0', sizeof (netSocketTable));

  for(i = 0; i < UIP_CONNS; i++)
    uip_conns[i].appstate.sock = NULL;

#if UIP_UDP
  for(i = 0; i < UIP_UDP_CONNS; i++)
    uip_udp_conns[i].appstate.sock = NULL;
#endif /* UIP_UDP */

  netInterfaceInit();
  uip_init();

#if NETSTACK_CONF_WITH_IPV6 == 0
  uip_arp_init();
#endif

  t = posTaskCreate(netMainThread, NULL, NETCFG_TASK_PRIORITY, NETCFG_STACK_SIZE);
  P_ASSERT("netInit2", t != NULL);
  POS_SETTASKNAME(t, "uip:main");
}

void netMainThread(void* arg)
{
  uint8_t i;
#if !NETSTACK_CONF_WITH_IPV6
  POSTIMER_t arpTimer;
#endif
  POSTIMER_t periodicTimer;
  int sendRequested;
  bool packetSeen;

#if !NETSTACK_CONF_WITH_IPV6
  arpTimer = posTimerCreate();
  P_ASSERT("netMainThread1", arpTimer != NULL);

  posTimerSet(arpTimer, uipGiant, MS(10000), MS(10000));
  posTimerStart(arpTimer);
#endif

  periodicTimer = posTimerCreate();
  P_ASSERT("netMainThread2", periodicTimer != NULL);

  posTimerSet(periodicTimer, uipGiant, MS(500), MS(500));
  posTimerStart(periodicTimer);

  posMutexLock(uipMutex);

  packetSeen = false;

  while(1) {

    posMutexUnlock(uipMutex);

    // Using semaphore here is not fully optimal.
    // As it is a counting one, it can get bumped
    // to larger value than 1 by upper or interrupt 
    // layer. However, not much harm is done,
    // this loop just spins extra times without
    // doing nothing useful.

    // A Pico]OS Flag object would be perfect,
    // but it doesn't work with posTimer* functions.

    if (!packetSeen || pollTicks == INFINITE)
      posSemaWait(uipGiant, pollTicks);

    posMutexLock(uipMutex);

    sendRequested = dataToSend;
    dataToSend = 0;
    packetSeen = false;

    if (sendRequested) {

      for(i = 0; i < UIP_CONNS; i++) {

        uip_len = 0;
        uip_poll_conn(&uip_conns[i]);
        if(uip_len > 0) {

#if NETCFG_UIP_SPLIT == 1
          uip_split_output();
#else
#if NETSTACK_CONF_WITH_IPV6
          tcpip_ipv6_output();
#else
          tcpip_output();
#endif
#endif
        }
      }

#if UIP_UDP
      for(i = 0; i < UIP_UDP_CONNS; i++) {

        uip_len = 0;
        uip_udp_periodic(i);
        if(uip_len > 0) {

#if NETSTACK_CONF_WITH_IPV6
          tcpip_ipv6_output();
#else
          tcpip_output();
#endif
        }
      }
#endif /* UIP_UDP */

    }

    packetSeen = netInterfacePoll();

    if (posTimerFired(periodicTimer)) {

      for(i = 0; i < UIP_CONNS; i++) {

        uip_periodic(i);
        if(uip_len > 0) {

#if NETCFG_UIP_SPLIT == 1
          uip_split_output();
#else
#if NETSTACK_CONF_WITH_IPV6
          tcpip_ipv6_output();
#else
          tcpip_output();
#endif
#endif
        }
      }

#if UIP_UDP
      for(i = 0; i < UIP_UDP_CONNS; i++) {

        uip_udp_periodic(i);
        if(uip_len > 0) {

#if NETSTACK_CONF_WITH_IPV6
          tcpip_ipv6_output();
#else
          tcpip_output();
#endif
        }
      }
#endif /* UIP_UDP */

    }

#if NETSTACK_CONF_WITH_IPV6 == 0
    if (posTimerFired(arpTimer)) {

      uip_arp_timer();
    }
#endif

// Run contiki-style timers.
// Instead of posting events to process like
// contiki does, it just calls common callback function
// to do the work.

    etimer_request_poll();

  }
}

void etimer_callback(struct etimer* et)
{
#if NETSTACK_CONF_WITH_IPV6
   
#if !UIP_CONF_ROUTER
    if (et == &uip_ds6_timer_rs) {

      uip_ds6_send_rs();
      tcpip_ipv6_output();
    }
#endif

    if (et == &uip_ds6_timer_periodic) {

      uip_ds6_periodic();
      tcpip_ipv6_output();
    }
#endif

}

void netEnableDevicePolling(UINT_t ticks)
{
  pollTicks = ticks;
  posSemaSignal(uipGiant);
}

void netInterrupt()
{
  posSemaSignal(uipGiant);
}

#endif
