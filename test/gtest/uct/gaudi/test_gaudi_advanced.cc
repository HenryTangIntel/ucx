/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "test_gaudi_common.h"
#include <uct/test_md.h>

extern "C" {
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <uct/gaudi/ipc/gaudi_ipc_md.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <uct/gaudi/base/gaudi_dma.h>
#include <habanalabs/hlthunk.h>
}

class test_gaudi_advanced : public test_md, public gaudi_test_base {
public:
    UCS_TEST_BASE_IMPL;

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        
        /* Initialize MD with gaudi_copy component */
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
        
        /* Open hlthunk device for direct memory operations */
        m_fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
        if (m_fd < 0) {
            UCS_TEST_SKIP_R("Failed to open hlthunk device");
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
};

/* Test memory allocation edge cases */
UCS_TEST_F(test_gaudi_advanced, alloc_edge_cases) {
    void *ptr;
    uct_mem_h memh;
    size_t size;
    ucs_status_t status;
    
    /* Test zero-sized allocation */
    size = 0;
    status = uct_mem_alloc(m_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, "zero_size", &memh);
    if (status == UCS_OK) {
        EXPECT_GT(size, 0);  /* Should round up to minimum */
        uct_mem_free(memh);
    }
    
    /* Test very large allocation */
    size = 1ULL << 32;  /* 4GB */
    status = uct_mem_alloc(m_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, "large_alloc", &memh);
    if (status == UCS_OK) {
        uct_mem_free(memh);
    }
    /* Large allocation failure is acceptable */
    
    /* Test allocation with specific alignment */
    size = 1024 * 1024;  /* 1MB */
    status = uct_mem_alloc(m_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                          UCS_SYS_DEVICE_ID_UNKNOWN, UCT_MD_MEM_FLAG_FIXED, 
                          "aligned", &memh);
    if (status == UCS_OK) {
        EXPECT_EQ((uintptr_t)ptr % 4096, 0);  /* Should be page-aligned */
        uct_mem_free(memh);
    }
}

/* Test multiple simultaneous allocations */
UCS_TEST_F(test_gaudi_advanced, multiple_allocations) {
    const int num_allocs = 10;
    const size_t alloc_size = 64 * 1024;  /* 64KB each */
    void *ptrs[num_allocs];
    uct_mem_h memhs[num_allocs];
    
    /* Allocate multiple buffers */
    for (int i = 0; i < num_allocs; ++i) {
        size_t size = alloc_size;
        ucs_status_t status = uct_mem_alloc(m_md, &size, &ptrs[i], UCS_MEMORY_TYPE_GAUDI,
                                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, "multi", &memhs[i]);
        ASSERT_UCS_OK(status);
        EXPECT_GE(size, alloc_size);
        
        /* Verify all pointers are different */
        for (int j = 0; j < i; ++j) {
            EXPECT_NE(ptrs[i], ptrs[j]);
        }
    }
    
    /* Free all buffers */
    for (int i = 0; i < num_allocs; ++i) {
        uct_mem_free(memhs[i]);
    }
}

/* Test memory registration edge cases */
UCS_TEST_F(test_gaudi_advanced, register_edge_cases) {
    void *host_ptr = alloc_host_memory(4096);
    ASSERT_NE(host_ptr, nullptr);
    
    uct_mem_h memh;
    ucs_status_t status;
    
    /* Register the same memory multiple times (should fail or handle gracefully) */
    status = uct_md_mem_reg(m_md, host_ptr, 4096, UCT_MD_MEM_ACCESS_ALL,
                           &memh);
    if (status == UCS_OK) {
        uct_mem_h memh2;
        status = uct_md_mem_reg(m_md, host_ptr, 4096, UCT_MD_MEM_ACCESS_ALL,
                               &memh2);
        /* Second registration might succeed or fail - both are acceptable */
        if (status == UCS_OK) {
            uct_md_mem_dereg(m_md, memh2);
        }
        uct_md_mem_dereg(m_md, memh);
    }
    
    /* Register overlapping memory regions */
    void *overlap_ptr = (char*)host_ptr + 2048;
    status = uct_md_mem_reg(m_md, host_ptr, 4096, UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status == UCS_OK) {
        uct_mem_h overlap_memh;
        status = uct_md_mem_reg(m_md, overlap_ptr, 2048, UCT_MD_MEM_ACCESS_ALL,
                               &overlap_memh);
        if (status == UCS_OK) {
            uct_md_mem_dereg(m_md, overlap_memh);
        }
        uct_md_mem_dereg(m_md, memh);
    }
    
    free(host_ptr);
}

/* Test system device detection and PCI mapping */
UCS_TEST_F(test_gaudi_advanced, system_device_detection) {
    int device_count = get_device_count();
    ASSERT_GT(device_count, 0);
    
    for (int dev_idx = 0; dev_idx < device_count; ++dev_idx) {
        int fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
        if (fd < 0) continue;
        
        /* Test PCI bus ID retrieval */
        char pci_bus_id[32];
        int ret = hlthunk_get_pci_bus_id_from_fd(fd, pci_bus_id, sizeof(pci_bus_id));
        if (ret == 0) {
            /* PCI bus ID should be in format DDDD:BB:DD.F */
            EXPECT_GE(strlen(pci_bus_id), 7);  /* Minimum length */
            EXPECT_NE(strchr(pci_bus_id, ':'), nullptr);  /* Should contain colon */
        }
        
        /* Test system device lookup */
        ucs_sys_device_t sys_dev = uct_gaudi_base_get_sys_dev(dev_idx);
        /* sys_dev should be valid (non-negative) if PCI detection works */
        
        hlthunk_close(fd);
    }
}

/* Test memory handle tracking under stress */
UCS_TEST_F(test_gaudi_advanced, memory_handle_stress) {
    const int num_iterations = 100;
    const size_t alloc_size = 1024;
    
    std::vector<uct_mem_h> memhs;
    
    /* Rapid allocation and deallocation */
    for (int iter = 0; iter < num_iterations; ++iter) {
        void *ptr;
        uct_mem_h memh;
        size_t size = alloc_size;
        
        ucs_status_t status = uct_mem_alloc(m_md, &size, &ptr, UCS_MEMORY_TYPE_GAUDI,
                                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, "stress", &memh);
        if (status == UCS_OK) {
            memhs.push_back(memh);
            
            /* Randomly free some handles */
            if ((iter % 3 == 0) && !memhs.empty()) {
                int idx = rand() % memhs.size();
                uct_mem_free(memhs[idx]);
                memhs.erase(memhs.begin() + idx);
            }
        }
    }
    
    /* Free remaining handles */
    for (auto memh : memhs) {
        uct_mem_free(memh);
    }
}

/* Test mixed memory types */
UCS_TEST_F(test_gaudi_advanced, mixed_memory_types) {
    void *host_ptr = alloc_host_memory(4096);
    ASSERT_NE(host_ptr, nullptr);
    
    void *gaudi_ptr;
    uct_mem_h gaudi_memh;
    size_t gaudi_size = 4096;
    
    /* Allocate Gaudi memory */
    ucs_status_t status = uct_mem_alloc(m_md, &gaudi_size, &gaudi_ptr, 
                                       UCS_MEMORY_TYPE_GAUDI,
                                       UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                       "mixed_test", &gaudi_memh);
    if (status != UCS_OK) {
        free(host_ptr);
        UCS_TEST_SKIP_R("Cannot allocate Gaudi memory");
    }
    
    /* Register host memory */
    uct_mem_h host_memh;
    status = uct_md_mem_reg(m_md, host_ptr, 4096, UCT_MD_MEM_ACCESS_ALL, &host_memh);
    
    /* Test operations with both memory types */
    uct_md_attr_t md_attr;
    status = uct_md_query(m_md, &md_attr);
    ASSERT_UCS_OK(status);
    
    /* Verify both memory types are supported */
    EXPECT_TRUE(md_attr.cap.alloc_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    EXPECT_TRUE(md_attr.cap.reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    
    /* Cleanup */
    if (status == UCS_OK) {
        uct_md_mem_dereg(m_md, host_memh);
    }
    uct_mem_free(gaudi_memh);
    free(host_ptr);
}

/* Test concurrent access patterns */
UCS_TEST_F(test_gaudi_advanced, concurrent_operations) {
    const int num_threads = 4;
    const int ops_per_thread = 50;
    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);
    
    auto worker = [&](int thread_id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            void *ptr;
            uct_mem_h memh;
            size_t size = 1024 * (1 + (thread_id % 4));  /* Varying sizes */
            
            ucs_status_t status = uct_mem_alloc(m_md, &size, &ptr, 
                                              UCS_MEMORY_TYPE_GAUDI,
                                              UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                              "concurrent", &memh);
            if (status == UCS_OK) {
                success_count++;
                /* Hold for a bit to test concurrent access */
                usleep(1000);  /* 1ms */
                uct_mem_free(memh);
            } else {
                failure_count++;
            }
        }
    };
    
    /* Launch threads */
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    
    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }
    
    /* At least some operations should succeed */
    EXPECT_GT(success_count.load(), 0);
    
    ucs_debug("Concurrent test: %d successes, %d failures", 
              success_count.load(), failure_count.load());
}
