#include <ucs/type/status.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    int found_gaudi = 0;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT: %s\n", ucs_status_string(status));
        return -1;
    }
    
    /* Query the available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query UCT components: %s\n", ucs_status_string(status));
        uct_cleanup();
        return -1;
    }
    
    printf("Found %u UCT components:\n", num_components);
    for (unsigned i = 0; i < num_components; i++) {
        printf("  %s\n", components[i]->name);
        if (strcmp(components[i]->name, "gaudi") == 0) {
            found_gaudi = 1;
        }
    }
    
    uct_release_component_list(components);
    uct_cleanup();
    
    if (found_gaudi) {
        printf("\nSUCCESS: Gaudi component is registered with UCT\n");
        return 0;
    } else {
        printf("\nFAILURE: Gaudi component is NOT registered with UCT\n");
        return -1;
    }
}
