#!/bin/bash
# Unified Gaudi module test script

# Colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "\n${YELLOW}===== Running Gaudi Module Verification =====${NC}"

# Set up environment
export LD_LIBRARY_PATH=/workspace/ucx/install/lib:$LD_LIBRARY_PATH
cd /workspace/ucx

# Compile simple test program
echo -e "\n${YELLOW}Compiling Gaudi module verification program...${NC}"
cat > gaudi_verify.c << 'EOL'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    int found_gaudi = 0;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT: %s\n", ucs_status_string(status));
        return -1;
    }
    
    /* Query the available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query UCT components: %s\n", ucs_status_string(status));
        uct_cleanup();
        return -1;
    }
    
    printf("Found %u UCT components:\n", num_components);
    for (unsigned i = 0; i < num_components; i++) {
        printf("  %u: %s\n", i, components[i]->name);
        if (strcmp(components[i]->name, "gaudi") == 0) {
            found_gaudi = 1;
        }
    }
    
    uct_release_component_list(components);
    uct_cleanup();
    
    if (found_gaudi) {
        printf("\nSUCCESS: Gaudi component is registered with UCT\n");
        return 0;
    } else {
        printf("\nFAILURE: Gaudi component is NOT registered with UCT\n");
        return -1;
    }
}
EOL

# Build the test program
gcc -o gaudi_verify gaudi_verify.c -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -luct -lucs

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Compilation successful${NC}"
    # Run the test
    echo -e "\n${YELLOW}Running component verification...${NC}"
    ./gaudi_verify
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Gaudi module is properly registered with UCX${NC}"
        RESULT=0
    else
        echo -e "${RED}❌ Gaudi module is NOT properly registered with UCX${NC}"
        RESULT=1
    fi
else
    echo -e "${RED}❌ Compilation failed${NC}"
    RESULT=1
fi

# Check for hardware
echo -e "\n${YELLOW}Checking for Gaudi hardware...${NC}"

if lspci -d 1da3: | grep -q ""; then
    echo -e "${GREEN}Habana Labs devices found:${NC}"
    lspci -d 1da3:
else
    echo -e "${YELLOW}No Habana Labs devices detected${NC}"
fi

if lsmod | grep -q habanalabs; then
    echo -e "\n${GREEN}Habanalabs kernel module is loaded${NC}"
else
    echo -e "\n${YELLOW}Habanalabs kernel module is not loaded${NC}"
fi

# Look for HL-Thunk library
echo -e "\n${YELLOW}Checking for HL-Thunk library...${NC}"
for path in /usr/lib /usr/lib64 /usr/local/lib /usr/lib/habanalabs; do
    if [ -f "$path/libhl-thunk.so" ]; then
        echo -e "${GREEN}Found libhl-thunk.so at $path/libhl-thunk.so${NC}"
        HL_FOUND=1
        break
    fi
done

if [ -z "$HL_FOUND" ]; then
    echo -e "${YELLOW}libhl-thunk.so not found in standard library paths${NC}"
fi

# Summary
echo -e "\n${YELLOW}===== Summary =====${NC}"
if [ $RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ UCX Gaudi module is properly installed and registered${NC}"
    echo -e "The UCX Gaudi module has been successfully integrated into UCX."
    
    if [ -n "$HL_FOUND" ] && lsmod | grep -q habanalabs && lspci -d 1da3: | grep -q ""; then
        echo -e "${GREEN}✅ Gaudi hardware and drivers are properly installed${NC}"
        echo -e "You can use the Gaudi module with your Habana Labs hardware."
    else
        echo -e "${YELLOW}⚠️ Gaudi hardware or drivers may not be fully installed${NC}"
        echo -e "The module is registered correctly, but full functionality may be limited."
        echo -e "This is expected if running in an environment without Gaudi hardware."
    fi
else
    echo -e "${RED}❌ UCX Gaudi module verification failed${NC}"
    echo -e "The module may not be properly integrated into UCX."
fi

exit $RESULT
