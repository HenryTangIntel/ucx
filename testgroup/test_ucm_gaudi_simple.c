#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    void *handle;
    
    printf("Testing UCM Gaudi module loading...\n");
    
    // Try to load the UCM Gaudi module
    handle = dlopen("/workspace/ucx/install/lib/ucx/libucm_gaudi.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load UCM Gaudi module: %s\n", dlerror());
        return 1;
    }
    
    printf("✓ UCM Gaudi module loaded successfully\n");
    
    // Check if our hook functions are available
    void *alloc_func = dlsym(handle, "ucm_hlthunk_device_memory_alloc");
    void *free_func = dlsym(handle, "ucm_hlthunk_device_memory_free");
    void *map_func = dlsym(handle, "ucm_hlthunk_device_memory_map");
    
    if (alloc_func && free_func && map_func) {
        printf("✓ All UCM hlthunk hook functions found\n");
    } else {
        printf("✗ Missing some hook functions\n");
    }
    
    dlclose(handle);
    printf("✓ Test completed successfully\n");
    
    return 0;
}
