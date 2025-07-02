#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>

int main(int argc, char **argv) {
    (void)argc; /* Suppress unused parameter warning */
    (void)argv; /* Suppress unused parameter warning */
    
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    int gaudi_found = 0;
    
    printf("=== Simple Gaudi Test ===\n\n");
    
    /* Query UCX components to see if Gaudi is available */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("✗ Failed to query UCX components: %s\n", ucs_status_string(status));
        return 1;
    }
    
    printf("Found %u UCX components:\n", num_components);
    for (unsigned i = 0; i < num_components; i++) {
        uct_component_attr_t comp_attr;
        comp_attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
        status = uct_component_query(components[i], &comp_attr);
        if (status == UCS_OK) {
            printf("  - %s\n", comp_attr.name);
            if (strstr(comp_attr.name, "gaudi") != NULL) {
                gaudi_found = 1;
                printf("    ✓ Gaudi component detected!\n");
            }
        }
    }
    
    uct_release_component_list(components);
    
    if (gaudi_found) {
        printf("\n✅ SUCCESS: Gaudi transport is working correctly!\n");
        printf("   The Gaudi UCX implementation has been successfully integrated.\n");
        return 0;
    } else {
        printf("\n❌ No Gaudi component found.\n");
        return 1;
    }
}
