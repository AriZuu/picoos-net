/*
 * Copyright (c) 2012-2013, Ari Suutari <ari@stonepile.fi>.
 * Copyright (c) 2003,      Adam Dunkels.
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
#include <picoos-net.h>
#include <string.h>
#include "sys/socket.h"

#if NETCFG_TELNETD == 1

#define ISO_nl       0x0a
#define ISO_cr       0x0d

#define STATE_NORMAL 0
#define STATE_IAC    1
#define STATE_WILL   2
#define STATE_WONT   3
#define STATE_DO     4
#define STATE_DONT   5
#define STATE_CLOSE  6
#define STATE_CR     7

#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254

void telnetInit(NetTelnet* state, int sock)
{
  struct timeval tv;

  state->outPtr = state->outBuf;
  state->inPtr  = state->inBuf;
  state->inLen  = 0;
  state->state = STATE_NORMAL;
  
  tv.tv_sec = 0;
  tv.tv_usec = 500 * 1000L;

  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  state->inLen = recv(sock, state->inBuf, sizeof(state->inBuf), 0);
  state->sock = sock;
}

void telnetWrite(NetTelnet* conn, char* data)
{
  int len = strlen(data);

  while (len) {

    if ((size_t)(conn->outPtr - conn->outBuf + 1) >= sizeof(conn->outBuf)) {

      send(conn->sock, conn->outBuf, conn->outPtr - conn->outBuf, 0);
      conn->outPtr = conn->outBuf;
    }

    if (*data == '\n') {

      *conn->outPtr++ = '\r';
      *conn->outPtr++ = '\n';
    }
    else if (*((unsigned char*)data) == TELNET_IAC) {

      *conn->outPtr++ = TELNET_IAC;
      *conn->outPtr++ = TELNET_IAC;
    }
    else
      *conn->outPtr++ = *data;

    ++data;
    --len;
  }
}

void telnetFlush(NetTelnet* conn)
{
  if (conn->outPtr != conn->outBuf) {

    send(conn->sock, conn->outBuf, conn->outPtr - conn->outBuf, 0);
    conn->outPtr = conn->outBuf;
  }
}

static void sendOpt(NetTelnet* conn, uint8_t option, uint8_t value)
{
  if ((size_t)(conn->outPtr - conn->outBuf + 2) >= sizeof(conn->outBuf)) {

     send(conn->sock, conn->outBuf, conn->outPtr - conn->outBuf, 0);
     conn->outPtr = conn->outBuf;
   }

   *conn->outPtr++ = TELNET_IAC;
   *conn->outPtr++ = option;
   *conn->outPtr++ = value;
}

int telnetReadLine(NetTelnet* conn, char* data, int max, int timeout)
{
  uint8_t c;
  int len = 0;
  bool gotLine = false;
  max = max - 1;
  struct timeval tv;

  do {
    
    if (conn->inLen == 0) {

      tv.tv_sec = 0;
      tv.tv_usec = timeout * 1000L;

      setsockopt(conn->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      conn->inLen = recv(conn->sock, conn->inBuf, sizeof(conn->inBuf), 0);
      conn->inPtr = conn->inBuf;
      if (conn->inLen <= 0)
        return conn->inLen;
    }

    c = *conn->inPtr;
    ++conn->inPtr;
    --conn->inLen;

    switch(conn->state) {
    case STATE_IAC:
      //nosPrintf("IAC state %d\n", (int)c);
      if(c == TELNET_IAC) {

        *data++ = c;
        ++len;
       
	conn->state = STATE_NORMAL;
      }
      else {

	switch(c) {
	case TELNET_WILL:
	  conn->state = STATE_WILL;
	  break;

	case TELNET_WONT:
	  conn->state = STATE_WONT;
	  break;

	case TELNET_DO:
	  conn->state = STATE_DO;
	  break;

	case TELNET_DONT:
	  conn->state = STATE_DONT;
	  break;

	default:
	  conn->state = STATE_NORMAL;
	  break;
	}
      }
      break;

    case STATE_WILL:
      /* Reply with a DONT */
      sendOpt(conn, TELNET_DONT, c);
      conn->state = STATE_NORMAL;
      break;
      
    case STATE_WONT:
      /* Reply with a DONT */
      sendOpt(conn, TELNET_DONT, c);
      conn->state = STATE_NORMAL;
      break;

    case STATE_DO:
      /* Reply with a WONT */
      sendOpt(conn, TELNET_WONT, c);
      conn->state = STATE_NORMAL;
      break;

    case STATE_DONT:
      /* Reply with a WONT */
      sendOpt(conn, TELNET_WONT, c);
      conn->state = STATE_NORMAL;
      break;

    case STATE_CR:
      conn->state = STATE_NORMAL;
      *data++ = '\n';
      ++len;
      gotLine = true;
      break;

    case STATE_NORMAL:
      if(c == TELNET_IAC) {

	conn->state = STATE_IAC;
      }
      else if (c == '\r') {

        conn->state = STATE_CR;
      }
      else {

        *data++ = c;
        ++len;
      }
      break;
    }

  } while(!gotLine);

  telnetFlush(conn);

  *data = '\0';
  return len;
}
#endif
