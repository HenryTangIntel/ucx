#!/bin/bash

# Quick Gaudi Integration Verification Script
# This script performs a fast check to verify Gaudi integration status

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}$1${NC}"
}

print_check() {
    echo -n "Checking $1... "
}

print_ok() {
    echo -e "${GREEN}OK${NC}"
}

print_fail() {
    echo -e "${RED}FAIL${NC}"
    HAS_FAILURES=true
}

HAS_FAILURES=false

print_header "🔍 Quick Gaudi Integration Verification"

# 1. Check if UCX is built and installed
print_check "UCX installation"
if [ -f "/scratch2/ytang/integration/ucx/install/bin/ucx_info" ]; then
    print_ok
else
    print_fail
fi

# 2. Check Gaudi device access
print_check "Gaudi device access"
if ls /dev/hl* > /dev/null 2>&1; then
    print_ok
else
    print_fail
fi

# 3. Check hl-thunk headers
print_check "hl-thunk headers"
if [ -d "/usr/include/habanalabs" ] && [ -f "/usr/include/habanalabs/hlthunk.h" ]; then
    print_ok
else
    print_fail
fi

# 4. Check hl-thunk library
print_check "hl-thunk library"
if find /usr/lib* -name "*hlthunk*" 2>/dev/null | grep -q hlthunk; then
    print_ok
else
    print_fail
fi

# 5. Check if Gaudi tests can be built
print_check "Gaudi test compilation"
cd /scratch2/ytang/integration/ucx/test/gaudi
if make simple_gaudi_test > /dev/null 2>&1; then
    print_ok
else
    print_fail
fi

# 6. Check UCX Gaudi transport detection
print_check "UCX Gaudi transport"
UCX_INFO="/scratch2/ytang/integration/ucx/install/bin/ucx_info"
if [ -f "$UCX_INFO" ] && $UCX_INFO -d 2>/dev/null | grep -q gaudi_copy; then
    print_ok
else
    print_fail
fi

# 7. Check basic Gaudi functionality
print_check "Basic Gaudi test"
UCX_ROOT="/scratch2/ytang/integration/ucx/install"
if LD_LIBRARY_PATH=${UCX_ROOT}/lib:$LD_LIBRARY_PATH ./simple_gaudi_test > /dev/null 2>&1; then
    print_ok
else
    print_fail
fi

# 8. Check memory type support
print_check "Gaudi memory type support"
if make gaudi_memory_type_test > /dev/null 2>&1 && LD_LIBRARY_PATH=${UCX_ROOT}/lib:$LD_LIBRARY_PATH ./gaudi_memory_type_test > /dev/null 2>&1; then
    print_ok
else
    print_fail
fi

echo ""
if [ "$HAS_FAILURES" = true ]; then
    echo -e "${RED}❌ Some checks failed. Run the full test suite for detailed diagnostics:${NC}"
    echo -e "   ${YELLOW}./run_all_gaudi_tests.sh${NC}"
    exit 1
else
    echo -e "${GREEN}✅ All checks passed! Gaudi integration is working correctly.${NC}"
    echo ""
    echo -e "Available test scripts:"
    echo -e "  ${BLUE}./run_all_gaudi_tests.sh${NC}     - Comprehensive test suite"
    echo -e "  ${BLUE}./gaudi_stress_tests.sh${NC}      - Stress testing"
    echo -e "  ${BLUE}./demo.sh${NC}                    - Interactive demo"
    exit 0
fi
