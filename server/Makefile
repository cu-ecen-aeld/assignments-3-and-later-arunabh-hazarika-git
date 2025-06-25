# Read from environment
CROSS_COMPILE ?= aarch64-none-linux-gnu-
CFLAGS ?= -Wall -Werror
CC ?= $(CROSS_COMPILE)gcc
TARGET ?= aesdsocket 
LDFLAGS ?= -lpthread -lrt

default: all

aesdsocket: all

all: clean compile

compile: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket.o aesdsocket *~
