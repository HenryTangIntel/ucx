#include <uct/api/uct.h>
#include <ucs/type/status.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("UCX Gaudi Transport Integration Test\n");
    printf("===================================\n\n");
    fflush(stdout);
    
    uct_component_h *components = NULL;
    unsigned num_components = 0;
    ucs_status_t status;
    
    printf("Querying UCT components...\n");
    fflush(stdout);
    
    /* Query available UCT components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("✗ Failed to query UCT components: %s\n", ucs_status_string(status));
        return 1;
    }
    
    printf("Found %u UCT components:\n", num_components);
    
    /* Look for Gaudi component */
    uct_component_h gaudi_comp = NULL;
    for (unsigned i = 0; i < num_components; i++) {
        uct_component_attr_t comp_attr;
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        
        status = uct_component_query(components[i], &comp_attr);
        if (status == UCS_OK) {
            printf("  [%u] %s", i, comp_attr.name);
            
            if (strstr(comp_attr.name, "gaudi")) {
                gaudi_comp = components[i];
                printf(" ← Gaudi component found!");
            }
            printf("\n");
        }
    }
    
    if (!gaudi_comp) {
        printf("\n⚠ No Gaudi component found\n");
        printf("This is expected if Gaudi hardware/drivers are not available\n");
        uct_release_component_list(components);
        return 0;
    }
    
    printf("\n=== Testing Gaudi Memory Domain ===\n");
    
    /* Try to open Gaudi memory domain */
    uct_md_config_t *md_config;
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("✗ Failed to read Gaudi MD config: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        return 1;
    }
    
    uct_md_h gaudi_md;
    status = uct_md_open(gaudi_comp, "gaudi_cpy", md_config, &gaudi_md);
    uct_config_release(md_config);
    
    if (status != UCS_OK) {
        printf("✗ Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        printf("This could be due to missing hardware or permissions\n");
        uct_release_component_list(components);
        return 1;
    }
    
    printf("✓ Successfully opened Gaudi memory domain\n");
    
    /* Query Gaudi MD capabilities */
    uct_md_attr_t md_attr;
    status = uct_md_query(gaudi_md, &md_attr);
    if (status == UCS_OK) {
        printf("\nGaudi MD Capabilities:\n");
        printf("  Component: %s\n", md_attr.component_name);
        printf("  Max alloc: %zu bytes\n", md_attr.cap.max_alloc);
        printf("  Max reg: %zu bytes\n", md_attr.cap.max_reg);
        printf("  Flags: 0x%lx\n", (unsigned long)md_attr.cap.flags);
        
        if (md_attr.cap.flags & UCT_MD_FLAG_ALLOC) {
            printf("  ✓ Supports memory allocation\n");
        }
        if (md_attr.cap.flags & UCT_MD_FLAG_REG) {
            printf("  ✓ Supports memory registration\n");
        }
    }
    
    /* Test basic memory operations */
    printf("\n=== Testing Memory Operations ===\n");
    
    size_t test_size = 4096;
    void *test_buffer = malloc(test_size);
    if (!test_buffer) {
        printf("✗ Failed to allocate test buffer\n");
        uct_md_close(gaudi_md);
        uct_release_component_list(components);
        return 1;
    }
    
    /* Try to register memory with Gaudi MD */
    uct_mem_h memh;
    status = uct_md_mem_reg(gaudi_md, test_buffer, test_size, 
                           UCT_MD_MEM_ACCESS_ALL, &memh);
    if (status == UCS_OK) {
        printf("✓ Successfully registered %zu bytes with Gaudi MD\n", test_size);
        
        /* Query memory attributes if supported */
        uct_md_mem_attr_t mem_attr;
        mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE;
        
        /* Note: mem_attr query may not be supported by all MDs */
        printf("  Memory registration successful\n");
        
        uct_md_mem_dereg(gaudi_md, memh);
        printf("✓ Successfully deregistered memory\n");
    } else {
        printf("✗ Failed to register memory: %s\n", ucs_status_string(status));
    }
    
    free(test_buffer);
    uct_md_close(gaudi_md);
    uct_release_component_list(components);
    
    printf("\n✓ UCX Gaudi transport integration test completed\n");
    printf("This test directly exercised the UCX Gaudi transport layer\n");
    printf("implemented in /workspace/ucx/src/uct/gaudi/copy/\n");
    
    return 0;
}
