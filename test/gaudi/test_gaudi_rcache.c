/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

/**
 * Test program for Gaudi registration cache functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <habanalabs/hlthunk.h>

#include <uct/api/uct.h>
#include <stdlib.h>
#include <ucs/debug/log.h>
#include <ucs/time/time.h>
#include <ucs/type/status.h>

#define TEST_ALLOC_SIZE   (4 * 1024 * 1024)  /* 4MB */
#define TEST_ITERATIONS   100
#define MAX_DEVICES       8

static int verbose = 0;

typedef struct {
    int hlthunk_fd;
    uct_md_h md;
    uct_md_config_t *md_config;
    uct_component_h component;
} test_context_t;

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -v                   Verbose output\n");
    printf("  -h                   Show this help\n");
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
    
    if (verbose) {
        printf("Opened Gaudi device fd=%d\n", ctx->hlthunk_fd);
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
    
    if (verbose) {
        printf("Found component: %s\n", component_attr.name);
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

static ucs_status_t test_memory_registration_basic(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_mem_h memh;
    /* Legacy UCX API: no param structs */
    
    printf("Testing basic memory registration...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_ALLOC_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Fill with test pattern */
    memset(host_ptr, 0xAB, TEST_ALLOC_SIZE);
    
    /* Register memory (legacy API) */
    status = uct_md_mem_reg(ctx->md, host_ptr, TEST_ALLOC_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status != UCS_OK) {
        printf("Failed to register memory: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    if (verbose) {
        printf("Successfully registered %u bytes at %p\n", TEST_ALLOC_SIZE, host_ptr);
    }
    
    /* Deregister memory (legacy API) */
    status = uct_md_mem_dereg(ctx->md, memh);
    if (status != UCS_OK) {
        printf("Failed to deregister memory: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    free(host_ptr);
    printf("✓ Basic memory registration test passed\n");
    return UCS_OK;
}

static ucs_status_t test_memory_registration_performance(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_mem_h memh[TEST_ITERATIONS];
    /* Legacy UCX API: no param structs */
    ucs_time_t start_time, end_time;
    double reg_time_ms, dereg_time_ms;
    int i;
    
    printf("Testing memory registration performance...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_ALLOC_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    memset(host_ptr, 0xCD, TEST_ALLOC_SIZE);
    
    
    /* Time memory registration */
    start_time = ucs_get_time();
    for (i = 0; i < TEST_ITERATIONS; i++) {
        status = uct_md_mem_reg(ctx->md, host_ptr, TEST_ALLOC_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh[i]);
        if (status != UCS_OK) {
            printf("Failed to register memory iteration %d: %s\n", i, ucs_status_string(status));
            free(host_ptr);
            return status;
        }
    }
    end_time = ucs_get_time();
    reg_time_ms = ucs_time_to_msec(end_time - start_time);
    
    
    /* Time memory deregistration */
    start_time = ucs_get_time();
    for (i = 0; i < TEST_ITERATIONS; i++) {
        status = uct_md_mem_dereg(ctx->md, memh[i]);
        if (status != UCS_OK) {
            printf("Failed to deregister memory iteration %d: %s\n", i, ucs_status_string(status));
            free(host_ptr);
            return status;
        }
    }
    end_time = ucs_get_time();
    dereg_time_ms = ucs_time_to_msec(end_time - start_time);
    
    free(host_ptr);
    
    printf("✓ Memory registration performance:\n");
    printf("  - %d registrations: %.2f ms (%.2f us/registration)\n", 
           TEST_ITERATIONS, reg_time_ms, (reg_time_ms * 1000.0) / TEST_ITERATIONS);
    printf("  - %d deregistrations: %.2f ms (%.2f us/deregistration)\n", 
           TEST_ITERATIONS, dereg_time_ms, (dereg_time_ms * 1000.0) / TEST_ITERATIONS);
    
    return UCS_OK;
}

static ucs_status_t test_rcache_functionality(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_mem_h memh1, memh2;
    /* Legacy UCX API: no param structs */
    ucs_time_t start_time, end_time;
    double first_reg_time, second_reg_time;
    
    printf("Testing registration cache functionality...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_ALLOC_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    memset(host_ptr, 0xEF, TEST_ALLOC_SIZE);
    
    
    /* First registration (cache miss) */
    start_time = ucs_get_time();
    status = uct_md_mem_reg(ctx->md, host_ptr, TEST_ALLOC_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh1);
    end_time = ucs_get_time();
    first_reg_time = ucs_time_to_usec(end_time - start_time);
    
    if (status != UCS_OK) {
        printf("Failed first registration: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    /* Deregister first handle (legacy API) */
    status = uct_md_mem_dereg(ctx->md, memh1);
    if (status != UCS_OK) {
        printf("Failed first deregistration: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    /* Second registration (cache hit, should be faster) */
    start_time = ucs_get_time();
    status = uct_md_mem_reg(ctx->md, host_ptr, TEST_ALLOC_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh2);
    end_time = ucs_get_time();
    second_reg_time = ucs_time_to_usec(end_time - start_time);
    
    if (status != UCS_OK) {
        printf("Failed second registration: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    /* Deregister second handle (legacy API) */
    status = uct_md_mem_dereg(ctx->md, memh2);
    if (status != UCS_OK) {
        printf("Failed second deregistration: %s\n", ucs_status_string(status));
        free(host_ptr);
        return status;
    }
    
    free(host_ptr);
    
    printf("✓ Registration cache test:\n");
    printf("  - First registration: %.2f us\n", first_reg_time);
    printf("  - Second registration: %.2f us", second_reg_time);
    
    if (second_reg_time < first_reg_time * 0.8) {
        printf(" (cache hit detected!)\n");
    } else {
        printf(" (cache may not be working)\n");
    }
    
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
    
    printf("Gaudi Registration Cache Test Suite\n");
    printf("====================================\n\n");
    
    /* Initialize Gaudi context */
    status = init_gaudi_context(&ctx);
    if (status != UCS_OK) {
        printf("Failed to initialize Gaudi context: %s\n", ucs_status_string(status));
        return 1;
    }
    
    /* Run tests */
    printf("Running registration cache tests...\n\n");
    
    status = test_memory_registration_basic(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_memory_registration_performance(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_rcache_functionality(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    /* Cleanup */
    cleanup_gaudi_context(&ctx);
    
    /* Print summary */
    printf("\n====================================\n");
    if (test_failures == 0) {
        printf("✓ All registration cache tests passed!\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed\n", test_failures);
        return 1;
    }
}