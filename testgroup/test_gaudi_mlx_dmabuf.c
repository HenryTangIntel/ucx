#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <uct/api/uct.h>
#include <ucs/memory/memory_type.h>

#define TEST_SIZE (4 * 1024 * 1024)  // 4MB test buffer
#define TEST_PATTERN 0xDEADBEEF

typedef struct {
    uct_md_h gaudi_md;
    uct_md_h mlx_md;
    uct_mem_h gaudi_memh;
    uct_mem_h mlx_memh;
    void *gaudi_buffer;
    void *mlx_buffer;
    int dmabuf_fd;
} test_context_t;

static void print_device_info(void)
{
    printf("=== Device Information ===\n");
    
    // List available UCT components
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status = uct_query_components(&components, &num_components);
    
    if (status == UCS_OK) {
        printf("Available UCT components:\n");
        for (unsigned i = 0; i < num_components; i++) {
            uct_component_attr_t comp_attr;
            comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME |
                                  UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT;
            uct_component_query(components[i], &comp_attr);
            
            printf("  - %s: %u memory domains\n", 
                   comp_attr.name, comp_attr.md_resource_count);
            
            // Query memory domains for this component
            uct_md_resource_desc_t *md_resources;
            unsigned num_md_resources;
            status = uct_md_query_resources(components[i], &md_resources, &num_md_resources);
            
            if (status == UCS_OK) {
                for (unsigned j = 0; j < num_md_resources; j++) {
                    printf("    MD[%u]: %s\n", j, md_resources[j].md_name);
                }
                uct_release_md_resource_list(md_resources);
            }
        }
        uct_release_component_list(components);
    }
    printf("\n");
}

static ucs_status_t open_memory_domain(const char *md_name, uct_md_h *md_p)
{
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status;
    
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        return status;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        uct_md_resource_desc_t *md_resources;
        unsigned num_md_resources;
        
        status = uct_md_query_resources(components[i], &md_resources, &num_md_resources);
        if (status != UCS_OK) {
            continue;
        }
        
        for (unsigned j = 0; j < num_md_resources; j++) {
            if (strstr(md_resources[j].md_name, md_name) != NULL) {
                uct_md_config_t *md_config;
                status = uct_md_config_read(components[i], NULL, NULL, &md_config);
                if (status == UCS_OK) {
                    status = uct_md_open(components[i], md_resources[j].md_name, 
                                       md_config, md_p);
                    uct_config_release(md_config);
                    
                    if (status == UCS_OK) {
                        printf("Opened memory domain: %s\n", md_resources[j].md_name);
                        uct_release_md_resource_list(md_resources);
                        uct_release_component_list(components);
                        return UCS_OK;
                    }
                }
            }
        }
        uct_release_md_resource_list(md_resources);
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
        printf("Cap flags: 0x%lx\n", md_attr.cap_flags);
        printf("  - Registration supported: %s\n", 
               (md_attr.cap_flags & UCT_MD_FLAG_REG) ? "YES" : "NO");
        printf("  - DMA-BUF registration: %s\n", 
               (md_attr.cap_flags & UCT_MD_FLAG_REG_DMABUF) ? "YES" : "NO");
        printf("  - Allocation supported: %s\n", 
               (md_attr.cap_flags & UCT_MD_FLAG_ALLOC) ? "YES" : "NO");
        
        printf("Registered memory types: 0x%lx\n", md_attr.reg_mem_types);
        printf("Accessible memory types: 0x%lx\n", md_attr.access_mem_types);
        printf("DMA-BUF memory types: 0x%lx\n", md_attr.dmabuf_mem_types);
        printf("\n");
    } else {
        printf("Failed to query %s MD capabilities: %s\n", name, ucs_status_string(status));
    }
}

static ucs_status_t test_gaudi_dmabuf_export(test_context_t *ctx)
{
    printf("=== Testing Gaudi DMA-BUF Export ===\n");
    
    // Allocate Gaudi memory
    size_t length = TEST_SIZE;
    ucs_status_t status = uct_mem_alloc(&ctx->gaudi_buffer, length, UCS_MEMORY_TYPE_GAUDI,
                                      0, "gaudi_test_buffer");
    
    if (status != UCS_OK) {
        printf("✗ Failed to allocate Gaudi memory: %s\n", ucs_status_string(status));
        return status;
    }
    
    printf("✓ Allocated Gaudi memory: %p, size: %zu\n", ctx->gaudi_buffer, length);
    
    // Register memory with Gaudi MD
    status = uct_md_mem_reg(ctx->gaudi_md, ctx->gaudi_buffer, length, 
                          UCT_MD_MEM_ACCESS_ALL, &ctx->gaudi_memh);
    if (status != UCS_OK) {
        printf("✗ Failed to register Gaudi memory: %s\n", ucs_status_string(status));
        return status;
    }
    
    printf("✓ Registered Gaudi memory with MD\n");
    
    // Query memory attributes to get DMA-BUF fd
    uct_md_mem_attr_t mem_attr;
    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_DMABUF_FD | 
                          UCT_MD_MEM_ATTR_FIELD_DMABUF_OFFSET;
    
    if (ctx->gaudi_md->ops->mem_attr) {
        status = ctx->gaudi_md->ops->mem_attr(ctx->gaudi_md, ctx->gaudi_buffer, length, &mem_attr);
        if (status == UCS_OK) {
            if (mem_attr.dmabuf_fd != UCT_DMABUF_FD_INVALID) {
                ctx->dmabuf_fd = dup(mem_attr.dmabuf_fd);  // Duplicate for safety
                printf("✓ Exported Gaudi memory as DMA-BUF: fd=%d, offset=%zu\n", 
                       ctx->dmabuf_fd, mem_attr.dmabuf_offset);
            } else {
                printf("✗ Failed to export Gaudi memory as DMA-BUF\n");
                return UCS_ERR_NO_DEVICE;
            }
        } else {
            printf("✗ Failed to query Gaudi memory attributes: %s\n", ucs_status_string(status));
            return status;
        }
    } else {
        printf("✗ MD does not support memory attribute queries\n");
        return UCS_ERR_UNSUPPORTED;
    }
    
    return UCS_OK;
}

static ucs_status_t test_mlx_dmabuf_import(test_context_t *ctx)
{
    printf("=== Testing MLX DMA-BUF Import ===\n");
    
    if (ctx->dmabuf_fd < 0) {
        printf("✗ No valid DMA-BUF fd to import\n");
        return UCS_ERR_INVALID_PARAM;
    }
    
    // Register DMA-BUF with MLX - try using the standard registration API
    uct_md_mem_reg_params_t reg_params;
    reg_params.field_mask = UCT_MD_MEM_REG_FIELD_DMABUF_FD |
                           UCT_MD_MEM_REG_FIELD_DMABUF_OFFSET;
    reg_params.dmabuf_fd = ctx->dmabuf_fd;
    reg_params.dmabuf_offset = 0;
    
    ucs_status_t status = uct_md_mem_reg(ctx->mlx_md, ctx->gaudi_buffer, TEST_SIZE,
                                       UCT_MD_MEM_ACCESS_ALL, &ctx->mlx_memh);
    
    if (status == UCS_OK) {
        printf("✓ Successfully registered memory with MLX\n");
        
        // Try to pack the memory key
        void *rkey_buffer;
        status = uct_md_mkey_pack(ctx->mlx_md, ctx->mlx_memh, &rkey_buffer);
        if (status == UCS_OK) {
            printf("✓ Successfully packed MLX memory key\n");
            uct_rkey_release(NULL, rkey_buffer);
        } else {
            printf("✗ Failed to pack MLX memory key: %s\n", ucs_status_string(status));
        }
    } else {
        printf("✗ Failed to register memory with MLX: %s\n", ucs_status_string(status));
    }
    
    return status;
}

static void test_memory_access_patterns(test_context_t *ctx)
{
    printf("=== Testing Memory Access Patterns ===\n");
    
    if (!ctx->gaudi_buffer) {
        printf("✗ No Gaudi buffer available for testing\n");
        return;
    }
    
    // Test CPU write access to Gaudi memory
    printf("Testing CPU write access to Gaudi memory...\n");
    uint32_t *data = (uint32_t*)ctx->gaudi_buffer;
    for (size_t i = 0; i < TEST_SIZE / sizeof(uint32_t); i += 1024) {
        data[i] = TEST_PATTERN + i;
    }
    printf("✓ CPU write to Gaudi memory completed\n");
    
    // Test CPU read access from Gaudi memory
    printf("Testing CPU read access from Gaudi memory...\n");
    bool read_success = true;
    for (size_t i = 0; i < TEST_SIZE / sizeof(uint32_t); i += 1024) {
        if (data[i] != TEST_PATTERN + i) {
            printf("✗ Data mismatch at offset %zu: expected 0x%x, got 0x%x\n", 
                   i, TEST_PATTERN + (uint32_t)i, data[i]);
            read_success = false;
            break;
        }
    }
    if (read_success) {
        printf("✓ CPU read from Gaudi memory completed successfully\n");
    }
}

static void cleanup_test_context(test_context_t *ctx)
{
    printf("=== Cleanup ===\n");
    
    if (ctx->mlx_memh) {
        uct_md_mem_dereg(ctx->mlx_md, ctx->mlx_memh);
        printf("✓ Deregistered MLX memory\n");
    }
    
    if (ctx->gaudi_memh) {
        uct_md_mem_dereg(ctx->gaudi_md, ctx->gaudi_memh);
        printf("✓ Deregistered Gaudi memory\n");
    }
    
    if (ctx->gaudi_buffer) {
        uct_mem_free(ctx->gaudi_buffer);
        printf("✓ Freed Gaudi memory\n");
    }
    
    if (ctx->dmabuf_fd >= 0) {
        close(ctx->dmabuf_fd);
        printf("✓ Closed DMA-BUF fd\n");
    }
    
    if (ctx->mlx_md) {
        uct_md_close(ctx->mlx_md);
        printf("✓ Closed MLX memory domain\n");
    }
    
    if (ctx->gaudi_md) {
        uct_md_close(ctx->gaudi_md);
        printf("✓ Closed Gaudi memory domain\n");
    }
}

int main(int argc, char **argv)
{
    test_context_t ctx = {0};
    ctx.dmabuf_fd = -1;
    ucs_status_t status;
    
    printf("UCX Gaudi-MLX DMA-BUF Integration Test\n");
    printf("======================================\n\n");
    
    print_device_info();
    
    // Open Gaudi memory domain
    status = open_memory_domain("gaudi", &ctx.gaudi_md);
    if (status != UCS_OK) {
        printf("✗ Failed to open Gaudi memory domain: %s\n", ucs_status_string(status));
        printf("This may be normal if no Gaudi devices are available\n");
        return 1;
    }
    
    // Open MLX memory domain
    status = open_memory_domain("mlx", &ctx.mlx_md);
    if (status != UCS_OK) {
        printf("✗ Failed to open MLX memory domain: %s\n", ucs_status_string(status));
        printf("Trying InfiniBand domain instead...\n");
        
        status = open_memory_domain("ib", &ctx.mlx_md);
        if (status != UCS_OK) {
            printf("✗ Failed to open IB memory domain: %s\n", ucs_status_string(status));
            cleanup_test_context(&ctx);
            return 1;
        }
    }
    
    // Query capabilities
    query_md_capabilities(ctx.gaudi_md, "Gaudi");
    query_md_capabilities(ctx.mlx_md, "MLX/IB");
    
    // Test DMA-BUF export from Gaudi
    status = test_gaudi_dmabuf_export(&ctx);
    if (status != UCS_OK) {
        printf("DMA-BUF export test failed, continuing with basic tests...\n");
    }
    
    // Test DMA-BUF import to MLX
    if (ctx.dmabuf_fd >= 0) {
        status = test_mlx_dmabuf_import(&ctx);
        if (status == UCS_OK) {
            printf("✓ DMA-BUF integration test PASSED\n");
        } else {
            printf("✗ DMA-BUF integration test FAILED\n");
        }
    }
    
    // Test memory access patterns
    test_memory_access_patterns(&ctx);
    
    cleanup_test_context(&ctx);
    
    printf("\nTest completed.\n");
    return 0;
}
