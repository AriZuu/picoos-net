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
#include <picoos-u.h>
#include <picoos-net.h>
#include <sys/socket.h>
#include <string.h>
#include <net/ip/uiplib.h>

#if NETCFG_BSD_SOCKETS == 1

#if NETSTACK_CONF_WITH_IPV6
#define SOCKADDR2UIP(sa) (&(((struct sockaddr_in6*)sa)->sin6_addr.un.uip))
#define SOCKADDR2PORT(sa) (((struct sockaddr_in6*)sa)->sin6_port)
#else
#define SOCKADDR2UIP(sa) (&(((struct sockaddr_in*)sa)->sin_addr.uip))
#define SOCKADDR2PORT(sa) (((struct sockaddr_in*)sa)->sin_port)
#endif

int net_socket(int domain, int type, int protocol)
{
  UosFile* sock;

  if (type == SOCK_STREAM)
    sock = netSockAlloc(NET_SOCK_UNDEF_TCP);
  else if (type == SOCK_DGRAM)
    sock = netSockAlloc(NET_SOCK_UNDEF_UDP);
  else {

    return -1; // Bad socket type
  }

  return uosFile2Slot(sock);
}

int net_close(int s)
{
  uosFileClose(uosSlot2File(s));
  return 0;
}

int net_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
  return netSockConnect(uosSlot2File(s), SOCKADDR2UIP(name), uip_ntohs(SOCKADDR2PORT(name)));
}

int net_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
  UosFile* sock;

  sock = netSockAccept(uosSlot2File(s), SOCKADDR2UIP(addr));
  if (sock == NULL)
    return -1;

#if NETSTACK_CONF_WITH_IPV6
  *addrlen = sizeof(struct sockaddr_in6);
#else
  *addrlen = sizeof(struct sockaddr_in);
#endif

  return uosFile2Slot(sock);
}

int net_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
  return netSockBind(uosSlot2File(s), uip_ntohs(SOCKADDR2PORT(name)));
}

int net_listen(int s, int backlog)
{
  netSockListen(uosSlot2File(s));
  return 0;
}

int net_getsockopt (int s, int level, int optname, void *optval, socklen_t *optlen)
{
  return -1;
}

int net_setsockopt (int s, int level, int optname, const void *optval, socklen_t optlen)
{
  UosFile* file = uosSlot2File(s);

  if (optname == SO_RCVTIMEO) {

    const struct timeval* tv = optval;

    if (tv->tv_sec == 0 && tv->tv_usec == 0)
      netSockTimeout(file, INFINITE);
    else
      netSockTimeout(file, MS(tv->tv_sec * 1000 + tv->tv_usec / 1000));

    return 0;
  }

  return -1;
}

int net_recv(int s, void *dataptr, size_t size, int flags)
{
  UosFile* sock = uosSlot2File(s);
  int len;

  len = uosFileRead(sock, dataptr, size);
  if (len == NET_SOCK_EOF)
    return 0;

  if (len < 0)
    return -1;

  return len;
}

int net_send(int s, const void *dataptr, size_t size, int flags)
{
  UosFile* sock = uosSlot2File(s);

  if (uosFileWrite(sock, dataptr, size) < (int)size)
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

#if !NETSTACK_CONF_WITH_IPV6
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
