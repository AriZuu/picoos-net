/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
 * Copyright (c) 2014, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <picoos.h>
#include <picoos-net.h>
#include <string.h>

#if UIP_CONF_UDP > 0
#include "net/ip/dhcpc.h"

#if !UIP_BROADCAST
#error UIP_CONF_BROADCAST must be set to 1
#endif

#define STATE_INITIAL         0
#define STATE_SENDING         1
#define STATE_OFFER_RECEIVED  2
#define STATE_CONFIG_RECEIVED 3

static struct dhcpc_state s;

struct dhcp_msg {
  uint8_t op, htype, hlen, hops;
  uint8_t xid[4];
  uint16_t secs, flags;
  uint8_t ciaddr[4];
  uint8_t yiaddr[4];
  uint8_t siaddr[4];
  uint8_t giaddr[4];
  uint8_t chaddr[16];
#ifndef UIP_CONF_DHCP_LIGHT
  uint8_t sname[64];
  uint8_t file[128];
#endif
  uint8_t options[312];
};

#define BOOTP_BROADCAST 0x8000

#define DHCP_REQUEST        1
#define DHCP_REPLY          2
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6
#define DHCP_MSG_LEN      236

#define DHCPC_SERVER_PORT  67
#define DHCPC_CLIENT_PORT  68

#define DHCPDISCOVER  1
#define DHCPOFFER     2
#define DHCPREQUEST   3
#define DHCPDECLINE   4
#define DHCPACK       5
#define DHCPNAK       6
#define DHCPRELEASE   7

#define DHCP_OPTION_SUBNET_MASK   1
#define DHCP_OPTION_ROUTER        3
#define DHCP_OPTION_DNS_SERVER    6
#define DHCP_OPTION_REQ_IPADDR   50
#define DHCP_OPTION_LEASE_TIME   51
#define DHCP_OPTION_MSG_TYPE     53
#define DHCP_OPTION_SERVER_ID    54
#define DHCP_OPTION_REQ_LIST     55
#define DHCP_OPTION_END         255

static uint32_t xid;
static const uint8_t magic_cookie[4] = {99, 130, 83, 99};
/*---------------------------------------------------------------------------*/
static uint8_t *
add_msg_type(uint8_t *optptr, uint8_t type)
{
  *optptr++ = DHCP_OPTION_MSG_TYPE;
  *optptr++ = 1;
  *optptr++ = type;
  return optptr;
}
/*---------------------------------------------------------------------------*/
static uint8_t *
add_server_id(uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_SERVER_ID;
  *optptr++ = 4;
  memcpy(optptr, s.serverid, 4);
  return optptr + 4;
}
/*---------------------------------------------------------------------------*/
static uint8_t *
add_req_ipaddr(uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_IPADDR;
  *optptr++ = 4;
  memcpy(optptr, s.ipaddr.u16, 4);
  return optptr + 4;
}
/*---------------------------------------------------------------------------*/
static uint8_t *
add_req_options(uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_LIST;
  *optptr++ = 3;
  *optptr++ = DHCP_OPTION_SUBNET_MASK;
  *optptr++ = DHCP_OPTION_ROUTER;
  *optptr++ = DHCP_OPTION_DNS_SERVER;
  return optptr;
}
/*---------------------------------------------------------------------------*/
static uint8_t *
add_end(uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_END;
  return optptr;
}
/*---------------------------------------------------------------------------*/
static void
create_msg(struct dhcp_msg *m)
{
  m->op = DHCP_REQUEST;
  m->htype = DHCP_HTYPE_ETHERNET;
  m->hlen = s.mac_len;
  m->hops = 0;
  memcpy(m->xid, &xid, sizeof(m->xid));
  m->secs = 0;
  m->flags = UIP_HTONS(BOOTP_BROADCAST); /*  Broadcast bit. */
  /*  uip_ipaddr_copy(m->ciaddr, uip_hostaddr);*/
  memcpy(m->ciaddr, uip_hostaddr.u16, sizeof(m->ciaddr));
  memset(m->yiaddr, 0, sizeof(m->yiaddr));
  memset(m->siaddr, 0, sizeof(m->siaddr));
  memset(m->giaddr, 0, sizeof(m->giaddr));
  memcpy(m->chaddr, s.mac_addr, s.mac_len);
  memset(&m->chaddr[s.mac_len], 0, sizeof(m->chaddr) - s.mac_len);
#ifndef UIP_CONF_DHCP_LIGHT
  memset(m->sname, 0, sizeof(m->sname));
  memset(m->file, 0, sizeof(m->file));
#endif

  memcpy(m->options, magic_cookie, sizeof(magic_cookie));
}
/*---------------------------------------------------------------------------*/

static struct dhcp_msg dhcp_msg_buf;

static void
send_discover(void)
{
  uint8_t *end;
  struct dhcp_msg *m = (struct dhcp_msg *)&dhcp_msg_buf;

  create_msg(m);

  end = add_msg_type(&m->options[4], DHCPDISCOVER);
  end = add_req_options(end);
  end = add_end(end);

  uosFileWrite(s.conn, (void*)m, end - (uint8_t*)m);
}
/*---------------------------------------------------------------------------*/
static void
send_request(void)
{
  uint8_t *end;
  struct dhcp_msg *m = (struct dhcp_msg *)&dhcp_msg_buf;

  create_msg(m);
  
  end = add_msg_type(&m->options[4], DHCPREQUEST);
  end = add_server_id(end);
  end = add_req_ipaddr(end);
  end = add_end(end);
  
  uosFileWrite(s.conn, (void*)m, end - (uint8_t*)m);
}
/*---------------------------------------------------------------------------*/
static uint8_t
parse_options(uint8_t *optptr, int len)
{
  uint8_t *end = optptr + len;
  uint8_t type = 0;

  while(optptr < end) {
    switch(*optptr) {
    case DHCP_OPTION_SUBNET_MASK:
      memcpy(s.netmask.u16, optptr + 2, 4);
      break;
    case DHCP_OPTION_ROUTER:
      memcpy(s.default_router.u16, optptr + 2, 4);
      break;
    case DHCP_OPTION_DNS_SERVER:
      memcpy(s.dnsaddr.u16, optptr + 2, 4);
      break;
    case DHCP_OPTION_MSG_TYPE:
      type = *(optptr + 2);
      break;
    case DHCP_OPTION_SERVER_ID:
      memcpy(s.serverid, optptr + 2, 4);
      break;
    case DHCP_OPTION_LEASE_TIME:
      memcpy(s.lease_time, optptr + 2, 4);
      break;
    case DHCP_OPTION_END:
      return type;
    }

    optptr += optptr[1] + 2;
  }
  return type;
}
/*---------------------------------------------------------------------------*/
static uint8_t
parse_msg(int len)
{
  struct dhcp_msg *m = (struct dhcp_msg *)&dhcp_msg_buf;
  
  if(m->op == DHCP_REPLY &&
     memcmp(m->xid, &xid, sizeof(xid)) == 0 &&
     memcmp(m->chaddr, s.mac_addr, s.mac_len) == 0) {
    memcpy(s.ipaddr.u16, m->yiaddr, 4);
    return parse_options(&m->options[4], len);
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/*
 * Is this a "fresh" reply for me? If it is, return the type.
 */
static int
msg_for_me(int len)
{
  struct dhcp_msg *m = (struct dhcp_msg *)&dhcp_msg_buf;
  uint8_t *optptr = &m->options[4];
  uint8_t *end = (uint8_t*)&dhcp_msg_buf + len;

  if(m->op == DHCP_REPLY &&
     memcmp(m->xid, &xid, sizeof(xid)) == 0 &&
     memcmp(m->chaddr, s.mac_addr, s.mac_len) == 0) {
    while(optptr < end) {
      if(*optptr == DHCP_OPTION_MSG_TYPE) {
	return *(optptr + 2);
      } else if (*optptr == DHCP_OPTION_END) {
	return -1;
      }
      optptr += optptr[1] + 2;
    }
  }
  return -1;
}

static int readResponse(int type, int timeoutSecs)
{
  int   len;
  JIF_t end;
  int wait;

  wait = timeoutSecs * HZ;
  end = jiffies + wait;
  
  do {

    if ((len = netSockRead(s.conn, &dhcp_msg_buf, sizeof(dhcp_msg_buf), wait)) > 0) {

      if(msg_for_me(len) == type) {

        parse_msg(len);
        return len;
      }
    }

    wait = end - jiffies;
    
  } while (wait > 0);
 
  return -1;
}

/*---------------------------------------------------------------------------*/

static void dhcpTask(void* arg)
{
  int wait;

 init:
  xid++;
  s.state = STATE_SENDING;
  wait = 1;

  while (1) {

    send_discover();

    if (readResponse(DHCPOFFER, wait) != -1) {

      s.state = STATE_OFFER_RECEIVED;
      goto selecting;
    }

    if(wait < 60)
      wait = wait * 2;
  }
  
 selecting:
  xid++;
  wait = 1;

  do {

    send_request();

    if (readResponse(DHCPACK, wait) != -1) {

      s.state = STATE_CONFIG_RECEIVED;
      goto bound;
    }

    if(wait < 10) {

      wait++;
    }
    else {

      goto init;
    }

  } while(s.state != STATE_CONFIG_RECEIVED);
  
 bound:
#if 0
  nosPrintf("Got IP address %d.%d.%d.%d\n", uip_ipaddr_to_quad(&s.ipaddr));
  nosPrintf("Got netmask %d.%d.%d.%d\n",	 uip_ipaddr_to_quad(&s.netmask));
  nosPrintf("Got DNS server %d.%d.%d.%d\n", uip_ipaddr_to_quad(&s.dnsaddr));
  nosPrintf("Got default router %d.%d.%d.%d\n",
	 uip_ipaddr_to_quad(&s.default_router));
  nosPrintf("Lease expires in %ld seconds\n",
	 uip_ntohs(s.lease_time[0])*65536ul + uip_ntohs(s.lease_time[1]));
#endif

  dhcpc_configured(&s);

  uint32_t leaseLeft = uip_ntohs(s.lease_time[0])*65536ul + uip_ntohs(s.lease_time[1]);
  uint32_t sleepLeft = leaseLeft / 2;

#define MAX_SECS ((INFINITE - 1) / HZ)

  while (sleepLeft > 0) {

    if (sleepLeft > MAX_SECS) {

      wait = MAX_SECS * HZ;
      sleepLeft -= MAX_SECS;
    }
    else {

      wait = sleepLeft * HZ;
      sleepLeft = 0;
    }

    posTaskSleep(wait);
  }

  leaseLeft = leaseLeft / 2;

  /* renewing: */
  xid++;
  do {

    send_request();

    if (leaseLeft / 2 > MAX_SECS)
      wait = MAX_SECS;
    else
      wait = (leaseLeft / 2);

    if (readResponse(DHCPACK, wait) != -1) {

      s.state = STATE_CONFIG_RECEIVED;
      goto bound;
    }

    leaseLeft -= wait;

  } while (leaseLeft > 3);

  /* rebinding: */

  /* lease_expired: */
  dhcpc_unconfigured(&s);
  goto init;
}

/*---------------------------------------------------------------------------*/
void
dhcpc_init(const void *mac_addr, int mac_len)
{
  uip_ipaddr_t ipaddr;

  s.mac_addr = mac_addr;
  s.mac_len  = mac_len;

  s.state = STATE_INITIAL;

  s.conn = netSockAlloc(NET_SOCK_UNDEF_UDP);
  if (s.conn != NULL) {

    netSockBind(s.conn, DHCPC_CLIENT_PORT);

    uip_ipaddr(&ipaddr, 255, 255, 255, 255);
    netSockConnect(s.conn, &ipaddr, DHCPC_SERVER_PORT);
  }
}
/*---------------------------------------------------------------------------*/
void
dhcpc_request(void)
{
  uip_ipaddr_t ipaddr;
  
  if(s.state == STATE_INITIAL) {
    uip_ipaddr(&ipaddr, 0,0,0,0);
    uip_sethostaddr(&ipaddr);
    s.task = posTaskCreate(dhcpTask, NULL, 1, 1000);
    POS_SETTASKNAME(s.task, "uip:dhcpc");

  }
}
/*---------------------------------------------------------------------------*/

#endif
