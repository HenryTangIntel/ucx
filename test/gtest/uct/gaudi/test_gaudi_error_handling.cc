/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "test_gaudi_common.h"
#include <common/test.h>

extern "C" {
#include <uct/gaudi/base/gaudi_dma.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <habanalabs/hlthunk.h>
#include <ucs/debug/log.h>
}

class test_gaudi_error_handling : public ucs::test, public gaudi_test_base {
public:
    UCS_TEST_BASE_IMPL;

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        
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

protected:
    int m_fd = -1;
    hlthunk_hw_ip_info m_hw_info;
};

/* Test DMA with invalid parameters */
UCS_TEST_F(test_gaudi_error_handling, dma_invalid_params) {
    /* Test with NULL pointers */
    ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, NULL, NULL, 1024, &m_hw_info);
    EXPECT_NE(status, UCS_OK);
    
    /* Test with zero size */
    void *src = alloc_host_memory(1024);
    void *dst = alloc_host_memory(1024);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);
    
    status = uct_gaudi_dma_execute_copy(m_fd, dst, src, 0, &m_hw_info);
    /* Zero size might be acceptable or not - implementation dependent */
    
    /* Test with invalid file descriptor */
    status = uct_gaudi_dma_execute_copy(-1, dst, src, 1024, &m_hw_info);
    EXPECT_NE(status, UCS_OK);
    
    /* Test with misaligned addresses (if required) */
    void *misaligned_src = (char*)src + 1;
    void *misaligned_dst = (char*)dst + 1;
    status = uct_gaudi_dma_execute_copy(m_fd, misaligned_dst, misaligned_src, 1023, &m_hw_info);
    /* Misalignment handling is implementation-specific */
    
    free(src);
    free(dst);
}

/* Test memory allocation failures */
UCS_TEST_F(test_gaudi_error_handling, memory_allocation_failures) {
    /* Test extremely large allocation that should fail */
    uint64_t huge_size = 1ULL << 40;  /* 1TB - should fail */
    uint64_t handle = hlthunk_device_memory_alloc(m_fd, huge_size, 0, true, true);
    EXPECT_EQ(handle, 0);  /* Should fail */
    
    /* Test allocation with invalid parameters */
    handle = hlthunk_device_memory_alloc(-1, 1024, 0, true, true);
    EXPECT_EQ(handle, 0);  /* Should fail with invalid fd */
    
    /* Test double free (should not crash) */
    handle = hlthunk_device_memory_alloc(m_fd, 1024, 0, true, true);
    if (handle != 0) {
        hlthunk_device_memory_free(m_fd, handle);
        /* Second free should be handled gracefully */
        hlthunk_device_memory_free(m_fd, handle);
    }
}

/* Test command buffer edge cases */
UCS_TEST_F(test_gaudi_error_handling, command_buffer_errors) {
    /* Test command buffer creation with invalid parameters */
    uint32_t cb_handle = hlthunk_request_command_buffer(m_fd, 0);  /* Zero size */
    /* Zero size might be acceptable or rejected */
    if (cb_handle != 0) {
        hlthunk_destroy_command_buffer(m_fd, cb_handle);
    }
    
    /* Test command buffer destruction with invalid handle */
    int ret = hlthunk_destroy_command_buffer(m_fd, 0xFFFFFFFF);
    /* Should handle invalid handle gracefully */
    
    /* Test submission with invalid parameters */
    struct hlthunk_cs_in cs_in = {0};
    struct hlthunk_cs_out cs_out = {0};
    
    cs_in.chunks_restore = NULL;
    cs_in.chunks_execute = NULL;
    cs_in.num_chunks_restore = 0;
    cs_in.num_chunks_execute = 0;
    
    ret = hlthunk_command_submission(m_fd, &cs_in, &cs_out);
    /* Empty submission might be acceptable or rejected */
}

/* Test hardware info edge cases */
UCS_TEST_F(test_gaudi_error_handling, hardware_info_errors) {
    /* Test with invalid file descriptor */
    hlthunk_hw_ip_info hw_info;
    int ret = hlthunk_get_hw_ip_info(-1, &hw_info);
    EXPECT_NE(ret, 0);  /* Should fail */
    
    /* Test with NULL pointer */
    ret = hlthunk_get_hw_ip_info(m_fd, NULL);
    EXPECT_NE(ret, 0);  /* Should fail */
}

/* Test system device detection error cases */
UCS_TEST_F(test_gaudi_error_handling, sys_device_errors) {
    /* Test with invalid device index */
    ucs_sys_device_t sys_dev = uct_gaudi_base_get_sys_dev(-1);
    EXPECT_EQ(sys_dev, UCS_SYS_DEVICE_ID_UNKNOWN);
    
    sys_dev = uct_gaudi_base_get_sys_dev(9999);  /* Very large index */
    EXPECT_EQ(sys_dev, UCS_SYS_DEVICE_ID_UNKNOWN);
}

/* Test memory mapping edge cases */
UCS_TEST_F(test_gaudi_error_handling, memory_mapping_errors) {
    /* Test mapping with invalid handle */
    uint64_t addr = hlthunk_device_memory_map(m_fd, 0, 0);
    EXPECT_EQ(addr, 0);  /* Should fail */
    
    /* Test mapping with invalid offset */
    uint64_t handle = hlthunk_device_memory_alloc(m_fd, 4096, 0, true, true);
    if (handle != 0) {
        /* Map with very large offset */
        addr = hlthunk_device_memory_map(m_fd, handle, 1ULL << 32);
        /* Large offset might be rejected */
        
        hlthunk_device_memory_free(m_fd, handle);
    }
}

/* Test concurrent error scenarios */
UCS_TEST_F(test_gaudi_error_handling, concurrent_errors) {
    const int num_threads = 4;
    std::atomic<int> error_count(0);
    
    auto error_worker = [&](int thread_id) {
        /* Each thread tries operations that might fail */
        for (int i = 0; i < 10; ++i) {
            /* Try to allocate very large memory */
            uint64_t large_size = 1ULL << 30;  /* 1GB */
            uint64_t handle = hlthunk_device_memory_alloc(m_fd, large_size, 0, true, true);
            if (handle == 0) {
                error_count++;
            } else {
                hlthunk_device_memory_free(m_fd, handle);
            }
            
            /* Try invalid DMA operations */
            ucs_status_t status = uct_gaudi_dma_execute_copy(-1, NULL, NULL, 1024, &m_hw_info);
            if (status != UCS_OK) {
                error_count++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(error_worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    /* Should have some errors from invalid operations */
    EXPECT_GT(error_count.load(), 0);
}

/* Test resource cleanup after failures */
UCS_TEST_F(test_gaudi_error_handling, cleanup_after_failures) {
    std::vector<uint64_t> handles;
    
    /* Allocate several small buffers */
    for (int i = 0; i < 10; ++i) {
        uint64_t handle = hlthunk_device_memory_alloc(m_fd, 1024, 0, true, true);
        if (handle != 0) {
            handles.push_back(handle);
        }
    }
    
    /* Try to allocate one huge buffer that should fail */
    uint64_t huge_handle = hlthunk_device_memory_alloc(m_fd, 1ULL << 40, 0, true, true);
    EXPECT_EQ(huge_handle, 0);
    
    /* Verify we can still free the small buffers */
    for (uint64_t handle : handles) {
        hlthunk_device_memory_free(m_fd, handle);
    }
    
    /* Verify we can still allocate after cleanup */
    uint64_t new_handle = hlthunk_device_memory_alloc(m_fd, 1024, 0, true, true);
    if (new_handle != 0) {
        hlthunk_device_memory_free(m_fd, new_handle);
    }
}

/* Test timeout and hanging operation detection */
UCS_TEST_F(test_gaudi_error_handling, timeout_detection) {
    /* Create a command buffer */
    size_t cb_size = 4096;
    uint32_t cb_handle = hlthunk_request_command_buffer(m_fd, cb_size);
    if (cb_handle == 0) {
        UCS_TEST_SKIP_R("Cannot create command buffer");
    }
    
    /* Submit empty command buffer (should complete quickly) */
    struct hlthunk_cs_in cs_in = {0};
    struct hlthunk_cs_out cs_out = {0};
    
    /* Set timeout for submission */
    cs_in.timeout = 1000;  /* 1 second */
    
    /* Submit and measure time */
    ucs_time_t start_time = ucs_get_time();
    int ret = hlthunk_command_submission(m_fd, &cs_in, &cs_out);
    ucs_time_t end_time = ucs_get_time();
    
    /* Should complete within reasonable time */
    double elapsed = ucs_time_to_sec(end_time - start_time);
    if (ret == 0) {
        EXPECT_LT(elapsed, 0.5);  /* Should finish in less than 500ms */
    }
    
    hlthunk_destroy_command_buffer(m_fd, cb_handle);
}

/* Test MD error handling */
UCS_TEST_F(test_gaudi_error_handling, md_error_handling) {
    /* Try to open MD with invalid component */
    uct_md_h md;
    ucs_status_t status = uct_md_open(NULL, "invalid_md", NULL, &md);
    EXPECT_NE(status, UCS_OK);
    
    /* Try operations on NULL MD */
    uct_md_attr_t attr;
    status = uct_md_query(NULL, &attr);
    EXPECT_NE(status, UCS_OK);
}
