#CROSS_COMPILE=aarch64-none-linux-gnu-
#CFLAGS=-Wall -Werror -pedantic -std=gnu99
#CC=$(CROSS_COMPILE)gcc

all: clean writer

writer: writer.c
	$(CC) $(CFLAGS) -o writer writer.c

clean:
	rm -f writer.o writer *~
