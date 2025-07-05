#include <uct/api/uct.h>
#include <ucs/memory/memory_type.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    uct_component_h *components;
    unsigned num_components;
    uct_md_resource_desc_t *md_resources;
    unsigned num_md_resources;
    uct_md_h md;
    uct_md_config_t *md_config;
    uct_allocated_memory_t alloc_mem;
    ucs_status_t status;
    
    printf("=== UCT Gaudi Memory Test ===\n");
    
    /* Query components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query components: %s\n", ucs_status_string(status));
        return -1;
    }
    
    printf("Found %u UCT components\n", num_components);
    
    /* Find Gaudi component and MD */
    for (unsigned i = 0; i < num_components; i++) {
        status = uct_component_query(components[i], &md_resources, &num_md_resources);
        if (status != UCS_OK) continue;
        
        for (unsigned j = 0; j < num_md_resources; j++) {
            printf("MD: %s\n", md_resources[j].md_name);
            
            if (strstr(md_resources[j].md_name, "gaudi")) {
                printf("Found Gaudi MD: %s\n", md_resources[j].md_name);
                
                /* Open the MD */
                status = uct_md_config_read(components[i], NULL, NULL, &md_config);
                if (status != UCS_OK) {
                    printf("Failed to read MD config: %s\n", ucs_status_string(status));
                    continue;
                }
                
                status = uct_md_open(components[i], md_resources[j].md_name, md_config, &md);
                uct_config_release(md_config);
                if (status != UCS_OK) {
                    printf("Failed to open MD: %s\n", ucs_status_string(status));
                    continue;
                }
                
                printf("Successfully opened Gaudi MD\n");
                
                /* Test allocation */
                status = uct_md_mem_alloc(md, 4096, UCT_MD_MEM_ACCESS_ALL,
                                        "test", &alloc_mem);
                if (status != UCS_OK) {
                    printf("Failed to allocate memory: %s\n", ucs_status_string(status));
                } else {
                    printf("SUCCESS: Allocated memory at %p\n", alloc_mem.address);
                    printf("Memory type: %d\n", alloc_mem.mem_type);
                    
                    /* Free memory */
                    status = uct_md_mem_free(md, &alloc_mem);
                    if (status != UCS_OK) {
                        printf("Failed to free memory: %s\n", ucs_status_string(status));
                    } else {
                        printf("Successfully freed memory\n");
                    }
                }
                
                uct_md_close(md);
                uct_release_component_list(md_resources);
                uct_release_component_list(components);
                return 0;
            }
        }
        uct_release_component_list(md_resources);
    }
    
    printf("Gaudi MD not found\n");
    uct_release_component_list(components);
    return -1;
}
