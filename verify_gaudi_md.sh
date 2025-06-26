#!/bin/bash
# Comprehensive Gaudi MD test script

echo "====================================================================="
echo "          UCX Gaudi Memory Domain Test - $(date)"
echo "====================================================================="

# Check if Gaudi kernel module is loaded
echo -e "\n1. Checking for Habana Labs kernel modules..."
KERNEL_MODULES=$(lsmod | grep -c habanalabs)
if [ $KERNEL_MODULES -gt 0 ]; then
    echo "✓ Habana Labs kernel modules are loaded"
    lsmod | grep habanalabs
else
    echo "✗ No Habana Labs kernel modules found"
    exit 1
fi

# Check for Gaudi devices
echo -e "\n2. Checking for Gaudi hardware..."
GAUDI_DEVICES=$(lspci | grep -c "Habana Labs")
if [ $GAUDI_DEVICES -gt 0 ]; then
    echo "✓ Found $GAUDI_DEVICES Gaudi device(s)"
    lspci | grep "Habana Labs"
else
    echo "✗ No Gaudi devices found"
    exit 1
fi

# Check if Gaudi UCT module exists
echo -e "\n3. Checking for Gaudi UCT module..."
MODULE_PATH="/workspace/ucx/modules/libuct_gaudi.so"
if [ -f "$MODULE_PATH" ]; then
    echo "✓ Gaudi UCT module exists: $MODULE_PATH"
    ls -lh $MODULE_PATH
    MODULE_SIZE=$(stat -c%s "$MODULE_PATH")
    echo "  Module size: $MODULE_SIZE bytes"
else
    echo "✗ Gaudi UCT module not found"
    exit 1
fi

# Check that hlthunk library is available
echo -e "\n4. Checking for hlthunk library..."
if [ -f "/usr/lib/habanalabs/libhl-thunk.so" ]; then
    echo "✓ hlthunk library found"
    ls -lh /usr/lib/habanalabs/libhl-thunk.so
else
    echo "✗ hlthunk library not found"
    exit 1
fi

# Compile direct test program
echo -e "\n5. Compiling direct Gaudi MD test program..."
cat > gaudi_direct_test.c << 'EOF'
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
EOF

gcc -o gaudi_direct_test gaudi_direct_test.c -I/workspace/ucx/src -L/workspace/ucx/src/ucs/.libs -L/usr/lib/habanalabs -lucs -ldl
if [ $? -ne 0 ]; then
    echo "✗ Compilation failed"
    exit 1
else
    echo "✓ Compilation successful"
fi

# Run the test
echo -e "\n6. Running Gaudi MD test..."
LD_LIBRARY_PATH=/workspace/ucx/src/ucs/.libs:/workspace/ucx/src/uct/.libs:/usr/lib/habanalabs ./gaudi_direct_test
if [ $? -ne 0 ]; then
    echo -e "\n✗ TEST FAILED"
    exit 1
else
    echo -e "\n✓ TEST PASSED"
fi

echo -e "\nVerification summary:"
echo "✓ Kernel modules loaded"
echo "✓ $GAUDI_DEVICES Gaudi devices found"
echo "✓ Gaudi UCT module exists"
echo "✓ hlthunk library available"
echo "✓ Successfully compiled test program"
echo "✓ Successfully ran uct_gaudi_md_open and uct_md_close"
echo -e "\nGaudi memory domain functionality verified!"

exit 0
