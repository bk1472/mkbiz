CC		 = gcc
RM		 = rm -f
TARGET	 = ../ccdv

OBJ_DIR	 = .

SRCS	 =
SRCS	+= ccdv.c
SRCS	+= sift-warn.c

OBJS	 = $(foreach src, $(SRCS), $(OBJ_DIR)/$(src:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(CC) -o $@ $(OBJS)

.c.o:
	@$(CC) -c -o $@ $^

clobber clean:
	@$(RM) *.o
	@$(RM) $(TARGET)
