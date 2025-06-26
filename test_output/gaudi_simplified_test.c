#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <uct/base/uct_md.h>
#include <ucs/type/status.h>


/* Simple test program that doesn't rely on UCT headers */
int main(int argc, char **argv) {
    void *uct_lib;
    void *comp_lib;
    void *(*uct_init)(void);
    void *(*uct_cleanup)(void);
    void *(*query_components)(void **, unsigned *);
    int status;
    int found_gaudi = 0;
    
    printf("=== Gaudi Hardware Test ===\n");
    
    /* Check if the Gaudi module exists */
    printf("Checking for Gaudi module...\n");
    comp_lib = dlopen("/workspace/ucx/modules/libuct_gaudi.so", RTLD_LAZY);
    if (!comp_lib) {
        printf("Failed to open Gaudi module: %s\n", dlerror());
        return 1;
    }
    
    printf("Successfully loaded Gaudi module\n");
    dlclose(comp_lib);
    
    /* Check for hardware device nodes */
    printf("\nChecking for Gaudi device nodes...\n");
    FILE *devfd = fopen("/dev/habanalabs/hl0", "r");
    if (devfd) {
        printf("Gaudi device node exists\n");
        fclose(devfd);
    } else {
        printf("Gaudi device node does not exist (this is expected if device nodes are not properly exposed)\n");
    }
    
    /* Check for Gaudi PCI devices */
    printf("\nChecking for Gaudi PCI devices...\n");
    FILE *cmd = popen("lspci -d 1da3: | wc -l", "r");
    if (cmd) {
        char buf[32];
        if (fgets(buf, sizeof(buf), cmd)) {
            int num_devices = atoi(buf);
            printf("Found %d Gaudi devices\n", num_devices);
            if (num_devices > 0) {
                found_gaudi = 1;
            }
        }
        pclose(cmd);
    }
    
    /* Test kernel driver status */
    printf("\nChecking Gaudi kernel driver status...\n");
    cmd = popen("lsmod | grep habanalabs | head -1", "r");
    if (cmd) {
        char buf[256];
        if (fgets(buf, sizeof(buf), cmd)) {
            printf("Habanalabs driver is loaded: %s", buf);
        } else {
            printf("Habanalabs driver is not loaded\n");
        }
        pclose(cmd);
    }
    
    /* Test HL-Thunk library */
    printf("\nChecking for HL-Thunk library...\n");
    void *hl_lib = dlopen("libhl-thunk.so", RTLD_LAZY);
    if (!hl_lib) {
        hl_lib = dlopen("/usr/lib/habanalabs/libhl-thunk.so", RTLD_LAZY);
    }
    
    if (hl_lib) {
        printf("Successfully loaded HL-Thunk library\n");
        
        /* Try to get a few key symbols */
        void *sym_open = dlsym(hl_lib, "hlthunk_open");
        void *sym_close = dlsym(hl_lib, "hlthunk_close");
        
        if (sym_open && sym_close) {
            printf("Found key API functions in HL-Thunk library\n");
        } else {
            printf("Could not find all required API functions\n");
        }
        
        dlclose(hl_lib);
    } else {
        printf("Failed to load HL-Thunk library: %s\n", dlerror());
    }
    
    /* Summary */
    printf("\n=== Test Summary ===\n");
    if (found_gaudi) {
        printf("Gaudi hardware is present in the system\n");
        printf("Module has been successfully loaded\n");
        printf("Further testing requires proper device node access\n");
    } else {
        printf("No Gaudi hardware detected or accessible\n");
    }
    
    return 0;
}
