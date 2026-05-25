CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -std=c11 -D_GNU_SOURCE
TARGET = minishell

SRC = src/main.c src/parser.c src/builtin.c src/executor.c src/job.c src/signal.c
OBJ = $(SRC:src/%.c=build/%.o)

# Check if readline headers are available
HAS_READLINE := $(shell test -f /usr/include/readline/readline.h && echo yes || echo no)
ifeq ($(HAS_READLINE),yes)
  CFLAGS += -DUSE_READLINE
  LDFLAGS = -lreadline
  $(info Building with readline support)
else
  LDFLAGS =
  $(info Building without readline (basic input mode))
endif

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
