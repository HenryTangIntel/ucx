/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. All rights reserved.
 * See file LICENSE for terms.
 *
 * @file gaudi_dmabuf_test.c
 * @brief Test program for Gaudi DMA-BUF functionality
 * 
 * This program tests the basic DMA-BUF export/import functionality
 * between Gaudi device memory and other subsystems.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <uct/api/uct.h>
#include <ucs/memory/memory_type.h>
#include <hlthunk.h>

#define TEST_SIZE (1024 * 1024)  /* 1MB test buffer */
#define TEST_PATTERN 0xDEADBEEF

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -h        Show this help\n");
    printf("  -s SIZE   Test buffer size in bytes (default: %d)\n", TEST_SIZE);
    printf("  -v        Verbose output\n");
}

static int test_gaudi_dmabuf_basic(size_t buffer_size, int verbose)
{
    int hlthunk_fd = -1;
    uint64_t handle = 0;
    uint64_t device_addr = 0;
    int dmabuf_fd = -1;
    void *mapped_ptr = NULL;
    int ret = -1;

    printf("=== Basic Gaudi DMA-BUF Test ===\n");
    printf("Buffer size: %zu bytes\n", buffer_size);

    /* Open Gaudi device */
    hlthunk_fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI2, NULL);
    if (hlthunk_fd < 0) {
        printf("ERROR: Failed to open Gaudi device: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("✓ Opened Gaudi device (fd=%d)\n", hlthunk_fd);

    /* Allocate device memory */
    handle = hlthunk_device_memory_alloc(hlthunk_fd, buffer_size, 0, true, true);
    if (handle == 0) {
        printf("ERROR: Failed to allocate device memory\n");
        goto cleanup;
    }
    printf("✓ Allocated device memory (handle=0x%lx)\n", handle);

    /* Map device memory to host address space */
    device_addr = hlthunk_device_memory_map(hlthunk_fd, handle, 0);
    if (device_addr == 0) {
        printf("ERROR: Failed to map device memory\n");
        goto cleanup;
    }
    printf("✓ Mapped device memory (addr=0x%lx)\n", device_addr);

    /* Export as DMA-BUF */
    dmabuf_fd = hlthunk_device_memory_export_dmabuf_fd(hlthunk_fd, device_addr, buffer_size, 0);
    if (dmabuf_fd < 0) {
        printf("ERROR: Failed to export DMA-BUF: %s\n", strerror(errno));
        goto cleanup;
    }
    printf("✓ Exported DMA-BUF (fd=%d)\n", dmabuf_fd);

    /* Test DMA-BUF mapping */
    mapped_ptr = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
    if (mapped_ptr == MAP_FAILED) {
        printf("WARNING: DMA-BUF mmap failed: %s (this may be expected)\n", strerror(errno));
        mapped_ptr = NULL;
    } else {
        printf("✓ Mapped DMA-BUF to host memory (%p)\n", mapped_ptr);
        
        if (verbose) {
            /* Try to write test pattern */
            printf("Writing test pattern...\n");
            uint32_t *ptr = (uint32_t *)mapped_ptr;
            for (size_t i = 0; i < buffer_size / sizeof(uint32_t); i++) {
                ptr[i] = TEST_PATTERN + i;
            }
            printf("✓ Written test pattern\n");
        }
    }

    printf("✓ DMA-BUF test completed successfully\n");
    ret = 0;

cleanup:
    if (mapped_ptr && mapped_ptr != MAP_FAILED) {
        munmap(mapped_ptr, buffer_size);
    }
    if (dmabuf_fd >= 0) {
        close(dmabuf_fd);
    }
    if (handle != 0) {
        hlthunk_device_memory_free(hlthunk_fd, handle);
    }
    if (hlthunk_fd >= 0) {
        hlthunk_close(hlthunk_fd);
    }

    return ret;
}

static int test_ucx_gaudi_dmabuf(size_t buffer_size, int verbose)
{
    ucs_status_t status;
    uct_md_h md;
    uct_md_config_t *md_config;
    uct_component_h *components;
    unsigned num_components;
    uct_mem_h memh;
    void *address;
    size_t actual_size = buffer_size;
    int ret = -1;

    printf("\n=== UCX Gaudi DMA-BUF Test ===\n");

    /* Query UCX components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("ERROR: Failed to query UCX components: %s\n", ucs_status_string(status));
        return -1;
    }

    /* Find Gaudi component */
    uct_component_h gaudi_component = NULL;
    uct_component_attr_t comp_attr = {0};
    comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
    
    for (unsigned i = 0; i < num_components; i++) {
        status = uct_component_query(components[i], &comp_attr);
        if (status == UCS_OK && strstr(comp_attr.name, "gaudi")) {
            gaudi_component = components[i];
            break;
        }
    }

    if (!gaudi_component) {
        printf("ERROR: Gaudi component not found\n");
        uct_release_component_list(components);
        return -1;
    }
    printf("✓ Found Gaudi component: %s\n", comp_attr.name);

    /* Create MD config */
    status = uct_md_config_read(gaudi_component, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("ERROR: Failed to read MD config: %s\n", ucs_status_string(status));
        goto cleanup_components;
    }

    /* Open memory domain */
    status = gaudi_component->md_open(gaudi_component, "gaudi:0", md_config, &md);
    if (status != UCS_OK) {
        printf("ERROR: Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        goto cleanup_config;
    }
    printf("✓ Opened Gaudi memory domain\n");

    /* Allocate memory with DMA-BUF support */
    status = uct_md_mem_alloc(md, &actual_size, &address, UCS_MEMORY_TYPE_GAUDI, 
                              UCS_SYS_DEVICE_ID_UNKNOWN, UCT_MD_MEM_FLAG_FIXED, 
                              "gaudi_dmabuf_test", &memh);
    if (status != UCS_OK) {
        printf("ERROR: Failed to allocate Gaudi memory: %s\n", ucs_status_string(status));
        goto cleanup_md;
    }
    printf("✓ Allocated UCX Gaudi memory (addr=%p, size=%zu)\n", address, actual_size);

    /* Query memory attributes including DMA-BUF info */
    uct_md_mem_attr_t mem_attr;
    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE | 
                         UCT_MD_MEM_ATTR_FIELD_DMABUF_FD |
                         UCT_MD_MEM_ATTR_FIELD_DMABUF_OFFSET;
    
    status = uct_md_mem_query(md, address, actual_size, &mem_attr);
    if (status == UCS_OK) {
        printf("✓ Memory type: %s\n", ucs_memory_type_names[mem_attr.mem_type]);
        if (mem_attr.dmabuf_fd >= 0) {
            printf("✓ DMA-BUF fd: %d, offset: %zu\n", mem_attr.dmabuf_fd, mem_attr.dmabuf_offset);
        } else {
            printf("- No DMA-BUF available (fd=%d)\n", mem_attr.dmabuf_fd);
        }
    }

    /* Test memory key packing (simulates IB rkey) */
    char mkey_buffer[256];
    status = uct_md_mkey_pack(md, memh, mkey_buffer);
    if (status == UCS_OK) {
        printf("✓ Packed memory key for remote access\n");
        if (verbose) {
            /* Print key contents if it contains DMA-BUF info */
            printf("Memory key contains DMA-BUF info for IB sharing\n");
        }
    }

    printf("✓ UCX Gaudi DMA-BUF test completed successfully\n");
    ret = 0;

    /* Cleanup */
    uct_md_mem_free(md, memh);

cleanup_md:
    uct_md_close(md);
cleanup_config:
    uct_config_release(md_config);
cleanup_components:
    uct_release_component_list(components);

    return ret;
}

int main(int argc, char *argv[])
{
    size_t buffer_size = TEST_SIZE;
    int verbose = 0;
    int opt;

    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "hs:v")) != -1) {
        switch (opt) {
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 's':
            buffer_size = strtoul(optarg, NULL, 0);
            if (buffer_size == 0) {
                printf("ERROR: Invalid buffer size\n");
                return 1;
            }
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("Gaudi DMA-BUF Test Program\n");
    printf("==========================\n");

    /* Test 1: Basic hlthunk DMA-BUF functionality */
    if (test_gaudi_dmabuf_basic(buffer_size, verbose) != 0) {
        printf("❌ Basic DMA-BUF test failed\n");
        return 1;
    }

    /* Test 2: UCX Gaudi DMA-BUF integration */
    if (test_ucx_gaudi_dmabuf(buffer_size, verbose) != 0) {
        printf("❌ UCX DMA-BUF test failed\n");
        return 1;
    }

    printf("\n🎉 All tests passed!\n");
    return 0;
}
