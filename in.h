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
 */

/*
 * This is originally from lwIP network stack, but stripped
 * down and modified heavily for picoos-net.
 */

#ifndef __IN_H
#define __IN_H

#ifdef __cplusplus
extern "C" {
#endif

#if !NETSTACK_CONF_WITH_IPV6

typedef uint32_t in_addr_t;

struct in_addr {
  union {
    in_addr_t    s_addr;
    uip_ipaddr_t uip;
  };
};

#define	INADDR_ANY		(uint32_t)0x00000000
#define	INADDR_BROADCAST	(uint32_t)0xffffffff

int inet_aton(const char *cp, struct in_addr *pin);
int inet_pton(int af, const char *src, void *dst);

#else
#include "in6.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __IN_H */
