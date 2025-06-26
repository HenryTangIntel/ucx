#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/base/uct_component.h>

void test_gaudi_md() {
    uct_component_h *components;
    unsigned num_components;
    uct_component_h gaudi_component = NULL;
    ucs_status_t status;

    /* Query available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query components: %s\n", ucs_status_string(status));
        return;
    }

    /* Find the Gaudi component */
    for (unsigned i = 0; i < num_components; i++) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            gaudi_component = components[i];
            printf("Found Gaudi component!\n");
            break;
        }
    }

    if (!gaudi_component) {
        printf("Gaudi component not found in UCX\n");
        uct_release_component_list(components);
        return;
    }

    /* Query memory domains */
    uct_md_resource_desc_t *resources;
    unsigned num_resources;
    
    status = uct_component_query_md_resources(gaudi_component, &resources, &num_resources);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query MD resources: %s\n", ucs_status_string(status));
        uct_release_component_list(components);
        return;
    }
    
    printf("Found %u Gaudi memory domains:\n", num_resources);
    for (unsigned i = 0; i < num_resources; i++) {
        printf("  MD[%u]: %s\n", i, resources[i].md_name);
    }
    
    if (num_resources == 0) {
        printf("No Gaudi memory domains found\n");
        uct_release_md_resource_list(resources);
        uct_release_component_list(components);
        return;
    }
    
    /* Open the Gaudi memory domain */
    uct_md_config_t *md_config;
    uct_md_h md;

    status = uct_md_config_read(gaudi_component, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to read MD config: %s\n", ucs_status_string(status));
        uct_release_md_resource_list(resources);
        uct_release_component_list(components);
        return;
    }
    
    status = uct_md_open(gaudi_component, resources[0].md_name, md_config, &md);
    uct_config_release(md_config);
    
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to open Gaudi MD: %s\n", ucs_status_string(status));
        uct_release_md_resource_list(resources);
        uct_release_component_list(components);
        return;
    }
    
    printf("Successfully opened Gaudi memory domain\n");
    
    /* Query the MD attributes */
    uct_md_attr_t md_attr;
    status = uct_md_query(md, &md_attr);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query MD attributes: %s\n", ucs_status_string(status));
        uct_md_close(md);
        uct_release_md_resource_list(resources);
        uct_release_component_list(components);
        return;
    }
    
    printf("Gaudi memory domain attributes:\n");
    printf("  Component name: %s\n", md_attr.component_name);
    printf("  Capabilities: 0x%lx\n", md_attr.cap.flags);
    printf("    Registration supported: %s\n", 
        (md_attr.cap.flags & UCT_MD_FLAG_REG) ? "yes" : "no");
    printf("    Allocation supported: %s\n", 
        (md_attr.cap.flags & UCT_MD_FLAG_ALLOC) ? "yes" : "no");
    printf("  Max allocation: %zu bytes\n", (size_t)md_attr.cap.max_alloc);
    printf("  Max registration: %zu bytes\n", md_attr.cap.max_reg);
    
    /* Test memory allocation if supported */
    if (md_attr.cap.flags & UCT_MD_FLAG_ALLOC) {
        void *address = NULL;
        size_t length = 4096;
        uct_mem_h memh;
        
        printf("Testing memory allocation (4KB)...\n");
        status = uct_mem_alloc(length, address, 
                                  UCS_MEMORY_TYPE_HOST, 0,
                                  "test_alloc", md, &memh);
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
            status = uct_md_mem_dereg(md, memh);
            if (status != UCS_OK) {
                fprintf(stderr, "Failed to free memory: %s\n", 
                        ucs_status_string(status));
            } else {
                printf("Successfully freed memory\n");
            }
        }
    } else {
        printf("Memory allocation not supported by Gaudi MD\n");
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
        printf("Memory registration not supported by Gaudi MD\n");
    }
    
    /* Close resources */
    uct_md_close(md);
    uct_release_md_resource_list(resources);
    uct_release_component_list(components);
    
    printf("Test completed\n");
}

int main() {
    printf("Testing Gaudi memory domain functionality...\n");
    test_gaudi_md();
    return 0;
}
