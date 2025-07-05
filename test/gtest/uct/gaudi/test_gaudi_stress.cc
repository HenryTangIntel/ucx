/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "test_gaudi_common.h"
#include <uct/test_md.h>

extern "C" {
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <uct/gaudi/base/gaudi_dma.h>
#include <habanalabs/hlthunk.h>
#include <ucs/sys/sys.h>
}

class test_gaudi_stress : public test_md, public gaudi_test_base {
public:
    UCS_TEST_BASE_IMPL;

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        
        /* Initialize MD */
        uct_md_resource_desc_t *md_resources;
        unsigned num_md_resources;
        
        ucs_status_t status = uct_gaudi_base_query_md_resources(&uct_gaudi_copy_component,
                                                              &md_resources, 
                                                              &num_md_resources);
        ASSERT_UCS_OK(status);
        
        if (num_md_resources == 0) {
            UCS_TEST_SKIP_R("No Gaudi MD resources found");
        }
        
        uct_md_config_t *md_config;
        status = uct_md_config_read(&uct_gaudi_copy_component, NULL, NULL, &md_config);
        ASSERT_UCS_OK(status);
        
        status = uct_md_open(&uct_gaudi_copy_component, md_resources[0].md_name, 
                           md_config, &m_md);
        ASSERT_UCS_OK(status);
        
        uct_config_release(md_config);
        ucs_free(md_resources);
        
        /* Open hlthunk device */
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
        if (m_md != NULL) {
            uct_md_close(m_md);
        }
    }

protected:
    int m_fd = -1;
    hlthunk_hw_ip_info m_hw_info;
};

/* Stress test: Heavy memory allocation/deallocation */
UCS_TEST_F(test_gaudi_stress, memory_allocation_stress) {
    const int num_iterations = 1000;
    const size_t max_alloc_size = 1024 * 1024;  /* 1MB */
    
    std::vector<std::pair<void*, uct_mem_h>> allocations;
    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);
    
    ucs_debug("Starting memory allocation stress test with %d iterations", num_iterations);
    
    for (int i = 0; i < num_iterations; ++i) {
        /* Random allocation size */
        size_t alloc_size = 1024 + (rand() % (max_alloc_size - 1024));
        
        void *ptr;
        uct_mem_h memh;
        size_t actual_size = alloc_size;
        
        ucs_status_t status = uct_mem_alloc(m_md, &actual_size, &ptr, 
                                          UCS_MEMORY_TYPE_GAUDI,
                                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                          "stress_test", &memh);
        
        if (status == UCS_OK) {
            allocations.push_back(std::make_pair(ptr, memh));
            success_count++;
            
            /* Randomly free some allocations */
            if ((i % 10 == 0) && !allocations.empty()) {
                int free_count = std::min(5, (int)allocations.size());
                for (int j = 0; j < free_count; ++j) {
                    int idx = rand() % allocations.size();
                    uct_mem_free(allocations[idx].second);
                    allocations.erase(allocations.begin() + idx);
                }
            }
        } else {
            failure_count++;
        }
        
        /* Progress indicator */
        if (i % 100 == 0) {
            ucs_debug("Stress test progress: %d/%d (success: %d, failure: %d)", 
                      i, num_iterations, success_count.load(), failure_count.load());
        }
    }
    
    /* Free remaining allocations */
    for (auto& alloc : allocations) {
        uct_mem_free(alloc.second);
    }
    
    ucs_debug("Memory stress test completed: %d successes, %d failures", 
              success_count.load(), failure_count.load());
    
    /* Should have reasonable success rate */
    EXPECT_GT(success_count.load(), num_iterations / 4);
}

/* Stress test: Concurrent DMA operations */
UCS_TEST_F(test_gaudi_stress, concurrent_dma_stress) {
    const int num_threads = 8;
    const int ops_per_thread = 100;
    const size_t transfer_size = 4096;
    
    std::atomic<int> total_ops(0);
    std::atomic<int> successful_ops(0);
    std::atomic<int> failed_ops(0);
    
    auto dma_worker = [&](int thread_id) {
        for (int op = 0; op < ops_per_thread; ++op) {
            total_ops++;
            
            void *src = alloc_host_memory(transfer_size);
            void *dst = alloc_host_memory(transfer_size);
            
            if (!src || !dst) {
                if (src) free(src);
                if (dst) free(dst);
                failed_ops++;
                continue;
            }
            
            /* Fill with thread-specific pattern */
            uint32_t pattern = 0x80000000 + (thread_id << 16) + op;
            fill_buffer(src, transfer_size, pattern);
            memset(dst, 0, transfer_size);
            
            /* Perform DMA */
            ucs_status_t status = uct_gaudi_dma_execute_copy(m_fd, dst, src, 
                                                           transfer_size, &m_hw_info);
            
            if (status == UCS_OK) {
                if (verify_buffer(dst, transfer_size, pattern)) {
                    successful_ops++;
                } else {
                    failed_ops++;
                }
            } else {
                failed_ops++;
            }
            
            free(src);
            free(dst);
            
            /* Small delay to avoid overwhelming the system */
            usleep(100);  /* 100μs */
        }
    };
    
    ucs_debug("Starting concurrent DMA stress test: %d threads × %d ops", 
              num_threads, ops_per_thread);
    
    /* Launch threads */
    std::vector<std::thread> threads;
    ucs_time_t start_time = ucs_get_time();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(dma_worker, i);
    }
    
    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }
    
    ucs_time_t end_time = ucs_get_time();
    double elapsed = ucs_time_to_sec(end_time - start_time);
    
    ucs_debug("Concurrent DMA stress test completed in %.2fs", elapsed);
    ucs_debug("Total ops: %d, Successful: %d, Failed: %d", 
              total_ops.load(), successful_ops.load(), failed_ops.load());
    
    /* Should have reasonable success rate */
    EXPECT_GT(successful_ops.load(), total_ops.load() / 4);
}

/* Stress test: Memory handle tracking under pressure */
UCS_TEST_F(test_gaudi_stress, handle_tracking_stress) {
    const int num_threads = 4;
    const int ops_per_thread = 200;
    const size_t alloc_size = 8192;
    
    std::atomic<int> allocations_made(0);
    std::atomic<int> deallocations_made(0);
    std::atomic<int> errors_encountered(0);
    
    /* Shared data structure for cross-thread handle sharing */
    std::mutex handles_mutex;
    std::vector<std::pair<void*, uct_mem_h>> shared_handles;
    
    auto handle_worker = [&](int thread_id) {
        std::vector<std::pair<void*, uct_mem_h>> local_handles;
        
        for (int op = 0; op < ops_per_thread; ++op) {
            int action = rand() % 3;  /* 0=alloc, 1=free_local, 2=free_shared */
            
            if (action == 0 || local_handles.empty()) {
                /* Allocate new memory */
                void *ptr;
                uct_mem_h memh;
                size_t size = alloc_size;
                
                ucs_status_t status = uct_mem_alloc(m_md, &size, &ptr, 
                                                  UCS_MEMORY_TYPE_GAUDI,
                                                  UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                                  "handle_stress", &memh);
                
                if (status == UCS_OK) {
                    local_handles.push_back(std::make_pair(ptr, memh));
                    allocations_made++;
                    
                    /* Sometimes share handle with other threads */
                    if (rand() % 5 == 0) {
                        std::lock_guard<std::mutex> lock(handles_mutex);
                        shared_handles.push_back(std::make_pair(ptr, memh));
                    }
                } else {
                    errors_encountered++;
                }
                
            } else if (action == 1 && !local_handles.empty()) {
                /* Free local handle */
                int idx = rand() % local_handles.size();
                uct_mem_free(local_handles[idx].second);
                local_handles.erase(local_handles.begin() + idx);
                deallocations_made++;
                
            } else if (action == 2) {
                /* Try to free shared handle */
                std::lock_guard<std::mutex> lock(handles_mutex);
                if (!shared_handles.empty()) {
                    int idx = rand() % shared_handles.size();
                    uct_mem_free(shared_handles[idx].second);
                    shared_handles.erase(shared_handles.begin() + idx);
                    deallocations_made++;
                }
            }
            
            /* Small delay */
            usleep(50);  /* 50μs */
        }
        
        /* Clean up local handles */
        for (auto& handle : local_handles) {
            uct_mem_free(handle.second);
            deallocations_made++;
        }
    };
    
    ucs_debug("Starting handle tracking stress test: %d threads × %d ops", 
              num_threads, ops_per_thread);
    
    /* Launch threads */
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(handle_worker, i);
    }
    
    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }
    
    /* Clean up remaining shared handles */
    for (auto& handle : shared_handles) {
        uct_mem_free(handle.second);
        deallocations_made++;
    }
    
    ucs_debug("Handle tracking stress test completed");
    ucs_debug("Allocations: %d, Deallocations: %d, Errors: %d", 
              allocations_made.load(), deallocations_made.load(), errors_encountered.load());
    
    /* Basic sanity checks */
    EXPECT_GT(allocations_made.load(), 0);
    EXPECT_GT(deallocations_made.load(), 0);
}

/* Stress test: Rapid interface creation/destruction */
UCS_TEST_F(test_gaudi_stress, interface_lifecycle_stress) {
    const int num_cycles = 50;
    
    std::atomic<int> successful_cycles(0);
    std::atomic<int> failed_cycles(0);
    
    ucs_debug("Starting interface lifecycle stress test: %d cycles", num_cycles);
    
    for (int cycle = 0; cycle < num_cycles; ++cycle) {
        bool cycle_success = true;
        
        try {
            /* Create interface config */
            uct_md_config_t *md_config;
            ucs_status_t status = uct_md_config_read(&uct_gaudi_copy_component, NULL, NULL, &md_config);
            if (status != UCS_OK) {
                cycle_success = false;
                continue;
            }
            
            /* Open MD */
            uct_md_h test_md;
            uct_md_resource_desc_t *md_resources;
            unsigned num_md_resources;
            
            status = uct_gaudi_base_query_md_resources(&uct_gaudi_copy_component,
                                                     &md_resources, &num_md_resources);
            if (status != UCS_OK || num_md_resources == 0) {
                uct_config_release(md_config);
                cycle_success = false;
                continue;
            }
            
            status = uct_md_open(&uct_gaudi_copy_component, md_resources[0].md_name, 
                               md_config, &test_md);
            if (status != UCS_OK) {
                ucs_free(md_resources);
                uct_config_release(md_config);
                cycle_success = false;
                continue;
            }
            
            /* Query MD capabilities */
            uct_md_attr_t md_attr;
            status = uct_md_query(test_md, &md_attr);
            if (status != UCS_OK) {
                cycle_success = false;
            }
            
            /* Test a small allocation */
            void *ptr;
            uct_mem_h memh;
            size_t size = 1024;
            status = uct_mem_alloc(test_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                                 UCS_SYS_DEVICE_ID_UNKNOWN, 0, "lifecycle_test", &memh);
            if (status == UCS_OK) {
                uct_mem_free(memh);
            }
            
            /* Cleanup */
            uct_md_close(test_md);
            ucs_free(md_resources);
            uct_config_release(md_config);
            
        } catch (...) {
            cycle_success = false;
        }
        
        if (cycle_success) {
            successful_cycles++;
        } else {
            failed_cycles++;
        }
        
        /* Progress indicator */
        if (cycle % 10 == 0) {
            ucs_debug("Lifecycle stress progress: %d/%d (success: %d, failed: %d)", 
                      cycle, num_cycles, successful_cycles.load(), failed_cycles.load());
        }
    }
    
    ucs_debug("Interface lifecycle stress test completed: %d successful, %d failed", 
              successful_cycles.load(), failed_cycles.load());
    
    /* Should have reasonable success rate */
    EXPECT_GT(successful_cycles.load(), num_cycles / 2);
}

/* Stress test: System resource exhaustion recovery */
UCS_TEST_F(test_gaudi_stress, resource_exhaustion_recovery) {
    const size_t large_alloc_size = 16 * 1024 * 1024;  /* 16MB */
    const int max_attempts = 100;
    
    std::vector<std::pair<void*, uct_mem_h>> large_allocations;
    int successful_allocs = 0;
    int failed_allocs = 0;
    
    ucs_debug("Starting resource exhaustion recovery test");
    
    /* Try to exhaust memory resources */
    for (int i = 0; i < max_attempts; ++i) {
        void *ptr;
        uct_mem_h memh;
        size_t size = large_alloc_size;
        
        ucs_status_t status = uct_mem_alloc(m_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                          "exhaustion_test", &memh);
        
        if (status == UCS_OK) {
            large_allocations.push_back(std::make_pair(ptr, memh));
            successful_allocs++;
        } else {
            failed_allocs++;
            /* Stop when we can't allocate anymore */
            if (failed_allocs > 5) break;
        }
    }
    
    ucs_debug("Allocated %d large buffers, %d failures", successful_allocs, failed_allocs);
    
    /* Free half the allocations */
    int to_free = large_allocations.size() / 2;
    for (int i = 0; i < to_free; ++i) {
        uct_mem_free(large_allocations.back().second);
        large_allocations.pop_back();
    }
    
    /* Try to allocate small buffers (should succeed) */
    int small_alloc_successes = 0;
    std::vector<std::pair<void*, uct_mem_h>> small_allocations;
    
    for (int i = 0; i < 10; ++i) {
        void *ptr;
        uct_mem_h memh;
        size_t size = 4096;
        
        ucs_status_t status = uct_mem_alloc(m_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                          "recovery_test", &memh);
        
        if (status == UCS_OK) {
            small_allocations.push_back(std::make_pair(ptr, memh));
            small_alloc_successes++;
        }
    }
    
    ucs_debug("After partial cleanup, allocated %d small buffers", small_alloc_successes);
    
    /* Cleanup all remaining allocations */
    for (auto& alloc : large_allocations) {
        uct_mem_free(alloc.second);
    }
    for (auto& alloc : small_allocations) {
        uct_mem_free(alloc.second);
    }
    
    /* Should be able to recover and allocate small buffers */
    EXPECT_GT(small_alloc_successes, 5);
    
    ucs_debug("Resource exhaustion recovery test completed");
}
