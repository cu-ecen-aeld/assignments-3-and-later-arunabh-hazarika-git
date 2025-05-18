#CROSS_COMPILE=aarch64-none-linux-gnu-
CFLAGS=-Wall -Werror -pedantic -std=gnu99
CC=$(CROSS_COMPILE)gcc

default: all

all: clean aesdsocket

writer: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean:
	rm -f aesdsocket.o aesdsocket *~
