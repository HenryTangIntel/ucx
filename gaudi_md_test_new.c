#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <uct/api/uct.h>

/* Forward declarations for UCT API functions */
extern ucs_status_t uct_init(void);
extern void uct_cleanup(void);
extern ucs_status_t uct_query_components(uct_component_h **components_p, unsigned *num_components_p);
extern void uct_release_component_list(uct_component_h *components);
extern ucs_status_t uct_component_query(uct_component_h component, uct_component_attr_t *component_attr);
extern ucs_status_t uct_component_query_md_resources(uct_component_h component, 
                                                    uct_md_resource_desc_t **resources_p,
                                                    unsigned *num_resources_p);
extern void uct_release_md_resource_list(uct_md_resource_desc_t *resources);

int main() {
    ucs_status_t status;
    uct_md_config_t *md_config = NULL;
    uct_md_h md = NULL;
    uct_component_h *components = NULL;
    uct_md_resource_desc_t *md_resources = NULL;
    uct_md_attr_t md_attr;
    unsigned num_components;
    unsigned num_md_resources;
    int found = 0;

    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT: %s\n", ucs_status_string(status));
        return 1;
    }
    
    printf("UCT initialized successfully\n");
    
    /* Get list of available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query components: %s\n", ucs_status_string(status));
        uct_cleanup();
        return 1;
    }
    
    printf("Found %u components\n", num_components);
    
    /* Find the Gaudi component and memory domain */
    for (unsigned i = 0; i < num_components; i++) {
        uct_component_attr_t component_attr;
        
        /* Get component name */
        memset(&component_attr, 0, sizeof(component_attr));
        component_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        status = uct_component_query(components[i], &component_attr);
        if (status != UCS_OK) {
            printf("Failed to query component attributes: %s\n", ucs_status_string(status));
            continue;
        }
        
        printf("Component[%u]: %s\n", i, component_attr.name);
        
        /* Query memory domain resources */
        status = uct_component_query_md_resources(components[i], &md_resources, &num_md_resources);
        if (status != UCS_OK) {
            printf("Failed to query MD resources: %s\n", ucs_status_string(status));
            continue;
        }
        
        /* Look for Gaudi MD */
        for (unsigned j = 0; j < num_md_resources; j++) {
            printf("  MD[%u]: %s\n", j, md_resources[j].md_name);
            
            if (strcmp(md_resources[j].md_name, "gaudi") == 0) {
                printf("Found Gaudi memory domain!\n");
                found = 1;
                
                /* Read MD configuration */
                status = uct_md_config_read(components[i], NULL, NULL, &md_config);
                if (status != UCS_OK) {
                    printf("Failed to read MD config: %s\n", ucs_status_string(status));
                    uct_release_md_resource_list(md_resources);
                    continue;
                }
                
                /* Open Gaudi memory domain */
                printf("Opening Gaudi memory domain...\n");
                status = uct_md_open(components[i], md_resources[j].md_name, md_config, &md);
                uct_config_release(md_config);
                
                if (status != UCS_OK) {
                    printf("Failed to open Gaudi MD: %s\n", ucs_status_string(status));
                    uct_release_md_resource_list(md_resources);
                    continue;
                }
                
                /* Success! */
                printf("Successfully opened Gaudi memory domain\n");
                uct_release_md_resource_list(md_resources);
                break;
            }
        }
        
        if (found) {
            break;
        }
        
        uct_release_md_resource_list(md_resources);
    }
    
    if (!found) {
        printf("Gaudi memory domain not found.\n");
        uct_release_component_list(components);
        uct_cleanup();
        return 1;
    }

    /* Query memory domain capabilities */
    memset(&md_attr, 0, sizeof(md_attr));
    status = uct_md_query(md, &md_attr);
    if (status == UCS_OK) {
        printf("Memory domain attributes:\n");
        printf("  Component name:     %s\n", md_attr.component_name);
        printf("  Max allocation:     %zu\n", (size_t)md_attr.cap.max_alloc);
        printf("  Max registration:   %zu\n", md_attr.cap.max_reg);
        printf("  Flags:              0x%lx\n", md_attr.cap.flags);
        printf("  Reg mem types:      0x%lx\n", md_attr.cap.reg_mem_types);
        printf("  Detect mem types:   0x%lx\n", md_attr.cap.detect_mem_types);
        printf("  Alloc mem types:    0x%lx\n", md_attr.cap.alloc_mem_types);
        printf("  Access mem types:   0x%lx\n", md_attr.cap.access_mem_types);
        printf("  Rkey packed size:   %zu\n", md_attr.rkey_packed_size);
    } else {
        printf("Failed to query memory domain attributes: %s\n", ucs_status_string(status));
    }
    
    /* Close the memory domain */
    printf("Closing memory domain...\n");
    uct_md_close(md);
    printf("Memory domain closed successfully\n");
    
    /* Cleanup */
    uct_release_component_list(components);
    uct_cleanup();
    
    printf("Test completed successfully\n");
    return 0;
}
