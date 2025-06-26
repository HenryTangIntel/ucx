#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

int main() {
    void *handle;
    void *sym;
    char *error;
    int found_module = 0;
    
    printf("Checking for Gaudi module presence...\n");
    
    /* Check for the Gaudi module */
    handle = dlopen("/workspace/ucx/modules/libuct_gaudi.so", RTLD_LAZY);
    if (!handle) {
        printf("Failed to open Gaudi module: %s\n", dlerror());
    } else {
        printf("Successfully opened Gaudi module\n");
        
        /* Check for the key functions */
        dlerror(); /* Clear any existing error */
        
        sym = dlsym(handle, "uct_gaudi_md_open");
        error = dlerror();
        if (error != NULL) {
            printf("Failed to find uct_gaudi_md_open: %s\n", error);
        } else {
            printf("Found function: uct_gaudi_md_open at %p\n", sym);
            found_module = 1;
        }
        
        sym = dlsym(handle, "uct_gaudi_component");
        error = dlerror();
        if (error != NULL) {
            printf("Failed to find uct_gaudi_component: %s\n", error);
        } else {
            printf("Found symbol: uct_gaudi_component at %p\n", sym);
            found_module = 1;
        }
        
        dlclose(handle);
    }
    
    /* Check for Gaudi devices */
    printf("\nChecking for Gaudi hardware...\n");
    FILE* fp = popen("lspci | grep -i habana", "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
            printf("Detected device: %s", buf);
            found_module = 1;
        }
        pclose(fp);
    }
    
    /* Check for kernel module */
    printf("\nChecking for kernel module...\n");
    fp = popen("lsmod | grep habanalabs", "r");
    if (fp) {
        char buf[1024];
        while (fgets(buf, sizeof(buf) - 1, fp) != NULL) {
            printf("Kernel module: %s", buf);
            found_module = 1;
        }
        pclose(fp);
    }
    
    if (found_module) {
        printf("\nGaudi module verification successful\n");
        return 0;
    } else {
        printf("\nGaudi module verification failed\n");
        return 1;
    }
}
