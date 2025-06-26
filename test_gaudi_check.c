#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/base/uct_component.h>

void show_components() {
    uct_component_h *components;
    unsigned num_components;
    ucs_status_t status;

    /* Query available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        fprintf(stderr, "Failed to query components: %s\n", ucs_status_string(status));
        return;
    }

    printf("Found %u components:\n", num_components);
    for (unsigned i = 0; i < num_components; i++) {
        printf("Component[%u]: %s\n", i, components[i]->name);
    }

    uct_release_component_list(components);
}

int main() {
    printf("Checking for Gaudi module in UCX...\n");
    show_components();
    return 0;
}
