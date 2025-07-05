/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include <uct/uct_test.h>

extern "C" {
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <uct/gaudi/copy/gaudi_copy_iface.h>
#include <uct/gaudi/ipc/gaudi_ipc_md.h>
#include <uct/gaudi/ipc/gaudi_ipc_iface.h>
#include <habanalabs/hlthunk.h>
}

class test_gaudi_transport : public uct_test {
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
    
    static bool is_gaudi_available() {
        int device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
        return device_count > 0;
    }
    
    /* Helper to allocate and register memory */
    void* alloc_and_register(size_t size, uct_mem_h *memh_p, 
                           ucs_memory_type_t mem_type = UCS_MEMORY_TYPE_HOST) {
        void *ptr;
        ucs_status_t status;
        
        if (mem_type == UCS_MEMORY_TYPE_GAUDI) {
            /* Allocate Gaudi memory using MD */
            status = uct_mem_alloc(md(), &size, &ptr, mem_type, 
                                 UCS_SYS_DEVICE_ID_UNKNOWN, 0, "test", memh_p);
            if (status != UCS_OK) {
                return NULL;
            }
        } else {
            /* Allocate host memory */
            ptr = malloc(size);
            if (ptr == NULL) {
                return NULL;
            }
            
            /* Register the memory */
            status = uct_mem_reg(md(), ptr, size, UCT_MD_MEM_ACCESS_ALL, memh_p);
            if (status != UCS_OK) {
                free(ptr);
                return NULL;
            }
        }
        
        return ptr;
    }
    
    void free_and_deregister(void *ptr, uct_mem_h memh, 
                           ucs_memory_type_t mem_type = UCS_MEMORY_TYPE_HOST) {
        if (mem_type == UCS_MEMORY_TYPE_GAUDI) {
            uct_mem_free(md(), memh);
        } else {
            uct_mem_dereg(md(), memh);
            free(ptr);
        }
    }

protected:
    std::vector<std::string> m_component_names;
};

UCS_TEST_P(test_gaudi_transport, basic_connectivity) {
    const size_t size = 1024;
    
    void *send_buffer = alloc_and_register(size, &m_send_memh);
    void *recv_buffer = alloc_and_register(size, &m_recv_memh);
    
    if (send_buffer == NULL || recv_buffer == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate memory");
    }
    
    /* Fill send buffer with pattern */
    uint32_t pattern = 0xCAFEBABE;
    uint32_t *send_data = (uint32_t*)send_buffer;
    for (size_t i = 0; i < size / sizeof(uint32_t); ++i) {
        send_data[i] = pattern + i;
    }
    
    /* Clear receive buffer */
    memset(recv_buffer, 0, size);
    
    /* Test PUT operation if supported */
    if (UCS_OK == uct_iface_put_zcopy(sender().iface(), NULL, 0, 
                                     (uintptr_t)recv_buffer, 
                                     pack_rkey(), NULL)) {
        /* PUT is supported, but we can't test it easily without proper setup */
        UCS_TEST_MESSAGE << "PUT operation is supported by transport";
    }
    
    /* Test GET operation if supported */
    if (UCS_OK == uct_iface_get_zcopy(sender().iface(), NULL, 0,
                                     (uintptr_t)send_buffer,
                                     pack_rkey(), NULL)) {
        /* GET is supported */
        UCS_TEST_MESSAGE << "GET operation is supported by transport";
    }
    
    free_and_deregister(send_buffer, m_send_memh);
    free_and_deregister(recv_buffer, m_recv_memh);
}

UCS_TEST_P(test_gaudi_transport, memory_type_support) {
    uct_md_attr_t md_attr;
    ucs_status_t status = uct_md_query(md(), &md_attr);
    ASSERT_UCS_OK(status);
    
    /* Check memory type support */
    EXPECT_TRUE(md_attr.cap.alloc_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    EXPECT_TRUE(md_attr.cap.reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    EXPECT_TRUE(md_attr.cap.access_mem_types & UCS_BIT(UCS_MEMORY_TYPE_HOST));
    
    if (GetParam().component_name == "gaudi_copy") {
        EXPECT_TRUE(md_attr.cap.alloc_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
        EXPECT_TRUE(md_attr.cap.reg_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
        EXPECT_TRUE(md_attr.cap.access_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
        EXPECT_TRUE(md_attr.cap.detect_mem_types & UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    }
}

UCS_TEST_P(test_gaudi_transport, iface_attributes) {
    uct_iface_attr_t iface_attr;
    ucs_status_t status = uct_iface_query(sender().iface(), &iface_attr);
    ASSERT_UCS_OK(status);
    
    /* Check basic interface properties */
    EXPECT_GT(iface_attr.bandwidth.dedicated, 0.0);
    EXPECT_GT(iface_attr.bandwidth.shared, 0.0);
    EXPECT_GE(iface_attr.latency.overhead, 0.0);
    EXPECT_GE(iface_attr.latency.growth, 0.0);
    
    /* Check transport-specific capabilities */
    if (GetParam().component_name == "gaudi_copy") {
        EXPECT_TRUE(iface_attr.cap.flags & UCT_IFACE_FLAG_PUT_ZCOPY);
        EXPECT_TRUE(iface_attr.cap.flags & UCT_IFACE_FLAG_GET_ZCOPY);
    }
    
    if (GetParam().component_name == "gaudi_ipc") {
        EXPECT_TRUE(iface_attr.cap.flags & UCT_IFACE_FLAG_PUT_ZCOPY);
        EXPECT_TRUE(iface_attr.cap.flags & UCT_IFACE_FLAG_GET_ZCOPY);
    }
}

UCS_TEST_P(test_gaudi_transport, endpoint_creation) {
    uct_ep_h ep;
    uct_ep_params_t ep_params;
    
    ep_params.field_mask = UCT_EP_PARAM_FIELD_IFACE;
    ep_params.iface = sender().iface();
    
    ucs_status_t status = uct_ep_create(&ep_params, &ep);
    if (status == UCS_ERR_UNSUPPORTED) {
        UCS_TEST_SKIP_R("Endpoint creation not supported");
    }
    
    ASSERT_UCS_OK(status);
    EXPECT_NE(ep, (uct_ep_h)NULL);
    
    /* Test endpoint destroy */
    uct_ep_destroy(ep);
}

UCS_TEST_P(test_gaudi_transport, memory_operations) {
    const size_t size = 4096;
    
    /* Test with host memory */
    void *host_buffer = alloc_and_register(size, &m_send_memh, UCS_MEMORY_TYPE_HOST);
    if (host_buffer == NULL) {
        UCS_TEST_SKIP_R("Failed to allocate host memory");
    }
    
    /* Fill with pattern */
    memset(host_buffer, 0xAB, size);
    
    /* Test memory registration worked */
    EXPECT_NE(m_send_memh, (uct_mem_h)NULL);
    
    /* Test remote key packing */
    void *rkey_buffer;
    ucs_status_t status = uct_md_mkey_pack(md(), m_send_memh, &rkey_buffer);
    ASSERT_UCS_OK(status);
    EXPECT_NE(rkey_buffer, (void*)NULL);
    
    /* Test remote key unpacking */
    uct_rkey_t rkey;
    status = uct_rkey_unpack(component(), rkey_buffer, &rkey);
    ASSERT_UCS_OK(status);
    
    /* Release remote key */
    status = uct_rkey_release(component(), rkey, rkey_buffer);
    ASSERT_UCS_OK(status);
    
    free_and_deregister(host_buffer, m_send_memh, UCS_MEMORY_TYPE_HOST);
    
    /* Test with Gaudi memory if supported and available */
    if (GetParam().component_name == "gaudi_copy") {
        void *gaudi_buffer = alloc_and_register(size, &m_recv_memh, UCS_MEMORY_TYPE_GAUDI);
        if (gaudi_buffer != NULL) {
            UCS_TEST_MESSAGE << "Successfully allocated and registered Gaudi memory";
            free_and_deregister(gaudi_buffer, m_recv_memh, UCS_MEMORY_TYPE_GAUDI);
        }
    }
}

UCS_TEST_P(test_gaudi_transport, error_handling) {
    /* Test invalid memory registration */
    uct_mem_h memh;
    ucs_status_t status = uct_mem_reg(md(), NULL, 1024, UCT_MD_MEM_ACCESS_ALL, &memh);
    EXPECT_NE(UCS_OK, status);
    
    /* Test zero-size allocation */
    void *ptr;
    size_t size = 0;
    status = uct_mem_alloc(md(), &size, &ptr, UCS_MEMORY_TYPE_HOST,
                          UCS_SYS_DEVICE_ID_UNKNOWN, 0, "test", &memh);
    EXPECT_TRUE((status == UCS_OK) || (status == UCS_ERR_INVALID_PARAM));
    
    if (status == UCS_OK) {
        uct_mem_free(md(), memh);
    }
}

UCS_TEST_P(test_gaudi_transport, component_capabilities) {
    uct_component_attr_t comp_attr;
    comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME |
                          UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT |
                          UCT_COMPONENT_ATTR_FIELD_FLAGS;
    
    ucs_status_t status = uct_component_query(component(), &comp_attr);
    ASSERT_UCS_OK(status);
    
    EXPECT_STREQ(GetParam().component_name.c_str(), comp_attr.name);
    EXPECT_GT(comp_attr.md_resource_count, 0u);
}

UCS_TEST_P(test_gaudi_transport, stress_allocations) {
    const size_t num_allocs = 50;
    const size_t base_size = 1024;
    
    std::vector<void*> pointers;
    std::vector<uct_mem_h> memhs;
    
    /* Allocate multiple buffers */
    for (size_t i = 0; i < num_allocs; ++i) {
        size_t size = base_size * (i + 1);
        uct_mem_h memh;
        
        void *ptr = alloc_and_register(size, &memh, UCS_MEMORY_TYPE_HOST);
        if (ptr != NULL) {
            pointers.push_back(ptr);
            memhs.push_back(memh);
            
            /* Fill with pattern */
            memset(ptr, (int)(i & 0xFF), size);
        }
    }
    
    EXPECT_GT(pointers.size(), 0u) << "No allocations succeeded";
    
    /* Verify patterns and free */
    for (size_t i = 0; i < pointers.size(); ++i) {
        size_t size = base_size * (i + 1);
        uint8_t expected = (uint8_t)(i & 0xFF);
        uint8_t *data = (uint8_t*)pointers[i];
        
        /* Check first and last bytes */
        EXPECT_EQ(expected, data[0]) << "Pattern mismatch at allocation " << i;
        EXPECT_EQ(expected, data[size-1]) << "Pattern mismatch at allocation " << i;
        
        free_and_deregister(pointers[i], memhs[i], UCS_MEMORY_TYPE_HOST);
    }
}

/* Test instantiation for both copy and IPC transports */
UCT_INSTANTIATE_TEST_CASE_TLS(test_gaudi_transport, gaudi_copy, "gaudi_cpy")
UCT_INSTANTIATE_TEST_CASE_TLS(test_gaudi_transport, gaudi_ipc, "gaudi_ipc")
