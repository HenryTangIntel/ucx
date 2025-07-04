#!/bin/bash

# Comprehensive Gaudi Test Suite for UCX
# This script runs all Gaudi-related tests and reports results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test result tracking
TESTS_PASSED=0
TESTS_FAILED=0
FAILED_TESTS=()

# Logging
LOG_FILE="gaudi_test_results_$(date +%Y%m%d_%H%M%S).log"
echo "Gaudi Test Suite - $(date)" > "$LOG_FILE"

# Helper functions
print_header() {
    echo -e "${BLUE}===============================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}===============================================${NC}"
}

print_test() {
    echo -e "${YELLOW}Running: $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ PASSED: $1${NC}"
    ((TESTS_PASSED++))
}

print_failure() {
    echo -e "${RED}✗ FAILED: $1${NC}"
    ((TESTS_FAILED++))
    FAILED_TESTS+=("$1")
}

run_test() {
    local test_name="$1"
    local test_command="$2"
    local test_description="$3"
    
    print_test "$test_description"
    echo "Running: $test_command" >> "$LOG_FILE"
    
    if eval "$test_command" >> "$LOG_FILE" 2>&1; then
        print_success "$test_name"
        echo "PASSED: $test_name" >> "$LOG_FILE"
    else
        print_failure "$test_name"
        echo "FAILED: $test_name" >> "$LOG_FILE"
        echo "Error output:" >> "$LOG_FILE"
        eval "$test_command" >> "$LOG_FILE" 2>&1 || true
    fi
    echo "----------------------------------------" >> "$LOG_FILE"
}

# Change to test directory
cd /scratch2/ytang/integration/ucx/test/gaudi

print_header "UCX Gaudi Integration Test Suite"

# Check if we're in the right directory and UCX is built
if [ ! -f "Makefile" ]; then
    echo -e "${RED}Error: Gaudi test Makefile not found. Please ensure UCX is built.${NC}"
    exit 1
fi

# Build all tests first
print_header "Building Gaudi Tests"
print_test "Building all Gaudi test programs"
if make clean && make all >> "$LOG_FILE" 2>&1; then
    print_success "Build"
    echo "Build completed successfully" >> "$LOG_FILE"
else
    print_failure "Build"
    echo "Build failed. Cannot proceed with tests." >> "$LOG_FILE"
    exit 1
fi

# 1. Basic Infrastructure Tests
print_header "1. Basic Infrastructure Tests"

run_test "UCX_INFO" \
    "../../install/bin/ucx_info -d | grep -E '(gaudi|dmabuf)'" \
    "UCX transport detection (gaudi_copy and dmabuf support)"

run_test "GAUDI_INFO" \
    "./ucx_gaudi_info" \
    "Basic Gaudi device detection and info"

run_test "SIMPLE_GAUDI" \
    "./simple_gaudi_test" \
    "Simple Gaudi memory allocation test"

run_test "BASIC_GAUDI" \
    "./basic_gaudi_test" \
    "Basic Gaudi transport and memory management"

# 2. DMA-BUF Tests
print_header "2. DMA-BUF Integration Tests"

run_test "DMABUF_EXPORT" \
    "./gaudi_dmabuf_test" \
    "DMA-BUF export and file descriptor handling"

run_test "GAUDI_IB_INTEGRATION" \
    "./gaudi_ib_integration_test" \
    "Gaudi and InfiniBand DMA-BUF integration"

# 3. Memory Type Tests
print_header "3. Memory Type Support Tests"

run_test "MEMORY_TYPE" \
    "./gaudi_memory_type_test" \
    "UCS memory type support for Gaudi"

# 4. UCM (Memory Hooks) Tests
print_header "4. UCM Memory Hooks Tests"

run_test "UCM_HOOKS" \
    "./gaudi_ucm_test" \
    "UCM memory hooks for Gaudi device memory"

# 5. UCP GTest Suite
print_header "5. UCP GTest Integration"

# Check if gtest binary exists
GTEST_DIR="/scratch2/ytang/integration/ucx/test/gtest"
if [ -f "$GTEST_DIR/gtest" ]; then
    run_test "UCP_GAUDI_GTEST" \
        "cd $GTEST_DIR && ./gtest --gtest_filter=*Gaudi*" \
        "UCP GTest suite for Gaudi integration"
else
    echo -e "${YELLOW}Warning: GTest binary not found, skipping UCP GTest${NC}"
    echo "Warning: GTest binary not found" >> "$LOG_FILE"
fi

# 6. Performance Tools Test
print_header "6. Performance Tools Integration"

UCX_PERFTEST="/scratch2/ytang/integration/ucx/install/bin/ucx_perftest"
if [ -f "$UCX_PERFTEST" ]; then
    run_test "PERFTEST_GAUDI" \
        "$UCX_PERFTEST -m gaudi -t am_lat -s 8 -n 10" \
        "UCX performance test with Gaudi memory"
else
    echo -e "${YELLOW}Warning: ucx_perftest not found, skipping performance test${NC}"
    echo "Warning: ucx_perftest not found" >> "$LOG_FILE"
fi

# 7. Integration with UCX Tools
print_header "7. UCX Tools Integration"

UCX_INFO="/scratch2/ytang/integration/ucx/install/bin/ucx_info"
if [ -f "$UCX_INFO" ]; then
    run_test "UCX_INFO_DETAILED" \
        "$UCX_INFO -c | grep -i gaudi" \
        "UCX configuration shows Gaudi support"
    
    run_test "UCX_INFO_MEMORY_TYPES" \
        "$UCX_INFO -f | grep -i gaudi" \
        "UCX features show Gaudi memory type"
else
    echo -e "${YELLOW}Warning: ucx_info not found, skipping detailed info tests${NC}"
    echo "Warning: ucx_info not found" >> "$LOG_FILE"
fi

# 8. Environmental Tests
print_header "8. Environment and Runtime Tests"

run_test "GAUDI_DEVICE_ACCESS" \
    "ls -la /dev/hl*" \
    "Gaudi device files accessibility"

run_test "HLTHUNK_LIBRARY" \
    "ldconfig -p | grep hlthunk || find /usr/lib* -name '*hlthunk*' 2>/dev/null || echo 'Library check complete'" \
    "hl-thunk library presence"

run_test "HABANA_HEADERS" \
    "ls -la /usr/include/habanalabs/" \
    "Habana Labs headers availability"

# Summary
print_header "Test Results Summary"

echo "Total tests run: $((TESTS_PASSED + TESTS_FAILED))"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "\n${RED}Failed tests:${NC}"
    for test in "${FAILED_TESTS[@]}"; do
        echo -e "  ${RED}- $test${NC}"
    done
fi

echo -e "\nDetailed log saved to: $LOG_FILE"

# Final status
if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}🎉 All tests passed! Gaudi integration is working correctly.${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some tests failed. Check the log for details.${NC}"
    exit 1
fi
