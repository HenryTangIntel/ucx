#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <ucs/type/status.h>

/* Structure needed for the test */
typedef struct uct_component uct_component_t;
typedef struct uct_md_config uct_md_config_t;
typedef struct uct_md uct_md_t;
typedef uct_md_t* uct_md_h;

/* Function pointer types */
typedef ucs_status_t (*uct_gaudi_md_open_func_t)(uct_component_t *component,
                                                const char *md_name,
                                                const uct_md_config_t *config,
                                                uct_md_h *md_p);

typedef void (*uct_md_close_func_t)(uct_md_h md);

int main() {
    void *handle;
    uct_gaudi_md_open_func_t gaudi_md_open_func;
    uct_md_close_func_t md_close_func;
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
