#
# Copyright (c) 2012-2013, Ari Suutari <ari@stonepile.fi>.
# All rights reserved. 
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote
#     products derived from this software without specific prior written
#     permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Compile uIP & drivers using Pico]OS library Makefile
#

RELROOT = ../picoos/
PORT ?= lpc2xxx
BUILD ?= RELEASE
AUTO_COPY_CONTIKI_SRC ?= Y

include $(RELROOT)make/common.mak

TARGET = picoos-net

CONTIKI_DIR = ../contiki-uip

SRC_TXT_CONTIKI4 =	net/ipv4/uip.c	\
			net/ipv4/uip_arp.c  \
			net/ipv4/uip-neighbor.c  

SRC_TXT_CONTIKI6 =	net/ipv6/uip6.c      \
			net/ipv6/uip-icmp6.c \
			net/ipv6/uip-ds6.c   \
			net/ipv6/uip-ds6-route.c \
			net/ipv6/uip-nd6.c       \
			net/ipv6/uip-ds6-nbr.c  \
			net/nbr-table.c     \
			net/linkaddr.c

SRC_TXT_CONTIKI =	net/ip/uip-debug.c \
			net/ip/uip-split.c \
			net/ip/tcpip.c     \
			net/ip/uiplib.c    \
			net/ip/uip-nameserver.c    \
			lib/random.c	\
		        lib/list.c	\
		        lib/memb.c	\

SRC_HDR_CONTIKI     =   net/ip/uip-debug.h	\
			net/ip/uip-split.h	\
			net/ip/tcpip.h		\
			net/ip/uiplib.h		\
			net/ip/uip-nameserver.h		\
			net/ip/uip_arch.h	\
			net/ip/uipopt.h		\
			net/ip/uip.h		\
			net/ipv4/uip-fw.h	\
			net/ipv4/uip_arp.h	\
			net/ipv4/uip-neighbor.h	\
			net/ipv6/uip-ds6-nbr.h	\
			net/ipv6/uip-ds6-route.h	\
			net/ipv6/uip-ds6.h	\
			net/ipv6/uip-icmp6.h	\
			net/ipv6/uip-nd6.h	\
			net/nbr-table.h		\
			net/linkaddr.h		\
		 	lib/list.h		\
			lib/memb.h		\
			lib/random.h

SRC_TXT =		sock.c \
			bsdsock.c \
			telnetd.c \
			tcpip-glue.c \
			ethernet.c \
			sys/stimer.c	\
			sys/timer.c	\
			sys/etimer.c	\
			sys/ctimer.c	\
			sys/clock.c	\
			net/dhcpc.c	\
			$(SRC_TXT_CONTIKI)

ifeq '$(strip $(NETCFG_STACK))' '6'
CDEFINES += NETSTACK_CONF_WITH_IPV4=0
CDEFINES += NETSTACK_CONF_WITH_IPV6=1 UIP_CONF_IPV6=1
SRC_TXT += $(SRC_TXT_CONTIKI6)
else
ifeq '$(strip $(NETCFG_STACK))' '4'
CDEFINES += NETSTACK_CONF_WITH_IPV4=1
CDEFINES += NETSTACK_CONF_WITH_IPV6=0 UIP_CONF_IPV6=0
SRC_TXT += $(SRC_TXT_CONTIKI4)
else
$(error Network stack must be selected by setting NETCFG_STACK to 4 or 6)
endif
endif

SRC_HDR = $(SRC_HDR_CONTIKI) in.h in6.h picoos-net.h
SRC_OBJ =
CDEFINES += $(BSP_DEFINES)  _XOPEN_SOURCE=700

MODULES  += ../picoos-micro

include drivers/Makefile

ifeq '$(strip $(DIR_OUTPUT))' ''
DIR_OUTPUT = $(CURRENTDIR)/bin
endif

ifeq '$(AUTO_COPY_CONTIKI_SRC)' 'Y'

$(SRC_TXT_CONTIKI): %.c:	$(CONTIKI_DIR)/core/%.c
	cp $< $@

$(SRC_HDR_CONTIKI): %.h:	$(CONTIKI_DIR)/core/%.h
	cp $< $@

$(SRC_TXT_CONTIKI4): %.c:	$(CONTIKI_DIR)/core/%.c
	cp $< $@

$(SRC_TXT_CONTIKI6): %.c:	$(CONTIKI_DIR)/core/%.c
	cp $< $@

endif

include $(MAKE_LIB)

changelog:
	git log --date=short --format="%ad %ae %s" --date-order --follow .

copyall: $(SRC_TXT_CONTIKI) $(SRC_HDR_CONTIKI) $(SRC_TXT_CONTIKI4) $(SRC_TXT_CONTIKI6)

dox: doxygen.cfg
	mkdir -p doc
	doxygen doxygen.cfg
