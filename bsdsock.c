/*
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
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
#include <sys/socket.h>
#include <string.h>
#include <net/uiplib.h>

#if NETCFG_BSD_SOCKETS == 1

#define	SOCKET_FREE       ((void*)0)
#define SOCKET_UNDEF_TCP  ((void*)1)
#define SOCKET_UNDEF_UDP  ((void*)2)

#if UIP_CONF_IPV6
#define SOCKADDR2UIP(sa) (&(((struct sockaddr_in6*)sa)->sin6_addr.un.uip))
#define SOCKADDR2PORT(sa) (((struct sockaddr_in6*)sa)->sin6_port)
#else
#define SOCKADDR2UIP(sa) (&(((struct sockaddr_in*)sa)->sin_addr.uip))
#define SOCKADDR2PORT(sa) (((struct sockaddr_in*)sa)->sin_port)
#endif

static NetSock* socketTable[UIP_CONF_MAX_CONNECTIONS + UIP_CONF_UDP_CONNS];
POSMUTEX_t socketTableMutex;

void netInitBSDSockets()
{
  memset(socketTable, '\0', sizeof (socketTable));
  socketTableMutex = posMutexCreate();
}

NetSock* net_connection(int s)
{
  NetSock* sock;

  sock = socketTable[s];
  if (sock == SOCKET_UNDEF_TCP || sock == SOCKET_UNDEF_UDP)
    sock = NULL;

  return sock;
}

int net_socket(int domain, int type, int protocol)
{
  int i;

  // Ensure that this is the only task which manipulates socket table.
  posMutexLock(socketTableMutex);

  // Find free socket descriptor
  for (i = 0; i < UIP_CONF_MAX_CONNECTIONS + UIP_CONF_UDP_CONNS; i++)
    if (socketTable[i] == SOCKET_FREE)
      break;

  if (i >= UIP_CONF_MAX_CONNECTIONS + UIP_CONF_UDP_CONNS) {

    posMutexUnlock(socketTableMutex);
    return -1; // No free sockets
  }

  if (type == SOCK_STREAM)
    socketTable[i] = SOCKET_UNDEF_TCP;
  else if (type == SOCK_DGRAM)
    socketTable[i] = SOCKET_UNDEF_UDP;
  else {

    posMutexUnlock(socketTableMutex);
    return -1; // Bad socket type
  }

  posMutexUnlock(socketTableMutex);
  return i;
}

int net_close(int s)
{
  P_ASSERT("net_close", socketTable[s] != SOCKET_FREE);

  if (socketTable[s] != SOCKET_UNDEF_TCP && socketTable[s] != SOCKET_UNDEF_UDP)
    netSockClose(socketTable[s]);

  posMutexLock(socketTableMutex);
  socketTable[s] = NULL;
  posMutexUnlock(socketTableMutex);
  return 0;
}

int net_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
  NetSock* sock;

  if (socketTable[s] == SOCKET_UNDEF_TCP) {

    sock = netSockConnect(SOCKADDR2UIP(name), uip_ntohs(SOCKADDR2PORT(name)));
  }
  else if (socketTable[s] == SOCKET_UNDEF_UDP) {

    sock = netSockUdpCreate(SOCKADDR2UIP(name), uip_ntohs(SOCKADDR2PORT(name)));
  }
  else
    return -1;

  if (sock == NULL)
    return -1;

  socketTable[s] = sock;
  return 0;
}

int net_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
  NetSock* lsn = socketTable[s];
  NetSock* sock;
  int i;

  P_ASSERT("net_close", (lsn != SOCKET_FREE && lsn != SOCKET_UNDEF_TCP && lsn != SOCKET_UNDEF_UDP));

  sock = netSockAccept(lsn, SOCKADDR2UIP(addr));
  if (sock == NULL)
    return -1;

#if UIP_CONF_IPV6
  *addrlen = sizeof(struct sockaddr_in6);
#else
  *addrlen = sizeof(struct sockaddr_in);
#endif

  // Ensure that this is the only task which manipulates socket table.
  posMutexLock(socketTableMutex);

  // Find free socket descriptor
  for (i = 0; i < UIP_CONF_MAX_CONNECTIONS + UIP_CONF_UDP_CONNS; i++)
    if (socketTable[i] == SOCKET_FREE)
      break;

  if (i >= UIP_CONF_MAX_CONNECTIONS + UIP_CONF_UDP_CONNS) {

    posMutexUnlock(socketTableMutex);
    netSockClose(sock);
    return -1; // No free sockets
  }

  socketTable[i] = sock;
  posMutexUnlock(socketTableMutex);

  return i;
}

int net_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
  NetSock* sock;

  if (socketTable[s] == SOCKET_UNDEF_TCP) {

    sock = netSockServerCreate(uip_ntohs(SOCKADDR2PORT(name)));
  }
  else
    return -1;

  if (sock == NULL)
    return -1;

  socketTable[s] = sock;
  return 0;
}

int net_listen(int s, int backlog)
{
  NetSock* sock = socketTable[s];

  P_ASSERT("net_close", (sock != SOCKET_FREE && sock != SOCKET_UNDEF_TCP && sock != SOCKET_UNDEF_UDP));

  netSockListen(sock);
  return 0;
}

int net_getsockopt (int s, int level, int optname, void *optval, socklen_t *optlen)
{
  return -1;
}

int net_setsockopt (int s, int level, int optname, const void *optval, socklen_t optlen)
{
  NetSock* sock = socketTable[s];

  P_ASSERT("net_close", (sock != SOCKET_FREE && sock != SOCKET_UNDEF_TCP && sock != SOCKET_UNDEF_UDP));

  if (optname == SO_RCVTIMEO) {

    const struct timeval* tv = optval;

    if (tv->tv_sec == 0 && tv->tv_usec == 0)
      sock->timeout = INFINITE;
    else
      sock->timeout = MS(tv->tv_sec * 1000 + tv->tv_usec / 1000);

    return 0;
  }

  return -1;
}

int net_recv(int s, void *dataptr, size_t size, int flags)
{
  NetSock* sock = socketTable[s];
  int len;

  P_ASSERT("net_close", (sock != SOCKET_FREE && sock != SOCKET_UNDEF_TCP && sock != SOCKET_UNDEF_UDP));
  len = netSockRead(sock, dataptr, size, sock->timeout);
  if (len == NET_SOCK_EOF)
    return 0;

  if (len < 0)
    return -1;

  return len;
}

int net_send(int s, const void *dataptr, size_t size, int flags)
{
  NetSock* sock = socketTable[s];

  P_ASSERT("net_close", (sock != SOCKET_FREE && sock != SOCKET_UNDEF_TCP && sock != SOCKET_UNDEF_UDP));
  if (netSockWrite(sock, dataptr, size) < (int)size)
    return -1;

  return size;
}

int net_read(int s, void *mem, size_t len)
{
  return net_recv(s, mem, len, 0);
}

int net_write(int s, const void *dataptr, size_t size)
{
  return net_send(s, dataptr, size, 0);
}

#if !UIP_CONF_IPV6
int inet_aton(const char *cp, struct in_addr *pin)
{
  return uiplib_ipaddrconv(cp, &pin->uip);
}
#endif

int inet_pton(int af, const char *src, void *dst)
{
  return uiplib_ipaddrconv(src, (uip_ipaddr_t*)dst);
}

#endif