CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGETS = server32 client32

all: $(TARGETS)

server32: server32.c
	$(CC) $(CFLAGS) -o $@ $< -lrt

client32: client32.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
