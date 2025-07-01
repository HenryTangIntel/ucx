#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <uct/api/uct.h>

#define TEST_SIZE (4 * 1024)  // 4KB test buffer

typedef struct {
    uct_md_h gaudi_md;
    uct_md_h ib_md;
    uct_mem_h gaudi_memh;
    uct_mem_h ib_memh;
    void *test_buffer;
    int dmabuf_fd;
} simple_test_ctx_t;

static void test_dmabuf_functionality(void)
{
    printf("=== Testing DMA-BUF Kernel Support ===\n");
    
    // Check if /dev/udmabuf exists
    int fd = open("/dev/udmabuf", O_RDWR);
    if (fd >= 0) {
        printf("✓ /dev/udmabuf is available\n");
        close(fd);
    } else {
        printf("✗ /dev/udmabuf not available: %s\n", strerror(errno));
    }
    
    // Check /proc/filesystems for dmabuf
    FILE *f = fopen("/proc/filesystems", "r");
    if (f) {
        char line[256];
        bool dmabuf_found = false;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "dmabuf")) {
                dmabuf_found = true;
                break;
            }
        }
        fclose(f);
        printf("%s DMA-BUF filesystem support found in kernel\n", 
               dmabuf_found ? "✓" : "✗");
    }
    
    printf("\n");
}

static ucs_status_t find_and_open_md(const char *md_type, uct_md_h *md_p)
{
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status;
    
    printf("Looking for %s memory domain...\n", md_type);
    
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("✗ Failed to query components: %s\n", ucs_status_string(status));
        return status;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        uct_component_attr_t comp_attr;
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME |
                              UCT_COMPONENT_ATTR_FIELD_MD_RESOURCE_COUNT;
        status = uct_component_query(components[i], &comp_attr);
        if (status != UCS_OK) continue;
        
        printf("  Component: %s (%u MDs)\n", comp_attr.name, comp_attr.md_resource_count);
        
        // Get memory domains for this component
        uct_md_resource_desc_t *md_resources = comp_attr.md_resources;
        for (unsigned j = 0; j < comp_attr.md_resource_count; j++) {
            const char *md_name = (md_resources[j].md_name && 
                                  strlen(md_resources[j].md_name) > 0) ? 
                                  md_resources[j].md_name : "<null>";
            printf("    MD[%u]: %s\n", j, md_name);
            
            if ((strstr(comp_attr.name, md_type) != NULL) || 
                (md_resources[j].md_name && strstr(md_resources[j].md_name, md_type) != NULL)) {
                // Found matching MD, try to open it
                uct_md_config_t *md_config;
                status = uct_md_config_read(components[i], NULL, NULL, &md_config);
                if (status == UCS_OK) {
                    status = uct_md_open(components[i], md_resources[j].md_name, 
                                       md_config, md_p);
                    uct_config_release(md_config);
                    
                    if (status == UCS_OK) {
                        printf("✓ Opened %s MD: %s\n", md_type, md_name);
                        uct_release_component_list(components);
                        return UCS_OK;
                    } else {
                        printf("✗ Failed to open %s MD: %s\n", md_type, ucs_status_string(status));
                    }
                }
            }
        }
    }
    
    uct_release_component_list(components);
    printf("✗ No %s memory domain found\n", md_type);
    return UCS_ERR_NO_DEVICE;
}

static void query_md_dmabuf_support(uct_md_h md, const char *name)
{
    uct_md_attr_t md_attr;
    ucs_status_t status = uct_md_query(md, &md_attr);
    
    if (status == UCS_OK) {
        printf("=== %s Memory Domain Info ===\n", name);
        printf("Flags: 0x%lx\n", md_attr.cap.flags);
        printf("  - DMA-BUF registration: %s\n", 
               (md_attr.cap.flags & UCT_MD_FLAG_REG_DMABUF) ? "YES" : "NO");
        printf("  - Memory registration: %s\n", 
               (md_attr.cap.flags & UCT_MD_FLAG_REG) ? "YES" : "NO");
        printf("  - Memory allocation: %s\n", 
               (md_attr.cap.flags & UCT_MD_FLAG_ALLOC) ? "YES" : "NO");
        printf("Registered memory types: 0x%lx\n", md_attr.cap.reg_mem_types);
        printf("Accessible memory types: 0x%lx\n", md_attr.cap.access_mem_types);
        printf("Detectable memory types: 0x%lx\n", md_attr.cap.detect_mem_types);
        printf("Component: %s\n", md_attr.component_name);
        printf("\n");
    } else {
        printf("✗ Failed to query %s MD: %s\n", name, ucs_status_string(status));
    }
}

static ucs_status_t test_memory_registration(simple_test_ctx_t *ctx)
{
    printf("=== Testing Memory Registration ===\n");
    
    // Allocate test buffer
    ctx->test_buffer = malloc(TEST_SIZE);
    if (!ctx->test_buffer) {
        printf("✗ Failed to allocate test buffer\n");
        return UCS_ERR_NO_MEMORY;
    }
    
    memset(ctx->test_buffer, 0xAB, TEST_SIZE);
    printf("✓ Allocated test buffer: %p, size: %d\n", ctx->test_buffer, TEST_SIZE);
    
    // Register with Gaudi MD
    if (ctx->gaudi_md) {
        ucs_status_t status = uct_md_mem_reg(ctx->gaudi_md, ctx->test_buffer, TEST_SIZE,
                                           UCT_MD_MEM_ACCESS_ALL, &ctx->gaudi_memh);
        if (status == UCS_OK) {
            printf("✓ Registered memory with Gaudi MD\n");
        } else {
            printf("✗ Failed to register with Gaudi MD: %s\n", ucs_status_string(status));
        }
    }
    
    // Register with IB MD
    if (ctx->ib_md) {
        ucs_status_t status = uct_md_mem_reg(ctx->ib_md, ctx->test_buffer, TEST_SIZE,
                                           UCT_MD_MEM_ACCESS_ALL, &ctx->ib_memh);
        if (status == UCS_OK) {
            printf("✓ Registered memory with IB MD\n");
        } else {
            printf("✗ Failed to register with IB MD: %s\n", ucs_status_string(status));
        }
    }
    
    return UCS_OK;
}

static void cleanup_simple_test(simple_test_ctx_t *ctx)
{
    printf("=== Cleanup ===\n");
    
    if (ctx->gaudi_memh && ctx->gaudi_md) {
        uct_md_mem_dereg(ctx->gaudi_md, ctx->gaudi_memh);
        printf("✓ Deregistered Gaudi memory\n");
    }
    
    if (ctx->ib_memh && ctx->ib_md) {
        uct_md_mem_dereg(ctx->ib_md, ctx->ib_memh);
        printf("✓ Deregistered IB memory\n");
    }
    
    if (ctx->test_buffer) {
        free(ctx->test_buffer);
        printf("✓ Freed test buffer\n");
    }
    
    if (ctx->dmabuf_fd >= 0) {
        close(ctx->dmabuf_fd);
        printf("✓ Closed DMA-BUF fd\n");
    }
    
    if (ctx->gaudi_md) {
        uct_md_close(ctx->gaudi_md);
        printf("✓ Closed Gaudi MD\n");
    }
    
    if (ctx->ib_md) {
        uct_md_close(ctx->ib_md);
        printf("✓ Closed IB MD\n");
    }
}

int main(int argc, char **argv)
{
    simple_test_ctx_t ctx = {0};
    ctx.dmabuf_fd = -1;
    
    printf("UCX DMA-BUF Support Test\n");
    printf("========================\n\n");
    
    test_dmabuf_functionality();
    
    // Try to find and open memory domains
    ucs_status_t status;
    
    status = find_and_open_md("gaudi", &ctx.gaudi_md);
    if (status == UCS_OK) {
        query_md_dmabuf_support(ctx.gaudi_md, "Gaudi");
    }
    
    status = find_and_open_md("ib", &ctx.ib_md);
    if (status != UCS_OK) {
        status = find_and_open_md("mlx", &ctx.ib_md);
        if (status != UCS_OK) {
            status = find_and_open_md("gga", &ctx.ib_md);  // GGA might be the IB component
        }
    }
    if (status == UCS_OK) {
        query_md_dmabuf_support(ctx.ib_md, "IB/MLX");
    }
    
    // Test basic memory registration
    test_memory_registration(&ctx);
    
    cleanup_simple_test(&ctx);
    
    printf("\nSimple test completed.\n");
    return 0;
}
