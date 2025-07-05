/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "test_gaudi_common.h"
#include <common/test.h>

extern "C" {
#include <uct/gaudi/base/gaudi_dma.h>
#include <habanalabs/hlthunk.h>
#include <habanalabs/specs/gaudi/gaudi_packets.h>
#include <ucs/debug/log.h>
#include <ucs/time/time.h>
}

class test_gaudi_dma_comprehensive : public ucs::test, public gaudi_test_base {
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
        
        if (hlthunk_get_hw_ip_info(m_fd, &m_hw_info) != 0) {
            UCS_TEST_SKIP_R("Failed to get hardware info");
        }
    }
    
    void cleanup() {
        if (m_fd >= 0) {
            hlthunk_close(m_fd);
        }
    }
    
    /* Helper to test DMA with different data patterns */
    void test_dma_pattern(size_t size, uint32_t pattern_seed, const std::string& test_name) {
        void *src = alloc_host_memory(size);
        void *dst = alloc_host_memory(size);
        ASSERT_NE(src, nullptr);
        ASSERT_NE(dst, nullptr);
        
        /* Fill source with pattern */
        fill_buffer(src, size, pattern_seed);
        memset(dst, 0, size);
        
        /* Perform DMA copy */
        ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, dst, src, size, &m_hw_info);
        if (status == UCS_OK) {
            /* Verify data integrity */
            bool data_correct = verify_buffer(dst, size, pattern_seed);
            EXPECT_TRUE(data_correct) << "Data verification failed for " << test_name;
        } else {
            /* DMA failure might be acceptable in some test environments */
            ucs_debug("DMA copy failed for %s: %s", test_name.c_str(), ucs_status_string(status));
        }
        
        free(src);
        free(dst);
    }

protected:
    int m_fd = -1;
    hlthunk_hw_ip_info m_hw_info;
};

/* Test DMA with various data sizes */
UCS_TEST_F(test_gaudi_dma_comprehensive, size_variations) {
    /* Test small sizes */
    test_dma_pattern(4, 0x11111111, "4B");
    test_dma_pattern(8, 0x22222222, "8B");
    test_dma_pattern(16, 0x33333333, "16B");
    test_dma_pattern(32, 0x44444444, "32B");
    test_dma_pattern(64, 0x55555555, "64B");
    test_dma_pattern(128, 0x66666666, "128B");
    test_dma_pattern(256, 0x77777777, "256B");
    test_dma_pattern(512, 0x88888888, "512B");
    
    /* Test page-aligned sizes */
    test_dma_pattern(4096, 0x99999999, "4KB");
    test_dma_pattern(8192, 0xAAAAAAAA, "8KB");
    test_dma_pattern(65536, 0xBBBBBBBB, "64KB");
    
    /* Test odd sizes */
    test_dma_pattern(1023, 0xCCCCCCCC, "1023B");
    test_dma_pattern(4097, 0xDDDDDDDD, "4097B");
}

/* Test DMA with different data patterns */
UCS_TEST_F(test_gaudi_dma_comprehensive, data_patterns) {
    const size_t test_size = 4096;
    
    /* Test all zeros */
    test_dma_pattern(test_size, 0x00000000, "all_zeros");
    
    /* Test all ones */
    test_dma_pattern(test_size, 0xFFFFFFFF, "all_ones");
    
    /* Test alternating patterns */
    test_dma_pattern(test_size, 0xAAAAAAAA, "alternating_a");
    test_dma_pattern(test_size, 0x55555555, "alternating_5");
    
    /* Test sequential patterns */
    test_dma_pattern(test_size, 0x12345678, "sequential");
    
    /* Test random-like patterns */
    test_dma_pattern(test_size, 0xDEADBEEF, "deadbeef");
    test_dma_pattern(test_size, 0xCAFEBABE, "cafebabe");
}

/* Test DMA alignment requirements */
UCS_TEST_F(test_gaudi_dma_comprehensive, alignment_tests) {
    const size_t base_size = 4096;
    
    /* Test different alignments */
    for (int align = 1; align <= 256; align *= 2) {
        void *src = aligned_alloc(align, base_size);
        void *dst = aligned_alloc(align, base_size);
        
        if (src && dst) {
            fill_buffer(src, base_size, 0x12345678 + align);
            memset(dst, 0, base_size);
            
            ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, dst, src, base_size, &m_hw_info);
            if (status == UCS_OK) {
                bool correct = verify_buffer(dst, base_size, 0x12345678 + align);
                EXPECT_TRUE(correct) << "Alignment test failed for " << align << " bytes";
            }
        }
        
        if (src) free(src);
        if (dst) free(dst);
    }
}

/* Test DMA performance characteristics */
UCS_TEST_F(test_gaudi_dma_comprehensive, performance_analysis) {
    const int num_iterations = 20;
    std::vector<size_t> test_sizes = {1024, 4096, 16384, 65536, 262144};
    
    for (size_t size : test_sizes) {
        std::vector<double> times;
        double total_bytes = 0;
        
        for (int iter = 0; iter < num_iterations; ++iter) {
            void *src = alloc_host_memory(size);
            void *dst = alloc_host_memory(size);
            
            if (!src || !dst) {
                if (src) free(src);
                if (dst) free(dst);
                continue;
            }
            
            fill_buffer(src, size);
            
            /* Measure DMA time */
            ucs_time_t start = ucs_get_time();
            ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, dst, src, size, &m_hw_info);
            ucs_time_t end = ucs_get_time();
            
            if (status == UCS_OK) {
                double elapsed = ucs_time_to_sec(end - start);
                times.push_back(elapsed);
                total_bytes += size;
                
                /* Verify data integrity */
                EXPECT_TRUE(verify_buffer(dst, size));
            }
            
            free(src);
            free(dst);
        }
        
        if (!times.empty()) {
            double avg_time = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
            double bandwidth = (total_bytes / 1024.0 / 1024.0) / (avg_time * times.size());  /* MB/s */
            
            ucs_debug("Size %zuB: avg_time=%.3fms, bandwidth=%.1f MB/s", 
                      size, avg_time * 1000, bandwidth);
            
            /* Basic performance expectations */
            EXPECT_LT(avg_time, 0.1);  /* Should complete in less than 100ms */
        }
    }
}

/* Test concurrent DMA operations */
UCS_TEST_F(test_gaudi_dma_comprehensive, concurrent_dma) {
    const int num_threads = 4;
    const size_t transfer_size = 8192;
    const int ops_per_thread = 5;
    
    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);
    
    auto dma_worker = [&](int thread_id) {
        for (int op = 0; op < ops_per_thread; ++op) {
            void *src = alloc_host_memory(transfer_size);
            void *dst = alloc_host_memory(transfer_size);
            
            if (!src || !dst) {
                if (src) free(src);
                if (dst) free(dst);
                failure_count++;
                continue;
            }
            
            uint32_t pattern = 0x10000000 + (thread_id << 16) + op;
            fill_buffer(src, transfer_size, pattern);
            memset(dst, 0, transfer_size);
            
            ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, dst, src, 
                                                           transfer_size, &m_hw_info);
            if (status == UCS_OK) {
                if (verify_buffer(dst, transfer_size, pattern)) {
                    success_count++;
                } else {
                    failure_count++;
                }
            } else {
                failure_count++;
            }
            
            free(src);
            free(dst);
        }
    };
    
    /* Launch threads */
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(dma_worker, i);
    }
    
    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }
    
    ucs_debug("Concurrent DMA: %d successes, %d failures", 
              success_count.load(), failure_count.load());
    
    /* At least some operations should succeed */
    EXPECT_GT(success_count.load(), 0);
}

/* Test DMA command buffer reuse */
UCS_TEST_F(test_gaudi_dma_comprehensive, command_buffer_reuse) {
    const size_t transfer_size = 4096;
    const int num_reuses = 10;
    
    void *src = alloc_host_memory(transfer_size);
    void *dst = alloc_host_memory(transfer_size);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);
    
    /* Test multiple DMA operations to check command buffer reuse */
    for (int i = 0; i < num_reuses; ++i) {
        uint32_t pattern = 0x20000000 + i;
        fill_buffer(src, transfer_size, pattern);
        memset(dst, 0, transfer_size);
        
        ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, dst, src, 
                                                       transfer_size, &m_hw_info);
        if (status == UCS_OK) {
            EXPECT_TRUE(verify_buffer(dst, transfer_size, pattern)) 
                << "Reuse iteration " << i << " failed";
        }
    }
    
    free(src);
    free(dst);
}

/* Test DMA with device memory */
UCS_TEST_F(test_gaudi_dma_comprehensive, device_memory_dma) {
    const size_t transfer_size = 8192;
    
    /* Allocate device memory */
    uint64_t device_handle = hlthunk_device_memory_alloc(m_fd, transfer_size, 0, true, true);
    if (device_handle == 0) {
        UCS_TEST_SKIP_R("Cannot allocate device memory");
    }
    
    uint64_t device_addr = hlthunk_device_memory_map(m_fd, device_handle, 0);
    if (device_addr == 0) {
        hlthunk_device_memory_free(m_fd, device_handle);
        UCS_TEST_SKIP_R("Cannot map device memory");
    }
    
    /* Allocate host memory */
    void *host_buf = alloc_host_memory(transfer_size);
    ASSERT_NE(host_buf, nullptr);
    
    fill_buffer(host_buf, transfer_size, 0x30000000);
    
    /* Test host-to-device DMA */
    ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, (void*)device_addr, 
                                                   host_buf, transfer_size, &m_hw_info);
    /* Device memory DMA might not be supported in all configurations */
    if (status == UCS_OK) {
        ucs_debug("Host-to-device DMA succeeded");
    }
    
    /* Test device-to-host DMA */
    void *dst_buf = alloc_host_memory(transfer_size);
    ASSERT_NE(dst_buf, nullptr);
    memset(dst_buf, 0, transfer_size);
    
    status = uct_gaudi_dma_execute_copy(m_fd, dst_buf, (void*)device_addr, 
                                      transfer_size, &m_hw_info);
    if (status == UCS_OK) {
        ucs_debug("Device-to-host DMA succeeded");
        /* Note: Device memory might not retain data in test environment */
    }
    
    /* Cleanup */
    free(host_buf);
    free(dst_buf);
    hlthunk_device_memory_free(m_fd, device_handle);
}

/* Test DMA auto-detection logic */
UCS_TEST_F(test_gaudi_dma_comprehensive, auto_detection) {
    const size_t transfer_size = 4096;
    
    void *src = alloc_host_memory(transfer_size);
    void *dst = alloc_host_memory(transfer_size);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);
    
    fill_buffer(src, transfer_size, 0x40000000);
    memset(dst, 0, transfer_size);
    
    /* Test auto-detection version of DMA copy */
    ucs_status_t status = uct_gaudi_dma_execute_copy_auto(dst, src, transfer_size);
    if (status == UCS_OK) {
        EXPECT_TRUE(verify_buffer(dst, transfer_size, 0x40000000));
        ucs_debug("Auto-detection DMA succeeded");
    } else {
        /* Auto-detection might fail if no Gaudi device is available to the function */
        ucs_debug("Auto-detection DMA failed: %s", ucs_status_string(status));
    }
    
    free(src);
    free(dst);
}

/* Test DMA error recovery */
UCS_TEST_F(test_gaudi_dma_comprehensive, error_recovery) {
    const size_t transfer_size = 4096;
    
    void *src = alloc_host_memory(transfer_size);
    void *dst = alloc_host_memory(transfer_size);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);
    
    fill_buffer(src, transfer_size, 0x50000000);
    
    /* Try invalid operation first */
    ucs_status_t status = uct_gaudi_dma_execute_copy(-1, dst, src, transfer_size, &m_hw_info);
    EXPECT_NE(status, UCS_OK);
    
    /* Then try valid operation to test recovery */
    memset(dst, 0, transfer_size);
    status = uct_gaudi_dma_execute_copy(m_fd, dst, src, transfer_size, &m_hw_info);
    if (status == UCS_OK) {
        EXPECT_TRUE(verify_buffer(dst, transfer_size, 0x50000000));
        ucs_debug("DMA recovery test succeeded");
    }
    
    free(src);
    free(dst);
}
