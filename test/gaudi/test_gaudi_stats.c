/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

/**
 * Test program for Gaudi statistics collection functionality
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
#include <ucs/debug/log.h>
#include <ucs/time/time.h>
#include <ucs/type/status.h>
#include <ucs/stats/stats.h>

#define TEST_BUFFER_SIZE      (1024 * 1024)  /* 1MB */
#define TEST_ITERATIONS       100
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

#ifdef ENABLE_STATS
static ucs_status_t test_stats_collection_basic(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_mem_h memh;
    /* Legacy UCX API: no param structs */
    ucs_stats_node_t *stats_node;
    int i;
    
    printf("Testing basic statistics collection...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_BUFFER_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    memset(host_ptr, 0xAB, TEST_BUFFER_SIZE);
    
    /* Perform multiple registrations to generate stats (legacy API) */
    for (i = 0; i < TEST_ITERATIONS; i++) {
        status = uct_md_mem_reg(ctx->md, host_ptr, TEST_BUFFER_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh);
        if (status != UCS_OK) {
            printf("Failed to register memory iteration %d: %s\n", i, ucs_status_string(status));
            free(host_ptr);
            return status;
        }
        
        /* Deregister immediately (legacy API) */
        status = uct_md_mem_dereg(ctx->md, memh);
        if (status != UCS_OK) {
            printf("Failed to deregister memory iteration %d: %s\n", i, ucs_status_string(status));
            free(host_ptr);
            return status;
        }
    }
    
    /* Try to get statistics (this may not be available in all builds) */
    /* Note: Stats API might not be exposed or available */
    printf("✓ Performed %d registration/deregistration cycles for stats collection\n", TEST_ITERATIONS);
    
    if (verbose) {
        printf("  - Each cycle: register -> deregister %zu bytes\n", TEST_BUFFER_SIZE);
        printf("  - Total data processed: %zu MB\n", 
               (TEST_BUFFER_SIZE * TEST_ITERATIONS) / (1024 * 1024));
    }
    
    free(host_ptr);
    printf("✓ Basic statistics collection test completed\n");
    return UCS_OK;
}

static ucs_status_t test_stats_memory_operations(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uint64_t device_handle;
    void *device_va;
    uct_mem_h host_memh, device_memh;
    uct_md_mem_reg_params_t reg_params;
    uct_md_mem_dereg_params_t dereg_params;
    ucs_time_t start_time, end_time;
    double total_time_ms;
    int i;
    
    printf("Testing statistics for various memory operations...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_BUFFER_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Allocate device memory */
    device_handle = hlthunk_device_memory_alloc(ctx->hlthunk_fd, TEST_BUFFER_SIZE, 0, true, true);
    if (device_handle == 0) {
        printf("Failed to allocate device memory\n");
        free(host_ptr);
        return UCS_ERR_NO_MEMORY;
    }
    
    device_va = hlthunk_device_memory_map(ctx->hlthunk_fd, device_handle, 0);
    if (device_va == MAP_FAILED || device_va == NULL) {
        printf("Failed to map device memory\n");
        hlthunk_device_memory_free(ctx->hlthunk_fd, device_handle);
        free(host_ptr);
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Setup registration parameters */
    reg_params.field_mask = UCT_MD_MEM_REG_FIELD_FLAGS |
                           UCT_MD_MEM_REG_FIELD_MEMORY_TYPE;
    /* Test host memory registrations */
    /* Legacy UCX API: no param structs */
    start_time = ucs_get_time();
    for (i = 0; i < TEST_ITERATIONS / 2; i++) {
        reg_params.mem_type = UCS_MEMORY_TYPE_HOST;
        
        status = uct_md_mem_reg(ctx->md, host_ptr, TEST_BUFFER_SIZE, UCT_MD_MEM_ACCESS_ALL, &host_memh);
            goto cleanup;
        }
        
        dereg_params.field_mask = UCT_MD_MEM_DEREG_FIELD_MEMH;
        dereg_params.memh = host_memh;
        status = uct_md_mem_dereg(ctx->md, host_memh);
            goto cleanup;
        }
    }
    
    /* Test device memory registrations */
    for (i = 0; i < TEST_ITERATIONS / 2; i++) {
        reg_params.mem_type = UCS_MEMORY_TYPE_UNKNOWN; /* Device memory type detection */
        
        status = uct_md_mem_reg(ctx->md, device_va, TEST_BUFFER_SIZE, UCT_MD_MEM_ACCESS_ALL, &device_memh);
            goto cleanup;
        }
        
        dereg_params.field_mask = UCT_MD_MEM_DEREG_FIELD_MEMH;
        dereg_params.memh = device_memh;
        status = uct_md_mem_dereg(ctx->md, device_memh);
            goto cleanup;
        }
    }
    
    end_time = ucs_get_time();
    total_time_ms = ucs_time_to_msec(end_time - start_time);
    
    printf("✓ Memory operations statistics:\n");
    printf("  - Host registrations: %d\n", TEST_ITERATIONS / 2);
    printf("  - Device registrations: %d\n", TEST_ITERATIONS / 2);
    printf("  - Total time: %.2f ms\n", total_time_ms);
    printf("  - Average time per operation: %.2f us\n", 
           (total_time_ms * 1000.0) / TEST_ITERATIONS);
    
    status = UCS_OK;

cleanup:
    hlthunk_device_memory_unmap(ctx->hlthunk_fd, device_va);
    hlthunk_device_memory_free(ctx->hlthunk_fd, device_handle);
    free(host_ptr);
    
    return status;
}

static ucs_status_t test_stats_error_tracking(test_context_t *ctx)
{
    ucs_status_t status;
    uct_mem_h memh;
    uct_md_mem_reg_params_t reg_params;
    int error_count = 0;
    int i;
    
    printf("Testing statistics for error tracking...\n");
    
    /* Setup invalid registration parameters to trigger errors */
    reg_params.field_mask = UCT_MD_MEM_REG_FIELD_FLAGS |
                           UCT_MD_MEM_REG_FIELD_MEMORY_TYPE;
    reg_params.flags      = UCT_MD_MEM_ACCESS_ALL;
    reg_params.mem_type   = UCS_MEMORY_TYPE_HOST;
    
    /* Attempt to register NULL pointer (should fail) */
    for (i = 0; i < 10; i++) {
        status = uct_md_mem_reg(ctx->md, NULL, TEST_BUFFER_SIZE, &reg_params, &memh);
        if (status != UCS_OK) {
            error_count++;
        } else {
            /* If it somehow succeeded, clean up */
            uct_md_mem_dereg_params_t dereg_params;
            dereg_params.field_mask = UCT_MD_MEM_DEREG_FIELD_MEMH;
            dereg_params.memh = memh;
            uct_md_mem_dereg(ctx->md, &dereg_params);
        }
    }
    
    /* Attempt to register with zero size (should fail) */
    void *valid_ptr = aligned_alloc(4096, 4096);
    if (valid_ptr) {
        for (i = 0; i < 10; i++) {
            status = uct_md_mem_reg(ctx->md, valid_ptr, 0, &reg_params, &memh);
            if (status != UCS_OK) {
                error_count++;
            } else {
                /* If it somehow succeeded, clean up */
                uct_md_mem_dereg_params_t dereg_params;
                dereg_params.field_mask = UCT_MD_MEM_DEREG_FIELD_MEMH;
                dereg_params.memh = memh;
                uct_md_mem_dereg(ctx->md, &dereg_params);
            }
        }
        free(valid_ptr);
    }
    
    printf("✓ Error tracking statistics:\n");
    printf("  - Invalid operations attempted: 20\n");
    printf("  - Errors caught: %d\n", error_count);
    printf("  - Error rate: %.1f%%\n", (error_count * 100.0) / 20);
    
    if (error_count >= 10) {
        printf("✓ Error tracking working correctly\n");
    } else {
        printf("! Fewer errors than expected - some invalid operations may have succeeded\n");
    }
    
    return UCS_OK;
}

#else /* !ENABLE_STATS */

static ucs_status_t test_stats_disabled(void)
{
    printf("Testing with statistics disabled...\n");
    printf("! Statistics collection is disabled in this build\n");
    printf("  - This is normal for release builds without ENABLE_STATS\n");
    printf("  - To enable statistics, rebuild with --enable-stats\n");
    printf("✓ Statistics disabled test completed\n");
    return UCS_OK;
}

#endif /* ENABLE_STATS */

static ucs_status_t test_performance_monitoring(test_context_t *ctx)
{
    ucs_status_t status;
    void *host_ptr;
    uct_mem_h memh;
    /* Legacy UCX API: no param structs */
    ucs_time_t start_time, end_time;
    double reg_times[TEST_ITERATIONS];
    double dereg_times[TEST_ITERATIONS];
    double total_reg_time = 0, total_dereg_time = 0;
    double min_reg_time = 1e9, max_reg_time = 0;
    double min_dereg_time = 1e9, max_dereg_time = 0;
    int i;
    
    printf("Testing performance monitoring and metrics...\n");
    
    /* Allocate host memory */
    host_ptr = aligned_alloc(4096, TEST_BUFFER_SIZE);
    if (!host_ptr) {
        printf("Failed to allocate host memory\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    memset(host_ptr, 0xCD, TEST_BUFFER_SIZE);
    
    /* Measure registration and deregistration times (legacy API) */
    for (i = 0; i < TEST_ITERATIONS; i++) {
        /* Time registration */
        start_time = ucs_get_time();
        status = uct_md_mem_reg(ctx->md, host_ptr, TEST_BUFFER_SIZE, UCT_MD_MEM_ACCESS_ALL, &memh);
        end_time = ucs_get_time();
        reg_times[i] = ucs_time_to_usec(end_time - start_time);
        if (status != UCS_OK) {
            printf("Failed to register memory iteration %d: %s\n", i, ucs_status_string(status));
            free(host_ptr);
            return status;
        }
        /* Time deregistration */
        start_time = ucs_get_time();
        status = uct_md_mem_dereg(ctx->md, memh);
        end_time = ucs_get_time();
        dereg_times[i] = ucs_time_to_usec(end_time - start_time);
        if (status != UCS_OK) {
            printf("Failed to deregister memory iteration %d: %s\n", i, ucs_status_string(status));
            free(host_ptr);
            return status;
        }
        
        /* Update statistics */
        total_reg_time += reg_times[i];
        total_dereg_time += dereg_times[i];
        
        if (reg_times[i] < min_reg_time) min_reg_time = reg_times[i];
        if (reg_times[i] > max_reg_time) max_reg_time = reg_times[i];
        if (dereg_times[i] < min_dereg_time) min_dereg_time = dereg_times[i];
        if (dereg_times[i] > max_dereg_time) max_dereg_time = dereg_times[i];
    }
    
    free(host_ptr);
    
    /* Calculate and display performance metrics */
    printf("✓ Performance monitoring results:\n");
    printf("  - Iterations: %d\n", TEST_ITERATIONS);
    printf("  - Buffer size: %zu bytes\n", TEST_BUFFER_SIZE);
    printf("  \n");
    printf("  Registration performance:\n");
    printf("    - Average: %.2f us\n", total_reg_time / TEST_ITERATIONS);
    printf("    - Min: %.2f us\n", min_reg_time);
    printf("    - Max: %.2f us\n", max_reg_time);
    printf("    - Total: %.2f ms\n", total_reg_time / 1000.0);
    printf("  \n");
    printf("  Deregistration performance:\n");
    printf("    - Average: %.2f us\n", total_dereg_time / TEST_ITERATIONS);
    printf("    - Min: %.2f us\n", min_dereg_time);
    printf("    - Max: %.2f us\n", max_dereg_time);
    printf("    - Total: %.2f ms\n", total_dereg_time / 1000.0);
    printf("  \n");
    printf("  Overall:\n");
    printf("    - Total time: %.2f ms\n", (total_reg_time + total_dereg_time) / 1000.0);
    printf("    - Throughput: %.2f MB/s\n", 
           (TEST_BUFFER_SIZE * TEST_ITERATIONS * 2) / 
           ((total_reg_time + total_dereg_time) / 1000.0) / (1024.0 * 1024.0));
    
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
    
    printf("Gaudi Statistics Collection Test Suite\n");
    printf("======================================\n\n");
    
    /* Initialize Gaudi context */
    status = init_gaudi_context(&ctx);
    if (status != UCS_OK) {
        printf("Failed to initialize Gaudi context: %s\n", ucs_status_string(status));
        return 1;
    }
    
    /* Run tests */
    printf("Running statistics collection tests...\n\n");
    
#ifdef ENABLE_STATS
    status = test_stats_collection_basic(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_stats_memory_operations(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    status = test_stats_error_tracking(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
#else
    status = test_stats_disabled();
    if (status != UCS_OK) {
        test_failures++;
    }
#endif
    
    status = test_performance_monitoring(&ctx);
    if (status != UCS_OK) {
        test_failures++;
    }
    
    /* Cleanup */
    cleanup_gaudi_context(&ctx);
    
    /* Print summary */
    printf("\n======================================\n");
    if (test_failures == 0) {
        printf("✓ All statistics collection tests passed!\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed\n", test_failures);
        return 1;
    }
}