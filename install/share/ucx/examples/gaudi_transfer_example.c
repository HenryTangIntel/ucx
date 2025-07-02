#include <uct/api/uct.h>
#include <ucs/type/status.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gaudi_transfer_context {
    void *host_buffer;
    void *gaudi_buffer;
    size_t buffer_size;
    uct_md_h gaudi_md;
    uct_mem_h host_memh;
    uct_mem_h gaudi_memh;
};

static ucs_status_t allocate_buffers(struct gaudi_transfer_context *ctx, size_t size) {
    ucs_status_t status;
    
    ctx->buffer_size = size;
    
    // Allocate host buffer
    ctx->host_buffer = malloc(size);
    if (!ctx->host_buffer) {
        return UCS_ERR_NO_MEMORY;
    }
    
    // Register host buffer with UCT
    status = uct_md_mem_reg(ctx->gaudi_md, ctx->host_buffer, size, 
                           UCT_MD_MEM_ACCESS_ALL, &ctx->host_memh);
    if (status != UCS_OK) {
        free(ctx->host_buffer);
        return status;
    }
    
    // Allocate Gaudi device memory (simplified - just register host memory for this example)
    ctx->gaudi_buffer = malloc(size);
    if (!ctx->gaudi_buffer) {
        uct_md_mem_dereg(ctx->gaudi_md, ctx->host_memh);
        free(ctx->host_buffer);
        return UCS_ERR_NO_MEMORY;
    }
    
    status = uct_md_mem_reg(ctx->gaudi_md, ctx->gaudi_buffer, size,
                           UCT_MD_MEM_ACCESS_ALL, &ctx->gaudi_memh);
    if (status != UCS_OK) {
        free(ctx->gaudi_buffer);
        uct_md_mem_dereg(ctx->gaudi_md, ctx->host_memh);
        free(ctx->host_buffer);
        return status;
    }
    
    return UCS_OK;
}

static void cleanup_buffers(struct gaudi_transfer_context *ctx) {
    if (ctx->gaudi_memh != UCT_MEM_HANDLE_NULL) {
        uct_md_mem_dereg(ctx->gaudi_md, ctx->gaudi_memh);
    }
    if (ctx->host_memh != UCT_MEM_HANDLE_NULL) {
        uct_md_mem_dereg(ctx->gaudi_md, ctx->host_memh);
    }
    if (ctx->gaudi_buffer) {
        free(ctx->gaudi_buffer);
    }
    if (ctx->host_buffer) {
        free(ctx->host_buffer);
    }
}

int main() {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    uct_md_h gaudi_md = NULL;
    uct_component_h gaudi_comp = NULL;
    uct_component_attr_t comp_attr;
    uct_md_config_t *md_config;
    struct gaudi_transfer_context ctx = {0};
    size_t transfer_size = 1024 * 1024; // 1MB
    
    printf("Gaudi Data Transfer Example\n");
    
    // Query available components
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query components\n");
        return 1;
    }
    
    // Find Gaudi component
    for (unsigned i = 0; i < num_components; i++) {
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        status = uct_component_query(components[i], &comp_attr);
        if (status == UCS_OK && strstr(comp_attr.name, "gaudi")) {
            gaudi_comp = components[i];
            printf("Found Gaudi component: %s\n", comp_attr.name);
            break;
        }
    }
    
    if (!gaudi_comp) {
        printf("No Gaudi component found\n");
        goto cleanup;
    }
    
    // Open Gaudi memory domain
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read MD config\n");
        goto cleanup;
    }
    
    status = uct_md_open(gaudi_comp, "gaudi_cpy", md_config, &gaudi_md);
    uct_config_release(md_config);
    if (status != UCS_OK) {
        printf("Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        goto cleanup;
    }
    
    printf("Opened Gaudi MD successfully\n");
    ctx.gaudi_md = gaudi_md;
    
    // Allocate buffers
    status = allocate_buffers(&ctx, transfer_size);
    if (status != UCS_OK) {
        printf("Failed to allocate buffers: %s\n", ucs_status_string(status));
        goto cleanup;
    }
    
    printf("Allocated host buffer: %p\n", ctx.host_buffer);
    printf("Allocated Gaudi buffer: %p\n", ctx.gaudi_buffer);
    printf("Buffer size: %zu bytes\n", transfer_size);
    
    // Initialize host buffer with test pattern
    for (size_t i = 0; i < transfer_size / sizeof(uint32_t); i++) {
        ((uint32_t*)ctx.host_buffer)[i] = (uint32_t)i;
    }
    printf("Initialized host buffer with test pattern\n");
    
    // In a real implementation, you would use:
    // - uct_ep_put_short/bcopy/zcopy for host-to-device transfers
    // - uct_ep_get_short/bcopy/zcopy for device-to-host transfers
    // - uct_ep_atomic operations for atomic updates
    
    printf("Note: In a real implementation, use UCT endpoint operations\n");
    printf("for actual data transfers between host and Gaudi memory\n");
    
    // Clean up
    cleanup_buffers(&ctx);
    
cleanup:
    if (gaudi_md) {
        uct_md_close(gaudi_md);
    }
    
    uct_release_component_list(components);
    
    printf("Transfer example completed\n");
    return 0;
}
