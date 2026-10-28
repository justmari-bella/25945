CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGETS = server30 client30

all: $(TARGETS)

server30: server30.c
	$(CC) $(CFLAGS) -o $@ $<

client30: client30.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
