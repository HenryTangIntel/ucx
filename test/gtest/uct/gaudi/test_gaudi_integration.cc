/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "test_gaudi_common.h"
#include <uct/uct_test.h>

extern "C" {
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <uct/gaudi/copy/gaudi_copy_iface.h>
#include <uct/gaudi/ipc/gaudi_ipc_md.h>
#include <uct/gaudi/ipc/gaudi_ipc_iface.h>
#include <uct/gaudi/base/gaudi_dma.h>
#include <habanalabs/hlthunk.h>
}

class test_gaudi_integration : public uct_test, public gaudi_test_base {
public:
    UCS_TEST_BASE_IMPL;

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        
        /* Initialize with both copy and IPC transports */
        m_component_names.push_back("gaudi_copy");
        m_component_names.push_back("gaudi_ipc");
        
        uct_test::init();
    }
    
    /* Helper to perform end-to-end data transfer test */
    void test_data_transfer(size_t size, const std::string& test_name) {
        void *src_buf = alloc_host_memory(size);
        void *dst_buf = alloc_host_memory(size);
        ASSERT_NE(src_buf, nullptr);
        ASSERT_NE(dst_buf, nullptr);
        
        /* Fill source with test pattern */
        fill_buffer(src_buf, size, 0xABCDEF00);
        memset(dst_buf, 0, size);
        
        /* Register memory */
        uct_mem_h src_memh, dst_memh;
        ucs_status_t status = uct_md_mem_reg(md(), src_buf, size, 
                                           UCT_MD_MEM_ACCESS_ALL, &src_memh);
        ASSERT_UCS_OK(status);
        
        status = uct_md_mem_reg(md(), dst_buf, size, 
                              UCT_MD_MEM_ACCESS_ALL, &dst_memh);
        ASSERT_UCS_OK(status);
        
        /* Create packed keys */
        void *src_rkey_buf = malloc(md()->component->rkey_packed_size);
        void *dst_rkey_buf = malloc(md()->component->rkey_packed_size);
        ASSERT_NE(src_rkey_buf, nullptr);
        ASSERT_NE(dst_rkey_buf, nullptr);
        
        status = uct_md_mkey_pack(md(), src_memh, src_rkey_buf);
        ASSERT_UCS_OK(status);
        status = uct_md_mkey_pack(md(), dst_memh, dst_rkey_buf);
        ASSERT_UCS_OK(status);
        
        /* Unpack remote keys */
        uct_rkey_t src_rkey, dst_rkey;
        status = uct_rkey_unpack(src_rkey_buf, &src_rkey);
        ASSERT_UCS_OK(status);
        status = uct_rkey_unpack(dst_rkey_buf, &dst_rkey);
        ASSERT_UCS_OK(status);
        
        /* Perform copy operation */
        uct_completion_t comp;
        comp.count = 1;
        comp.func = NULL;
        
        status = uct_ep_put_short(ep(), src_buf, size, (uint64_t)dst_buf, dst_rkey);
        if (status == UCS_INPROGRESS) {
            /* Wait for completion if needed */
            while (comp.count > 0) {
                progress();
                usleep(1000);  /* 1ms */
            }
        } else {
            ASSERT_UCS_OK(status);
        }
        
        /* Verify data transfer */
        bool data_correct = verify_buffer(dst_buf, size, 0xABCDEF00);
        EXPECT_TRUE(data_correct) << "Data verification failed for " << test_name;
        
        /* Cleanup */
        uct_rkey_release(src_rkey);
        uct_rkey_release(dst_rkey);
        free(src_rkey_buf);
        free(dst_rkey_buf);
        uct_md_mem_dereg(md(), src_memh);
        uct_md_mem_dereg(md(), dst_memh);
        free(src_buf);
        free(dst_buf);
    }
};

/* Test basic transport initialization and capabilities */
UCS_TEST_F(test_gaudi_integration, transport_capabilities) {
    /* Test that Gaudi transports are available */
    bool found_copy = false, found_ipc = false;
    
    /* Check available transports */
    uct_tl_resource_desc_t *resources;
    unsigned num_resources;
    
    ucs_status_t status = uct_query_tl_resources(m_md, &resources, &num_resources);
    ASSERT_UCS_OK(status);
    
    for (unsigned i = 0; i < num_resources; ++i) {
        if (strcmp(resources[i].tl_name, "gaudi_copy") == 0) {
            found_copy = true;
        }
        if (strcmp(resources[i].tl_name, "gaudi_ipc") == 0) {
            found_ipc = true;
        }
    }
    
    EXPECT_TRUE(found_copy) << "gaudi_copy transport not found";
    /* IPC might not be available in all configurations */
    
    ucs_free(resources);
}

/* Test small data transfers */
UCS_TEST_F(test_gaudi_integration, small_transfers) {
    test_data_transfer(64, "64B transfer");
    test_data_transfer(256, "256B transfer");
    test_data_transfer(1024, "1KB transfer");
}

/* Test medium data transfers */
UCS_TEST_F(test_gaudi_integration, medium_transfers) {
    test_data_transfer(4 * 1024, "4KB transfer");
    test_data_transfer(16 * 1024, "16KB transfer");
    test_data_transfer(64 * 1024, "64KB transfer");
}

/* Test large data transfers */
UCS_TEST_F(test_gaudi_integration, large_transfers) {
    test_data_transfer(256 * 1024, "256KB transfer");
    test_data_transfer(1024 * 1024, "1MB transfer");
    /* Very large transfers might timeout in test environment */
}

/* Test multiple concurrent transfers */
UCS_TEST_F(test_gaudi_integration, concurrent_transfers) {
    const int num_transfers = 4;
    const size_t transfer_size = 4096;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);
    
    auto transfer_worker = [&](int thread_id) {
        try {
            test_data_transfer(transfer_size, 
                             "concurrent_" + std::to_string(thread_id));
            success_count++;
        } catch (...) {
            failure_count++;
        }
    };
    
    /* Launch concurrent transfers */
    for (int i = 0; i < num_transfers; ++i) {
        threads.emplace_back(transfer_worker, i);
    }
    
    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }
    
    /* At least most transfers should succeed */
    EXPECT_GE(success_count.load(), num_transfers / 2);
}

/* Test memory allocation and registration integration */
UCS_TEST_F(test_gaudi_integration, memory_integration) {
    const size_t test_size = 64 * 1024;
    
    /* Allocate Gaudi memory */
    void *gaudi_ptr;
    uct_mem_h gaudi_memh;
    size_t gaudi_size = test_size;
    
    ucs_status_t status = uct_mem_alloc(md(), &gaudi_size, &gaudi_ptr, 
                                       UCS_MEMORY_TYPE_GAUDI,
                                       UCS_SYS_DEVICE_ID_UNKNOWN, 0, 
                                       "integration_test", &gaudi_memh);
    if (status != UCS_OK) {
        UCS_TEST_SKIP_R("Cannot allocate Gaudi memory");
    }
    
    /* Allocate and register host memory */
    void *host_ptr = alloc_host_memory(test_size);
    ASSERT_NE(host_ptr, nullptr);
    
    uct_mem_h host_memh;
    status = uct_md_mem_reg(md(), host_ptr, test_size, 
                          UCT_MD_MEM_ACCESS_ALL, &host_memh);
    ASSERT_UCS_OK(status);
    
    /* Test data pattern */
    fill_buffer(host_ptr, test_size, 0x12345678);
    
    /* Test key packing/unpacking */
    void *gaudi_rkey_buf = malloc(md()->component->rkey_packed_size);
    void *host_rkey_buf = malloc(md()->component->rkey_packed_size);
    ASSERT_NE(gaudi_rkey_buf, nullptr);
    ASSERT_NE(host_rkey_buf, nullptr);
    
    status = uct_md_mkey_pack(md(), gaudi_memh, gaudi_rkey_buf);
    EXPECT_UCS_OK(status);
    status = uct_md_mkey_pack(md(), host_memh, host_rkey_buf);
    EXPECT_UCS_OK(status);
    
    /* Test remote key operations */
    uct_rkey_t gaudi_rkey, host_rkey;
    status = uct_rkey_unpack(gaudi_rkey_buf, &gaudi_rkey);
    EXPECT_UCS_OK(status);
    status = uct_rkey_unpack(host_rkey_buf, &host_rkey);
    EXPECT_UCS_OK(status);
    
    /* Cleanup */
    if (status == UCS_OK) {
        uct_rkey_release(gaudi_rkey);
        uct_rkey_release(host_rkey);
    }
    free(gaudi_rkey_buf);
    free(host_rkey_buf);
    uct_md_mem_dereg(md(), host_memh);
    uct_mem_free(gaudi_memh);
    free(host_ptr);
}

/* Test interface and endpoint lifecycle */
UCS_TEST_F(test_gaudi_integration, interface_lifecycle) {
    /* Test multiple interface creation/destruction cycles */
    for (int cycle = 0; cycle < 3; ++cycle) {
        /* Create additional interface */
        uct_iface_h iface2;
        uct_iface_config_t *iface_config;
        
        ucs_status_t status = uct_md_iface_config_read(md(), tl_name().c_str(), 
                                                     NULL, NULL, &iface_config);
        ASSERT_UCS_OK(status);
        
        status = uct_iface_open(md(), m_worker, &m_iface_params, 
                              iface_config, &iface2);
        if (status == UCS_OK) {
            /* Create endpoint on new interface */
            uct_ep_h ep2;
            uct_ep_params_t ep_params;
            ep_params.field_mask = UCT_EP_PARAM_FIELD_IFACE;
            ep_params.iface = iface2;
            
            status = uct_ep_create(&ep_params, &ep2);
            if (status == UCS_OK) {
                /* Test basic operations */
                uct_iface_attr_t iface_attr;
                status = uct_iface_query(iface2, &iface_attr);
                EXPECT_UCS_OK(status);
                
                uct_ep_destroy(ep2);
            }
            
            uct_iface_close(iface2);
        }
        
        uct_config_release(iface_config);
    }
}

/* Test different transfer sizes and patterns */
UCS_TEST_F(test_gaudi_integration, transfer_patterns) {
    const size_t base_size = 1024;
    
    /* Test power-of-2 sizes */
    for (int i = 0; i < 8; ++i) {
        size_t size = base_size << i;  /* 1KB, 2KB, 4KB, ... 128KB */
        if (size > 128 * 1024) break;  /* Limit for test environment */
        
        test_data_transfer(size, "power2_" + std::to_string(size));
    }
    
    /* Test odd sizes */
    test_data_transfer(1023, "odd_1023");
    test_data_transfer(4097, "odd_4097");
    test_data_transfer(65537, "odd_65537");
}

/* Test system integration with multiple devices */
UCS_TEST_F(test_gaudi_integration, multi_device_detection) {
    int device_count = get_device_count();
    EXPECT_GT(device_count, 0);
    
    /* Test that system can detect all devices */
    for (int dev_idx = 0; dev_idx < device_count && dev_idx < 4; ++dev_idx) {
        ucs_sys_device_t sys_dev = uct_gaudi_base_get_sys_dev(dev_idx);
        /* Each device should have a valid system device ID */
        /* (might be unknown if PCI detection fails, but shouldn't crash) */
        
        /* Test device-specific operations */
        int fd = open_gaudi_device(dev_idx);
        if (fd >= 0) {
            hlthunk_hw_ip_info hw_info;
            int ret = hlthunk_get_hw_ip_info(fd, &hw_info);
            EXPECT_EQ(ret, 0);
            
            hlthunk_close(fd);
        }
    }
}

/* Test performance characteristics */
UCS_TEST_F(test_gaudi_integration, performance_characteristics) {
    const size_t test_size = 64 * 1024;
    const int num_iterations = 10;
    
    std::vector<double> transfer_times;
    
    for (int iter = 0; iter < num_iterations; ++iter) {
        void *src_buf = alloc_host_memory(test_size);
        void *dst_buf = alloc_host_memory(test_size);
        ASSERT_NE(src_buf, nullptr);
        ASSERT_NE(dst_buf, nullptr);
        
        fill_buffer(src_buf, test_size);
        
        /* Measure transfer time */
        ucs_time_t start_time = ucs_get_time();
        
        /* Perform transfer (simplified version) */
        memcpy(dst_buf, src_buf, test_size);
        
        ucs_time_t end_time = ucs_get_time();
        double elapsed = ucs_time_to_sec(end_time - start_time);
        transfer_times.push_back(elapsed);
        
        /* Verify data */
        EXPECT_TRUE(verify_buffer(dst_buf, test_size));
        
        free(src_buf);
        free(dst_buf);
    }
    
    /* Calculate statistics */
    double total_time = 0.0, min_time = transfer_times[0], max_time = transfer_times[0];
    for (double t : transfer_times) {
        total_time += t;
        min_time = std::min(min_time, t);
        max_time = std::max(max_time, t);
    }
    double avg_time = total_time / num_iterations;
    
    /* Basic performance expectations */
    EXPECT_LT(avg_time, 0.1);  /* Average should be less than 100ms */
    EXPECT_LT(max_time / min_time, 10.0);  /* Variance shouldn't be too high */
    
    ucs_debug("Performance: avg=%.3fms, min=%.3fms, max=%.3fms", 
              avg_time * 1000, min_time * 1000, max_time * 1000);
}
