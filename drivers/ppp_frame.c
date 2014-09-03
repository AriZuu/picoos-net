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

/*
 * This is derived from ng_async.c in FreeBSD operating
 * system, which has the following copyright:
 */

/*-
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Archie Cobbs <archie@freebsd.org>
 *
 */

#include <string.h>
#include <stdint.h>

#include "ppp_defs.h"
#include "ppp_frame.h"

/* Async decode state */

#define MODE_HUNT	0
#define MODE_NORMAL	1
#define MODE_ESC	2
#define MODE_OUTPUT     3

/*
 * CRC table
 *
 * Taken from RFC 1171 Appendix B
 */
static const uint16_t fcstab[256] = {
	 0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	 0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	 0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	 0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	 0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	 0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	 0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	 0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	 0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	 0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	 0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	 0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	 0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	 0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	 0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	 0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	 0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

#define ESCAPE_MASK 0 //(~0)

void pppOutputBegin(PPPContext* ctx, uint16_t protocol)
{
  uint8_t* ptr = ctx->buf;

  memset(&ctx->stat, '\0', sizeof(ctx->stat));

  ctx->fcs = PPP_INITFCS;
  ctx->mode = MODE_OUTPUT;

  *ptr++ = PPP_FLAG;

#ifdef NO_COMPRESS

  *ptr = PPP_ALLSTATIONS;
  ctx->fcs = PPP_FCS(ctx->fcs, *ptr);
  ++ptr;

  *ptr = PPP_UI;
  ctx->fcs = PPP_FCS(ctx->fcs, *ptr);
  ++ptr;
#endif

  if (protocol > 0xff) {

    *ptr = (protocol >> 8) & 0xff;
    ctx->fcs = PPP_FCS(ctx->fcs, *ptr);
    ++ptr;
  }

  *ptr = protocol & 0xff;
  ctx->fcs = PPP_FCS(ctx->fcs, *ptr);
  ++ptr;

  ctx->ptr = ptr;
}

void pppOutputAppend(PPPContext* ctx, uint8_t ch)
{
  uint8_t* ptr = ctx->ptr;

  if (ctx->ptr - ctx->buf + 1 >= ctx->max)
    return;

  ctx->fcs = PPP_FCS(ctx->fcs, ch);
  if ((ch < 32 && ((1 << ch) & ESCAPE_MASK)) ||
      (ch == PPP_ESCAPE) || (ch == PPP_FLAG)) {
    
    *ptr = PPP_ESCAPE;
    ++ptr;
    ch ^= PPP_TRANS;
  }

  *ptr = ch;
  ptr++;

  ctx->ptr = ptr;
}

void pppOutputEnd(PPPContext* ctx)
{
  uint16_t fcs = ctx->fcs;

  pppOutputAppend(ctx, ~fcs & 0xff);
  pppOutputAppend(ctx, ~fcs >> 8);

  *(ctx->ptr) = PPP_FLAG;
  ctx->ptr++;
}

void pppInputBegin(PPPContext* ctx)
{
  memset(&ctx->stat, '\0', sizeof(ctx->stat));

  ctx->fcs = PPP_INITFCS;
  ctx->mode = MODE_HUNT;
  ctx->ptr = ctx->buf;
}

static void pppInputReset(PPPContext* ctx)
{
  ctx->fcs = PPP_INITFCS;
  ctx->mode = MODE_NORMAL;
  ctx->ptr = ctx->buf;
}

void pppInputAppend(PPPContext* ctx, uint8_t ch)
{
  int len;
  uint8_t* packet;

  if (ch == PPP_FLAG) {

    len = ctx->ptr - ctx->buf;
    if (len < 2) {

      // Too short or no packet at all, restart.
      if (len > 0) {

        ctx->stat.tooShort++;
      }

      pppInputReset(ctx);
      return;
    }

    if (ctx->fcs != PPP_GOODFCS) {
      
      ctx->stat.badCRC++;
      pppInputReset(ctx);
      return;
    }   

    ctx->ptr -= 2; // Drop CRC.
    len -= 2;
    packet = ctx->buf;

    // RFC1662: Check if address & control fields have been
    //          compressed (they are simply omitted).

    if (len >= 2 &&
        ctx->buf[0] == PPP_ALLSTATIONS &&
        ctx->buf[1] == PPP_UI) {

      packet += 2;
      len -= 2;
    }

    uint16_t protocol;

    if (len >= 1) {

      if (packet[0] & 1) {

        protocol = packet[0];
        packet++;
        len--;
      }
      else {

        if (len >= 2) {

          protocol = ((uint16_t)packet[0]) << 8;
          protocol |= packet[1];
          packet += 2;
          len -= 2;
        }
        else {

          ctx->stat.tooShort++;
          pppInputReset(ctx);
          return;
        }
      }
    }
          
    ctx->inputHook(protocol, packet, len);

    pppInputReset(ctx);
    return;
  }

  switch (ctx->mode) {
  case MODE_NORMAL:
    if (ch == PPP_ESCAPE) {

      ctx->mode = MODE_ESC;
      return;
    }

    break;

  case MODE_ESC:
    ch ^= PPP_TRANS;
    ctx->mode = MODE_NORMAL;
    break;

  case MODE_HUNT:
  default:
    return;
  }

  if (ctx->ptr - ctx->buf >= ctx->max)
    return;

  ctx->fcs = PPP_FCS(ctx->fcs, ch);
  *(ctx->ptr) = ch;
  ctx->ptr++;
}
