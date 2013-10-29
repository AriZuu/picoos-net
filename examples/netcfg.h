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

/** @defgroup uip-config uIP configuration settings.
 * @ingroup config
 * uIP configuration settings that are related to picoos-net.
 * For complete documentation, see related Contiki OS documentation.
 * @{
 */

/**
 * Max number of connections. If using socket layer (::NETCFG_SOCKETS == 1)
 * Each socket consumes one Pico]OS mutex and two flags. In addition to that, 
 * network main loop uses one semaphore and one mutex. If uIP listen is enabled to 
 * accept incoming connections, a task is required for each connection. 
 * So if number of connections is for example 4, 5 * 3 + 2 Pico]OS events are needed.
 * If using ::NOSCFG_FEATURE_CONOUT add 2 to that, which gives us 19 event objects.
 *
 * Number of tasks needed for tcp sockets is 4, but network system itself uses one
 * tasks and we must also have main and idle tasks. This results in 7 tasks.
 * Values values can be used in poscfg.h for ::POSCFG_MAX_TASKS and ::POSCFG_MAX_EVENTS.
 */
#define UIP_CONF_MAX_CONNECTIONS 4

/**
 * Maximum number of ports being listened for
 * incoming connections.
 */
#define UIP_CONF_MAX_LISTENPORTS 2

/**
 * Size of UIP packet buffer. Using 590 gives
 * TCP MTU of 536 bytes, which is the minimum
 * allowed.
 */
#define UIP_CONF_BUFFER_SIZE     590

/** 
 * Set to 1 if UDP connections should be included.
 */
#define UIP_CONF_UDP              1

/** 
 * Set to 1 to enable UDP checksumming.
 */
#define UIP_CONF_UDP_CHECKSUMS    1

/**
 * Number of UDP connections.
 */
#define UIP_CONF_UDP_CONNS        1

/**
 * Set to 1 to include uIP runtime statistics support.
 */
#define UIP_CONF_STATISTICS       1

/**
 * Set to 1 to include uIP error logging.
 */

#define UIP_CONF_LOGGING          1

/** 
 * IPv4/IPv6 configuration:
 * - 0: Use IPv4
 * - 1: Use IPv6
 *
 * There is no support for dual-stack configuration (ie. both IPv4 & IPv6 enabled).
 */
#define UIP_CONF_IPV6 		  1

/** @} */

/** @defgroup net-config picoos-net settings.
 * @ingroup config
 * Configuration settings that are not related to uIP.
 * @{
 */

/**
 * Set to 1 to include socket layer.
 */
#define NETCFG_SOCKETS 1

/**
 * Set to 1 to include telnet protocol.
 */
#define NETCFG_TELNETD 1

/**
 * Unix TAP driver configuration.
 * - 0: Don't compile driver
 * - 1: Compile driver
 * - 2: Compile driver and use it as default for socket layer api.
 */
#define NETCFG_DRIVER_TAP 2

/**
 * CS8900A driver configuration. Currently driver supports
 * Olimex LPC-E2129 board.
 * - 0: Don't compile driver
 * - 1: Compile driver
 * - 2: Compile driver and use it as default for socket layer api.
 */
#define NETCFG_DRIVER_CS8900A 0

/**
 * ENC28J60 driver configuration. Currently driver supports
 * Olimex STM32 board + UEXT ENC28J60 module.
 * - 0: Don't compile driver
 * - 1: Compile driver
 * - 2: Compile driver and use it as default for socket layer api.
 */
#define NETCFG_DRIVER_ENC28J60 0

/** @} */
