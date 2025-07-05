#!/bin/bash
# Test runner script for Gaudi UCX integration tests
# Copyright (c) 2025, Habana Labs Ltd. an Intel Company.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UCX_BUILD_DIR="${SCRIPT_DIR}/../../../../.."
TEST_BINARY="${UCX_BUILD_DIR}/test/gtest/gtest"

# Check if Gaudi devices are available
check_gaudi_devices() {
    if [ ! -d "/dev/accel" ] || [ -z "$(ls -A /dev/accel/accel_controlD* 2>/dev/null)" ]; then
        echo "No Gaudi devices found. Skipping Gaudi tests."
        exit 0
    fi
    
    echo "Found Gaudi devices:"
    ls -la /dev/accel/accel_controlD* 2>/dev/null || true
    echo
}

# Run specific Gaudi test suite
run_gaudi_tests() {
    local test_filter="$1"
    local config_file="$2"
    local test_name="$3"
    
    echo "Running $test_name tests..."
    echo "Filter: $test_filter"
    echo "Config: $config_file"
    echo
    
    # Set environment
    export UCX_LOG_LEVEL=info
    export UCX_GAUDI_ENABLE=y
    
    if [ -n "$config_file" ] && [ -f "$config_file" ]; then
        export UCX_CONFIG_FILE="$config_file"
    fi
    
    # Run the tests
    "$TEST_BINARY" --gtest_filter="$test_filter" \
                   --gtest_color=yes \
                   --gtest_print_time=1 \
                   --gtest_output="xml:gaudi_${test_name}_results.xml"
    
    echo "$test_name tests completed."
    echo
}

# Main execution
main() {
    echo "=== Gaudi UCX Integration Test Suite ==="
    echo
    
    # Check prerequisites
    check_gaudi_devices
    
    if [ ! -x "$TEST_BINARY" ]; then
        echo "Error: Test binary not found at $TEST_BINARY"
        echo "Please build UCX tests first: make check"
        exit 1
    fi
    
    # Set up test environment
    cd "$SCRIPT_DIR"
    
    # Run different test suites
    echo "Starting Gaudi test suites..."
    echo
    
    # 1. Memory Domain Tests
    run_gaudi_tests "test_gaudi_md.*" \
                    "" \
                    "md"
    
    # 2. DMA Utility Tests  
    run_gaudi_tests "test_gaudi_dma.*" \
                    "" \
                    "dma"
    
    # 3. Transport Tests - Copy
    run_gaudi_tests "test_gaudi_transport.*gaudi_copy*" \
                    "gaudi_copy.conf" \
                    "transport_copy"
    
    # 4. Transport Tests - IPC
    run_gaudi_tests "test_gaudi_transport.*gaudi_ipc*" \
                    "gaudi_ipc.conf" \
                    "transport_ipc"
    
    # 5. Performance Tests
    run_gaudi_tests "test_gaudi_performance.*" \
                    "" \
                    "performance"
    
    # 6. UCP Level Tests
    run_gaudi_tests "test_ucp_gaudi.*" \
                    "" \
                    "ucp"
    
    echo "=== All Gaudi tests completed ==="
    echo
    
    # Summary
    echo "Test results saved to:"
    ls -la gaudi_*_results.xml 2>/dev/null || echo "No XML results found"
    echo
    
    echo "To view detailed logs, check UCX_LOG_LEVEL=debug output"
    echo "To run individual test suites:"
    echo "  $0 md          # Memory domain tests"
    echo "  $0 dma         # DMA utility tests" 
    echo "  $0 transport   # Transport tests"
    echo "  $0 performance # Performance tests"
    echo "  $0 ucp         # UCP level tests"
}

# Handle command line arguments for running specific test suites
if [ $# -eq 1 ]; then
    case "$1" in
        md)
            check_gaudi_devices
            run_gaudi_tests "test_gaudi_md.*" "" "md"
            ;;
        dma)
            check_gaudi_devices
            run_gaudi_tests "test_gaudi_dma.*" "" "dma"
            ;;
        transport)
            check_gaudi_devices
            run_gaudi_tests "test_gaudi_transport.*" "gaudi_copy.conf" "transport"
            ;;
        performance)
            check_gaudi_devices
            run_gaudi_tests "test_gaudi_performance.*" "" "performance"
            ;;
        ucp)
            check_gaudi_devices
            run_gaudi_tests "test_ucp_gaudi.*" "" "ucp"
            ;;
        *)
            echo "Usage: $0 [md|dma|transport|performance|ucp]"
            exit 1
            ;;
    esac
else
    main
fi
