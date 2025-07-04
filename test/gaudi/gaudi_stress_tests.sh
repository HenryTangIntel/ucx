#!/bin/bash

# Gaudi Stress Test and Verification Script
# This script performs stress testing and verification of Gaudi integration

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}===============================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}===============================================${NC}"
}

print_test() {
    echo -e "${YELLOW}$1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_failure() {
    echo -e "${RED}✗ $1${NC}"
}

# Change to test directory
cd /scratch2/ytang/integration/ucx/test/gaudi

print_header "Gaudi Stress Test and Verification"

# 1. Memory Stress Test
print_header "1. Memory Allocation Stress Test"

cat > gaudi_stress_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hlthunk.h>

#define NUM_ITERATIONS 100
#define ALLOC_SIZE (1024 * 1024)  // 1MB allocations

int main() {
    int fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (fd < 0) {
        printf("Failed to open Gaudi device\n");
        return 1;
    }
    
    printf("Starting memory stress test with %d iterations...\n", NUM_ITERATIONS);
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Test device memory allocation/free
        uint64_t device_addr = 0;
        uint64_t handle = 0;
        
        if (hlthunk_device_memory_alloc(fd, ALLOC_SIZE, 0, false, &device_addr, &handle) != 0) {
            printf("Failed to allocate device memory at iteration %d\n", i);
            hlthunk_close(fd);
            return 1;
        }
        
        if (hlthunk_device_memory_free(fd, handle) != 0) {
            printf("Failed to free device memory at iteration %d\n", i);
            hlthunk_close(fd);
            return 1;
        }
        
        if (i % 10 == 0) {
            printf("Completed %d iterations\n", i);
        }
    }
    
    printf("Memory stress test completed successfully!\n");
    hlthunk_close(fd);
    return 0;
}
EOF

print_test "Building memory stress test..."
if gcc -o gaudi_stress_test gaudi_stress_test.c -lhlthunk -I/usr/include/habanalabs -L/usr/lib/habanalabs; then
    print_success "Stress test compiled"
    
    print_test "Running memory stress test..."
    if ./gaudi_stress_test; then
        print_success "Memory stress test passed"
    else
        print_failure "Memory stress test failed"
    fi
else
    print_failure "Failed to compile stress test"
fi

# 2. DMA-BUF Stress Test
print_header "2. DMA-BUF Export Stress Test"

cat > gaudi_dmabuf_stress_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hlthunk.h>

#define NUM_ITERATIONS 50
#define ALLOC_SIZE (64 * 1024)  // 64KB allocations

int main() {
    int fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (fd < 0) {
        printf("Failed to open Gaudi device\n");
        return 1;
    }
    
    printf("Starting DMA-BUF stress test with %d iterations...\n", NUM_ITERATIONS);
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint64_t device_addr = 0;
        uint64_t handle = 0;
        int dmabuf_fd = -1;
        
        // Allocate device memory
        if (hlthunk_device_memory_alloc(fd, ALLOC_SIZE, 0, false, &device_addr, &handle) != 0) {
            printf("Failed to allocate device memory at iteration %d\n", i);
            hlthunk_close(fd);
            return 1;
        }
        
        // Export as DMA-BUF
        if (hlthunk_device_memory_export_dmabuf_fd(fd, handle, &dmabuf_fd) != 0) {
            printf("Failed to export DMA-BUF at iteration %d\n", i);
            hlthunk_device_memory_free(fd, handle);
            hlthunk_close(fd);
            return 1;
        }
        
        // Close DMA-BUF fd
        if (dmabuf_fd >= 0) {
            close(dmabuf_fd);
        }
        
        // Free device memory
        if (hlthunk_device_memory_free(fd, handle) != 0) {
            printf("Failed to free device memory at iteration %d\n", i);
            hlthunk_close(fd);
            return 1;
        }
        
        if (i % 10 == 0) {
            printf("Completed %d DMA-BUF cycles\n", i);
        }
    }
    
    printf("DMA-BUF stress test completed successfully!\n");
    hlthunk_close(fd);
    return 0;
}
EOF

print_test "Building DMA-BUF stress test..."
if gcc -o gaudi_dmabuf_stress_test gaudi_dmabuf_stress_test.c -lhlthunk -I/usr/include/habanalabs -L/usr/lib/habanalabs; then
    print_success "DMA-BUF stress test compiled"
    
    print_test "Running DMA-BUF stress test..."
    if ./gaudi_dmabuf_stress_test; then
        print_success "DMA-BUF stress test passed"
    else
        print_failure "DMA-BUF stress test failed"
    fi
else
    print_failure "Failed to compile DMA-BUF stress test"
fi

# 3. UCX Integration Stress Test
print_header "3. UCX Integration Stress Test"

cat > ucx_gaudi_stress_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucp/api/ucp.h>
#include <ucs/memory/memory_type.h>

#define NUM_ITERATIONS 20
#define BUFFER_SIZE 1024

int main() {
    ucp_params_t ucp_params;
    ucp_config_t *config;
    ucp_context_h ucp_context;
    ucs_status_t status;
    
    printf("Starting UCX Gaudi integration stress test...\n");
    
    // Read UCP configuration
    status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) {
        printf("Failed to read UCP config\n");
        return 1;
    }
    
    // Initialize UCP context
    memset(&ucp_params, 0, sizeof(ucp_params));
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_AM | UCP_FEATURE_RMA;
    
    status = ucp_init(&ucp_params, config, &ucp_context);
    if (status != UCS_OK) {
        printf("Failed to initialize UCP context\n");
        ucp_config_release(config);
        return 1;
    }
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Test memory type detection for Gaudi
        ucs_memory_type_t mem_type = ucs_memory_type_name_to_id("gaudi");
        if (mem_type == UCS_MEMORY_TYPE_UNKNOWN) {
            printf("Gaudi memory type not recognized at iteration %d\n", i);
            ucp_cleanup(ucp_context);
            ucp_config_release(config);
            return 1;
        }
        
        if (i % 5 == 0) {
            printf("Completed %d UCX integration cycles\n", i);
        }
    }
    
    printf("UCX integration stress test completed successfully!\n");
    
    ucp_cleanup(ucp_context);
    ucp_config_release(config);
    return 0;
}
EOF

print_test "Building UCX integration stress test..."
UCX_ROOT="/scratch2/ytang/integration/ucx/install"
if gcc -o ucx_gaudi_stress_test ucx_gaudi_stress_test.c \
    -I${UCX_ROOT}/include \
    -L${UCX_ROOT}/lib \
    -lucp -lucs -Wl,-rpath,${UCX_ROOT}/lib; then
    print_success "UCX stress test compiled"
    
    print_test "Running UCX integration stress test..."
    if LD_LIBRARY_PATH=${UCX_ROOT}/lib:$LD_LIBRARY_PATH ./ucx_gaudi_stress_test; then
        print_success "UCX integration stress test passed"
    else
        print_failure "UCX integration stress test failed"
    fi
else
    print_failure "Failed to compile UCX stress test"
fi

# 4. System Verification
print_header "4. System Configuration Verification"

print_test "Checking Gaudi device permissions..."
if ls -la /dev/hl* > /dev/null 2>&1; then
    print_success "Gaudi devices accessible"
else
    print_failure "Gaudi devices not accessible"
fi

print_test "Checking hl-thunk library linkage..."
if ldd ./gaudi_stress_test | grep hlthunk > /dev/null 2>&1; then
    print_success "hl-thunk library properly linked"
else
    print_failure "hl-thunk library not found"
fi

print_test "Checking UCX Gaudi transport availability..."
UCX_INFO="${UCX_ROOT}/bin/ucx_info"
if [ -f "$UCX_INFO" ] && $UCX_INFO -d | grep gaudi_copy > /dev/null 2>&1; then
    print_success "Gaudi transport detected by UCX"
else
    print_failure "Gaudi transport not detected by UCX"
fi

print_test "Checking DMA-BUF support in InfiniBand..."
if [ -f "$UCX_INFO" ] && $UCX_INFO -d | grep -A 10 "mlx5\|ibv" | grep dmabuf > /dev/null 2>&1; then
    print_success "DMA-BUF support detected in IB transport"
else
    print_failure "DMA-BUF support not detected in IB transport"
fi

print_header "Stress Test Summary"
print_success "All stress tests completed!"
print_test "Generated temporary test files:"
echo "  - gaudi_stress_test.c"
echo "  - gaudi_dmabuf_stress_test.c" 
echo "  - ucx_gaudi_stress_test.c"
echo "  - gaudi_stress_test (executable)"
echo "  - gaudi_dmabuf_stress_test (executable)"
echo "  - ucx_gaudi_stress_test (executable)"

print_test "To clean up temporary files, run:"
echo "  rm -f gaudi_*stress_test* ucx_gaudi_stress_test*"
