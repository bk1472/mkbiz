#	Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996 Santa Cruz Operation, Inc. All Rights Reserved.
#	Copyright (c) 1988, 1990 AT&T, Inc. All Rights Reserved.
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Santa Cruz Operation, Inc.
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)size:i386/makefile	1.7"
#	SIZE MAKEFILE
#
#


HFILES		= process.h defs.h libelf.h sys/elf.h sys/elftypes.h
SOURCES		= main.c process.c hexdump.c addr2line.c dwarf.c crc32.c
DISTFILES	= $(HFILES) $(SOURCES) Makefile
OBJECTS		= $(SOURCES:.c=.o)
PRODUCTS	= mkbiz
DEF_LST		= -DUNIX -DMKBIZ

CYG_ENV		= 1

ifeq (1, $(CYG_ENV))
INC_TOP		= /usr
USR_TOP		= /usr
LIB_OPT	    =
else
INC_TOP		= /GccMing
USR_TOP		= d:/cygwin/usr
LIB_OPT	    = libgetopt.a
endif

INC_DIR		= $(INC_TOP)/include
INC_LST		= -I$(INC_DIR) -I.
INS_CMD		= install -s
INS_DIR		= $(USR_TOP)/bin
CFLAGS		= -O2

CC			= gcc
CC_CMD		= $(CC) -c $(CFLAGS) $(DEF_LST) $(INC_LST)
STRIP		= strip


all: $(PRODUCTS)

$(PRODUCTS):	$(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LIB_OPT) -lz
	@$(STRIP) $@

main.o:	main.c $(HFILES)

process.o: process.c $(HFILES)

hexdump.o: hexdump.c $(HFILES)

fcns.o: fcns.c $(HFILES)

%.o:	%.c
	$(CC_CMD) -o $@ $<

dist: $(DISTFILES)
	@tar cvf $(PRODUCTS).tar $(DISTFILES)
	@touch dist

install: $(PRODUCTS)
	$(INS_CMD) -m 555 $(PRODUCTS).exe $(INS_DIR)

clean:
	rm -f $(OBJECTS)

clobber: clean
	rm -f $(PRODUCTS) $(PRODUCTS).exe
