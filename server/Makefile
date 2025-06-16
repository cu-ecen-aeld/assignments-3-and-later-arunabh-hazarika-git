# Read from environment
CROSS_COMPILE ?= aarch64-none-linux-gnu-
CFLAGS ?= -Wall -Werror
CC ?= $(CROSS_COMPILE)gcc
TARGET ?= aesdsocket 
LDFLAGS ?= -lpthread -lrt

default: all

all: clean aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c $(LDFLAGS)

clean:
	rm -f aesdsocket.o aesdsocket *~
