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
#include <net/uip-split.h>

#ifndef NETCFG_STACK_SIZE
#define NETCFG_STACK_SIZE 500
#endif

#ifndef NETCFG_TASK_PRIORITY
#define NETCFG_TASK_PRIORITY 3
#endif

#if UIP_CONF_IPV6
#include "net/uip-ds6.h"
#endif

#if NETCFG_SOCKETS == 1

extern void srvTask(void* arg);

POSSEMA_t uipGiant;
static POSMUTEX_t uipMutex;
static volatile int dataToSend = 0;
static NetSockAcceptHook acceptHook = NULL;
static volatile UINT_t pollTicks;

static NetSock listeningSockets[UIP_LISTENPORTS];

static void netSockInit(NetSock* sock)
{
  sock->state = NET_SOCK_BUSY;
  sock->mutex = posMutexCreate();
  sock->sockChange = posFlagCreate();
  sock->uipChange = posFlagCreate();
  sock->timeout = INFINITE;

  P_ASSERT("netSockInit", sock->mutex != NULL && sock->sockChange != NULL && sock->uipChange != NULL);

  POS_SETEVENTNAME(sock->mutex, "sock:mutex");
  POS_SETEVENTNAME(sock->sockChange, "sock:api");
  POS_SETEVENTNAME(sock->uipChange, "sock:uip");
}

void netSockAcceptHookSet(NetSockAcceptHook hook)
{
  acceptHook = hook;
}

NetSock* netSockUdpCreate(uip_ipaddr_t* ip, int port)
{
  struct uip_udp_conn* conn;

  posMutexLock(uipMutex);
  conn = uip_udp_new(ip, uip_htons(port));
  netSockInit(&conn->appstate);
  posMutexUnlock(uipMutex);

  return &conn->appstate;
}

NetSock* netSockServerCreate(int port)
{
  int      i;
  NetSock* sock;

  posMutexLock(uipMutex);

  for (i = 0; i < UIP_LISTENPORTS; i++)
    if (listeningSockets[i].port == 0)
      break;

  if (i >= UIP_LISTENPORTS) {

    posMutexUnlock(uipMutex);
    return NULL;
  }
 
  sock = listeningSockets + i;
  netSockInit(sock);

  sock->port = uip_htons(port);
  sock->state = NET_SOCK_LISTENING;

  posMutexUnlock(uipMutex);

  return sock;
}

void netSockListen(NetSock* sock)
{
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
  sock = &listenSock->newConnection->appstate;
  listenSock->newConnection = NULL;
  listenSock->state = NET_SOCK_LISTENING;

  posMutexUnlock(listenSock->mutex);

  return sock;
}

#if UIP_ACTIVE_OPEN == 1
NetSock* netSockConnect(uip_ipaddr_t* ip, int port)
{
  struct uip_conn* conn;
  NetSock* sock;

  posMutexLock(uipMutex);
  conn = uip_connect(ip, uip_htons(port));
  sock = &conn->appstate;

  netSockInit(sock);
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
    return NULL;
  }

  P_ASSERT("sockConnect", sock->state == NET_SOCK_CONNECT_OK);
  sock->state = NET_SOCK_BUSY;
  posMutexUnlock(sock->mutex);

  return &conn->appstate;
}
#endif

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

static void netSockDestroy(NetSock* sock)
{
  posMutexDestroy(sock->mutex);
  posFlagDestroy(sock->sockChange);
  posFlagDestroy(sock->uipChange);
  sock->state = NET_SOCK_NULL;
  sock->mutex = NULL;
  sock->sockChange = NULL;
  sock->uipChange = NULL;
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

    uip_unlisten(sock->port);
    sock->port = 0;
    sock->state = NET_SOCK_CLOSE_OK;
  }

  P_ASSERT("CloseState", (sock->state == NET_SOCK_PEER_CLOSED ||
                          sock->state == NET_SOCK_PEER_ABORTED || 
                          sock->state == NET_SOCK_CLOSE_OK));

  sock->state = NET_SOCK_DESTROY;
  posFlagSet(sock->sockChange, 0);
  posMutexUnlock(sock->mutex);
}

static void netTcpAppcallMutex(NetSock* sock);

void netTcpAppcall()
{
  NetSock *sock;
  sock = &uip_conn->appstate;

  if (uip_connected()) {
    
    if (sock->state == NET_SOCK_NULL) {

      if (acceptHook != NULL) {

        netSockInit(sock);
        sock->buf = NULL;
        sock->len = 0;
        if ((*acceptHook)(sock, uip_ntohs(uip_conn->lport)) == -1) {

          netSockDestroy(sock);
          uip_abort();
          return;
        }
      }
      else {

        int i;

        for (i = 0; i < UIP_LISTENPORTS; i++)
          if (listeningSockets[i].port == uip_conn->lport)
            break;

        if (i >= UIP_LISTENPORTS) {

          uip_abort();
          return;
        }

        NetSock* listenSock = listeningSockets + i;
        bool timeout = false;
 
        posMutexLock(listenSock->mutex);
        while (listenSock->state != NET_SOCK_ACCEPTING) {
 
          posMutexUnlock(listenSock->mutex);
          timeout = posFlagWait(listenSock->sockChange, MS(500)) == 0;
          posMutexLock(listenSock->mutex);
        }

        if (timeout) {
      
          uip_abort();
          posMutexUnlock(listenSock->mutex);
          return;
        }

        netSockInit(sock);

        sock->buf = NULL;
        sock->len = 0;
        listenSock->newConnection = uip_conn;
        listenSock->state = NET_SOCK_ACCEPTED;

        posFlagSet(listenSock->uipChange, 0);
        posMutexUnlock(listenSock->mutex);
      }
    }

    if (sock->state == NET_SOCK_CONNECT) {

      posMutexLock(sock->mutex);
      sock->state = NET_SOCK_ACCEPTED;
      posFlagSet(sock->uipChange, 1);
      posMutexUnlock(sock->mutex);
    }
  }

  if (sock->mutex == NULL) {

    return;
  }

  posMutexLock(sock->mutex);
  netTcpAppcallMutex(sock);
  if (sock->mutex != NULL)
    posMutexUnlock(sock->mutex);
}

static void netAppcallClose(NetSock* sock, NetSockState nextState)
{
  sock->state = nextState;
  posFlagSet(sock->uipChange, 0);

  while (sock->state != NET_SOCK_DESTROY) {

    posMutexUnlock(sock->mutex);
    posFlagGet(sock->sockChange, POSFLAG_MODE_GETMASK);
    posMutexLock(sock->mutex);
  }

  netSockDestroy(sock);
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
  sock = &uip_udp_conn->appstate;

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

  memset((void*)listeningSockets, '\0', sizeof(listeningSockets));
  for(i = 0; i < UIP_CONNS; i++)
    uip_conns[i].appstate.state = NET_SOCK_NULL;

#if UIP_UDP
  for(i = 0; i < UIP_UDP_CONNS; i++)
    uip_udp_conns[i].appstate.state = NET_SOCK_NULL;
#endif /* UIP_UDP */

#if NETCFG_BSD_SOCKETS == 1
  netInitBSDSockets();
#endif

  netInterfaceInit();
  uip_init();

#if UIP_CONF_IPV6 == 0
  uip_arp_init();
#endif

  t = posTaskCreate(netMainThread, NULL, NETCFG_TASK_PRIORITY, NETCFG_STACK_SIZE);
  P_ASSERT("netInit2", t != NULL);
  POS_SETTASKNAME(t, "uip:main");
}

void netMainThread(void* arg)
{
  uint8_t i;
#if !UIP_CONF_IPV6
  POSTIMER_t arpTimer;
#endif
  POSTIMER_t periodicTimer;
  int sendRequested;
  bool packetSeen;

#if !UIP_CONF_IPV6
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
#if UIP_CONF_IPV6
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

#if UIP_CONF_IPV6
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
#if UIP_CONF_IPV6
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

#if UIP_CONF_IPV6
          tcpip_ipv6_output();
#else
          tcpip_output();
#endif
        }
      }
#endif /* UIP_UDP */

    }

#if UIP_CONF_IPV6 == 0
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
#if UIP_CONF_IPV6
   
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
