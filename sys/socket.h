/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 */

/*
 * This is originally from lwIP network stack, but stripped
 * down and modified heavily for picoos-net.
 */

#ifndef __SYS_SOCKET_H
#define __SYS_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "in.h"

typedef uint8_t sa_family_t;
typedef uint16_t in_port_t;

#if !UIP_CONF_IPV6
struct __attribute__((aligned(4))) sockaddr_in {
  uint8_t         sin_len;
  sa_family_t     sin_family;
  in_port_t       sin_port;
  struct in_addr  sin_addr;
#define SIN_ZERO_LEN 8
  char            sin_zero[SIN_ZERO_LEN];
};
#endif

#if UIP_CONF_IPV6
struct __attribute__((aligned(4))) sockaddr_in6 {
  uint8_t         sin6_len;      /* length of this structure */
  sa_family_t     sin6_family;   /* AF_INET6                 */
  in_port_t       sin6_port;     /* Transport layer port #   */
  uint32_t        sin6_flowinfo; /* IPv6 flow information    */
  struct in6_addr sin6_addr;     /* IPv6 address             */
};
#endif

struct __attribute__((aligned(4))) sockaddr {
  uint8_t     sa_len;
  sa_family_t sa_family;
#if UIP_CONF_IPV6
  char        sa_data[22];
#else
  char        sa_data[14];
#endif
};

struct __attribute__((aligned(4))) sockaddr_storage {
  uint8_t     s2_len;
  sa_family_t ss_family;
  char        s2_data1[2];
  uint32_t    s2_data2[3];
#if UIP_CONF_IPV6
  uint32_t    s2_data3[2];
#endif
};

typedef uint32_t socklen_t;

/* Socket protocol types (TCP/UDP) */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2

/*
 * Option flags per-socket. These must match the SOF_ flags in ip.h (checked in init.c)
 */
#define  SO_DEBUG       0x0001 /* Unimplemented */
#define  SO_ACCEPTCONN  0x0002 /* socket has had listen() */
#define  SO_REUSEADDR   0x0004 /* Unimplemented */
#define  SO_KEEPALIVE   0x0008 /* Unimplemented */
#define  SO_DONTROUTE   0x0010 /* Unimplemented */
#define  SO_BROADCAST   0x0020 /* Unimplemented */
#define  SO_USELOOPBACK 0x0040 /* Unimplemented */
#define  SO_LINGER      0x0080 /* Unimplemented */
#define  SO_OOBINLINE   0x0100 /* Unimplemented */
#define  SO_REUSEPORT   0x0200 /* Unimplemented */

#define SO_DONTLINGER   ((int)(~SO_LINGER))

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF    0x1001    /* Unimplemented */
#define SO_RCVBUF    0x1002    /* Unimplemented */
#define SO_SNDLOWAT  0x1003    /* Unimplemented */
#define SO_RCVLOWAT  0x1004    /* Unimplemented */
#define SO_SNDTIMEO  0x1005    /* Unimplemented */
#define SO_RCVTIMEO  0x1006    /* receive timeout */
#define SO_ERROR     0x1007    /* Unimplemented */
#define SO_TYPE      0x1008    /* Unimplemented */
#define SO_CONTIMEO  0x1009    /* Unimplemented */
#define SO_NO_CHECK  0x100a    /* Unimplemented */


/*
 * Structure used for manipulating linger option.
 */
struct linger {
       int l_onoff;                /* option on/off */
       int l_linger;               /* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define  SOL_SOCKET  0xfff    /* options for socket level */


#define AF_UNSPEC       0
#define AF_INET         2
#if UIP_CONF_IPV6
#define AF_INET6        10
#else
#define AF_INET6        AF_UNSPEC
#endif
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6
#define PF_UNSPEC       AF_UNSPEC

#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#if UIP_CONF_IPV6
#define IPPROTO_IPV6    41
#define IPPROTO_ICMPV6  58
#endif
#define IPPROTO_UDPLITE 136
#define IPPROTO_RAW     255

struct timeval {
  long    tv_sec;         /* seconds */
  long    tv_usec;        /* and microseconds */
};

int net_accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int net_bind(int s, const struct sockaddr *name, socklen_t namelen);
int net_getsockopt (int s, int level, int optname, void *optval, socklen_t *optlen);
int net_setsockopt (int s, int level, int optname, const void *optval, socklen_t optlen);
int net_close(int s);
int net_connect(int s, const struct sockaddr *name, socklen_t namelen);
int net_listen(int s, int backlog);
int net_recv(int s, void *mem, size_t len, int flags);
int net_send(int s, const void *dataptr, size_t size, int flags);
int net_socket(int domain, int type, int protocol);
int net_read(int s, void *mem, size_t len);
int net_write(int s, const void *dataptr, size_t size);

#if NETCFG_COMPAT_SOCKETS
#define accept(a,b,c)         net_accept(a,b,c)
#define bind(a,b,c)           net_bind(a,b,c)
#define closesocket(s)        net_close(s)
#define connect(a,b,c)        net_connect(a,b,c)
#define setsockopt(a,b,c,d,e) net_setsockopt(a,b,c,d,e)
#define getsockopt(a,b,c,d,e) net_getsockopt(a,b,c,d,e)
#define listen(a,b)           net_listen(a,b)
#define recv(a,b,c,d)         net_recv(a,b,c,d)
#define send(a,b,c,d)         net_send(a,b,c,d)
#define socket(a,b,c)         net_socket(a,b,c)
#define htons(v)              uip_htons(v)
#endif

#if NETCFG_POSIX_SOCKETS_IO_NAMES
#define read(a,b,c)           net_read(a,b,c)
#define write(a,b,c)          net_write(a,b,c)
#define close(s)              net_close(s)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SOCKET_H */
