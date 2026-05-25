# Makefile for web-server

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpthread

# Debug build
DEBUG_CFLAGS = -Wall -Wextra -g -O0 -I./include

SRC_DIR = src
INC_DIR = include
TEST_DIR = test
BUILD_DIR = build
WWW_DIR = www

# Source files
SRC = $(SRC_DIR)/http.c
OBJ = $(BUILD_DIR)/http.o

# Main program
MAIN_SRC = $(SRC_DIR)/main.c
MAIN_BIN = $(BUILD_DIR)/web-server

# Test files
TEST_SRC = $(TEST_DIR)/test_http.c
TEST_BIN = $(BUILD_DIR)/test_http

.PHONY: all clean test debug run

all: $(MAIN_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(WWW_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(MAIN_BIN): $(MAIN_SRC) $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MAIN_SRC) $(SRC) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRC) -o $@ $(LDFLAGS)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(MAIN_BIN)

run: $(MAIN_BIN)
	./$(MAIN_BIN) -v

clean:
	rm -rf $(BUILD_DIR)

# Create sample www directory
www:
	mkdir -p $(WWW_DIR)
	echo "<html><body><h1>Hello World</h1></body></html>" > $(WWW_DIR)/index.html
	echo "<html><body><h1>About Page</h1></body></html>" > $(WWW_DIR)/about.html
