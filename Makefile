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
AUTO_COPY_CONTIKI_SRC ?= N

include $(RELROOT)make/common.mak

TARGET = picoos-net

CONTIKI_DIR = ../contiki-uip

SRC_TXT_CONTIKI =	net/uip.c	\
			net/uip-split.c \
			net/uip_arp.c   \
			net/uip-debug.c \
			net/tcpip.c     \
			net/uip6.c      \
			net/uip-icmp6.c \
			net/uip-ds6.c   \
			net/uip-ds6-route.c \
			net/uip-ds6-nbr.c   \
			net/nbr-table.c     \
			net/rime/rimeaddr.c \
			net/uip-nd6.c       \
			net/uiplib.c    \
			lib/random.c	\
		        lib/list.c	\
		        lib/memb.c	\

SRC_HDR_CONTIKI     =   net/nbr-table.h		\
			net/rime/rimeaddr.h	\
			net/tcpip.h		\
			net/uip-debug.h		\
			net/uip-ds6-nbr.h	\
			net/uip-ds6-route.h	\
			net/uip-ds6.h		\
			net/uip-fw.h		\
			net/uip-icmp6.h		\
			net/uip-nd6.h		\
			net/uip-split.h		\
			net/uip.h		\
			net/uip_arch.h		\
			net/uip_arp.h		\
			net/uiplib.h		\
			net/uipopt.h		\
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

SRC_HDR = $(SRC_HDR_CONTIKI) in.h in6.h picoos-net.h
SRC_OBJ =
CDEFINES += $(BSP_DEFINES) _XOPEN_SOURCE=700

MODULES = ../picoos-micro

include drivers/Makefile

ifeq '$(strip $(DIR_OUTPUT))' ''
DIR_OUTPUT = $(CURRENTDIR)/bin
endif

ifeq '$(AUTO_COPY_CONTIKI_SRC)' 'Y'

$(SRC_TXT_CONTIKI): %.c:	$(CONTIKI_DIR)/core/%.c
	cp $< $@

$(SRC_HDR_CONTIKI): %.h:	$(CONTIKI_DIR)/core/%.h
	cp $< $@

endif

include $(MAKE_LIB)

changelog:
	git log --date=short --format="%ad %ae %s" --date-order --follow .

dox: doxygen.cfg
	mkdir -p doc
	doxygen doxygen.cfg
