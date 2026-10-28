CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = process_manager
SRC = process_manager.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
