#!/bin/bash

# Automated UCX Gaudi Communication Test
# ======================================

set -e

export LD_LIBRARY_PATH="/workspace/ucx/install/lib:$LD_LIBRARY_PATH"

GAUDI_TEST="./gaudi_comm_test"
PORT=12345
SERVER_IP="127.0.0.1"
BUFFER_SIZE="1048576"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  UCX Gaudi Communication - Automated Test${NC}"
echo -e "${BLUE}================================================${NC}"
echo

echo "Test Configuration:"
echo "  Port: $PORT"
echo "  Buffer size: $BUFFER_SIZE bytes"
echo "  Server IP: $SERVER_IP"
echo

# Check if binary exists
if [ ! -f "$GAUDI_TEST" ]; then
    echo -e "${RED}Error: $GAUDI_TEST not found${NC}"
    echo "Compiling..."
    gcc -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -o gaudi_comm_test gaudi_comm_test.c -lucp -lucs -luct
    echo -e "${GREEN}✓ Compiled successfully${NC}"
fi

echo -e "${YELLOW}Step 1: Starting server in background...${NC}"
$GAUDI_TEST -p "$PORT" -s "$BUFFER_SIZE" &
SERVER_PID=$!

echo "Server PID: $SERVER_PID"
echo "Waiting for server to initialize..."
sleep 5

echo
echo -e "${YELLOW}Step 2: Running client to connect to server...${NC}"
echo

# Run client and capture result
if $GAUDI_TEST -c "$SERVER_IP" -p "$PORT" -s "$BUFFER_SIZE"; then
    CLIENT_SUCCESS=1
    echo
    echo -e "${GREEN}✅ Client completed successfully!${NC}"
else
    CLIENT_SUCCESS=0
    echo
    echo -e "${RED}❌ Client failed!${NC}"
fi

echo
echo -e "${YELLOW}Step 3: Cleaning up server...${NC}"
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo
echo "============================================"
if [ $CLIENT_SUCCESS -eq 1 ]; then
    echo -e "${GREEN}🎉 UCX Gaudi Communication Test PASSED!${NC}"
    echo -e "${GREEN}   ✓ Server initialization successful${NC}"
    echo -e "${GREEN}   ✓ Client connection successful${NC}"
    echo -e "${GREEN}   ✓ Data transfer and verification successful${NC}"
    echo -e "${GREEN}   ✓ Async/event progress simulation successful${NC}"
else
    echo -e "${RED}❌ UCX Gaudi Communication Test FAILED!${NC}"
fi
echo "============================================"
echo

exit $((1 - CLIENT_SUCCESS))
