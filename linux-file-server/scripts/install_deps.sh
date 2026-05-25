#!/bin/bash
# install_deps.sh - 安装 v2.0.0 所需依赖

set -e

echo "Installing dependencies for linux-file-server v2.0.0..."

sudo apt-get update
sudo apt-get install -y libssl-dev libsqlite3-dev zlib1g-dev

echo "Dependencies installed successfully."
