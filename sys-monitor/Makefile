CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -std=c11 -D_GNU_SOURCE -I./include -I./include/ncursesw
LDFLAGS = -L./lib -lncursesw -ltinfo -lm
TARGET = sysmon

SRC = src/main.c src/cpu.c src/memory.c src/process.c src/disk.c src/network.c src/ui.c
OBJ = $(SRC:src/%.c=build/%.o)

all: build $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
