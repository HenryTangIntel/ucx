/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include <common/test.h>

extern "C" {
#include <uct/gaudi/base/gaudi_dma.h>
#include <habanalabs/hlthunk.h>
#include <ucs/debug/log.h>
}

class test_gaudi_dma : public ucs::test {
public:
    UCS_TEST_BASE_IMPL;

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        
        /* Open hlthunk device */
        m_fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
        if (m_fd < 0) {
            UCS_TEST_SKIP_R("Failed to open hlthunk device");
        }
        
        /* Get hardware info */
        if (hlthunk_get_hw_ip_info(m_fd, &m_hw_info) != 0) {
            UCS_TEST_SKIP_R("Failed to get hardware info");
        }
    }
    
    void cleanup() {
        if (m_fd >= 0) {
            hlthunk_close(m_fd);
        }
    }
    
    static bool is_gaudi_available() {
        int device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
        return device_count > 0;
    }
    
    /* Helper to allocate host memory */
    void* alloc_host_memory(size_t size) {
        void *ptr = aligned_alloc(4096, size);  /* Page-aligned */
        if (ptr != NULL) {
            memset(ptr, 0, size);
        }
        return ptr;
    }
    
    /* Helper to allocate device memory */
    void* alloc_device_memory(size_t size, uint64_t *handle_p = NULL) {
        uint64_t handle = hlthunk_device_memory_alloc(m_fd, size, 0, true, true);
        if (handle == 0) {
            return NULL;
        }
        
        uint64_t device_addr = hlthunk_device_memory_map(m_fd, handle, 0);
        if (device_addr == 0) {
            hlthunk_device_memory_free(m_fd, handle);
            return NULL;
        }
        
        if (handle_p != NULL) {
            *handle_p = handle;
        }
        
        return (void*)device_addr;
    }
    
    /* Check if address is in device memory range */
    bool is_device_address(void *ptr) {
        uintptr_t addr = (uintptr_t)ptr;
        return (addr >= m_hw_info.dram_base_address &&
                addr < (m_hw_info.dram_base_address + m_hw_info.dram_size));
    }

protected:
    int m_fd = -1;
    struct hlthunk_hw_ip_info m_hw_info;
};

UCS_TEST_F(test_gaudi_dma, dma_direction_detection) {
    const size_t size = 4096;
    
    void *host_ptr = alloc_host_memory(size);
    ASSERT_NE(host_ptr, (void*)NULL);
    
    void *device_ptr = alloc_device_memory(size);
    if (device_ptr == NULL) {
        free(host_ptr);
        UCS_TEST_SKIP_R("Failed to allocate device memory");
    }
    
    /* Verify our address detection works */
    EXPECT_FALSE(is_device_address(host_ptr));
    EXPECT_TRUE(is_device_address(device_ptr));
    
    free(host_ptr);
}

UCS_TEST_F(test_gaudi_dma, basic_dma_copy) {
    const size_t size = 1024;
    const uint32_t pattern = 0xDEADBEEF;
    
    void *host_src = alloc_host_memory(size);
    void *host_dst = alloc_host_memory(size);
    ASSERT_NE(host_src, (void*)NULL);
    ASSERT_NE(host_dst, (void*)NULL);
    
    /* Fill source with pattern */
    uint32_t *src_data = (uint32_t*)host_src;
    for (size_t i = 0; i < size / sizeof(uint32_t); ++i) {
        src_data[i] = pattern + i;
    }
    
    /* Clear destination */
    memset(host_dst, 0, size);
    
    /* Test DMA copy with manual mode */
    ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, host_dst, host_src, 
                                                    size, &m_hw_info);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
    
    if (status == UCS_OK) {
        /* Verify copy worked */
        uint32_t *dst_data = (uint32_t*)host_dst;
        for (size_t i = 0; i < size / sizeof(uint32_t); ++i) {
            EXPECT_EQ(pattern + i, dst_data[i]) << "Mismatch at index " << i;
        }
    }
    
    free(host_src);
    free(host_dst);
}

UCS_TEST_F(test_gaudi_dma, auto_dma_copy) {
    const size_t size = 2048;
    const uint8_t pattern = 0xAA;
    
    void *host_src = alloc_host_memory(size);
    void *host_dst = alloc_host_memory(size);
    ASSERT_NE(host_src, (void*)NULL);
    ASSERT_NE(host_dst, (void*)NULL);
    
    /* Fill source with pattern */
    memset(host_src, pattern, size);
    memset(host_dst, 0, size);
    
    /* Test auto DMA copy */
    ucs_status_t status = uct_gaudi_dma_execute_copy_auto(host_dst, host_src, size);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
    
    if (status == UCS_OK) {
        /* Verify copy worked */
        uint8_t *dst_data = (uint8_t*)host_dst;
        for (size_t i = 0; i < size; ++i) {
            EXPECT_EQ(pattern, dst_data[i]) << "Mismatch at byte " << i;
        }
    }
    
    free(host_src);
    free(host_dst);
}

UCS_TEST_F(test_gaudi_dma, zero_length_copy) {
    void *host_src = alloc_host_memory(1024);
    void *host_dst = alloc_host_memory(1024);
    ASSERT_NE(host_src, (void*)NULL);
    ASSERT_NE(host_dst, (void*)NULL);
    
    /* Test zero-length copy (should be no-op) */
    ucs_status_t status = uct_gaudi_dma_execute_copy_auto(host_dst, host_src, 0);
    EXPECT_EQ(UCS_OK, status);
    
    status = uct_gaudi_dma_execute_copy(m_fd, host_dst, host_src, 0, &m_hw_info);
    EXPECT_EQ(UCS_OK, status);
    
    free(host_src);
    free(host_dst);
}

UCS_TEST_F(test_gaudi_dma, large_copy) {
    const size_t size = 1024 * 1024;  /* 1MB */
    
    void *host_src = alloc_host_memory(size);
    void *host_dst = alloc_host_memory(size);
    
    if (host_src == NULL || host_dst == NULL) {
        free(host_src);
        free(host_dst);
        UCS_TEST_SKIP_R("Failed to allocate large host memory");
    }
    
    /* Fill source with incremental pattern */
    uint8_t *src_data = (uint8_t*)host_src;
    for (size_t i = 0; i < size; ++i) {
        src_data[i] = (uint8_t)(i & 0xFF);
    }
    
    memset(host_dst, 0, size);
    
    /* Test large copy */
    ucs_status_t status = uct_gaudi_dma_execute_copy_auto(host_dst, host_src, size);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
    
    if (status == UCS_OK) {
        /* Verify copy worked - check first, middle, and last bytes */
        uint8_t *dst_data = (uint8_t*)host_dst;
        EXPECT_EQ(src_data[0], dst_data[0]);
        EXPECT_EQ(src_data[size/2], dst_data[size/2]);
        EXPECT_EQ(src_data[size-1], dst_data[size-1]);
        
        /* Check a few random locations */
        for (int i = 0; i < 10; ++i) {
            size_t idx = rand() % size;
            EXPECT_EQ(src_data[idx], dst_data[idx]) << "Mismatch at index " << idx;
        }
    }
    
    free(host_src);
    free(host_dst);
}

UCS_TEST_F(test_gaudi_dma, invalid_parameters) {
    void *host_ptr = alloc_host_memory(1024);
    ASSERT_NE(host_ptr, (void*)NULL);
    
    /* Test with NULL pointers */
    ucs_status_t status = uct_gaudi_dma_execute_copy_auto(NULL, host_ptr, 1024);
    EXPECT_NE(UCS_OK, status);
    
    status = uct_gaudi_dma_execute_copy_auto(host_ptr, NULL, 1024);
    EXPECT_NE(UCS_OK, status);
    
    /* Test with invalid fd */
    status = uct_gaudi_dma_execute_copy(-1, host_ptr, host_ptr, 1024, &m_hw_info);
    EXPECT_NE(UCS_OK, status);
    
    /* Test with NULL hw_info */
    status = uct_gaudi_dma_execute_copy(m_fd, host_ptr, host_ptr, 1024, NULL);
    EXPECT_NE(UCS_OK, status);
    
    free(host_ptr);
}

UCS_TEST_F(test_gaudi_dma, stress_test) {
    const size_t num_iterations = 100;
    const size_t base_size = 1024;
    
    for (size_t i = 0; i < num_iterations; ++i) {
        size_t size = base_size + (i * 64);  /* Varying sizes */
        
        void *host_src = alloc_host_memory(size);
        void *host_dst = alloc_host_memory(size);
        
        if (host_src == NULL || host_dst == NULL) {
            free(host_src);
            free(host_dst);
            continue;
        }
        
        /* Fill with iteration-specific pattern */
        uint8_t pattern = (uint8_t)(i & 0xFF);
        memset(host_src, pattern, size);
        memset(host_dst, ~pattern, size);
        
        /* Execute copy */
        ucs_status_t status = uct_gaudi_dma_execute_copy_auto(host_dst, host_src, size);
        
        if (status == UCS_OK) {
            /* Verify pattern */
            uint8_t *dst_data = (uint8_t*)host_dst;
            bool pattern_ok = true;
            for (size_t j = 0; j < size; ++j) {
                if (dst_data[j] != pattern) {
                    pattern_ok = false;
                    break;
                }
            }
            EXPECT_TRUE(pattern_ok) << "Pattern mismatch in iteration " << i;
        }
        
        free(host_src);
        free(host_dst);
    }
}

/* Test with device memory if available */
UCS_TEST_F(test_gaudi_dma, device_memory_copy) {
    const size_t size = 4096;
    
    void *host_ptr = alloc_host_memory(size);
    void *device_ptr = alloc_device_memory(size);
    
    if (host_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate host memory");
    }
    
    if (device_ptr == NULL) {
        free(host_ptr);
        UCS_TEST_SKIP_R("Failed to allocate device memory");
    }
    
    /* Fill host memory with pattern */
    uint32_t pattern = 0x12345678;
    uint32_t *host_data = (uint32_t*)host_ptr;
    for (size_t i = 0; i < size / sizeof(uint32_t); ++i) {
        host_data[i] = pattern + i;
    }
    
    /* Test host to device copy */
    ucs_status_t status = uct_gaudi_dma_execute_copy_auto(device_ptr, host_ptr, size);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
    
    if (status == UCS_OK) {
        UCS_TEST_MESSAGE << "Successfully executed host-to-device DMA copy";
        
        /* Clear host memory and try device to host copy */
        memset(host_ptr, 0, size);
        
        status = uct_gaudi_dma_execute_copy_auto(host_ptr, device_ptr, size);
        EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
        
        if (status == UCS_OK) {
            /* Verify the round-trip worked */
            for (size_t i = 0; i < size / sizeof(uint32_t); ++i) {
                EXPECT_EQ(pattern + i, host_data[i]) << "Round-trip failed at index " << i;
            }
        }
    }
    
    free(host_ptr);
}
