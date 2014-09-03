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

typedef void (*PPPInputHook)(int proto, uint8_t* pkt, int len);

struct pppContext {

  uint8_t* buf;       // Start of buffer
  uint8_t* ptr;       // Buffer pointer
  uint16_t fcs;       // Current CRC
  uint8_t  mode;      // Input state machine mode
  int max;
  PPPInputHook inputHook;

  struct {

    int      badCRC;
    int      tooShort;
    int      ok;
  } stat;
};

#define PPP_ETHERNET	0x81
#define PPP_DEBUG	0x83

typedef struct pppContext PPPContext;

void pppOutputBegin(PPPContext* state, uint16_t protocol);
void pppOutputAppend(PPPContext* state, uint8_t ch);
void pppOutputEnd(PPPContext* state);
void pppInputBegin(PPPContext* state);
void pppInputAppend(PPPContext* state, uint8_t ch);
