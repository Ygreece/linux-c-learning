# Makefile for log-system

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpthread

# Debug build
DEBUG_CFLAGS = -Wall -Wextra -g -O0 -I./include

SRC_DIR = src
INC_DIR = include
TEST_DIR = test
BUILD_DIR = build

# Source files
SRC = $(SRC_DIR)/log.c
OBJ = $(BUILD_DIR)/log.o

# Test files
TEST_SRC = $(TEST_DIR)/test_log.c
TEST_BIN = $(BUILD_DIR)/test_log

.PHONY: all clean test debug

all: $(OBJ)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRC) -o $@ $(LDFLAGS)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(OBJ)
	$(CC) $(DEBUG_CFLAGS) $(TEST_SRC) $(SRC) -o $(BUILD_DIR)/test_log_debug $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

# Install (copy to /usr/local)
install: $(OBJ)
	cp $(INC_DIR)/log.h /usr/local/include/
	cp $(OBJ) /usr/local/lib/
