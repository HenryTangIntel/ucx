#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <uct/api/uct.h>

#define TEST_SIZE (4 * 1024 * 1024)  // 4MB test buffer

typedef struct {
    uct_md_h gaudi_md;
    void *gaudi_buffer;
    uct_mem_h gaudi_memh;
} test_context_t;

static void print_device_info(void)
{
    printf("=== Device Information ===\n");
    
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status = uct_query_components(&components, &num_components);
    
    if (status == UCS_OK) {
        printf("Available UCT components:\n");
        for (unsigned i = 0; i < num_components; i++) {
            uct_component_attr_t comp_attr;
            comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME |
                                  UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT;
            status = uct_component_query(components[i], &comp_attr);
            if (status == UCS_OK) {
                printf("  - %s: %u memory domains\n", 
                       comp_attr.name, comp_attr.md_resource_count);
            }
        }
        uct_release_component_list(components);
    }
    printf("\n");
}

static ucs_status_t open_gaudi_memory_domain(test_context_t *ctx)
{
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status;
    
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        return status;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        uct_component_attr_t comp_attr;
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        status = uct_component_query(components[i], &comp_attr);
        
        if (status == UCS_OK && strstr(comp_attr.name, "gaudi") != NULL) {
            uct_md_config_t *md_config;
            status = uct_md_config_read(components[i], NULL, NULL, &md_config);
            if (status == UCS_OK) {
                status = uct_md_open(components[i], comp_attr.name, md_config, &ctx->gaudi_md);
                uct_config_release(md_config);
                
                if (status == UCS_OK) {
                    printf("✓ Opened Gaudi memory domain: %s\n", comp_attr.name);
                    uct_release_component_list(components);
                    return UCS_OK;
                }
            }
        }
    }
    
    uct_release_component_list(components);
    return UCS_ERR_NO_DEVICE;
}

static void query_md_capabilities(uct_md_h md, const char *name)
{
    uct_md_attr_t md_attr;
    
    ucs_status_t status = uct_md_query(md, &md_attr);
    if (status == UCS_OK) {
        printf("=== %s Memory Domain Capabilities ===\n", name);
        printf("Component: %s\n", md_attr.component_name);
        printf("Cap flags: 0x%lx\n", md_attr.cap.flags);
        printf("  - Registration supported: %s\n", 
               (md_attr.cap.flags & UCT_MD_FLAG_REG) ? "YES" : "NO");
        printf("  - Allocation supported: %s\n", 
               (md_attr.cap.flags & UCT_MD_FLAG_ALLOC) ? "YES" : "NO");
        printf("Registered memory types: 0x%lx\n", md_attr.cap.reg_mem_types);
        printf("Accessible memory types: 0x%lx\n", md_attr.cap.access_mem_types);
        printf("Detect memory types: 0x%lx\n", md_attr.cap.detect_mem_types);
        printf("\n");
    } else {
        printf("Failed to query %s MD capabilities: %s\n", name, ucs_status_string(status));
    }
}

static ucs_status_t test_gaudi_memory_operations(test_context_t *ctx)
{
    printf("=== Testing Gaudi Memory Operations ===\n");
    
    // Allocate system memory for testing
    ctx->gaudi_buffer = malloc(TEST_SIZE);
    if (!ctx->gaudi_buffer) {
        printf("✗ Failed to allocate test buffer\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    printf("✓ Allocated test buffer: %p, size: %d\n", ctx->gaudi_buffer, TEST_SIZE);
    
    // Register memory with Gaudi MD
    ucs_status_t status = uct_md_mem_reg(ctx->gaudi_md, ctx->gaudi_buffer, TEST_SIZE, 
                                        UCT_MD_MEM_ACCESS_ALL, &ctx->gaudi_memh);
    if (status != UCS_OK) {
        printf("✗ Failed to register memory: %s\n", ucs_status_string(status));
        return status;
    }
    
    printf("✓ Registered memory with Gaudi MD\n");
    
    // Test memory access
    uint32_t *data = (uint32_t*)ctx->gaudi_buffer;
    uint32_t test_pattern = 0xDEADBEEF;
    
    printf("Testing memory write/read...\n");
    for (size_t i = 0; i < TEST_SIZE / sizeof(uint32_t); i += 1024) {
        data[i] = test_pattern + i;
    }
    
    bool read_success = true;
    for (size_t i = 0; i < TEST_SIZE / sizeof(uint32_t); i += 1024) {
        if (data[i] != test_pattern + i) {
            printf("✗ Data mismatch at offset %zu\n", i);
            read_success = false;
            break;
        }
    }
    
    if (read_success) {
        printf("✓ Memory read/write test passed\n");
    }
    
    return UCS_OK;
}

static void cleanup_test_context(test_context_t *ctx)
{
    printf("=== Cleanup ===\n");
    
    if (ctx->gaudi_memh) {
        uct_md_mem_dereg(ctx->gaudi_md, ctx->gaudi_memh);
        printf("✓ Deregistered memory\n");
    }
    
    if (ctx->gaudi_buffer) {
        free(ctx->gaudi_buffer);
        printf("✓ Freed test buffer\n");
    }
    
    if (ctx->gaudi_md) {
        uct_md_close(ctx->gaudi_md);
        printf("✓ Closed Gaudi memory domain\n");
    }
}

int main(int argc, char **argv)
{
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    
    test_context_t ctx = {0};
    ucs_status_t status;
    
    printf("UCX Gaudi Memory Domain Test\n");
    printf("============================\n\n");
    
    print_device_info();
    
    // Open Gaudi memory domain
    status = open_gaudi_memory_domain(&ctx);
    if (status != UCS_OK) {
        printf("✗ Failed to open Gaudi memory domain: %s\n", ucs_status_string(status));
        printf("This may be normal if no Gaudi devices are available\n");
        return 1;
    }
    
    // Query capabilities
    query_md_capabilities(ctx.gaudi_md, "Gaudi");
    
    // Test memory operations
    status = test_gaudi_memory_operations(&ctx);
    if (status == UCS_OK) {
        printf("✓ Gaudi memory operations test PASSED\n");
    } else {
        printf("✗ Gaudi memory operations test FAILED\n");
    }
    
    cleanup_test_context(&ctx);
    
    printf("\nTest completed.\n");
    return (status == UCS_OK) ? 0 : 1;
}
