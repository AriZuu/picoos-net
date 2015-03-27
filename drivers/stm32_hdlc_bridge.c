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
 * hdbcbridge peer for Pico]OS.
 */

#include <picoos.h>
#include <picoos-net.h>
#include <string.h>

#if NETCFG_DRIVER_HDLC_BRIDGE > 0

#include "stm32_hdlc_bridge.h"
#include "ppp_defs.h"
#include "ppp_frame.h"

static volatile int packetInLen;
static uint8_t packetIn[UIP_BUFSIZE];
static volatile uint8_t* packetInBegin;
static PPPContext outCtx;
static PPPContext inCtx;
static uint8_t packetOut[2*UIP_BUFSIZE];

static void packetInReady(int proto, uint8_t* data, int len);

void hdlcInit()
{
  USART_InitTypeDef init;

  // USART3 is connected to HLK-RM04 /dev/ttyS0

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

  init.USART_BaudRate = 115200;
  init.USART_WordLength = USART_WordLength_8b;
  init.USART_StopBits = USART_StopBits_1;
  init.USART_Parity = USART_Parity_No;
  init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

  USART_Init(USART3, &init);
  USART_Cmd(USART3, ENABLE);

  // Initialize frame encoder/decoder.

  packetInLen = 0;
  inCtx.buf = packetIn;
  inCtx.max = sizeof(packetIn);
  inCtx.inputHook = packetInReady;
  pppInputBegin(&inCtx);

  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
  NVIC_SetPriority(USART3_IRQn, PORT_PENDSV_PRI - 2);
  NVIC_EnableIRQ(USART3_IRQn);
}

int hdlcPoll()
{
  int len;

  NVIC_DisableIRQ(USART3_IRQn);
  len = packetInLen;

  if (len > 0) {

    if (len > UIP_BUFSIZE)
      len = UIP_BUFSIZE;
      
    memcpy(uip_buf, (void*)packetInBegin, len);
    packetInLen = 0;
  }

  NVIC_EnableIRQ(USART3_IRQn);
  return len;
}

void hdlcSend()
{
  int i;

  outCtx.buf = packetOut;
  outCtx.max = sizeof(packetOut);
  pppOutputBegin(&outCtx, PPP_ETHERNET);
  for (i = 0; i < uip_len; i++)
    pppOutputAppend(&outCtx, uip_buf[i]);

  pppOutputEnd(&outCtx);

  int len = outCtx.ptr - outCtx.buf;

  for (i = 0; i < len; i++) {

    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, outCtx.buf[i]);
  }
}

void packetInReady(int proto, uint8_t* data, int len)
{
  if (proto == PPP_ETHERNET) {

    packetInBegin = data;
    packetInLen = len;
    netInterrupt();
  }
}

void USART3_IRQHandler()
{
  c_pos_intEnter();

  if (USART_GetITStatus(USART3, USART_IT_ORE_RX) == SET) {

    USART_ReceiveData(USART3);
  }

  if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {

    unsigned char ch = USART_ReceiveData(USART3);
    if (packetInLen == 0)
      pppInputAppend(&inCtx, ch);
  }

  c_pos_intExitQuick();
}

#endif

