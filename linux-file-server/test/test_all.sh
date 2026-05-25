#!/bin/bash
# test_all.sh - 集成测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

PORT=19876
CLIENT="$PROJECT_DIR/client"
SERVER="$PROJECT_DIR/server"
PASS=0
FAIL=0

cleanup() {
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    rm -f test_upload.txt test_download.txt
    rm -rf "$PROJECT_DIR/shared/testdir" 2>/dev/null || true
}
trap cleanup EXIT

run_test() {
    local name="$1"
    local result="$2"
    if [ "$result" = "0" ]; then
        echo "  PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Building ==="
make clean && make
echo ""

echo "=== Unit Tests ==="
gcc -I./include test/test_log.c src/log.c -o test_log -lpthread
./test_log
rm -f test_log

gcc -I./include test/test_thread_pool.c src/thread_pool.c src/log.c -o test_thread_pool -lpthread
./test_thread_pool
rm -f test_thread_pool
echo ""

echo "=== Integration Tests ==="

# 启动服务器
echo "test upload content" > test_upload.txt
$SERVER -p $PORT &
SERVER_PID=$!
sleep 1

# 测试 list
echo "--- Test: list ---"
OUTPUT=$(echo -e "list\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Server Files"
run_test "list command" $?

# 测试 upload
echo "--- Test: upload ---"
OUTPUT=$(echo -e "upload test_upload.txt\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Upload"
run_test "upload command" $?

# 测试 download
echo "--- Test: download ---"
OUTPUT=$(echo -e "download test_upload.txt\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Download"
run_test "download command" $?

# 测试 delete
echo "--- Test: delete ---"
OUTPUT=$(echo -e "delete test_upload.txt\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Delete"
run_test "delete command" $?

# 测试 download after delete (should fail)
echo "--- Test: download deleted file ---"
OUTPUT=$(echo -e "download test_upload.txt\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Error"
run_test "download deleted file returns error" $?

# 测试 mkdir
echo "--- Test: mkdir ---"
OUTPUT=$(echo -e "mkdir testdir\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Directory created"
run_test "mkdir command" $?

# 测试 pwd
echo "--- Test: pwd ---"
OUTPUT=$(echo -e "pwd\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Remote directory"
run_test "pwd command" $?

# 测试 rmdir
echo "--- Test: rmdir ---"
OUTPUT=$(echo -e "rmdir testdir\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Directory removed"
run_test "rmdir command" $?

# 测试 mkdir with path traversal (should fail)
echo "--- Test: mkdir path traversal ---"
OUTPUT=$(echo -e "mkdir ../evil\nquit" | timeout 5 $CLIENT -p $PORT -s 127.0.0.1 2>&1)
echo "$OUTPUT" | grep -q "Invalid directory name"
run_test "mkdir path traversal rejected" $?

# 停止服务器
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
else
    echo "ALL TESTS PASSED"
    exit 0
fi
