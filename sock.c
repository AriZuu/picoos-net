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

#if UIP_CONF_IPV6
#include "net/uip-ds6.h"
#endif

#if NETCFG_SOCKETS == 1

extern void srvTask(void* arg);

static POSSEMA_t giant;
static POSMUTEX_t uipMutex;
static volatile int dataToSend = 0;
static NetSockAcceptHook acceptHook = NULL;
static volatile UINT_t pollTicks;

static void netSockInit(NetSock* sock)
{
  sock->state = NET_SOCK_BUSY;
  sock->mutex = posMutexCreate();
  sock->sockChange = posFlagCreate();
  sock->uipChange = posFlagCreate();

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

  //posFlagSet(sock->sockChange, 0);

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
  posSemaSignal(giant);

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

    //nosPrint("close request to uip\n");
    sock->state = NET_SOCK_CLOSE;

    dataToSend = 1;
    posSemaSignal(giant);

    while (sock->state == NET_SOCK_CLOSE) {

      //nosPrint("close wait... \n");
      posMutexUnlock(sock->mutex);
      posFlagGet(sock->uipChange, POSFLAG_MODE_GETMASK);
      posMutexLock(sock->mutex);
    }

    //nosPrint("close done \n");
  }

  P_ASSERT("CloseState", (sock->state == NET_SOCK_PEER_CLOSED ||
                          sock->state == NET_SOCK_PEER_ABORTED || 
                          sock->state == NET_SOCK_CLOSE_OK));

  //nosPrint("close destroy req \n");
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

      //nosPrint("New connection\n");
      netSockInit(sock);
      sock->buf = NULL;
      sock->len = 0;
      if ((*acceptHook)(sock, uip_ntohs(uip_conn->lport)) == -1) {

        netSockDestroy(sock);
        uip_abort();
        return;
      }
    }

    if (sock->state == NET_SOCK_CONNECT) {

      posMutexLock(sock->mutex);
      sock->state = NET_SOCK_CONNECT_OK;
      posFlagSet(sock->uipChange, 1);
      posMutexUnlock(sock->mutex);
    }
  }

  if (sock->mutex == NULL) {

    //nosPrint("sock already gone.\n");
    return;
  }

  posMutexLock(sock->mutex);
  netTcpAppcallMutex(sock);
  if (sock->mutex != NULL)
    posMutexUnlock(sock->mutex);
}

static void netAppcallClose(NetSock* sock, NetSockState nextState)
{
  //nosPrint("uip service close\n");
  sock->state = nextState;
  posFlagSet(sock->uipChange, 0);

  while (sock->state != NET_SOCK_DESTROY) {

    //nosPrint("uip wait destroy permission\n");
    posMutexUnlock(sock->mutex);
    posFlagGet(sock->sockChange, POSFLAG_MODE_GETMASK);
    posMutexLock(sock->mutex);
  }

  //nosPrint("uipdestroy sock\n");
  netSockDestroy(sock);
}

static void netTcpAppcallMutex(NetSock* sock)
{
  if (uip_aborted()) {

    //nosPrint("aborted\n");
    netAppcallClose(sock, NET_SOCK_PEER_ABORTED);
    //srvClose(s);
  }

  if (uip_timedout()) {

    //nosPrint("timeout\n");
    netAppcallClose(sock, NET_SOCK_PEER_ABORTED);
    //srvClose(s);
  }

  if(uip_acked()) {

    //nosPrintf("Ack len %d\n", uip_conn->len);

    if (sock->state == NET_SOCK_WRITING) {

      if (sock->len <= uip_mss()) {

        //nosPrint("all sent");
        sock->len = 0;
        sock->state = NET_SOCK_WRITE_OK;
        posFlagSet(sock->uipChange, 0);
        // done signal sem
      }
      else {

        sock->buf = sock->buf + uip_mss();
        sock->len -= uip_mss();
      //nosPrintf("write rest %d\n", sock->len);
        uip_send(sock->buf, sock->len);
      }
    }
  }

  if (uip_newdata()) {

    bool timeout = false;
    int dataLeft = uip_datalen();
    char* dataPtr = uip_appdata;

    //nosPrintf("new data on lport %d\n", uip_ntohs(uip_conn->lport));
    while (dataLeft > 0 && !timeout) {

      while (sock->state != NET_SOCK_READING &&
             sock->state != NET_SOCK_READING_LINE && 
             !timeout) {

      //nosPrintf(" read wait, sock state now %d\n", sock->state);
        posMutexUnlock(sock->mutex);
        timeout = posFlagWait(sock->sockChange, MS(500)) == 0;
        posMutexLock(sock->mutex);
      }

      if (timeout) {

        // Timeout or bad state
        //nosPrint("** ABORT IN READ **\n");
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

//          nosPrintf("line ok, len %d max %d, dataleft %d\n", sock->len, sock->max, dataLeft);
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
    //nosPrint("new data handled\n");
  }

  if (uip_rexmit()) {

    //nosPrint("re transmit\n");
    uip_send(sock->buf, sock->len);
  }

  if (uip_closed()) {

    //nosPrint("closed\n");
    netAppcallClose(sock, NET_SOCK_PEER_CLOSED);
    //srvClose(s);
  }

  if (uip_poll()) {

    if (sock->state == NET_SOCK_CLOSE) {

      uip_close();
      netAppcallClose(sock, NET_SOCK_CLOSE_OK);
    }
    else if (sock->state == NET_SOCK_WRITING) {

      //nosPrintf("write %d\n", sock->len);
      uip_send(sock->buf, sock->len);
    }
  }
}

static void netUdpAppcallMutex(NetSock* sock);

void netUdpAppcall()
{
  NetSock *sock;
  sock = &uip_udp_conn->appstate;

  if (sock->mutex == NULL) {

    //nosPrint("sock already gone.\n");
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

void netInit()
{
  POSTASK_t t;
  int i;

  giant = posSemaCreate(0);
  uipMutex = posMutexCreate();

  pollTicks = INFINITE;
  P_ASSERT("netInit", giant != NULL && uipMutex != NULL);

  POS_SETEVENTNAME(giant, "uip:giant");
  POS_SETEVENTNAME(uipMutex, "uip:mutex");

  dataToSend = 0;

  for(i = 0; i < UIP_CONNS; i++)
    uip_conns[i].appstate.state = NET_SOCK_NULL;

#if UIP_UDP
  for(i = 0; i < UIP_UDP_CONNS; i++)
    uip_udp_conns[i].appstate.state = NET_SOCK_NULL;
#endif /* UIP_UDP */

#if UIP_CONF_IPV6
  uip_ds6_timer_periodic.timer = posTimerCreate();
  uip_ds6_timer_periodic.sema = giant;

#if !UIP_CONF_ROUTER
  uip_ds6_timer_rs.timer = posTimerCreate();
  uip_ds6_timer_rs.sema = giant;
#endif
  
#endif

  netInterfaceInit();
  uip_init();
#if UIP_CONF_IPV6 == 0
  uip_arp_init();
#endif

  t = posTaskCreate(netMainThread, NULL, 10, 500);
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

  posTimerSet(arpTimer, giant, MS(10000), MS(10000));
  posTimerStart(arpTimer);
#endif

  periodicTimer = posTimerCreate();
  P_ASSERT("netMainThread2", periodicTimer != NULL);

  posTimerSet(periodicTimer, giant, MS(500), MS(500));
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

    if (!packetSeen)
      posSemaWait(giant, pollTicks);

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

#if UIP_CONF_IPV6
   
#if !UIP_CONF_ROUTER
    if (posTimerFired(uip_ds6_timer_rs.timer)) {
      uip_ds6_send_rs();
      tcpip_ipv6_output();
    }
#endif

    if (posTimerFired(uip_ds6_timer_periodic.timer)) {

      uip_ds6_periodic();
      tcpip_ipv6_output();
    }
#endif

  }
}

void netEnableDevicePolling(UINT_t ticks)
{
  pollTicks = ticks;
  posSemaSignal(giant);
}

void netInterrupt()
{
  posSemaSignal(giant);
}

#endif
