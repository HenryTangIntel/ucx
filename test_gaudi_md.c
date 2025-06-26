#include <ucs/debug/memtrack.h>
#include <uct/api/uct.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * Test program for Gaudi memory domain functionality
 * This program tests basic operations with the Gaudi memory domain:
 * 1. Initialize UCT
 * 2. Query memory domains
 * 3. Open Gaudi memory domain if available
 * 4. Query memory domain capabilities
 * 5. Close resources and cleanup
 */

static void print_md_attr(const uct_md_attr_t *md_attr)
{
    printf("Memory domain attributes:\n");
    printf("  Component name:     %s\n", md_attr->component_name);
    printf("  Max allocation:     %zu\n", (size_t)md_attr->cap.max_alloc);
    printf("  Max registration:   %zu\n", md_attr->cap.max_reg);
    printf("  Flags:              0x%lx\n", md_attr->cap.flags);
    printf("  Reg mem types:      0x%lx\n", md_attr->cap.reg_mem_types);
    printf("  Detect mem types:   0x%lx\n", md_attr->cap.detect_mem_types);
    printf("  Alloc mem types:    0x%lx\n", md_attr->cap.alloc_mem_types);
    printf("  Access mem types:   0x%lx\n", md_attr->cap.access_mem_types);
    printf("  Rkey packed size:   %zu\n", md_attr->rkey_packed_size);
}

int main(int argc, char **argv)
{
    ucs_status_t status;
    uct_md_resource_desc_t *md_resources = NULL;
    uct_component_h *components = NULL;
    uct_md_h md = NULL;
    uct_md_config_t *md_config = NULL;
    uct_md_attr_t md_attr;
    unsigned num_components;
    unsigned num_md_resources;
    unsigned i, j;
    int found_gaudi = 0;
    
    /* Initialize UCT */
    printf("Initializing UCT...\n");
    status = uct_init();
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to initialize UCT: %s\n", ucs_status_string(status));
        return 1;
    }
    
    /* Query UCT components */
    printf("Querying UCT components...\n");
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query UCT components: %s\n", ucs_status_string(status));
        uct_cleanup();
        return 1;
    }
    printf("Found %u components\n", num_components);
    
    /* Iterate through components looking for Gaudi */
    printf("Looking for Gaudi component...\n");
    for (i = 0; i < num_components; i++) {
        printf("Component[%u]: %s\n", i, components[i]->name);
        
        /* Query memory domain resources */
        status = uct_component_query_md_resources(components[i], &md_resources, &num_md_resources);
        if (status != UCS_OK) {
            fprintf(stderr, "Failed to query MD resources: %s\n", ucs_status_string(status));
            continue;
        }
        
        /* Look for Gaudi MD */
        for (j = 0; j < num_md_resources; j++) {
            printf("  MD[%u]: %s\n", j, md_resources[j].md_name);
            
            if (strcmp(md_resources[j].md_name, "gaudi") == 0) {
                printf("Found Gaudi memory domain!\n");
                found_gaudi = 1;
                
                /* Read MD configuration */
                status = uct_md_config_read(components[i], NULL, NULL, &md_config);
                if (status != UCS_OK) {
                    fprintf(stderr, "Failed to read MD config: %s\n", ucs_status_string(status));
                    uct_release_md_resource_list(md_resources);
                    continue;
                }
                
                /* Open Gaudi memory domain */
                printf("Opening Gaudi memory domain...\n");
                status = uct_md_open(components[i], md_resources[j].md_name, md_config, &md);
                uct_config_release(md_config);
                
                if (status != UCS_OK) {
                    fprintf(stderr, "Failed to open Gaudi MD: %s\n", ucs_status_string(status));
                    uct_release_md_resource_list(md_resources);
                    continue;
                }
                
                /* Query MD attributes */
                printf("Querying Gaudi memory domain attributes...\n");
                status = uct_md_query(md, &md_attr);
                if (status != UCS_OK) {
                    fprintf(stderr, "Failed to query Gaudi MD: %s\n", ucs_status_string(status));
                    uct_md_close(md);
                    uct_release_md_resource_list(md_resources);
                    continue;
                }
                
                /* Print MD attributes */
                print_md_attr(&md_attr);
                
                /* Test memory allocation if supported */
                if (md_attr.cap.flags & UCT_MD_FLAG_ALLOC) {
                    void *address = NULL;
                    size_t length = 4096;
                    uct_mem_h memh;
                    
                    printf("Testing memory allocation (4KB)...\n");
                    status = uct_md_mem_alloc(md, &length, &address, 
                                              UCS_MEMORY_TYPE_HOST, 0,
                                              "test_alloc", &memh);
                    if (status != UCS_OK) {
                        fprintf(stderr, "Failed to allocate memory: %s\n", 
                                ucs_status_string(status));
                    } else {
                        printf("Successfully allocated %zu bytes at %p\n", 
                               length, address);
                        
                        /* Write some data */
                        memset(address, 0xAB, length);
                        printf("Memory write test passed\n");
                        
                        /* Free the memory */
                        status = uct_md_mem_free(md, memh);
                        if (status != UCS_OK) {
                            fprintf(stderr, "Failed to free memory: %s\n", 
                                    ucs_status_string(status));
                        } else {
                            printf("Successfully freed memory\n");
                        }
                    }
                } else {
                    printf("Memory allocation not supported by this MD\n");
                }
                
                /* Test memory registration if supported */
                if (md_attr.cap.flags & UCT_MD_FLAG_REG) {
                    void *buffer = malloc(4096);
                    if (buffer) {
                        uct_mem_h memh;
                        
                        printf("Testing memory registration (4KB)...\n");
                        status = uct_md_mem_reg(md, buffer, 4096, 0, &memh);
                        if (status != UCS_OK) {
                            fprintf(stderr, "Failed to register memory: %s\n", 
                                    ucs_status_string(status));
                        } else {
                            printf("Successfully registered memory at %p\n", buffer);
                            
                            /* Write some data */
                            memset(buffer, 0xCD, 4096);
                            printf("Memory write test passed\n");
                            
                            /* Deregister memory */
                            status = uct_md_mem_dereg(md, memh);
                            if (status != UCS_OK) {
                                fprintf(stderr, "Failed to deregister memory: %s\n", 
                                        ucs_status_string(status));
                            } else {
                                printf("Successfully deregistered memory\n");
                            }
                        }
                        
                        free(buffer);
                    }
                } else {
                    printf("Memory registration not supported by this MD\n");
                }
                
                /* Close the MD */
                printf("Closing Gaudi memory domain...\n");
                uct_md_close(md);
            }
        }
        
        uct_release_md_resource_list(md_resources);
    }
    
    if (!found_gaudi) {
        printf("No Gaudi memory domain found in any component\n");
    }
    
    /* Cleanup */
    printf("Cleaning up...\n");
    uct_release_component_list(components);
    uct_cleanup();
    
    return 0;
}
