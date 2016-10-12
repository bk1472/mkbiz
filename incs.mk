.PHONY : ccdv tags


ifeq ($(OSTYPE), cygwin)
EXE			 = .exe
else
EXE			 =
endif

UTIL_DIR	 = $(TOP_DIR)/utils
OBJ_DIR		 = $(TOP_DIR)/objs

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

INCS		 =
INCS		+= -I$(INC_TOP)/include
INCS		+= -I$(TOP_DIR)/sys
INCS		+= -I.

#######################################################################
# Build Options
#######################################################################
CFLAGS	 =
CFLAGS	+= -O2
#CFLAGS	+= -g
CFLAGS	+= $(INCS)
CFLAGS	+= -DUNIX
CFLAGS	+= -DMKBIZ
CFLAGS	+= -Wno-format
CFLAGS	+= -Wno-unused-result

LFLAGS	 =
LFLAGS	+= -lz

include	$(TOP_DIR)/tools.mk

all: $(CCDV)

tags:
	@$(ECHO) "[MAKE TAGS]"
	@ctags -R
