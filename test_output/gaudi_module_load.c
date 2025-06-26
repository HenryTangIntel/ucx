#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef struct {
    const char *name;
    void *handle;
} module_info_t;

int main(int argc, char **argv) {
    const char *module_paths[] = {
        "/workspace/ucx/modules/libuct_gaudi.so",
        "../src/uct/gaudi/.libs/libuct_gaudi.so",
        "../modules/libuct_gaudi.so"
    };
    
    void *handle = NULL;
    int i, found = 0;
    char *error;
    
    for (i = 0; !handle && i < sizeof(module_paths)/sizeof(module_paths[0]); i++) {
        printf("Attempting to load module: %s\n", module_paths[i]);
        handle = dlopen(module_paths[i], RTLD_LAZY);
        if (handle) {
            found = 1;
            break;
        }
    }
    
    if (!handle) {
        printf("Failed to load the module: %s\n", dlerror());
        return 1;
    }
    
    printf("Successfully loaded the module: %s\n", module_paths[i]);
    
    /* Try to find component registration symbol */
    void *symbol = dlsym(handle, "uct_gaudi_component");
    if ((error = dlerror()) != NULL) {
        printf("Warning: Could not find uct_gaudi_component symbol: %s\n", error);
    } else {
        printf("Successfully found uct_gaudi_component symbol: %p\n", symbol);
    }
    
    /* Clean up */
    dlclose(handle);
    return 0;
}
