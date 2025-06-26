#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <ucs/type/status.h>
#include <uct/api/v2/uct_v2.h>



/* Structure needed for the test */
typedef struct uct_component uct_component_t;
typedef struct uct_md_config uct_md_config_t;
typedef struct uct_md uct_md_t;
typedef uct_md_t* uct_md_h;

typedef uct_component_t* uct_component_h;

/* Function pointer types */
typedef ucs_status_t (*uct_gaudi_md_open_func_t)(uct_component_t *component,
                                                const char *md_name,
                                                const uct_md_config_t *config,
                                                uct_md_h *md_p);

typedef void (*uct_md_close_func_t)(uct_md_h md);

typedef ucs_status_t (*uct_gaudi_md_query_func_t)(uct_md_h md, uct_md_attr_v2_t *md_attr);

typedef ucs_status_t (*uct_gaudi_query_md_resources_func_t)(
    uct_component_h component,
    uct_md_resource_desc_t **resources_p,
    unsigned *num_resources_p);

int main() {
    void *handle;
    uct_gaudi_md_open_func_t gaudi_md_open_func;
    uct_md_close_func_t md_close_func;
    uct_gaudi_md_query_func_t gaudi_md_query_func;
    uct_gaudi_query_md_resources_func_t gaudi_query_md_resources_func;
    uct_md_h md = NULL;
    char *error;
    int success = 0;

    /* Load hlthunk library first to ensure dependencies are resolved */
    void *hlthunk_handle = dlopen("/usr/lib/habanalabs/libhl-thunk.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!hlthunk_handle) {
        fprintf(stderr, "Cannot open hlthunk library: %s\n", dlerror());
        return 1;
    }
    printf("Successfully loaded hlthunk library\n");

    /* Open the Gaudi module */
    handle = dlopen("/workspace/ucx/modules/libuct_gaudi.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "Cannot open Gaudi module: %s\n", dlerror());
        dlclose(hlthunk_handle);
        return 1;
    }
    printf("Successfully loaded Gaudi module\n");

    /* Load the function pointers */
    dlerror(); /* Clear any existing error */

    gaudi_md_open_func = (uct_gaudi_md_open_func_t)dlsym(handle, "uct_gaudi_md_open");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Cannot find uct_gaudi_md_open: %s\n", error);
        dlclose(handle);
        dlclose(hlthunk_handle);
        return 1;
    }
    printf("Successfully loaded uct_gaudi_md_open function\n");

    gaudi_md_query_func = (uct_gaudi_md_query_func_t)dlsym(handle, "uct_gaudi_md_query");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Cannot find uct_gaudi_md_query: %s\n", error);
        dlclose(handle);
        dlclose(hlthunk_handle);
        return 1;
    }
    printf("Successfully loaded uct_gaudi_md_query function\n");

    gaudi_query_md_resources_func = (uct_gaudi_query_md_resources_func_t)dlsym(handle, "uct_gaudi_query_md_resources");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Cannot find uct_gaudi_query_md_resources: %s\n", error);
        dlclose(handle);
        dlclose(hlthunk_handle);
        return 1;
    }
    printf("Successfully loaded uct_gaudi_query_md_resources function\n");

    /* Load the UCT library to get md_close function */
    void *uct_handle = dlopen("/workspace/ucx/src/uct/.libs/libuct.so", RTLD_LAZY);
    if (!uct_handle) {
        fprintf(stderr, "Cannot open UCT library: %s\n", dlerror());
        dlclose(handle);
        dlclose(hlthunk_handle);
        return 1;
    }

    md_close_func = (uct_md_close_func_t)dlsym(uct_handle, "uct_md_close");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Cannot find uct_md_close: %s\n", error);
        dlclose(uct_handle);
        dlclose(handle);
        dlclose(hlthunk_handle);
        return 1;
    }
    printf("Successfully loaded uct_md_close function\n");

    printf("\nTesting uct_gaudi_md_open(NULL, \"gaudi\", NULL, &md)...\n");
    ucs_status_t status = gaudi_md_open_func(NULL, "gaudi", NULL, &md);

    if (status == UCS_OK && md != NULL) {
        printf("✓ uct_gaudi_md_open succeeded! MD handle: %p\n", md);

        /* Test uct_gaudi_md_query */
        uct_md_attr_v2_t md_attr;
        memset(&md_attr, 0, sizeof(md_attr));
        printf("Testing uct_gaudi_md_query(md, &md_attr)...\n");
        status = gaudi_md_query_func(md, &md_attr);
        if (status == UCS_OK) {
            printf("✓ uct_gaudi_md_query succeeded!\n");
            printf("  MD name: %s\n", md_attr.component_name);
            printf("  Max alloc: %zu\n", md_attr.max_alloc);
            printf("  Max reg: %zu\n", md_attr.max_reg);
        } else {
            printf("✗ uct_gaudi_md_query failed with status: %d\n", status);
        }

        /* Test uct_gaudi_query_md_resources */
        uct_md_resource_desc_t *resources = NULL;
        unsigned num_resources = 0;
        printf("Testing uct_gaudi_query_md_resources(NULL, &resources, &num_resources)...\n");
        status = gaudi_query_md_resources_func(NULL, &resources, &num_resources);
        if (status == UCS_OK) {
            printf("✓ uct_gaudi_query_md_resources succeeded! num_resources = %u\n", num_resources);
            for (unsigned i = 0; i < num_resources; ++i) {
                printf("  Resource[%u]: md_name = %s\n", i, resources[i].md_name);
            }
            free(resources);
        } else {
            printf("✗ uct_gaudi_query_md_resources failed with status: %d\n", status);
        }

        /* Close the memory domain */
        printf("Calling uct_md_close...\n");
        md_close_func(md);
        printf("✓ uct_md_close succeeded!\n");
        success = 1;
    } else {
        printf("✗ uct_gaudi_md_open failed with status: %d\n", status);
    }

    /* Cleanup */
    dlclose(uct_handle);
    dlclose(handle);
    dlclose(hlthunk_handle);

    return success ? 0 : 1;
}
