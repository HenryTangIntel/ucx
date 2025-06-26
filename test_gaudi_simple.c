#include <uct/api/uct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status;
    int found_gaudi = 0;
    unsigned i;

    /* Query available UCT components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query UCT components: %s\n", ucs_status_string(status));
        return 1;
    }

    printf("Found %u UCT components:\n", num_components);
    for (i = 0; i < num_components; i++) {
        printf("Component[%u]: %s\n", i, components[i]->name);
        if (strcmp(components[i]->name, "gaudi") == 0) {
            found_gaudi = 1;
            printf("Found Gaudi component!\n");
        }
    }

    if (!found_gaudi) {
        printf("Gaudi component not found\n");
    }

    uct_release_component_list(components);
    return 0;
}
