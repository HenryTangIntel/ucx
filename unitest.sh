#!/bin/bash
# unitest.sh - Build and run UCX unittests, including Gaudi support
# Usage: ./unitest.sh [gtest_filter]
set -e

# Build UCX with Gaudi and GTest support
echo "[INFO] Running autogen.sh and configure..."
./autogen.sh
echo "[INFO] Configuring UCX with Gaudi and GTest support..."
./configure --with-gaudi --enable-gtest

echo "[INFO] Building UCX and test suite..."
make -j$(nproc)
make -C test/gtest -j$(nproc)

echo "[INFO] Running UCX GTest suite..."
cd test/gtest
if [ -n "$1" ]; then
    ./gtest --gtest_filter="$1"
else
    ./gtest
fi
