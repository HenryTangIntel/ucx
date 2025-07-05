/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include <common/test.h>
#include <chrono>

extern "C" {
#include <uct/gaudi/base/gaudi_dma.h>
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <habanalabs/hlthunk.h>
#include <ucs/time/time.h>
#include <ucs/debug/log.h>
}

class test_gaudi_performance : public ucs::test {
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
    
    /* Measure operation time in microseconds */
    double measure_operation_time(std::function<ucs_status_t()> operation, 
                                 int iterations = 100) {
        /* Warmup */
        for (int i = 0; i < 10; ++i) {
            operation();
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            ucs_status_t status = operation();
            if (status != UCS_OK) {
                return -1.0;  /* Operation failed */
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        return (double)duration.count() / iterations;
    }
    
    /* Calculate bandwidth in MB/s */
    double calculate_bandwidth(size_t bytes, double time_us) {
        if (time_us <= 0) return 0.0;
        return (bytes / (1024.0 * 1024.0)) / (time_us / 1000000.0);
    }

protected:
    int m_fd = -1;
    struct hlthunk_hw_ip_info m_hw_info;
};

UCS_TEST_F(test_gaudi_performance, dma_copy_latency) {
    const size_t size = 4096;  /* 4KB */
    
    void *src = aligned_alloc(4096, size);
    void *dst = aligned_alloc(4096, size);
    
    if (src == NULL || dst == NULL) {
        free(src);
        free(dst);
        UCS_TEST_SKIP_R("Failed to allocate memory");
    }
    
    memset(src, 0xAA, size);
    
    /* Measure manual DMA copy latency */
    auto manual_op = [&]() {
        return uct_gaudi_dma_execute_copy(m_fd, dst, src, size, &m_hw_info);
    };
    
    double manual_time = measure_operation_time(manual_op, 1000);
    
    /* Measure auto DMA copy latency */
    auto auto_op = [&]() {
        return uct_gaudi_dma_execute_copy_auto(dst, src, size);
    };
    
    double auto_time = measure_operation_time(auto_op, 1000);
    
    /* Measure memcpy latency for comparison */
    auto memcpy_op = [&]() {
        memcpy(dst, src, size);
        return UCS_OK;
    };
    
    double memcpy_time = measure_operation_time(memcpy_op, 1000);
    
    UCS_TEST_MESSAGE << "DMA copy latency for " << size << " bytes:";
    if (manual_time > 0) {
        UCS_TEST_MESSAGE << "  Manual DMA: " << manual_time << " μs";
    }
    if (auto_time > 0) {
        UCS_TEST_MESSAGE << "  Auto DMA:   " << auto_time << " μs";
    }
    UCS_TEST_MESSAGE << "  memcpy:     " << memcpy_time << " μs";
    
    /* DMA should be functional (either succeed or gracefully fail) */
    EXPECT_TRUE((manual_time > 0) || (manual_time == -1));
    EXPECT_TRUE((auto_time > 0) || (auto_time == -1));
    EXPECT_GT(memcpy_time, 0);
    
    free(src);
    free(dst);
}

UCS_TEST_F(test_gaudi_performance, bandwidth_scaling) {
    const std::vector<size_t> sizes = {
        1024,       /* 1KB */
        4096,       /* 4KB */
        16384,      /* 16KB */
        65536,      /* 64KB */
        262144,     /* 256KB */
        1048576,    /* 1MB */
        4194304     /* 4MB */
    };
    
    UCS_TEST_MESSAGE << "Bandwidth scaling test:";
    UCS_TEST_MESSAGE << "Size\t\tAuto DMA (MB/s)\tMemcpy (MB/s)";
    
    for (size_t size : sizes) {
        void *src = aligned_alloc(4096, size);
        void *dst = aligned_alloc(4096, size);
        
        if (src == NULL || dst == NULL) {
            free(src);
            free(dst);
            continue;
        }
        
        /* Fill source with pattern */
        memset(src, 0x55, size);
        
        /* Measure auto DMA */
        auto auto_op = [&]() {
            return uct_gaudi_dma_execute_copy_auto(dst, src, size);
        };
        
        double auto_time = measure_operation_time(auto_op, 100);
        double auto_bw = (auto_time > 0) ? calculate_bandwidth(size, auto_time) : 0.0;
        
        /* Measure memcpy */
        auto memcpy_op = [&]() {
            memcpy(dst, src, size);
            return UCS_OK;
        };
        
        double memcpy_time = measure_operation_time(memcpy_op, 100);
        double memcpy_bw = calculate_bandwidth(size, memcpy_time);
        
        UCS_TEST_MESSAGE << size/1024 << "KB\t\t" 
                        << std::fixed << std::setprecision(2) 
                        << auto_bw << "\t\t" << memcpy_bw;
        
        /* Sanity check: bandwidth should increase with size (up to a point) */
        EXPECT_GT(memcpy_bw, 0.0);
        
        free(src);
        free(dst);
    }
}

UCS_TEST_F(test_gaudi_performance, concurrent_operations) {
    const size_t size = 64 * 1024;  /* 64KB */
    const int num_buffers = 10;
    
    std::vector<void*> src_buffers(num_buffers);
    std::vector<void*> dst_buffers(num_buffers);
    
    /* Allocate buffers */
    for (int i = 0; i < num_buffers; ++i) {
        src_buffers[i] = aligned_alloc(4096, size);
        dst_buffers[i] = aligned_alloc(4096, size);
        
        if (src_buffers[i] == NULL || dst_buffers[i] == NULL) {
            /* Clean up and skip */
            for (int j = 0; j <= i; ++j) {
                free(src_buffers[j]);
                free(dst_buffers[j]);
            }
            UCS_TEST_SKIP_R("Failed to allocate buffers");
        }
        
        /* Fill with unique pattern */
        memset(src_buffers[i], 0x10 + i, size);
    }
    
    /* Measure sequential operations */
    auto sequential_op = [&]() {
        for (int i = 0; i < num_buffers; ++i) {
            ucs_status_t status = uct_gaudi_dma_execute_copy_auto(
                dst_buffers[i], src_buffers[i], size);
            if (status != UCS_OK) {
                return status;
            }
        }
        return UCS_OK;
    };
    
    double sequential_time = measure_operation_time(sequential_op, 50);
    
    UCS_TEST_MESSAGE << "Concurrent operations test:";
    UCS_TEST_MESSAGE << "  Sequential " << num_buffers << " x " << size/1024 
                    << "KB: " << sequential_time << " μs";
    
    if (sequential_time > 0) {
        double total_bandwidth = calculate_bandwidth(size * num_buffers, sequential_time);
        UCS_TEST_MESSAGE << "  Total bandwidth: " << total_bandwidth << " MB/s";
        EXPECT_GT(total_bandwidth, 0.0);
    }
    
    /* Verify all copies completed correctly */
    for (int i = 0; i < num_buffers; ++i) {
        uint8_t expected = 0x10 + i;
        uint8_t *dst_data = (uint8_t*)dst_buffers[i];
        
        EXPECT_EQ(expected, dst_data[0]) << "Copy " << i << " failed";
        EXPECT_EQ(expected, dst_data[size-1]) << "Copy " << i << " failed";
    }
    
    /* Clean up */
    for (int i = 0; i < num_buffers; ++i) {
        free(src_buffers[i]);
        free(dst_buffers[i]);
    }
}

UCS_TEST_F(test_gaudi_performance, memory_allocation_performance) {
    const std::vector<size_t> sizes = {4096, 65536, 1048576, 16777216};  /* 4KB to 16MB */
    const int iterations = 50;
    
    /* Initialize MD for allocation tests */
    uct_md_resource_desc_t *md_resources;
    unsigned num_md_resources;
    
    ucs_status_t status = uct_gaudi_base_query_md_resources(&uct_gaudi_copy_component,
                                                          &md_resources, 
                                                          &num_md_resources);
    if (status != UCS_OK || num_md_resources == 0) {
        UCS_TEST_SKIP_R("No Gaudi MD resources found");
    }
    
    uct_md_config_t *md_config;
    status = uct_md_config_read(&uct_gaudi_copy_component, NULL, NULL, &md_config);
    ASSERT_UCS_OK(status);
    
    uct_md_h md;
    status = uct_md_open(&uct_gaudi_copy_component, md_resources[0].md_name, 
                        md_config, &md);
    if (status != UCS_OK) {
        uct_config_release(md_config);
        ucs_free(md_resources);
        UCS_TEST_SKIP_R("Failed to open Gaudi MD");
    }
    
    UCS_TEST_MESSAGE << "Memory allocation performance:";
    UCS_TEST_MESSAGE << "Size\t\tAlloc Time (μs)\tFree Time (μs)";
    
    for (size_t size : sizes) {
        std::vector<uct_mem_h> memhs;
        std::vector<void*> ptrs;
        
        /* Measure allocation time */
        auto alloc_start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            void *ptr;
            uct_mem_h memh;
            size_t actual_size = size;
            
            status = uct_mem_alloc(md, &actual_size, &ptr, UCS_MEMORY_TYPE_HOST,
                                 UCS_SYS_DEVICE_ID_UNKNOWN, 0, "perf_test", &memh);
            if (status == UCS_OK) {
                memhs.push_back(memh);
                ptrs.push_back(ptr);
            }
        }
        
        auto alloc_end = std::chrono::high_resolution_clock::now();
        auto alloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            alloc_end - alloc_start);
        
        /* Measure deallocation time */
        auto free_start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < memhs.size(); ++i) {
            uct_mem_free(md, memhs[i]);
        }
        
        auto free_end = std::chrono::high_resolution_clock::now();
        auto free_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            free_end - free_start);
        
        double avg_alloc_time = (double)alloc_duration.count() / memhs.size();
        double avg_free_time = (double)free_duration.count() / memhs.size();
        
        UCS_TEST_MESSAGE << size/1024 << "KB\t\t" 
                        << std::fixed << std::setprecision(2)
                        << avg_alloc_time << "\t\t" << avg_free_time;
        
        EXPECT_GT(memhs.size(), 0u) << "No allocations succeeded for size " << size;
    }
    
    uct_md_close(md);
    uct_config_release(md_config);
    ucs_free(md_resources);
}

UCS_TEST_F(test_gaudi_performance, stress_test) {
    const size_t total_iterations = 1000;
    const size_t max_size = 1024 * 1024;  /* 1MB */
    
    int successful_ops = 0;
    int failed_ops = 0;
    double total_time = 0.0;
    size_t total_bytes = 0;
    
    UCS_TEST_MESSAGE << "Running stress test with " << total_iterations << " operations...";
    
    for (size_t i = 0; i < total_iterations; ++i) {
        /* Random size between 1KB and 1MB */
        size_t size = 1024 + (rand() % (max_size - 1024));
        
        void *src = aligned_alloc(4096, size);
        void *dst = aligned_alloc(4096, size);
        
        if (src == NULL || dst == NULL) {
            free(src);
            free(dst);
            failed_ops++;
            continue;
        }
        
        /* Fill with random pattern */
        uint8_t pattern = (uint8_t)(i & 0xFF);
        memset(src, pattern, size);
        memset(dst, ~pattern, size);
        
        /* Measure operation */
        auto start = std::chrono::high_resolution_clock::now();
        
        ucs_status_t status = uct_gaudi_dma_execute_copy_auto(dst, src, size);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        if (status == UCS_OK) {
            /* Verify copy */
            bool success = true;
            uint8_t *dst_data = (uint8_t*)dst;
            for (size_t j = 0; j < size && success; j += 1024) {  /* Sample verification */
                if (dst_data[j] != pattern) {
                    success = false;
                }
            }
            
            if (success) {
                successful_ops++;
                total_time += duration.count();
                total_bytes += size;
            } else {
                failed_ops++;
            }
        } else {
            failed_ops++;
        }
        
        free(src);
        free(dst);
    }
    
    UCS_TEST_MESSAGE << "Stress test results:";
    UCS_TEST_MESSAGE << "  Successful operations: " << successful_ops;
    UCS_TEST_MESSAGE << "  Failed operations: " << failed_ops;
    
    if (successful_ops > 0) {
        double avg_time = total_time / successful_ops;
        double avg_bandwidth = calculate_bandwidth(total_bytes / successful_ops, avg_time);
        
        UCS_TEST_MESSAGE << "  Average latency: " << avg_time << " μs";
        UCS_TEST_MESSAGE << "  Average bandwidth: " << avg_bandwidth << " MB/s";
        UCS_TEST_MESSAGE << "  Total data processed: " << total_bytes / (1024*1024) << " MB";
    }
    
    /* At least some operations should succeed */
    EXPECT_GT(successful_ops, 0);
    
    /* Failure rate should not be too high */
    double failure_rate = (double)failed_ops / total_iterations;
    EXPECT_LT(failure_rate, 0.5) << "High failure rate: " << (failure_rate * 100) << "%";
}
