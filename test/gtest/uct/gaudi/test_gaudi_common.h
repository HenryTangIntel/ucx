/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifndef TEST_GAUDI_COMMON_H
#define TEST_GAUDI_COMMON_H

#include <common/test.h>

extern "C" {
#include <habanalabs/hlthunk.h>
#include <ucs/debug/log.h>
#include <ucs/time/time.h>
}

/* Common test utilities for Gaudi integration tests */
class gaudi_test_base {
public:
    /* Check if Gaudi devices are available on the system */
    static bool is_gaudi_available() {
        int device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
        return device_count > 0;
    }
    
    /* Get the number of available Gaudi devices */
    static int get_device_count() {
        return hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    }
    
    /* Open a Gaudi device and return file descriptor */
    static int open_gaudi_device(int device_index = 0) {
        return hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    }
    
    /* Allocate aligned host memory for tests */
    static void* alloc_host_memory(size_t size, size_t alignment = 4096) {
        void *ptr = aligned_alloc(alignment, size);
        if (ptr != NULL) {
            memset(ptr, 0, size);
        }
        return ptr;
    }
    
    /* Allocate device memory using hlthunk */
    static void* alloc_device_memory(int fd, size_t size, uint64_t *handle_out = NULL) {
        uint64_t handle = hlthunk_device_memory_alloc(fd, size, 0, true, true);
        if (handle == 0) {
            return NULL;
        }
        
        uint64_t device_addr = hlthunk_device_memory_map(fd, handle, 0);
        if (device_addr == 0) {
            hlthunk_device_memory_free(fd, handle);
            return NULL;
        }
        
        if (handle_out != NULL) {
            *handle_out = handle;
        }
        
        return (void*)device_addr;
    }
    
    /* Free device memory */
    static void free_device_memory(int fd, void *ptr, uint64_t handle) {
        if (ptr != NULL && handle != 0) {
            hlthunk_device_memory_free(fd, handle);
        }
    }
    
    /* Fill buffer with test pattern */
    static void fill_buffer(void *buffer, size_t size, uint32_t seed = 0x12345678) {
        uint32_t *data = (uint32_t*)buffer;
        size_t count = size / sizeof(uint32_t);
        
        for (size_t i = 0; i < count; ++i) {
            data[i] = seed + (uint32_t)i;
        }
        
        /* Fill remaining bytes */
        uint8_t *byte_data = (uint8_t*)buffer;
        for (size_t i = count * sizeof(uint32_t); i < size; ++i) {
            byte_data[i] = (uint8_t)(seed + i);
        }
    }
    
    /* Verify buffer contains expected pattern */
    static bool verify_buffer(void *buffer, size_t size, uint32_t seed = 0x12345678) {
        uint32_t *data = (uint32_t*)buffer;
        size_t count = size / sizeof(uint32_t);
        
        for (size_t i = 0; i < count; ++i) {
            if (data[i] != seed + (uint32_t)i) {
                return false;
            }
        }
        
        /* Check remaining bytes */
        uint8_t *byte_data = (uint8_t*)buffer;
        for (size_t i = count * sizeof(uint32_t); i < size; ++i) {
            if (byte_data[i] != (uint8_t)(seed + i)) {
                return false;
            }
        }
        
        return true;
    }
    
    /* Get current timestamp in microseconds */
    static uint64_t get_time_us() {
        return ucs_get_time();
    }
    
    /* Calculate bandwidth in MB/s */
    static double calculate_bandwidth_mbps(size_t bytes, double time_us) {
        if (time_us <= 0) return 0.0;
        return (bytes / (1024.0 * 1024.0)) / (time_us / 1000000.0);
    }
};

/* Macro to skip test if Gaudi is not available */
#define GAUDI_SKIP_IF_NOT_AVAILABLE() \
    do { \
        if (!gaudi_test_base::is_gaudi_available()) { \
            UCS_TEST_SKIP_R("Gaudi not available"); \
        } \
    } while(0)

/* Macro to create a test pattern based on test name and iteration */
#define GAUDI_TEST_PATTERN(test_name, iteration) \
    (0x##test_name##00 + (iteration & 0xFF))

/* Common test sizes for performance testing */
static const std::vector<size_t> GAUDI_TEST_SIZES = {
    1024,       /* 1KB */
    4096,       /* 4KB */
    16384,      /* 16KB */
    65536,      /* 64KB */
    262144,     /* 256KB */
    1048576,    /* 1MB */
    4194304,    /* 4MB */
    16777216    /* 16MB */
};

/* Test iteration counts for different test types */
static const int GAUDI_LATENCY_ITERATIONS = 1000;
static const int GAUDI_BANDWIDTH_ITERATIONS = 100;
static const int GAUDI_STRESS_ITERATIONS = 10000;

#endif /* TEST_GAUDI_COMMON_H */
