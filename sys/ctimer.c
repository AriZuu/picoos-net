/**
 * \addtogroup ctimer
 * @{
 */

/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * Copyright (c) 2013-2014, Ari Suutari <ari@stonepile.fi>.
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
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Callback timer implementation
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include <picoos.h>
#include <picoos-net.h>
#include "sys/ctimer.h"

/*---------------------------------------------------------------------------*/
void
ctimer_init(void)
{
  etimer_init();
}
/*---------------------------------------------------------------------------*/
void
ctimer_set(struct ctimer *c, clock_time_t t,
	   void (*f)(void *), void *ptr)
{
  etimer_set_callback(&c->timer, t, f, ptr);
}
/*---------------------------------------------------------------------------*/
void
ctimer_reset(struct ctimer *c)
{
  etimer_reset(&c->timer);
}
/*---------------------------------------------------------------------------*/
void
ctimer_restart(struct ctimer *c)
{
  etimer_restart(&c->timer);
}
/*---------------------------------------------------------------------------*/
void
ctimer_stop(struct ctimer *c)
{
  etimer_stop(&c->timer);
}
/*---------------------------------------------------------------------------*/
int
ctimer_expired(struct ctimer *c)
{
  return etimer_expired(&c->timer);
}
/*---------------------------------------------------------------------------*/
/** @} */
