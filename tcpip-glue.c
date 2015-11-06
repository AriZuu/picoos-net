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
#include <picoos-u.h>
#include <picoos-net.h>
#include <string.h>
#include <net/ip/uip-split.h>

void tcpip_input()
{
  uip_input();
  if(uip_len > 0) {

#if NETCFG_UIP_SPLIT == 1
    uip_split_output();
#else
    tcpip_output();
#endif
  }

  uip_len = 0;
#if NETSTACK_CONF_WITH_IPV6
  uip_ext_len = 0;
#endif /*NETSTACK_CONF_WITH_IPV6*/
}

#if NETSTACK_CONF_WITH_IPV6

uint8_t
tcpip_output(const uip_lladdr_t *a)
{
  netInterfaceOutput(a);
  return 0;
}

#else

uint8_t tcpip_output(void)
{
  netInterfaceOutput();
  return 0;
}

#endif

#if UIP_CONF_LOGGING
void uip_log(char* m)
{
  nosPrintf("uip: %s", m);
  nosPrint("\n");
}
#endif
