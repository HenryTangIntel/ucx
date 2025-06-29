#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <ucs/type/status.h>
#include <uct/api/uct.h>

// Forward declaration for the function pointer type
// (matches the static definition in gaudi_md.c)
typedef ucs_status_t (*uct_gaudi_query_md_resources_func_t)(
    void *component,
    uct_md_resource_desc_t **resources_p,
    unsigned *num_resources_p);

int main() {
    void *handle;
    uct_gaudi_query_md_resources_func_t query_func;
    uct_md_resource_desc_t *resources = NULL;
    unsigned num_resources = 0;
    char *error;
    int success = 0;

    printf("Opening Gaudi module for uct_gaudi_query_md_resources test...\n");
    handle = dlopen("/workspace/ucx/modules/libuct_gaudi.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "Cannot open Gaudi module: %s\n", dlerror());
        return 1;
    }

    // Try to load the symbol (may be static, so this may fail)
    query_func = (uct_gaudi_query_md_resources_func_t)dlsym(handle, "uct_gaudi_query_md_resources");
    error = dlerror();
    if (error != NULL || !query_func) {
        printf("Could not find uct_gaudi_query_md_resources symbol (likely static): %s\n", error ? error : "not found");
        dlclose(handle);
        return 77; // skip
    }

    printf("Successfully loaded uct_gaudi_query_md_resources function at %p\n", (void*)query_func);

    ucs_status_t status = query_func(NULL, &resources, &num_resources);
    if (status == UCS_OK && resources && num_resources > 0) {
        printf("✓ uct_gaudi_query_md_resources succeeded!\n");
        for (unsigned i = 0; i < num_resources; ++i) {
            printf("  Resource[%u]: md_name = %s\n", i, resources[i].md_name);
        }
        free(resources);
        success = 1;
    } else {
        printf("✗ uct_gaudi_query_md_resources failed with status: %d\n", status);
    }

    dlclose(handle);
    return success ? 0 : 1;
}
