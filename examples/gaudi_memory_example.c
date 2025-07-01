#include <ucp/api/ucp.h>
#include <uct/api/uct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    uct_md_h md;
    uct_md_config_t *md_config;
    void *gaudi_mem;
    size_t mem_size = 1024 * 1024; // 1MB
    uct_mem_h memh;
    uct_component_h gaudi_comp = NULL;
    uct_component_attr_t comp_attr;
    
    printf("UCX Gaudi Memory Management Example\n");
    
    // Query available components
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query components: %s\n", ucs_status_string(status));
        return 1;
    }
    
    // Look for Gaudi component by querying each component
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
        printf("Gaudi component not found\n");
        uct_release_component_list(components);
        return 1;
    }
    
    // Open Gaudi memory domain
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read MD config: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        return 1;
    }
    
    status = uct_md_open(gaudi_comp, "gaudi_cpy", md_config, &md);
    if (status != UCS_OK) {
        printf("Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        goto cleanup_config;
    }
    
    printf("Successfully opened Gaudi memory domain\n");
    
    // Allocate host memory first
    gaudi_mem = malloc(mem_size);
    if (!gaudi_mem) {
        printf("Failed to allocate host memory\n");
        goto cleanup_md;
    }
    
    // Register memory with Gaudi device
    status = uct_md_mem_reg(md, gaudi_mem, mem_size, UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status != UCS_OK) {
        printf("Failed to register Gaudi memory: %s\n", ucs_status_string(status));
        free(gaudi_mem);
        goto cleanup_md;
    }
    
    printf("Successfully registered %zu bytes on Gaudi device\n", mem_size);
    
    // Use the memory (example: write some data)
    // Note: In real usage, you'd use Gaudi APIs to perform compute operations
    memset(gaudi_mem, 0x42, mem_size);
    printf("Initialized Gaudi memory with pattern\n");
    
    // Deregister Gaudi memory
    status = uct_md_mem_dereg(md, memh);
    if (status != UCS_OK) {
        printf("Failed to deregister Gaudi memory: %s\n", ucs_status_string(status));
    } else {
        printf("Successfully deregistered Gaudi memory\n");
    }
    
    // Free host memory
    free(gaudi_mem);
    
cleanup_md:
    uct_md_close(md);
cleanup_config:
    uct_config_release(md_config);
    uct_release_component_list(components);
    
    printf("Example completed\n");
    return 0;
}
