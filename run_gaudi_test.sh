#!/bin/bash

# UCX Gaudi Communication Test Script
# ===================================

set -e

GAUDI_TEST="./gaudi_comm_test"
DEFAULT_PORT=12345
SERVER_IP="127.0.0.1"
BUFFER_SIZE="1048576"  # 1MB

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}  UCX Gaudi Client-Server Communication Test${NC}"
    echo -e "${BLUE}================================================${NC}"
    echo
}

print_usage() {
    echo "Usage: $0 [OPTIONS] MODE"
    echo
    echo "Modes:"
    echo "  server         Run server only"
    echo "  client         Run client only (requires running server)"
    echo "  both           Run server in background, then client"
    echo "  demo           Run a complete demo with both server and client"
    echo
    echo "Options:"
    echo "  -p PORT        Port number (default: $DEFAULT_PORT)"
    echo "  -s SIZE        Buffer size in bytes (default: $BUFFER_SIZE)"
    echo "  -i IP          Server IP address (default: $SERVER_IP)"
    echo "  -h             Show this help"
    echo
    echo "Examples:"
    echo "  $0 demo                    # Run complete demo"
    echo "  $0 server                  # Run server only"
    echo "  $0 client                  # Run client (needs server running)"
    echo "  $0 -p 9999 both           # Run on port 9999"
}

check_binary() {
    if [ ! -f "$GAUDI_TEST" ]; then
        echo -e "${RED}Error: $GAUDI_TEST not found${NC}"
        echo "Please compile first with:"
        echo "  gcc -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -o gaudi_comm_test gaudi_comm_test.c -lucp -lucs -luct"
        exit 1
    fi
    
    if [ ! -x "$GAUDI_TEST" ]; then
        chmod +x "$GAUDI_TEST"
    fi
    
    # Set library path for UCX
    export LD_LIBRARY_PATH="/workspace/ucx/install/lib:$LD_LIBRARY_PATH"
}

run_server() {
    echo -e "${GREEN}Starting server...${NC}"
    echo "Command: $GAUDI_TEST -p $PORT -s $BUFFER_SIZE"
    echo
    $GAUDI_TEST -p "$PORT" -s "$BUFFER_SIZE"
}

run_client() {
    echo -e "${GREEN}Starting client...${NC}"
    echo "Command: $GAUDI_TEST -c $SERVER_IP -p $PORT -s $BUFFER_SIZE"
    echo
    $GAUDI_TEST -c "$SERVER_IP" -p "$PORT" -s "$BUFFER_SIZE"
}

run_both() {
    echo -e "${GREEN}Starting server in background...${NC}"
    $GAUDI_TEST -p "$PORT" -s "$BUFFER_SIZE" &
    SERVER_PID=$!
    
    echo "Server PID: $SERVER_PID"
    sleep 2
    
    echo -e "${GREEN}Starting client...${NC}"
    $GAUDI_TEST -c "$SERVER_IP" -p "$PORT" -s "$BUFFER_SIZE"
    
    echo -e "${YELLOW}Stopping server...${NC}"
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
}

run_demo() {
    print_header
    
    echo -e "${YELLOW}This demo will test UCX Gaudi communication between client and server${NC}"
    echo
    echo "Test configuration:"
    echo "  Port: $PORT"
    echo "  Buffer size: $BUFFER_SIZE bytes"
    echo "  Server IP: $SERVER_IP"
    echo
    echo -e "${YELLOW}Press Enter to start the demo...${NC}"
    read -r
    
    echo -e "${BLUE}Starting server in background...${NC}"
    $GAUDI_TEST -p "$PORT" -s "$BUFFER_SIZE" &
    SERVER_PID=$!
    
    echo "Server started with PID: $SERVER_PID"
    echo "Waiting for server to initialize..."
    sleep 3
    
    echo
    echo -e "${BLUE}Starting client to connect to server...${NC}"
    echo
    
    if $GAUDI_TEST -c "$SERVER_IP" -p "$PORT" -s "$BUFFER_SIZE"; then
        echo
        echo -e "${GREEN}✅ Demo completed successfully!${NC}"
    else
        echo
        echo -e "${RED}❌ Demo failed!${NC}"
    fi
    
    echo
    echo -e "${YELLOW}Cleaning up server...${NC}"
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    
    echo -e "${GREEN}Demo finished.${NC}"
}

# Parse command line arguments
PORT="$DEFAULT_PORT"
while getopts "p:s:i:h" opt; do
    case $opt in
        p)
            PORT="$OPTARG"
            ;;
        s)
            BUFFER_SIZE="$OPTARG"
            ;;
        i)
            SERVER_IP="$OPTARG"
            ;;
        h)
            print_usage
            exit 0
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            print_usage
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

# Check if binary exists
check_binary

# Get mode argument
MODE="$1"

case "$MODE" in
    "server")
        print_header
        run_server
        ;;
    "client")
        print_header
        run_client
        ;;
    "both")
        print_header
        run_both
        ;;
    "demo")
        run_demo
        ;;
    "")
        echo -e "${RED}Error: Mode not specified${NC}"
        echo
        print_usage
        exit 1
        ;;
    *)
        echo -e "${RED}Error: Unknown mode '$MODE'${NC}"
        echo
        print_usage
        exit 1
        ;;
esac
