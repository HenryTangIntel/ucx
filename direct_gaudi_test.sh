#!/bin/bash
# Simplified Gaudi module direct check test

# Colors for readable output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   GAUDI MODULE DIRECT FUNCTIONALITY    ${NC}"
echo -e "${BLUE}========================================${NC}"

export LD_LIBRARY_PATH=/workspace/ucx/install/lib:/workspace/ucx/modules:$LD_LIBRARY_PATH

echo -e "\n${YELLOW}1. Checking Hardware Environment${NC}"
echo -e "   ${YELLOW}1.1 Checking for Gaudi devices${NC}"
NUM_DEVICES=$(lspci -d 1da3: | wc -l)
echo -e "   Found ${GREEN}$NUM_DEVICES${NC} Gaudi devices"

echo -e "\n   ${YELLOW}1.2 Checking kernel module${NC}"
if lsmod | grep -q habanalabs; then
    DRIVER_INFO=$(lsmod | grep habanalabs)
    echo -e "   ${GREEN}Habanalabs driver loaded:${NC} $DRIVER_INFO"
else
    echo -e "   ${RED}Habanalabs driver NOT loaded${NC}"
fi

echo -e "\n   ${YELLOW}1.3 Checking for HL-Thunk library${NC}"
if [ -f /usr/lib/habanalabs/libhl-thunk.so ]; then
    LIB_INFO=$(ls -la /usr/lib/habanalabs/libhl-thunk.so)
    echo -e "   ${GREEN}HL-Thunk library found:${NC} $LIB_INFO"
else
    echo -e "   ${RED}HL-Thunk library NOT found${NC}"
fi

echo -e "\n${YELLOW}2. Checking Module Structure${NC}"
echo -e "   ${YELLOW}2.1 Checking module source files${NC}"
if [ -f /workspace/ucx/src/uct/gaudi/gaudi_md.c ] && [ -f /workspace/ucx/src/uct/gaudi/gaudi_md.h ]; then
    echo -e "   ${GREEN}Module source files exist${NC}"
    MD_SIZE=$(wc -l /workspace/ucx/src/uct/gaudi/gaudi_md.c | awk '{print $1}')
    echo -e "   gaudi_md.c lines: $MD_SIZE"
else
    echo -e "   ${RED}Module source files missing${NC}"
fi

echo -e "\n   ${YELLOW}2.2 Checking module binary${NC}"
if [ -f /workspace/ucx/modules/libuct_gaudi.so ]; then
    echo -e "   ${GREEN}Module binary exists${NC}"
    MODULE_INFO=$(file /workspace/ucx/modules/libuct_gaudi.so)
    echo -e "   Module info: $MODULE_INFO"
    
    # Check if module is properly built
    if [ -L /workspace/ucx/modules/libuct_gaudi.so ]; then
        REAL_MODULE=$(readlink -f /workspace/ucx/modules/libuct_gaudi.so)
        if [ -f "$REAL_MODULE" ]; then
            echo -e "   ${GREEN}Module binary is properly built${NC}"
            echo -e "   Real path: $REAL_MODULE"
        else
            echo -e "   ${RED}Module symlink exists but target is missing${NC}"
        fi
    else
        echo -e "   ${GREEN}Module binary is direct file${NC}"
    fi
else
    echo -e "   ${RED}Module binary missing${NC}"
fi

echo -e "\n${YELLOW}3. Testing Direct Gaudi Hardware Access${NC}"
echo -e "   ${YELLOW}3.1 Checking device nodes${NC}"
if [ -d /dev/habanalabs ]; then
    DEV_INFO=$(ls -la /dev/habanalabs)
    echo -e "   ${GREEN}Habana device nodes found:${NC}"
    echo "$DEV_INFO"
else
    echo -e "   ${RED}No Habana device nodes found${NC}"
fi

echo -e "\n   ${YELLOW}3.2 Testing device open access${NC}"
# Create a simple C test program to try opening a Gaudi device
cat > /workspace/ucx/test_output/gaudi_direct_test.c << 'EOL'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *device_path = "/dev/habanalabs/hl0";
    int fd;
    
    printf("Attempting to open Gaudi device: %s\n", device_path);
    fd = open(device_path, O_RDWR);
    
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }
    
    printf("Successfully opened Gaudi device (fd=%d)\n", fd);
    close(fd);
    
    return 0;
}
EOL

# Try to build and run the direct test
gcc -o /workspace/ucx/test_output/gaudi_direct_test /workspace/ucx/test_output/gaudi_direct_test.c
if [ $? -eq 0 ]; then
    echo -e "   ${GREEN}Direct test program compiled successfully${NC}"
    /workspace/ucx/test_output/gaudi_direct_test
    if [ $? -eq 0 ]; then
        echo -e "   ${GREEN}Direct hardware access successful${NC}"
    else
        echo -e "   ${RED}Direct hardware access failed${NC}"
    fi
else
    echo -e "   ${RED}Failed to compile direct test program${NC}"
fi

echo -e "\n${YELLOW}4. Module Functionality Test${NC}"
echo -e "   ${YELLOW}4.1 Testing module symbol resolution${NC}"
# Use dlopen to test if the module can be loaded and symbols resolved
cat > /workspace/ucx/test_output/gaudi_module_load.c << 'EOL'
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
EOL

# Try to build and run the module load test
gcc -o /workspace/ucx/test_output/gaudi_module_load /workspace/ucx/test_output/gaudi_module_load.c -ldl
if [ $? -eq 0 ]; then
    echo -e "   ${GREEN}Module loading test compiled successfully${NC}"
    /workspace/ucx/test_output/gaudi_module_load
    if [ $? -eq 0 ]; then
        echo -e "   ${GREEN}Module loading successful${NC}"
    else
        echo -e "   ${RED}Module loading failed${NC}"
    fi
else
    echo -e "   ${RED}Failed to compile module loading test${NC}"
fi

echo -e "\n${BLUE}========================================${NC}"
echo -e "${BLUE}           TEST COMPLETE                ${NC}"
echo -e "${BLUE}========================================${NC}"
