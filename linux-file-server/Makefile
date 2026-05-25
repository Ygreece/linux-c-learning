# Makefile - 多目录编译
#
# 学习要点:
# 1. 多目标编译 - server 和 client
# 2. 自动依赖生成 - gcc -MMD
# 3. 伪目标 - .PHONY
# 4. 变量使用 - CC, CFLAGS, LDFLAGS

# 编译器
CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -D_GNU_SOURCE -I./include -I/usr/include/node
SERVER_LDFLAGS = -L./lib -lpthread -lssl -lcrypto -lsqlite3 -lz -lrt -ldl
CLIENT_LDFLAGS = -L./lib -lpthread -lz -lssl -lcrypto

# 源文件目录
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# 源文件
SERVER_SRC = $(SRC_DIR)/main_server.c $(SRC_DIR)/server.c $(SRC_DIR)/epoll_wrap.c $(SRC_DIR)/protocol.c $(SRC_DIR)/thread_pool.c $(SRC_DIR)/log.c $(SRC_DIR)/compress.c $(SRC_DIR)/auth.c $(SRC_DIR)/metadata.c $(SRC_DIR)/file_watch.c $(SRC_DIR)/shm_ipc.c $(SRC_DIR)/process_pool.c $(SRC_DIR)/ssl_wrap.c $(SRC_DIR)/mempool.c $(SRC_DIR)/rate_limit.c $(SRC_DIR)/hash_util.c $(SRC_DIR)/http_server.c $(SRC_DIR)/module.c
CLIENT_SRC = $(SRC_DIR)/main_client.c $(SRC_DIR)/client.c $(SRC_DIR)/protocol.c $(SRC_DIR)/log.c $(SRC_DIR)/compress.c $(SRC_DIR)/ssl_wrap.c $(SRC_DIR)/hash_util.c

# 目标文件
SERVER_OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SERVER_SRC))
CLIENT_OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLIENT_SRC))

# 可执行文件
SERVER_BIN = server
CLIENT_BIN = client

# 默认目标
all: $(SERVER_BIN) $(CLIENT_BIN)

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译服务器
$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $@ $(SERVER_LDFLAGS)
	@echo "Build server: $@"

# 编译客户端
$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $@ $(CLIENT_LDFLAGS)
	@echo "Build client: $@"

# 编译规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 测试文件
TEST_SRC = $(wildcard test/test_*.c)
TEST_BIN = $(patsubst test/%.c,test/%,$(TEST_SRC))

# 测试二进制文件模式规则
test/%: test/%.c
	$(CC) $(CFLAGS) $< src/log.c src/protocol.c src/thread_pool.c src/epoll_wrap.c -o $@ $(SERVER_LDFLAGS)

# 压缩测试 - 仅需 compress.c 和 zlib
test/test_compress: test/test_compress.c src/compress.c
	$(CC) $(CFLAGS) $^ -o $@ -L./lib -lz

# 认证测试 - 需要 auth.c 和 log.c，链接 OpenSSL
test/test_auth: test/test_auth.c src/auth.c src/log.c
	$(CC) $(CFLAGS) $^ -o $@ -L./lib -lssl -lcrypto -lpthread

# 文件监控测试 - 仅需 file_watch.c
test/test_file_watch: test/test_file_watch.c src/file_watch.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

# Shared memory test - 仅需 shm_ipc.c，链接 pthread 和 rt
test/test_shm: test/test_shm.c src/shm_ipc.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread -lrt

# SSL test - 仅需 ssl_wrap.c，链接 OpenSSL
test/test_ssl: test/test_ssl.c src/ssl_wrap.c
	$(CC) $(CFLAGS) $^ -o $@ -L./lib -lssl -lcrypto

# 元数据测试 - 需要 metadata.c 和 log.c，链接 sqlite3
test/test_metadata: test/test_metadata.c src/metadata.c src/log.c
	$(CC) $(CFLAGS) $^ -o $@ -L./lib -lsqlite3 -lpthread

# Hash test - 仅需 hash_util.c，链接 OpenSSL
test/test_hash: test/test_hash.c src/hash_util.c
	$(CC) $(CFLAGS) $^ -o $@ -L./lib -lssl -lcrypto

# 内存池测试 - 仅需 mempool.c
test/test_mempool: test/test_mempool.c src/mempool.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

# 限速器测试 - 仅需 rate_limit.c
test/test_rate_limit: test/test_rate_limit.c src/rate_limit.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

# HTTP server test - 需要 http_server.c, log.c, shm_ipc.c，链接 pthread 和 rt
test/test_http: test/test_http.c src/http_server.c src/log.c src/shm_ipc.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread -lrt

# Module test - 需要 module.c，链接 dl
test/test_module: test/test_module.c src/module.c
	$(CC) $(CFLAGS) $^ -o $@ -ldl

# 运行所有测试
test: $(TEST_BIN)
	@echo "=== Running tests ==="
	@PASS=0; FAIL=0; \
	for t in $(TEST_BIN); do \
		echo "Running $$t..."; \
		if ./$$t; then PASS=$$((PASS+1)); else FAIL=$$((FAIL+1)); fi; \
	done; \
	echo ""; \
	echo "Results: $$PASS passed, $$FAIL failed"; \
	[ "$$FAIL" -eq 0 ]

# Build plugins
plugins:
	$(MAKE) -C plugins

# 清理
clean:
	rm -rf $(BUILD_DIR) $(SERVER_BIN) $(CLIENT_BIN) $(TEST_BIN)
	$(MAKE) -C plugins clean 2>/dev/null || true

# 安装
install: all
	mkdir -p /usr/local/bin
	cp $(SERVER_BIN) $(CLIENT_BIN) /usr/local/bin/

# 运行服务器
run-server: all
	./$(SERVER_BIN) -v

# 运行客户端
run-client: all
	./$(CLIENT_BIN)

# 调试
debug: CFLAGS += -DDEBUG -fsanitize=address
debug: SERVER_LDFLAGS += -fsanitize=address
debug: CLIENT_LDFLAGS += -fsanitize=address
debug: all

# 伪目标
.PHONY: all clean install run-server run-client debug test plugins
