#!/bin/bash

# UCX Gaudi Transport Communication Test
# ======================================

set -e

export LD_LIBRARY_PATH="/workspace/ucx/install/lib:$LD_LIBRARY_PATH"

GAUDI_TEST="./gaudi_ucx_transport_test"
PORT=12346
SERVER_IP="127.0.0.1"
BUFFER_SIZE="1048576"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  UCX Gaudi Transport Communication Test${NC}"
echo -e "${BLUE}================================================${NC}"
echo

echo "Test Configuration:"
echo "  Port: $PORT (for address exchange)"
echo "  Buffer size: $BUFFER_SIZE bytes"
echo "  Server IP: $SERVER_IP"
echo "  Transport: UCX (Gaudi preferred, TCP fallback)"
echo

# Check if binary exists and compile if needed
if [ ! -f "$GAUDI_TEST" ]; then
    echo -e "${YELLOW}Compiling UCX transport test...${NC}"
    gcc -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -o gaudi_ucx_transport_test gaudi_ucx_transport_test.c -lucp -lucs -luct
    echo -e "${GREEN}✓ Compiled successfully${NC}"
fi

echo -e "${YELLOW}Step 1: Starting UCX server in background...${NC}"
$GAUDI_TEST -p "$PORT" -s "$BUFFER_SIZE" &
SERVER_PID=$!

echo "Server PID: $SERVER_PID"
echo "Waiting for server to initialize..."
sleep 5

echo
echo -e "${YELLOW}Step 2: Running UCX client...${NC}"
echo

# Run client and capture result
if $GAUDI_TEST -c "$SERVER_IP" -p "$PORT" -s "$BUFFER_SIZE"; then
    CLIENT_SUCCESS=1
    echo
    echo -e "${GREEN}✅ UCX client completed successfully!${NC}"
else
    CLIENT_SUCCESS=0
    echo
    echo -e "${RED}❌ UCX client failed!${NC}"
fi

echo
echo -e "${YELLOW}Step 3: Cleaning up server...${NC}"
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo
echo "============================================"
if [ $CLIENT_SUCCESS -eq 1 ]; then
    echo -e "${GREEN}🎉 UCX Gaudi Transport Test PASSED!${NC}"
    echo -e "${GREEN}   ✓ UCX context initialization${NC}"
    echo -e "${GREEN}   ✓ Worker address exchange${NC}"
    echo -e "${GREEN}   ✓ UCX endpoint creation${NC}"
    echo -e "${GREEN}   ✓ UCX tag-based communication${NC}"
    echo -e "${GREEN}   ✓ Gaudi transport selection (when available)${NC}"
    echo -e "${GREEN}   ✓ Data integrity verification${NC}"
else
    echo -e "${RED}❌ UCX Gaudi Transport Test FAILED!${NC}"
fi
echo "============================================"
echo

echo -e "${BLUE}Transport Notes:${NC}"
echo "• This test uses actual UCX transports for data transfer"
echo "• TCP sockets are only used for initial worker address exchange"
echo "• UCX automatically selects the best transport (Gaudi if available)"
echo "• The test validates proper async/event integration"

exit $((1 - CLIENT_SUCCESS))
