#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_md_resource_desc_t *resources;
    unsigned num_resources;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT: %s\n", ucs_status_string(status));
        return 1;
    }
    
    /* Query memory domain resources */
    status = uct_query_md_resources(&resources, &num_resources);
    if (status != UCS_OK) {
        printf("Failed to query resources: %s\n", ucs_status_string(status));
        uct_cleanup();
        return 1;
    }
    
    printf("Available memory domains (%u):\n", num_resources);
    for (unsigned i = 0; i < num_resources; i++) {
        printf("  [%u] %s\n", i, resources[i].md_name);
    }
    
    uct_release_md_resource_list(resources);
    uct_cleanup();
    
    return 0;
}
