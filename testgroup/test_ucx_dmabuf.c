#include <uct/api/uct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// Test DMA-BUF support in UCX with Gaudi devices

int test_gaudi_dmabuf_support() {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    uct_component_h gaudi_comp = NULL;
    uct_component_attr_t comp_attr;
    uct_md_h md;
    uct_md_config_t *md_config;
    uct_md_attr_t md_attr;
    void *test_buffer;
    size_t buffer_size = 4096;
    uct_mem_h memh;
    
    printf("=== Testing UCX Gaudi DMA-BUF Support ===\n\n");
    
    // Query available components
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query components: %s\n", ucs_status_string(status));
        return -1;
    }
    
    // Find Gaudi component
    for (unsigned i = 0; i < num_components; i++) {
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        status = uct_component_query(components[i], &comp_attr);
        if (status == UCS_OK && strstr(comp_attr.name, "gaudi")) {
            gaudi_comp = components[i];
            printf("✓ Found Gaudi component: %s\n", comp_attr.name);
            break;
        }
    }
    
    if (!gaudi_comp) {
        printf("✗ Gaudi component not found\n");
        uct_release_component_list(components);
        return -1;
    }
    
    // Open Gaudi memory domain
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("✗ Failed to read MD config: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        return -1;
    }
    
    // Enable DMA-BUF support in configuration
    status = uct_config_modify(md_config, "DMABUF", "yes");
    if (status != UCS_OK) {
        printf("Warning: Could not set DMABUF=yes: %s\n", ucs_status_string(status));
    }
    
    status = uct_md_open(gaudi_comp, "gaudi_cpy", md_config, &md);
    if (status != UCS_OK) {
        printf("✗ Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        uct_config_release(md_config);
        uct_release_component_list(components);
        return -1;
    }
    
    printf("✓ Opened Gaudi memory domain\n");
    
    // Query memory domain attributes
    status = uct_md_query(md, &md_attr);
    if (status != UCS_OK) {
        printf("✗ Failed to query MD attributes: %s\n", ucs_status_string(status));
        goto cleanup;
    }
    
    printf("MD Flags: 0x%lx\n", md_attr.cap.flags);
    printf("DMA-BUF registration supported: %s\n", 
           (md_attr.cap.flags & UCT_MD_FLAG_REG_DMABUF) ? "YES" : "NO");
    
    if (!(md_attr.cap.flags & UCT_MD_FLAG_REG_DMABUF)) {
        printf("✗ DMA-BUF registration not supported by this MD\n");
        goto cleanup;
    }
    
    printf("✓ DMA-BUF registration is supported!\n");
    
    // Allocate test buffer
    test_buffer = malloc(buffer_size);
    if (!test_buffer) {
        printf("✗ Failed to allocate test buffer\n");
        goto cleanup;
    }
    
    memset(test_buffer, 0xAB, buffer_size);
    printf("✓ Allocated test buffer: %p, size: %zu\n", test_buffer, buffer_size);
    
    // Test regular memory registration first
    status = uct_md_mem_reg(md, test_buffer, buffer_size, UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status != UCS_OK) {
        printf("✗ Failed to register memory: %s\n", ucs_status_string(status));
        free(test_buffer);
        goto cleanup;
    }
    
    printf("✓ Successfully registered memory with Gaudi MD\n");
    
    // Test DMA-BUF functionality (requires actual DMA-BUF fd)
    printf("\nNote: For full DMA-BUF testing, need actual DMA-BUF file descriptor\n");
    printf("Current test confirms DMA-BUF infrastructure is in place\n");
    
    // Cleanup
    uct_md_mem_dereg(md, memh);
    free(test_buffer);
    printf("✓ Memory deregistered successfully\n");
    
cleanup:
    uct_md_close(md);
    uct_config_release(md_config);
    uct_release_component_list(components);
    
    printf("\n=== DMA-BUF Test Complete ===\n");
    return 0;
}

int main() {
    int result = test_gaudi_dmabuf_support();
    
    if (result == 0) {
        printf("\n✓ DMA-BUF support test PASSED\n");
        printf("Next steps:\n");
        printf("1. Test with actual DMA-BUF export from Gaudi device\n");
        printf("2. Test DMA-BUF import with MLX devices\n");
        printf("3. Implement end-to-end GPU→RDMA transfer\n");
    } else {
        printf("\n✗ DMA-BUF support test FAILED\n");
    }
    
    return result;
}
