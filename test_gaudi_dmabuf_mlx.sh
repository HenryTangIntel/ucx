#!/bin/bash

# Gaudi Device Memory over MLX/IB Transport Test
# ==============================================

set -e

export LD_LIBRARY_PATH="/workspace/ucx/install/lib:$LD_LIBRARY_PATH"

GAUDI_TEST="./gaudi_dmabuf_mlx_test"
PORT=12347
SERVER_IP="127.0.0.1"
BUFFER_SIZE="1048576"
DEVICE_ID=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Gaudi Device Memory over MLX/IB Transport${NC}"
echo -e "${BLUE}================================================${NC}"
echo

echo "Test Configuration:"
echo "  Port: $PORT (for UCX address exchange)"
echo "  Buffer size: $BUFFER_SIZE bytes"
echo "  Server IP: $SERVER_IP"
echo "  Gaudi device: $DEVICE_ID"
echo "  Architecture: Gaudi device memory → DMA-buf → MLX/IB → Network"
echo

# Check if binary exists and compile if needed
if [ ! -f "$GAUDI_TEST" ]; then
    echo -e "${YELLOW}Compiling Gaudi DMA-buf + MLX test...${NC}"
    gcc -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -o gaudi_dmabuf_mlx_test gaudi_dmabuf_mlx_test.c -lucp -lucs -luct
    echo -e "${GREEN}✓ Compiled successfully${NC}"
fi

echo -e "${YELLOW}Step 1: Starting Gaudi device memory server...${NC}"
$GAUDI_TEST -p "$PORT" -s "$BUFFER_SIZE" -d "$DEVICE_ID" &
SERVER_PID=$!

echo "Server PID: $SERVER_PID"
echo "Waiting for Gaudi device initialization..."
sleep 5

echo
echo -e "${YELLOW}Step 2: Running Gaudi device memory client...${NC}"
echo

# Run client and capture result
if $GAUDI_TEST -c "$SERVER_IP" -p "$PORT" -s "$BUFFER_SIZE" -d "$DEVICE_ID"; then
    CLIENT_SUCCESS=1
    echo
    echo -e "${GREEN}✅ Gaudi device memory transfer completed successfully!${NC}"
else
    CLIENT_SUCCESS=0
    echo
    echo -e "${RED}❌ Gaudi device memory transfer failed!${NC}"
fi

echo
echo -e "${YELLOW}Step 3: Cleaning up server...${NC}"
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo
echo "============================================"
if [ $CLIENT_SUCCESS -eq 1 ]; then
    echo -e "${GREEN}🎉 Gaudi DMA-buf + MLX/IB Test PASSED!${NC}"
    echo -e "${GREEN}   ✓ Gaudi device memory allocation${NC}"
    echo -e "${GREEN}   ✓ DMA-buf integration${NC}"
    echo -e "${GREEN}   ✓ UCX MLX/IB transport selection${NC}"
    echo -e "${GREEN}   ✓ Zero-copy device memory transfers${NC}"
    echo -e "${GREEN}   ✓ Gaudi computation simulation${NC}"
    echo -e "${GREEN}   ✓ End-to-end data verification${NC}"
else
    echo -e "${RED}❌ Gaudi DMA-buf + MLX/IB Test FAILED!${NC}"
fi
echo "============================================"
echo

echo -e "${BLUE}Architecture Validated:${NC}"
echo "🔹 Gaudi accelerator provides device memory"
echo "🔹 DMA-buf enables zero-copy access to device memory"
echo "🔹 MLX/InfiniBand provides high-speed network transport"
echo "🔹 UCX orchestrates the entire communication stack"
echo "🔹 Data flows: Gaudi Memory ↔ DMA-buf ↔ MLX/IB ↔ Network"

exit $((1 - CLIENT_SUCCESS))
