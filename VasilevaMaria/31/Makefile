CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGETS = server31 client31

all: $(TARGETS)

server31: server31.c
	$(CC) $(CFLAGS) -o $@ $<

client31: client31.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
