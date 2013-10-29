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

/**
 * @file    netcfg.h
 * @brief   picoos-net library configuration file
 * @author  Ari Suutari
 */

/**
 * @ingroup config
 * @{
 */

/*
 * uIP settings (see Contiki/uIP documentation).
 */

#define UIP_CONF_MAX_CONNECTIONS 4
#define UIP_CONF_MAX_LISTENPORTS 2

// this gives default tcp mtu, 536 bytes
#define UIP_CONF_BUFFER_SIZE     590

#define UIP_CONF_UDP              1
#define UIP_CONF_UDP_CHECKSUMS    1
#define UIP_CONF_UDP_CONNS        1

#define UIP_CONF_STATISTICS       1
#define UIP_CONF_LOGGING          1
#define UIP_CONF_IPV6 		  1

/*
 * Socket layer settings.
 */
#define NETCFG_SOCKETS 1
#define NETCFG_TELNETD 1

/*
 * Driver
 */

#ifdef unix
#define NETCFG_DRIVER_TAP 2
#else
#define NETCFG_DRIVER_CS8900A 2
#endif

/** @} */
