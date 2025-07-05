/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "../test_md.h"
#include "../../common/test.h"

extern "C" {
#include <uct/base/uct_md.h>
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <uct/gaudi/base/gaudi_dma.h>
#include <habanalabs/hlthunk.h>
}

class test_gaudi_md : public test_md {
public:
    UCS_TEST_BASE_IMPL;

protected:
    void init() {
        if (!is_gaudi_available()) {
            UCS_TEST_SKIP_R("Gaudi not available");
        }
        
        /* Call parent init to set up MD */
        test_md::init();
    }
    
    void cleanup() {
        test_md::cleanup();
    }
    
    static bool is_gaudi_available() {
        int device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
        return device_count > 0;
    }
    
    /* Helper to allocate Gaudi device memory */
    void* alloc_gaudi_memory(size_t size, uint64_t *handle_p = NULL) {
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
        
        if (handle_p != NULL) {
            *handle_p = handle;
        }
        
        hlthunk_close(fd);
        return (void*)device_addr;
    }
};

UCS_TEST_F(test_gaudi_md, query_md_attr) {
    uct_md_attr_t md_attr;
    ucs_status_t status = uct_md_query(m_md, &md_attr);
    ASSERT_UCS_OK(status);
    
    /* Check that Gaudi memory types are supported */
    EXPECT_TRUE(md_attr.cap.alloc_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    EXPECT_TRUE(md_attr.cap.reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    EXPECT_TRUE(md_attr.cap.access_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    EXPECT_TRUE(md_attr.cap.detect_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    
    /* Check that host memory is also supported */
    EXPECT_TRUE(md_attr.cap.alloc_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    EXPECT_TRUE(md_attr.cap.reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    EXPECT_TRUE(md_attr.cap.access_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    
    /* Check DMABUF support */
    EXPECT_TRUE(md_attr.cap.flags & UCT_MD_FLAG_REG_DMABUF);
    
    /* Check remote key size */
    EXPECT_GT(md_attr.rkey_packed_size, 0u);
}

UCS_TEST_F(test_gaudi_md, memory_allocation) {
    size_t size = 4096;
    void *address;
    uct_mem_h memh;
    
    /* Test Gaudi memory allocation */
    ucs_status_t status = uct_md_mem_alloc(m_md, &size, &address, 
                                          UCS_MEMORY_TYPE_GAUDI, 
                                          UCS_SYS_DEVICE_ID_UNKNOWN, 
                                          0, "test_alloc", &memh);
    if (status == UCS_ERR_UNSUPPORTED) {
        UCS_TEST_SKIP_R("Gaudi memory allocation not supported");
    }
    ASSERT_UCS_OK(status);
    
    EXPECT_NE(address, (void*)NULL);
    EXPECT_NE(memh, (uct_mem_h)NULL);
    
    /* Test memory deallocation */
    status = uct_md_mem_free(m_md, memh);
    ASSERT_UCS_OK(status);
}

UCS_TEST_F(test_gaudi_md, memory_registration) {
    const size_t size = 8192;
    
    /* Allocate Gaudi memory using hlthunk directly */
    void *gaudi_ptr = alloc_gaudi_memory(size);
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }
    
    uct_mem_h memh;
    ucs_status_t status = uct_md_mem_reg(m_md, gaudi_ptr, size, 
                                        UCT_MD_MEM_ACCESS_ALL, &memh);
    ASSERT_UCS_OK(status);
    
    EXPECT_NE(memh, (uct_mem_h)NULL);
    
    /* Test memory deregistration */
    status = uct_md_mem_dereg(m_md, memh);
    ASSERT_UCS_OK(status);
}

UCS_TEST_F(test_gaudi_md, memory_type_detection) {
    const size_t size = 1024;
    
    /* Test host memory detection */
    void *host_ptr = malloc(size);
    ASSERT_NE(host_ptr, (void*)NULL);
    
    ucs_memory_type_t mem_type;
    ucs_status_t status = uct_md_detect_memory_type(m_md, host_ptr, size, &mem_type);
    ASSERT_UCS_OK(status);
    EXPECT_EQ(UCS_MEMORY_TYPE_HOST, mem_type);
    
    free(host_ptr);
    
    /* Test Gaudi memory detection */
    void *gaudi_ptr = alloc_gaudi_memory(size);
    if (gaudi_ptr != NULL) {
        status = uct_md_detect_memory_type(m_md, gaudi_ptr, size, &mem_type);
        ASSERT_UCS_OK(status);
        EXPECT_EQ(UCS_MEMORY_TYPE_GAUDI, mem_type);
    }
}

UCS_TEST_F(test_gaudi_md, remote_key_operations) {
    const size_t size = 2048;
    
    void *gaudi_ptr = alloc_gaudi_memory(size);
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }
    
    /* Register memory */
    uct_mem_h memh;
    ucs_status_t status = uct_md_mem_reg(m_md, gaudi_ptr, size, 
                                        UCT_MD_MEM_ACCESS_ALL, &memh);
    ASSERT_UCS_OK(status);
    
    /* Pack remote key */
    void *rkey_buffer;
    status = uct_md_mkey_pack(m_md, memh, &rkey_buffer);
    ASSERT_UCS_OK(status);
    EXPECT_NE(rkey_buffer, (void*)NULL);
    
    /* Unpack remote key */
    uct_rkey_bundle_t rkey_bundle;
    status = uct_rkey_unpack(&uct_gaudi_copy_component, rkey_buffer, &rkey_bundle);
    ASSERT_UCS_OK(status);
    
    /* Release remote key */
    status = uct_rkey_release(&uct_gaudi_copy_component, &rkey_bundle);
    ASSERT_UCS_OK(status);
    
    /* Deregister memory */
    status = uct_md_mem_dereg(m_md, memh);
    ASSERT_UCS_OK(status);
}

UCS_TEST_F(test_gaudi_md, memory_advise) {
    const size_t size = 4096;
    
    void *gaudi_ptr = alloc_gaudi_memory(size);
    if (gaudi_ptr == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate Gaudi memory");
    }
    
    /* Register memory first */
    uct_mem_h memh;
    ucs_status_t status = uct_md_mem_reg(m_md, gaudi_ptr, size, 
                                        UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status != UCS_OK) {
        UCS_TEST_SKIP_R("Failed to register Gaudi memory");
    }
    
    /* Test memory advise (should succeed or be unsupported) */
    status = uct_md_mem_advise(m_md, memh, gaudi_ptr, size, UCT_MADV_WILLNEED);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_UNSUPPORTED));
    
    /* Deregister memory */
    status = uct_md_mem_dereg(m_md, memh);
    ASSERT_UCS_OK(status);
}

UCS_TEST_F(test_gaudi_md, component_query) {
    uct_component_attr_t attr;
    attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME |
                      UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT;
    
    ucs_status_t status = uct_component_query(&uct_gaudi_copy_component, &attr);
    ASSERT_UCS_OK(status);
    
    EXPECT_STREQ("gaudi_copy", attr.name);
    EXPECT_GT(attr.md_resource_count, 0u);
}

class test_gaudi_system_device : public ucs::test {
public:
    UCS_TEST_BASE_IMPL;

protected:
    static bool is_gaudi_available() {
        int device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
        return device_count > 0;
    }
};

UCS_TEST_F(test_gaudi_system_device, device_detection) {
    if (!is_gaudi_available()) {
        UCS_TEST_SKIP_R("Gaudi not available");
    }
    
    /* Test system device detection for first Gaudi device */
    ucs_sys_device_t sys_dev;
    uct_gaudi_base_get_sys_dev(0, &sys_dev);
    
    /* Should either succeed or return unknown (both are valid) */
    EXPECT_TRUE((sys_dev != UCS_SYS_DEVICE_ID_UNKNOWN) || 
                (sys_dev == UCS_SYS_DEVICE_ID_UNKNOWN));
    
    if (sys_dev != UCS_SYS_DEVICE_ID_UNKNOWN) {
        /* Test reverse lookup */
        int device_index;
        ucs_status_t status = uct_gaudi_base_get_gaudi_device(sys_dev, &device_index);
        ASSERT_UCS_OK(status);
        EXPECT_EQ(0, device_index);
    }
}

UCS_TEST_F(test_gaudi_system_device, multiple_devices) {
    if (!is_gaudi_available()) {
        UCS_TEST_SKIP_R("Gaudi not available");
    }
    
    int num_devices = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    if (num_devices < 2) {
        UCS_TEST_SKIP_R("Need at least 2 Gaudi devices for this test");
    }
    
    std::vector<ucs_sys_device_t> sys_devices(num_devices);
    
    /* Get system devices for all Gaudi devices */
    for (int i = 0; i < num_devices; ++i) {
        uct_gaudi_base_get_sys_dev(i, &sys_devices[i]);
    }
    
    /* Check that successfully detected devices have unique system device IDs */
    for (int i = 0; i < num_devices; ++i) {
        if (sys_devices[i] == UCS_SYS_DEVICE_ID_UNKNOWN) {
            continue;
        }
        
        for (int j = i + 1; j < num_devices; ++j) {
            if (sys_devices[j] == UCS_SYS_DEVICE_ID_UNKNOWN) {
                continue;
            }
            EXPECT_NE(sys_devices[i], sys_devices[j]) 
                << "Devices " << i << " and " << j << " have same system device ID";
        }
    }
}

UCS_TEST_F(test_gaudi_system_device, md_resource_query) {
    if (!is_gaudi_available()) {
        UCS_TEST_SKIP_R("Gaudi not available");
    }
    
    uct_md_resource_desc_t *md_resources;
    unsigned num_md_resources;
    
    ucs_status_t status = uct_gaudi_base_query_md_resources(&uct_gaudi_copy_component,
                                                          &md_resources, 
                                                          &num_md_resources);
    ASSERT_UCS_OK(status);
    EXPECT_GT(num_md_resources, 0u);
    EXPECT_NE(md_resources, (uct_md_resource_desc_t*)NULL);
    
    /* Check MD resource properties */
    EXPECT_STREQ("gaudi_copy", md_resources[0].md_name);
    
    ucs_free(md_resources);
}

// Test instantiation
static std::vector<test_md_param> gaudi_md_params() {
    std::vector<test_md_param> params;
    
    uct_md_resource_desc_t *md_resources;
    unsigned num_md_resources;
    
    ucs_status_t status = uct_gaudi_base_query_md_resources(&uct_gaudi_copy_component,
                                                          &md_resources, 
                                                          &num_md_resources);
    if (status == UCS_OK && num_md_resources > 0) {
        for (unsigned i = 0; i < num_md_resources; ++i) {
            test_md_param param;
            param.component = &uct_gaudi_copy_component;
            param.md_name = md_resources[i].md_name;
            params.push_back(param);
        }
        ucs_free(md_resources);
    }
    
    return params;
}

INSTANTIATE_TEST_SUITE_P(gaudi_md, test_gaudi_md,
                         ::testing::ValuesIn(gaudi_md_params()));
