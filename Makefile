# ---------------------------------------------------------------------------- #
# Sclocka - Screen saver/lock for terminals                                    #
# ---------------------------------------------------------------------------- #

# Based on ASCII Saver - https://gitlab.com/mezantrop/ascsaver

# Copyright (c) 2023, Mikhail Zakharov <zmey20000@yahoo.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#	 this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# ---------------------------------------------------------------------------- #
CC		= cc -O3
CFLAGS += -Wall
NAME	= tvi_screensaver
LIBS	= -lutil
OBJS	= $(NAME).o
PREFIX ?= /usr/local

.PHONY: all with_pam without_pam FreeBSD NetBSD Linux OpenBSD install \
deinstall uninstall clean

all:	with_pam

with_pam:
	$(CC) $(CFLAGS) -DPIZZI -DWITH_PAM=1 -o $(NAME) $(NAME).c $(LIBS) -lpam

FreeBSD:	without_pam
NetBSD:		without_pam
Linux:		without_pam

without_pam:
	$(CC) $(CFLAGS) -o $(NAME) $(NAME).c $(LIBS)

OpenBSD:	without_pam

install:	all
	install -d $(PREFIX)/bin
	install -m 755 -s $(NAME) $(PREFIX)/bin/

deinstall:
	rm $(PREFIX)/bin/$(NAME)

uninstall: deinstall

clean:
	rm -rf $(NAME) *.o *.dSYM *.core
