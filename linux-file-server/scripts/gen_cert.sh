#!/bin/bash
# gen_cert.sh - 生成自签名证书（开发环境）
#
# 用法: ./scripts/gen_cert.sh
# 生成: config/server.crt (证书) + config/server.key (私钥)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CERT_DIR="$(dirname "$SCRIPT_DIR")/config"

echo "Generating self-signed certificate..."
echo "  Certificate: $CERT_DIR/server.crt"
echo "  Private key: $CERT_DIR/server.key"

openssl req -x509 -newkey rsa:2048 -keyout "$CERT_DIR/server.key" \
    -out "$CERT_DIR/server.crt" -days 365 -nodes \
    -subj "/CN=localhost/O=LinuxFileServer"

echo ""
echo "Certificate generated successfully."
echo "Valid for 365 days. For production use, replace with CA-signed certificates."
