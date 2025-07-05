/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "test_ucp_memheap.h"

extern "C" {
#include <ucp/core/ucp_context.h>
#include <ucp/core/ucp_mm.h>
#include <ucs/sys/sys.h>
#include <habanalabs/hlthunk.h>
}

class test_ucp_gaudi : public test_ucp_memheap {
public:
    static void get_test_variants(std::vector<ucp_test_variant>& variants) {
        add_variant(variants, UCP_FEATURE_TAG);
        add_variant(variants, UCP_FEATURE_RMA);
        add_variant(variants, UCP_FEATURE_AMO);
    }

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        test_ucp_memheap::init();
    }

    static bool is_gaudi_available() {
        int device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
        return device_count > 0;
    }

    void *gaudi_alloc(size_t size, uint64_t *handle_out = NULL) {
        if (!is_gaudi_available()) {
            return NULL;
        }

        int fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
        if (fd < 0) {
            return NULL;
        }

        uint64_t handle = hlthunk_device_memory_alloc(fd, size, 0, true, true);
        if (handle == 0) {
            hlthunk_close(fd);
            return NULL;
        }

        uint64_t device_addr = hlthunk_device_memory_map(fd, handle, 0);
        if (device_addr == 0) {
            hlthunk_device_memory_free(fd, handle);
            hlthunk_close(fd);
            return NULL;
        }

        if (handle_out != NULL) {
            *handle_out = handle;
        }

        hlthunk_close(fd);
        return (void*)device_addr;
    }

    void gaudi_free(void *ptr, uint64_t handle) {
        if (ptr != NULL && handle != 0) {
            int fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
            if (fd >= 0) {
                hlthunk_device_memory_free(fd, handle);
                hlthunk_close(fd);
            }
        }
    }
    
    /* Fill buffer with test pattern */
    void fill_test_pattern(void *buffer, size_t size, uint32_t seed) {
        uint32_t *data = (uint32_t*)buffer;
        size_t count = size / sizeof(uint32_t);
        
        for (size_t i = 0; i < count; ++i) {
            data[i] = seed + (uint32_t)i;
        }
    }
    
    /* Verify buffer contains expected pattern */
    bool verify_test_pattern(void *buffer, size_t size, uint32_t seed) {
        uint32_t *data = (uint32_t*)buffer;
        size_t count = size / sizeof(uint32_t);
        
        for (size_t i = 0; i < count; ++i) {
            if (data[i] != seed + (uint32_t)i) {
                return false;
            }
        }
        return true;
    }
};

UCS_TEST_P(test_ucp_gaudi, memory_type_detection) {
    const size_t size = 4096;
    uint64_t handle;
    void *gaudi_ptr = gaudi_alloc(size, &handle);
    
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }

    ucp_memory_info_t mem_info;
    ucp_memory_detect(sender().ucph(), gaudi_ptr, size, &mem_info);
    
    EXPECT_EQ(UCS_MEMORY_TYPE_GAUDI, mem_info.type);
    
    gaudi_free(gaudi_ptr, handle);
}

UCS_TEST_P(test_ucp_gaudi, memory_map_gaudi) {
    const size_t size = 8192;
    uint64_t handle;
    void *gaudi_ptr = gaudi_alloc(size, &handle);
    
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }

    ucp_mem_h memh;
    ucp_mem_map_params_t params;
    
    params.field_mask  = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                         UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                         UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    params.address     = gaudi_ptr;
    params.length      = size;
    params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    ucs_status_t status = ucp_mem_map(sender().ucph(), &params, &memh);
    ASSERT_UCS_OK(status);
    
    EXPECT_EQ(UCS_MEMORY_TYPE_GAUDI, memh->mem_type);
    
    status = ucp_mem_unmap(sender().ucph(), memh);
    ASSERT_UCS_OK(status);
    
    gaudi_free(gaudi_ptr, handle);
}

UCS_TEST_P(test_ucp_gaudi, rkey_pack_unpack) {
    const size_t size = 1024;
    uint64_t handle;
    void *gaudi_ptr = gaudi_alloc(size, &handle);
    
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }

    ucp_mem_h memh;
    ucp_mem_map_params_t params;
    
    params.field_mask  = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                         UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                         UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    params.address     = gaudi_ptr;
    params.length      = size;
    params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    ucs_status_t status = ucp_mem_map(sender().ucph(), &params, &memh);
    ASSERT_UCS_OK(status);
    
    void *rkey_buffer;
    size_t rkey_buffer_size;
    
    status = ucp_rkey_pack(sender().ucph(), memh, &rkey_buffer, &rkey_buffer_size);
    ASSERT_UCS_OK(status);
    
    EXPECT_GT(rkey_buffer_size, 0u);
    EXPECT_NE(rkey_buffer, (void*)NULL);
    
    ucp_rkey_buffer_release(rkey_buffer);
    
    status = ucp_mem_unmap(sender().ucph(), memh);
    ASSERT_UCS_OK(status);
    
    gaudi_free(gaudi_ptr, handle);
}

UCS_TEST_P(test_ucp_gaudi, memory_helpers) {
    /* Test the UCP memory type helper macros */
    EXPECT_TRUE(UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_GAUDI));
    EXPECT_FALSE(UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_HOST));
    EXPECT_FALSE(UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_CUDA));
    EXPECT_FALSE(UCP_MEM_IS_GAUDI(UCS_MEMORY_TYPE_ROCM));
    
    EXPECT_TRUE(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_GAUDI));
    EXPECT_TRUE(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_CUDA));
    EXPECT_TRUE(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_ROCM));
    EXPECT_FALSE(UCP_MEM_IS_GPU(UCS_MEMORY_TYPE_HOST));
}

UCS_TEST_P(test_ucp_gaudi, gaudi_memory_operations) {
    const size_t size = 16384;
    const uint32_t pattern = 0xDEADBEEF;
    
    uint64_t handle;
    void *gaudi_ptr = gaudi_alloc(size, &handle);
    
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }
    
    /* Map the memory */
    ucp_mem_h memh;
    ucp_mem_map_params_t params;
    
    params.field_mask  = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                         UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                         UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    params.address     = gaudi_ptr;
    params.length      = size;
    params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    ucs_status_t status = ucp_mem_map(sender().ucph(), &params, &memh);
    ASSERT_UCS_OK(status);
    
    /* Fill memory with pattern (if accessible from host) */
    fill_test_pattern(gaudi_ptr, size, pattern);
    
    /* Verify pattern */
    bool pattern_ok = verify_test_pattern(gaudi_ptr, size, pattern);
    if (pattern_ok) {
        UCS_TEST_MESSAGE << "Successfully wrote and read Gaudi memory from host";
    }
    
    /* Test memory advise */
    status = ucp_mem_advise(sender().ucph(), memh, gaudi_ptr, size, UCP_MADV_WILLNEED);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
    
    /* Unmap memory */
    status = ucp_mem_unmap(sender().ucph(), memh);
    ASSERT_UCS_OK(status);
    
    gaudi_free(gaudi_ptr, handle);
}

UCS_TEST_P(test_ucp_gaudi, mixed_memory_types) {
    const size_t size = 4096;
    const uint32_t host_pattern = 0x12345678;
    const uint32_t gaudi_pattern = 0x87654321;
    
    /* Allocate host memory */
    void *host_ptr = malloc(size);
    ASSERT_NE(host_ptr, (void*)NULL);
    fill_test_pattern(host_ptr, size, host_pattern);
    
    /* Allocate Gaudi memory */
    uint64_t handle;
    void *gaudi_ptr = gaudi_alloc(size, &handle);
    
    if (gaudi_ptr == NULL) {
        free(host_ptr);
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }
    
    fill_test_pattern(gaudi_ptr, size, gaudi_pattern);
    
    /* Test memory type detection for both */
    ucp_memory_info_t host_info, gaudi_info;
    
    ucp_memory_detect(sender().ucph(), host_ptr, size, &host_info);
    EXPECT_EQ(UCS_MEMORY_TYPE_HOST, host_info.type);
    
    ucp_memory_detect(sender().ucph(), gaudi_ptr, size, &gaudi_info);
    EXPECT_EQ(UCS_MEMORY_TYPE_GAUDI, gaudi_info.type);
    
    /* Map both memory regions */
    ucp_mem_h host_memh, gaudi_memh;
    ucp_mem_map_params_t params;
    
    /* Map host memory */
    params.field_mask  = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                         UCP_MEM_MAP_PARAM_FIELD_LENGTH;
    params.address     = host_ptr;
    params.length      = size;
    
    ucs_status_t status = ucp_mem_map(sender().ucph(), &params, &host_memh);
    ASSERT_UCS_OK(status);
    
    /* Map Gaudi memory */
    params.field_mask |= UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    params.address     = gaudi_ptr;
    params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    status = ucp_mem_map(sender().ucph(), &params, &gaudi_memh);
    ASSERT_UCS_OK(status);
    
    /* Verify memory types */
    EXPECT_EQ(UCS_MEMORY_TYPE_HOST, host_memh->mem_type);
    EXPECT_EQ(UCS_MEMORY_TYPE_GAUDI, gaudi_memh->mem_type);
    
    /* Verify patterns are intact */
    EXPECT_TRUE(verify_test_pattern(host_ptr, size, host_pattern));
    EXPECT_TRUE(verify_test_pattern(gaudi_ptr, size, gaudi_pattern));
    
    /* Cleanup */
    ucp_mem_unmap(sender().ucph(), host_memh);
    ucp_mem_unmap(sender().ucph(), gaudi_memh);
    
    free(host_ptr);
    gaudi_free(gaudi_ptr, handle);
}

UCS_TEST_P(test_ucp_gaudi, context_capabilities) {
    ucp_context_attr_t ctx_attr;
    ctx_attr.field_mask = UCP_ATTR_FIELD_MEMORY_TYPES;
    
    ucs_status_t status = ucp_context_query(sender().ucph(), &ctx_attr);
    ASSERT_UCS_OK(status);
    
    /* Check that Gaudi memory type is supported */
    EXPECT_TRUE(ctx_attr.memory_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    
    UCS_TEST_MESSAGE << "UCP context supports memory types: 0x" 
                    << std::hex << ctx_attr.memory_types;
}

UCS_TEST_P(test_ucp_gaudi, large_memory_allocation) {
    const size_t large_size = 64 * 1024 * 1024;  /* 64MB */
    
    uint64_t handle;
    void *gaudi_ptr = gaudi_alloc(large_size, &handle);
    
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate large Gaudi memory");
    }
    
    /* Map the large memory */
    ucp_mem_h memh;
    ucp_mem_map_params_t params;
    
    params.field_mask  = UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                         UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                         UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE;
    params.address     = gaudi_ptr;
    params.length      = large_size;
    params.memory_type = UCS_MEMORY_TYPE_GAUDI;
    
    ucs_status_t status = ucp_mem_map(sender().ucph(), &params, &memh);
    ASSERT_UCS_OK(status);
    
    /* Test write/read at different offsets */
    uint32_t *data = (uint32_t*)gaudi_ptr;
    const uint32_t test_pattern = 0xABCDEF00;
    
    /* Test beginning */
    data[0] = test_pattern;
    EXPECT_EQ(test_pattern, data[0]);
    
    /* Test middle */
    size_t mid_idx = (large_size / sizeof(uint32_t)) / 2;
    data[mid_idx] = test_pattern + 1;
    EXPECT_EQ(test_pattern + 1, data[mid_idx]);
    
    /* Test end */
    size_t end_idx = (large_size / sizeof(uint32_t)) - 1;
    data[end_idx] = test_pattern + 2;
    EXPECT_EQ(test_pattern + 2, data[end_idx]);
    
    UCS_TEST_MESSAGE << "Successfully allocated and tested " 
                    << large_size / (1024*1024) << "MB Gaudi memory";
    
    status = ucp_mem_unmap(sender().ucph(), memh);
    ASSERT_UCS_OK(status);
    
    gaudi_free(gaudi_ptr, handle);
}

UCP_INSTANTIATE_TEST_CASE_TLS(test_ucp_gaudi, all, "all")
