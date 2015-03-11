#######################################################################
# Directories
#######################################################################
TOP_DIR		 = .

include	$(TOP_DIR)/incs.mk

#######################################################################
# Target
#######################################################################
TGT			 = mkbiz

SRCS		 =
SRCS		+= main.c
SRCS		+= process.c
SRCS		+= hexdump.c
SRCS		+= addr2line.c
SRCS		+= dwarf.c
SRCS		+= crc32.c

OBJS		 = $(foreach src, $(SRCS), $(OBJ_DIR)/$(src:.c=.o))

all :  $(TGT)


include	$(TOP_DIR)/rules.mk
