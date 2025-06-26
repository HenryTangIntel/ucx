#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>

extern ucs_status_t uct_init(void);
extern ucs_status_t uct_cleanup(void);
extern ucs_status_t uct_query_components(uct_component_h **components_p, unsigned *num_components_p);
extern void uct_release_component_list(uct_component_h *components);

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    int found_gaudi = 0;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT\n");
        return 1;
    }
    
    /* Query the available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query UCT components\n");
        uct_cleanup();
        return 1;
    }
    
    printf("Found %u UCT components:\n", num_components);
    for (unsigned i = 0; i < num_components; i++) {
        printf("  %u: %s\n", i, components[i]->name);
        if (strcmp(components[i]->name, "gaudi") == 0) {
            found_gaudi = 1;
        }
    }
    
    uct_release_component_list(components);
    uct_cleanup();
    
    if (found_gaudi) {
        printf("SUCCESS: Gaudi component is registered with UCT\n");
        return 0;
    } else {
        printf("FAILURE: Gaudi component is NOT registered with UCT\n");
        return 1;
    }
}
