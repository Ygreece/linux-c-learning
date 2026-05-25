CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -std=c11 -D_GNU_SOURCE -pthread
TARGET = ipc_bench

SRC = src/main.c src/bench_pipe.c src/bench_fifo.c src/bench_mqueue.c \
      src/bench_shm.c src/bench_unix.c src/bench_tcp.c src/utils.c
OBJ = $(SRC:src/%.c=build/%.o)

all: build $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lrt

build/%.o: src/%.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
