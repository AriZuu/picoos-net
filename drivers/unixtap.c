/*
 * Copyright (c) 2012-2013, Ari Suutari <ari@stonepile.fi>.
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


/* 
 * Simple TAP driver for FreeBSD & Pico]OS
 */

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE // This driver needs BSD stuff.
#endif

#include <picoos.h>
#include <picoos-net.h>

#if NETCFG_DRIVER_TAP > 0

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <memory.h>
#include <signal.h>
#include <errno.h>

#include "unixtap.h"

static void ioReadyContext(void);
static void ioReady(int sig, siginfo_t *info, void *ucontext);

static int tap;
static ucontext_t sigContext;

#if PORTCFG_IRQ_STACK_SIZE >= PORTCFG_MIN_STACK_SIZE
static char sigStack[PORTCFG_IRQ_STACK_SIZE];
#else
static char sigStack[PORTCFG_MIN_STACK_SIZE];
#endif

static void ioReadyContext()
{
  c_pos_intEnter();
  netInterrupt();
  c_pos_intExit();
  setcontext(&posCurrentTask_g->ucontext);
  assert(0);
}

static void ioReady(int sig, siginfo_t *info, void *ucontext)
{
  getcontext(&sigContext);
  sigContext.uc_stack.ss_sp = sigStack;
  sigContext.uc_stack.ss_size = sizeof(sigStack);
  sigContext.uc_stack.ss_flags = 0;
  sigContext.uc_link = 0;
  sigfillset(&sigContext.uc_sigmask);

  makecontext(&sigContext, ioReadyContext, 0);
  swapcontext(&posCurrentTask_g->ucontext, &sigContext);
}

void tapInit()
{
  struct sigaction sig;
  int flags;
#if !NETSTACK_CONF_WITH_IPV6
  uip_ipaddr_t ip;
#endif
  char ifconfig[80];

  tap = open("/dev/tap0", O_RDWR);
  P_ASSERT("tap0", tap != -1);

#if NETSTACK_CONF_WITH_IPV6
  sprintf (ifconfig, "ifconfig tap0 inet6 -ifdisabled up");
#else
  uip_getdraddr(&ip);
  sprintf (ifconfig,
           "ifconfig tap0 %d.%d.%d.%d ",
           ip.u8[0],
           ip.u8[1],
           ip.u8[2],
           ip.u8[3]);
  uip_getnetmask(&ip);
  sprintf (ifconfig + strlen(ifconfig),
           "netmask %d.%d.%d.%d up",
           ip.u8[0],
           ip.u8[1],
           ip.u8[2],
           ip.u8[3]);
#endif

  system(ifconfig);
  memset(&sig, '\0', sizeof(sig));
  
  sig.sa_sigaction = ioReady;
  sig.sa_flags     = SA_RESTART | SA_SIGINFO;
  sigaction(SIGIO, &sig, NULL);

  fcntl(tap, F_SETOWN, getpid());
  flags = fcntl(tap, F_GETFL, 0);
  fcntl(tap, F_SETFL, flags | O_ASYNC | O_NONBLOCK);
}

int tapPoll()
{
  int i;

  i = read(tap, uip_buf, UIP_BUFSIZE);
  if (i == -1 && errno == EAGAIN)
    return 0;

  P_ASSERT("tap read", i != -1);

  return i;
}

void tapSend()
{
  int i;

  i = write(tap, uip_buf, uip_len);
  P_ASSERT("tap send", i == uip_len);
}

#endif
