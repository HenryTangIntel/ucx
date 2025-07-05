#!/bin/bash
# Enhanced Gaudi Test Runner Script
# Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UCX_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
BUILD_DIR="${UCX_ROOT}"
TEST_BINARY="${BUILD_DIR}/test/gtest/test_uct"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
VERBOSE=0
RUN_BASIC=1
RUN_ADVANCED=1
RUN_ERROR_HANDLING=1
RUN_INTEGRATION=1
RUN_DMA_COMPREHENSIVE=1
RUN_PERFORMANCE=1
RUN_STRESS=1
PARALLEL_JOBS=1
TIMEOUT=300  # 5 minutes per test suite
LOG_LEVEL=2

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}[$(date '+%Y-%m-%d %H:%M:%S')] ${message}${NC}"
}

print_info() {
    print_status "$BLUE" "INFO: $1"
}

print_success() {
    print_status "$GREEN" "SUCCESS: $1"
}

print_warning() {
    print_status "$YELLOW" "WARNING: $1"
}

print_error() {
    print_status "$RED" "ERROR: $1"
}

# Usage function
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Enhanced Gaudi Test Runner for UCX Integration

Options:
    -h, --help              Show this help message
    -v, --verbose           Enable verbose output
    -b, --basic-only        Run only basic tests
    -a, --advanced-only     Run only advanced tests
    -e, --error-only        Run only error handling tests
    -i, --integration-only  Run only integration tests
    -d, --dma-only          Run only comprehensive DMA tests
    -p, --performance-only  Run only performance tests
    -s, --stress-only       Run only stress tests
    -j, --jobs N            Run N test suites in parallel (default: 1)
    -t, --timeout N         Set timeout per test suite in seconds (default: 300)
    -l, --log-level N       Set log level (0=silent, 1=error, 2=info, 3=debug)
    --skip-basic            Skip basic tests
    --skip-advanced         Skip advanced tests
    --skip-error            Skip error handling tests
    --skip-integration      Skip integration tests
    --skip-dma              Skip DMA comprehensive tests
    --skip-performance      Skip performance tests
    --skip-stress           Skip stress tests

Examples:
    $0                      # Run all tests
    $0 -v                   # Run all tests with verbose output
    $0 -a                   # Run only advanced tests
    $0 -j 4                 # Run 4 test suites in parallel
    $0 --skip-performance   # Run all tests except performance

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -b|--basic-only)
            RUN_BASIC=1
            RUN_ADVANCED=0
            RUN_ERROR_HANDLING=0
            RUN_INTEGRATION=0
            RUN_DMA_COMPREHENSIVE=0
            RUN_PERFORMANCE=0
            shift
            ;;
        -a|--advanced-only)
            RUN_BASIC=0
            RUN_ADVANCED=1
            RUN_ERROR_HANDLING=0
            RUN_INTEGRATION=0
            RUN_DMA_COMPREHENSIVE=0
            RUN_PERFORMANCE=0
            shift
            ;;
        -e|--error-only)
            RUN_BASIC=0
            RUN_ADVANCED=0
            RUN_ERROR_HANDLING=1
            RUN_INTEGRATION=0
            RUN_DMA_COMPREHENSIVE=0
            RUN_PERFORMANCE=0
            shift
            ;;
        -i|--integration-only)
            RUN_BASIC=0
            RUN_ADVANCED=0
            RUN_ERROR_HANDLING=0
            RUN_INTEGRATION=1
            RUN_DMA_COMPREHENSIVE=0
            RUN_PERFORMANCE=0
            shift
            ;;
        -d|--dma-only)
            RUN_BASIC=0
            RUN_ADVANCED=0
            RUN_ERROR_HANDLING=0
            RUN_INTEGRATION=0
            RUN_DMA_COMPREHENSIVE=1
            RUN_PERFORMANCE=0
            shift
            ;;
        -p|--performance-only)
            RUN_BASIC=0
            RUN_ADVANCED=0
            RUN_ERROR_HANDLING=0
            RUN_INTEGRATION=0
            RUN_DMA_COMPREHENSIVE=0
            RUN_PERFORMANCE=1
            RUN_STRESS=0
            shift
            ;;
        -s|--stress-only)
            RUN_BASIC=0
            RUN_ADVANCED=0
            RUN_ERROR_HANDLING=0
            RUN_INTEGRATION=0
            RUN_DMA_COMPREHENSIVE=0
            RUN_PERFORMANCE=0
            RUN_STRESS=1
            shift
            ;;
        --skip-basic)
            RUN_BASIC=0
            shift
            ;;
        --skip-advanced)
            RUN_ADVANCED=0
            shift
            ;;
        --skip-error)
            RUN_ERROR_HANDLING=0
            shift
            ;;
        --skip-integration)
            RUN_INTEGRATION=0
            shift
            ;;
        --skip-dma)
            RUN_DMA_COMPREHENSIVE=0
            shift
            ;;
        --skip-performance)
            RUN_PERFORMANCE=0
            shift
            ;;
        --skip-stress)
            RUN_STRESS=0
            shift
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -t|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        -l|--log-level)
            LOG_LEVEL="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Check prerequisites
check_prerequisites() {
    print_info "Checking prerequisites..."
    
    # Check if test binary exists
    if [[ ! -x "$TEST_BINARY" ]]; then
        print_error "Test binary not found: $TEST_BINARY"
        print_info "Please build UCX with: make -j$(nproc)"
        return 1
    fi
    
    # Check if Gaudi is available
    if ! hlthunk_get_device_count >/dev/null 2>&1; then
        print_warning "hlthunk_get_device_count not available, some tests may be skipped"
    fi
    
    # Check for Gaudi devices
    local device_count
    device_count=$(hlthunk_get_device_count 2>/dev/null || echo "0")
    if [[ "$device_count" -eq 0 ]]; then
        print_warning "No Gaudi devices detected, tests may be skipped"
    else
        print_info "Found $device_count Gaudi device(s)"
    fi
    
    return 0
}

# Run a specific test suite
run_test_suite() {
    local test_name="$1"
    local test_filter="$2"
    local config_file="$3"
    local log_file="${SCRIPT_DIR}/logs/${test_name}_$(date +%Y%m%d_%H%M%S).log"
    
    print_info "Running $test_name tests..."
    
    # Create log directory
    mkdir -p "${SCRIPT_DIR}/logs"
    
    # Set environment variables
    local env_vars=""
    if [[ -n "$config_file" && -f "$config_file" ]]; then
        env_vars="UCX_CONFIG_FILE=$config_file"
    fi
    
    # Set log level
    case $LOG_LEVEL in
        0) env_vars="$env_vars UCX_LOG_LEVEL=error" ;;
        1) env_vars="$env_vars UCX_LOG_LEVEL=warn" ;;
        2) env_vars="$env_vars UCX_LOG_LEVEL=info" ;;
        3) env_vars="$env_vars UCX_LOG_LEVEL=debug" ;;
    esac
    
    # Build command
    local cmd="timeout $TIMEOUT env $env_vars $TEST_BINARY --gtest_filter=\"$test_filter\""
    if [[ $VERBOSE -eq 1 ]]; then
        cmd="$cmd --gtest_output=xml:${log_file%.log}.xml"
    fi
    
    # Run test
    local start_time=$(date +%s)
    if [[ $VERBOSE -eq 1 ]]; then
        eval "$cmd" 2>&1 | tee "$log_file"
        local result=${PIPESTATUS[0]}
    else
        eval "$cmd" >"$log_file" 2>&1
        local result=$?
    fi
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Report results
    if [[ $result -eq 0 ]]; then
        print_success "$test_name tests completed in ${duration}s"
        return 0
    elif [[ $result -eq 124 ]]; then
        print_error "$test_name tests timed out after ${TIMEOUT}s"
        return 1
    else
        print_error "$test_name tests failed (exit code: $result)"
        if [[ $VERBOSE -eq 0 ]]; then
            print_info "Log file: $log_file"
            print_info "Last 10 lines of log:"
            tail -n 10 "$log_file" | sed 's/^/  /'
        fi
        return 1
    fi
}

# Main test execution function
run_tests() {
    local total_tests=0
    local passed_tests=0
    local failed_tests=0
    
    print_info "Starting Gaudi integration tests..."
    print_info "Configuration: timeout=${TIMEOUT}s, parallel_jobs=${PARALLEL_JOBS}, log_level=${LOG_LEVEL}"
    
    # Test definitions
    declare -A test_suites
    test_suites=(
        ["basic"]="test_gaudi_md.*:test_gaudi_dma.*:test_gaudi_transport.*"
        ["advanced"]="test_gaudi_advanced.*"
        ["error_handling"]="test_gaudi_error_handling.*"
        ["integration"]="test_gaudi_integration.*"
        ["dma_comprehensive"]="test_gaudi_dma_comprehensive.*"
        ["performance"]="test_gaudi_performance.*"
        ["stress"]="test_gaudi_stress.*"
    )
    
    declare -A config_files
    config_files=(
        ["basic"]="${SCRIPT_DIR}/gaudi_copy.conf"
        ["advanced"]="${SCRIPT_DIR}/gaudi_advanced.conf"
        ["error_handling"]="${SCRIPT_DIR}/gaudi_advanced.conf"
        ["integration"]="${SCRIPT_DIR}/gaudi_copy.conf"
        ["dma_comprehensive"]="${SCRIPT_DIR}/gaudi_advanced.conf"
        ["performance"]="${SCRIPT_DIR}/gaudi_advanced.conf"
        ["stress"]="${SCRIPT_DIR}/gaudi_advanced.conf"
    )
    
    # Collect tests to run
    declare -a tests_to_run
    [[ $RUN_BASIC -eq 1 ]] && tests_to_run+=("basic")
    [[ $RUN_ADVANCED -eq 1 ]] && tests_to_run+=("advanced")
    [[ $RUN_ERROR_HANDLING -eq 1 ]] && tests_to_run+=("error_handling")
    [[ $RUN_INTEGRATION -eq 1 ]] && tests_to_run+=("integration")
    [[ $RUN_DMA_COMPREHENSIVE -eq 1 ]] && tests_to_run+=("dma_comprehensive")
    [[ $RUN_PERFORMANCE -eq 1 ]] && tests_to_run+=("performance")
    [[ $RUN_STRESS -eq 1 ]] && tests_to_run+=("stress")
    
    if [[ ${#tests_to_run[@]} -eq 0 ]]; then
        print_warning "No tests selected to run"
        return 0
    fi
    
    # Run tests
    if [[ $PARALLEL_JOBS -eq 1 ]]; then
        # Sequential execution
        for test_name in "${tests_to_run[@]}"; do
            total_tests=$((total_tests + 1))
            if run_test_suite "$test_name" "${test_suites[$test_name]}" "${config_files[$test_name]}"; then
                passed_tests=$((passed_tests + 1))
            else
                failed_tests=$((failed_tests + 1))
            fi
        done
    else
        # Parallel execution
        declare -a pids
        declare -A test_names
        
        for test_name in "${tests_to_run[@]}"; do
            total_tests=$((total_tests + 1))
            run_test_suite "$test_name" "${test_suites[$test_name]}" "${config_files[$test_name]}" &
            local pid=$!
            pids+=($pid)
            test_names[$pid]=$test_name
        done
        
        # Wait for all tests to complete
        for pid in "${pids[@]}"; do
            if wait $pid; then
                passed_tests=$((passed_tests + 1))
                print_success "${test_names[$pid]} tests completed"
            else
                failed_tests=$((failed_tests + 1))
                print_error "${test_names[$pid]} tests failed"
            fi
        done
    fi
    
    # Summary
    print_info "Test execution summary:"
    print_info "  Total test suites: $total_tests"
    print_success "  Passed: $passed_tests"
    if [[ $failed_tests -gt 0 ]]; then
        print_error "  Failed: $failed_tests"
    else
        print_info "  Failed: $failed_tests"
    fi
    
    return $failed_tests
}

# Main execution
main() {
    # Check prerequisites
    if ! check_prerequisites; then
        exit 1
    fi
    
    # Run tests
    if run_tests; then
        print_success "All Gaudi integration tests completed successfully!"
        exit 0
    else
        print_error "Some Gaudi integration tests failed!"
        exit 1
    fi
}

# Execute main function
main "$@"
