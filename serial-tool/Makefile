CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -std=c11 -D_GNU_SOURCE
TARGET = serialtool

SRC = src/main.c src/serial.c src/terminal.c src/hexdump.c src/script.c
OBJ = $(SRC:src/%.c=build/%.o)

all: build $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

build/%.o: src/%.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
