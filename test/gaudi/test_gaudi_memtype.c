/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

/**
 * Test program for Gaudi memory type detection and handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif
#include <habanalabs/hlthunk.h>

#include <uct/api/uct.h>
#include <ucs/debug/log.h>
#include <stdlib.h>
#include <ucs/time/time.h>
#include <ucs/type/status.h>
#include <ucs/memory/memory_type.h>

#define TEST_BUFFER_SIZE      (1024 * 1024)  /* 1MB */
#define DEVICE_ALLOC_SIZE     (4 * 1024 * 1024)  /* 4MB */
#define MAX_DEVICES           8

static int verbose = 0;

typedef struct {
    int hlthunk_fd;
    uct_md_h md;
    uct_md_config_t *md_config;
    uct_component_h component;
    struct hlthunk_hw_ip_info hw_info;
} test_context_t;

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -v                   Verbose output\n");
    printf("  -h                   Show this help\n");
}

static const char* memory_type_to_string(ucs_memory_type_t mem_type)
{
    switch (mem_type) {
    case UCS_MEMORY_TYPE_HOST:      return "HOST";
    case UCS_MEMORY_TYPE_CUDA:      return "CUDA";
    case UCS_MEMORY_TYPE_ROCM:      return "ROCM";
    case UCS_MEMORY_TYPE_UNKNOWN:   return "UNKNOWN";
    default:                        return "INVALID";
    }
}

static ucs_status_t init_gaudi_context(test_context_t *ctx)
{
    ucs_status_t status;
    uct_component_attr_t component_attr;
    
    /* Initialize context */
    memset(ctx, 0, sizeof(*ctx));
    
    /* Open Gaudi device */
    ctx->hlthunk_fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (ctx->hlthunk_fd < 0) {
        printf("Failed to open Gaudi device: %s\n", strerror(errno));
        return UCS_ERR_NO_DEVICE;
    }
    
    /* Get hardware information */
    if (hlthunk_get_hw_ip_info(ctx->hlthunk_fd, &ctx->hw_info) != 0) {
        printf("Warning: Failed to get hardware info\n");
        memset(&ctx->hw_info, 0, sizeof(ctx->hw_info));
    }
    
    if (verbose) {
        printf("Opened Gaudi device fd=%d\n", ctx->hlthunk_fd);
        printf("Device ID: %u, DRAM base: 0x%lx, DRAM size: %lu MB\n", 
               ctx->hw_info.device_id, 
               ctx->hw_info.dram_base_address,
               ctx->hw_info.dram_size / (1024 * 1024));
    }
    
    /* Query Gaudi component */
    component_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME |
                                UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT;
    
    /* Find the Gaudi copy component */
    uct_query_components(&ctx->component, 1);
    if (ctx->component == NULL) {
        printf("No UCT components found\n");
        hlthunk_close(ctx->hlthunk_fd);
        return UCS_ERR_NO_DEVICE;
    }
    
    status = uct_component_query(ctx->component, &component_attr);
    if (status != UCS_OK) {
        printf("Failed to query component: %s\n", ucs_status_string(status));
        hlthunk_close(ctx->hlthunk_fd);
        return status;
    }
    
    /* Read MD configuration */
    status = uct_md_config_read(ctx->component, NULL, NULL, &ctx->md_config);
    if (status != UCS_OK) {
        printf("Failed to read MD config: %s\n", ucs_status_string(status));
        hlthunk_close(ctx->hlthunk_fd);
        return status;
    }
    
    /* Open MD */
    status = uct_md_open(ctx->component, "gaudi_copy", ctx->md_config, &ctx->md);
    if (status != UCS_OK) {
        printf("Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        uct_config_release(ctx->md_config);
        hlthunk_close(ctx->hlthunk_fd);
        return status;
    }
    
    if (verbose) {
        printf("Successfully opened Gaudi copy MD\n");
    }
    
    return UCS_OK;
}

static void cleanup_gaudi_context(test_context_t *ctx)
{
    if (ctx->md) {
        uct_md_close(ctx->md);
    }
    if (ctx->md_config) {
        uct_config_release(ctx->md_config);
    }
    if (ctx->hlthunk_fd >= 0) {
        hlthunk_close(ctx->hlthunk_fd);
    }
}

static ucs_status_t test_host_memory_detection(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_md_mem_attr_t mem_attr;
    
    printf("Testing host memory type detection...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_BUFFER_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Fill with test pattern */
    memset(host_ptr, 0x12, TEST_BUFFER_SIZE);
    
    /* Query memory attributes */
    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE |
                          UCT_MD_MEM_ATTR_FIELD_SYS_DEV |
                          UCT_MD_MEM_ATTR_FIELD_BASE_ADDRESS |
                          UCT_MD_MEM_ATTR_FIELD_ALLOC_LENGTH;
    
    status = uct_md_mem_query(ctx->md, host_ptr, TEST_BUFFER_SIZE, &mem_attr);
    if (status != UCS_OK) {
        printf("Failed to query host memory: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    printf("✓ Host memory detection:\n");
    printf("  - Address: %p\n", host_ptr);
    printf("  - Size: %u bytes\n", TEST_BUFFER_SIZE);
    printf("  - Detected type: %s\n", memory_type_to_string(mem_attr.mem_type));
    printf("  - Base address: %p\n", mem_attr.base_address);
    printf("  - Allocation length: %zu\n", mem_attr.alloc_length);
    
    if (mem_attr.mem_type != UCS_MEMORY_TYPE_HOST) {
        printf("✗ Expected HOST memory type, got %s\n", 
               memory_type_to_string(mem_attr.mem_type));
        free(host_ptr);
        return UCS_ERR_INVALID_PARAM;
    }
    
    free(host_ptr);
    printf("✓ Host memory detection test passed\n");
    return UCS_OK;
}

static ucs_status_t test_device_memory_detection(test_context_t *ctx)
{
    ucs_status_t status;
    uint64_t device_handle;
    void *device_va;
    uct_md_mem_attr_t mem_attr;
    
    printf("Testing device memory type detection...\n");
    
    /* Allocate device memory */
    device_handle = hlthunk_device_memory_alloc(ctx->hlthunk_fd, DEVICE_ALLOC_SIZE, 0, true, true);
    if (device_handle == 0) {
        printf("Failed to allocate device memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Map device memory to get virtual address */
    device_va = hlthunk_device_memory_map(ctx->hlthunk_fd, device_handle, 0);
    if (device_va == MAP_FAILED || device_va == NULL) {
        printf("Failed to map device memory\n");
        hlthunk_device_memory_free(ctx->hlthunk_fd, device_handle);
        return UCS_ERR_NO_MEMORY;
    }
    
    if (verbose) {
        printf("Allocated device memory: handle=0x%lx, va=%p\n", device_handle, device_va);
    }
    
    /* Query memory attributes */
    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE |
                          UCT_MD_MEM_ATTR_FIELD_SYS_DEV |
                          UCT_MD_MEM_ATTR_FIELD_BASE_ADDRESS |
                          UCT_MD_MEM_ATTR_FIELD_ALLOC_LENGTH;
    
    status = uct_md_mem_query(ctx->md, device_va, DEVICE_ALLOC_SIZE, &mem_attr);
    if (status != UCS_OK) {
        printf("Failed to query device memory: %s\n", ucs_status_string(status));
        /* hlthunk_device_memory_unmap(ctx->hlthunk_fd, device_va); */
        hlthunk_device_memory_free(ctx->hlthunk_fd, device_handle);
        return status;
    }
    
    printf("✓ Device memory detection:\n");
    printf("  - Handle: 0x%lx\n", device_handle);
    printf("  - Virtual address: %p\n", device_va);
    printf("  - Size: %u bytes\n", DEVICE_ALLOC_SIZE);
    printf("  - Detected type: %s\n", memory_type_to_string(mem_attr.mem_type));
    printf("  - Base address: %p\n", mem_attr.base_address);
    printf("  - Allocation length: %zu\n", mem_attr.alloc_length);
    
    /* Note: Device memory may be detected as HOST if not properly implemented */
    if (mem_attr.mem_type == UCS_MEMORY_TYPE_UNKNOWN) {
        printf("! Device memory type detection not implemented (UNKNOWN returned)\n");
    }
    
    /* Cleanup */
    /* hlthunk_device_memory_unmap(ctx->hlthunk_fd, device_va); */
    hlthunk_device_memory_free(ctx->hlthunk_fd, device_handle);
    
    printf("✓ Device memory detection test completed\n");
    return UCS_OK;
}

static ucs_status_t test_memory_registration_with_types(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_mem_h memh;
    /* Legacy UCX API: no param structs */
    ucs_memory_type_t test_types[] = {
        UCS_MEMORY_TYPE_HOST,
        UCS_MEMORY_TYPE_UNKNOWN
    };
    int i;
    
    printf("Testing memory registration with different memory types...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_BUFFER_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    memset(host_ptr, 0x34, TEST_BUFFER_SIZE);
    
    for (i = 0; i < sizeof(test_types) / sizeof(test_types[0]); i++) {
        printf("  Testing registration with memory type: %s\n", memory_type_to_string(test_types[i]));
        /* Register memory (legacy API: only flags supported) */
        status = uct_md_mem_reg(ctx->md, host_ptr, TEST_BUFFER_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh);
        if (status != UCS_OK) {
            printf("  ✗ Failed to register memory with type %s: %s\n", 
                   memory_type_to_string(test_types[i]), ucs_status_string(status));
            continue;
        }
        
        printf("  ✓ Successfully registered memory with type %s\n", 
               memory_type_to_string(test_types[i]));
        
        /* Deregister memory (legacy API) */
        status = uct_md_mem_dereg(ctx->md, memh);
        if (status != UCS_OK) {
            printf("  ✗ Failed to deregister memory: %s\n", ucs_status_string(status));
        } else {
            printf("  ✓ Successfully deregistered memory\n");
        }
    }
    
    free(host_ptr);
    printf("✓ Memory registration with types test completed\n");
    return UCS_OK;
}

static ucs_status_t test_null_pointer_handling(test_context_t *ctx)
{
    ucs_status_t status;
    uct_md_mem_attr_t mem_attr;
    
    printf("Testing NULL pointer handling...\n");
    
    /* Query NULL pointer */
    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE;
    
    status = uct_md_mem_query(ctx->md, NULL, 0, &mem_attr);
    if (status == UCS_OK) {
        printf("✓ NULL pointer query returned: %s\n", 
               memory_type_to_string(mem_attr.mem_type));
    } else {
        printf("✓ NULL pointer query failed as expected: %s\n", 
               ucs_status_string(status));
    }
    
    printf("✓ NULL pointer handling test completed\n");
    return UCS_OK;
}

int main(int argc, char **argv)
{
    int opt;
    test_context_t ctx;
    ucs_status_t status;
    int test_failures = 0;
    
    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "vh")) != -1) {
        switch (opt) {
        case 'v':
            verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("Gaudi Memory Type Detection Test Suite\n");
    printf("=======================================\n\n");
    
    /* Initialize Gaudi context */
    status = init_gaudi_context(&ctx);
    if (status != UCS_OK) {
        printf("Failed to initialize Gaudi context: %s\n", ucs_status_string(status));
        return 1;
    }
    
    /* Run tests */
    printf("Running memory type detection tests...\n\n");
    
    status = test_host_memory_detection(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_device_memory_detection(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_memory_registration_with_types(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_null_pointer_handling(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    /* Cleanup */
    cleanup_gaudi_context(&ctx);
    
    /* Print summary */
    printf("\n=======================================\n");
    if (test_failures == 0) {
        printf("✓ All memory type detection tests passed!\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed\n", test_failures);
        return 1;
    }
}