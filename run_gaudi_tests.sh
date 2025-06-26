#!/bin/bash
# Full Gaudi module test suite

# Colors for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Keep track of test results
PASSED=0
FAILED=0
SKIPPED=0

echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}   UCX GAUDI MODULE FULL TEST SUITE    ${NC}"
echo -e "${BLUE}=======================================${NC}"
echo

# Function to run a test and track results
run_test() {
    local name=$1
    local cmd=$2
    local skip_condition=$3

    echo -e "${YELLOW}[TEST] ${name}${NC}"
    
    if [ -n "$skip_condition" ] && eval "$skip_condition"; then
        echo -e "  ${YELLOW}[SKIPPED] Test skipped due to dependency failure${NC}"
        SKIPPED=$((SKIPPED+1))
        return
    fi
    
    echo -e "  Running: $cmd"
    eval "$cmd"
    
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}[PASSED]${NC}"
        PASSED=$((PASSED+1))
    else
        echo -e "  ${RED}[FAILED]${NC}"
        FAILED=$((FAILED+1))
    fi
    echo
}

# Set up environment
export LD_LIBRARY_PATH=/workspace/ucx/install/lib:$LD_LIBRARY_PATH

# SECTION 1: Environment Check
echo -e "${BLUE}=== ENVIRONMENT CHECK ===${NC}"

# Test 1: Check for Gaudi hardware
run_test "Checking for Gaudi hardware devices" \
  "lspci -d 1da3: | grep -q 'Habana Labs' && echo 'Found Habana Labs devices' || (echo 'No Habana Labs devices found' && false)"

# Test 2: Check for kernel module
run_test "Checking for habanalabs kernel module" \
  "lsmod | grep -q habanalabs && echo 'habanalabs kernel module is loaded' || (echo 'habanalabs kernel module not loaded' && false)"

# Test 3: Check for library
run_test "Checking for HL-Thunk library" \
  "ldconfig -p | grep -q libhl-thunk.so || [ -f /usr/lib/habanalabs/libhl-thunk.so ] && echo 'libhl-thunk.so found' || (echo 'libhl-thunk.so not found' && false)"

# SECTION 2: Module Structure Check
echo -e "${BLUE}=== MODULE STRUCTURE CHECK ===${NC}"

# Test 4: Check for Gaudi module files
run_test "Checking for Gaudi module source files" \
  "[ -f /workspace/ucx/src/uct/gaudi/gaudi_md.c ] && [ -f /workspace/ucx/src/uct/gaudi/gaudi_md.h ] && echo 'Gaudi module source files exist' || (echo 'Gaudi module source files missing' && false)"

# Test 5: Check for built module
module_test_failed=""
run_test "Checking for built module" \
  "[ -f /workspace/ucx/modules/libuct_gaudi.so ] && echo 'Gaudi module binary exists' || (echo 'Gaudi module binary missing' && false)" 
if [ $? -ne 0 ]; then module_test_failed=true; fi

# SECTION 3: Basic Functionality Test
echo -e "${BLUE}=== BASIC FUNCTIONALITY TEST ===${NC}"

# Create a simple test program
mkdir -p /workspace/ucx/test_output
cat > /workspace/ucx/test_output/gaudi_module_check.c << 'EOL'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>

extern ucs_status_t uct_init(void);
extern ucs_status_t uct_cleanup(void);
extern ucs_status_t uct_query_components(uct_component_h **components_p, unsigned *num_components_p);
extern void uct_release_component_list(uct_component_h *components);

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h *components;
    unsigned num_components;
    int found_gaudi = 0;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT\n");
        return 1;
    }
    
    /* Query the available components */
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        printf("Failed to query UCT components\n");
        uct_cleanup();
        return 1;
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
        printf("SUCCESS: Gaudi component is registered with UCT\n");
        return 0;
    } else {
        printf("FAILURE: Gaudi component is NOT registered with UCT\n");
        return 1;
    }
}
EOL

# Test 6: Compile the module check program
run_test "Compiling Gaudi module check program" \
  "gcc -o /workspace/ucx/test_output/gaudi_module_check /workspace/ucx/test_output/gaudi_module_check.c -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -luct -lucs"

# Test 7: Run the module check program
run_test "Running Gaudi module check" \
  "LD_LIBRARY_PATH=/workspace/ucx/install/lib:/workspace/ucx/modules:$LD_LIBRARY_PATH /workspace/ucx/test_output/gaudi_module_check" \
  "[ \$FAILED -gt 0 ]"

# SECTION 4: Advanced Functionality Test
echo -e "${BLUE}=== ADVANCED FUNCTIONALITY TEST ===${NC}"

# Create a more comprehensive test
cat > /workspace/ucx/test_output/gaudi_md_test.c << 'EOL'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>

extern ucs_status_t uct_init(void);
extern ucs_status_t uct_cleanup(void);
extern ucs_status_t uct_query_components(uct_component_h **components_p, unsigned *num_components_p);
extern void uct_release_component_list(uct_component_h *components);
extern ucs_status_t uct_md_config_read(uct_component_h component, const char *env_prefix,
                                    const char *filename, uct_md_config_t **config_p);
extern void uct_config_release(void *config);
extern ucs_status_t uct_md_open(uct_component_h component, const char *name, 
                              const uct_md_config_t *config, uct_md_h *md_p);
extern void uct_md_close(uct_md_h md);

static uct_component_h find_gaudi_component() {
    ucs_status_t status;
    uct_component_h *components, gaudi_comp = NULL;
    unsigned num_components;
    
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        return NULL;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            gaudi_comp = components[i];
            break;
        }
    }
    
    uct_release_component_list(components);
    return gaudi_comp;
}

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h gaudi_comp;
    uct_md_config_t *md_config;
    uct_md_h md;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT\n");
        return 1;
    }
    
    /* Find Gaudi component */
    gaudi_comp = find_gaudi_component();
    if (gaudi_comp == NULL) {
        printf("Gaudi component not found\n");
        uct_cleanup();
        return 1;
    }
    
    printf("Found Gaudi component\n");
    
    /* Read MD config */
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read Gaudi MD config\n");
        uct_cleanup();
        return 1;
    }
    
    printf("Successfully read Gaudi MD config\n");
    
    /* Open MD */
    status = uct_md_open(gaudi_comp, "gaudi", md_config, &md);
    uct_config_release(md_config);
    
    if (status != UCS_OK) {
        printf("Failed to open Gaudi memory domain: %s\n", ucs_status_string(status));
        uct_cleanup();
        return 1;
    }
    
    printf("Successfully opened Gaudi memory domain\n");
    
    /* Close MD and cleanup */
    uct_md_close(md);
    uct_cleanup();
    
    printf("Test completed successfully\n");
    return 0;
}
EOL

# Test 8: Compile the advanced module test
run_test "Compiling Gaudi MD test" \
  "gcc -o /workspace/ucx/test_output/gaudi_md_test /workspace/ucx/test_output/gaudi_md_test.c -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -luct -lucs"

# Test 9: Run the advanced module test
run_test "Running Gaudi MD test" \
  "LD_LIBRARY_PATH=/workspace/ucx/install/lib:/workspace/ucx/modules:$LD_LIBRARY_PATH /workspace/ucx/test_output/gaudi_md_test" \
  "[ \$FAILED -gt 0 ]"

# SECTION 5: Hardware Interaction Test
echo -e "${BLUE}=== HARDWARE INTERACTION TEST ===${NC}"

# Create hardware interaction test
cat > /workspace/ucx/test_output/gaudi_hw_test.c << 'EOL'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>
#include <ucs/type/status.h>

extern ucs_status_t uct_init(void);
extern ucs_status_t uct_cleanup(void);
extern ucs_status_t uct_query_components(uct_component_h **components_p, unsigned *num_components_p);
extern void uct_release_component_list(uct_component_h *components);
extern ucs_status_t uct_md_config_read(uct_component_h component, const char *env_prefix,
                                    const char *filename, uct_md_config_t **config_p);
extern void uct_config_release(void *config);
extern ucs_status_t uct_md_open(uct_component_h component, const char *name, 
                              const uct_md_config_t *config, uct_md_h *md_p);
extern void uct_md_close(uct_md_h md);
extern ucs_status_t uct_md_mem_alloc(uct_md_h md, size_t *length_p, void **address_p,
                                   unsigned flags, const char *name, uct_mem_h *memh_p);
extern ucs_status_t uct_md_mem_free(uct_md_h md, uct_mem_h memh);

#define TEST_SIZE 1024

static uct_component_h find_gaudi_component() {
    ucs_status_t status;
    uct_component_h *components, gaudi_comp = NULL;
    unsigned num_components;
    
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        return NULL;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            gaudi_comp = components[i];
            break;
        }
    }
    
    uct_release_component_list(components);
    return gaudi_comp;
}

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h gaudi_comp;
    uct_md_config_t *md_config;
    uct_md_h md;
    void *address;
    uct_mem_h memh;
    size_t length = TEST_SIZE;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT\n");
        return 1;
    }
    
    /* Find Gaudi component */
    gaudi_comp = find_gaudi_component();
    if (gaudi_comp == NULL) {
        printf("Gaudi component not found\n");
        uct_cleanup();
        return 1;
    }
    
    /* Read MD config */
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read Gaudi MD config\n");
        uct_cleanup();
        return 1;
    }
    
    /* Open MD */
    status = uct_md_open(gaudi_comp, "gaudi", md_config, &md);
    uct_config_release(md_config);
    
    if (status != UCS_OK) {
        printf("Failed to open Gaudi memory domain\n");
        uct_cleanup();
        return 1;
    }
    
    /* Try to allocate memory */
    printf("Attempting to allocate %zu bytes using Gaudi MD\n", length);
    status = uct_md_mem_alloc(md, &length, &address, 0, "test", &memh);
    
    if (status != UCS_OK) {
        printf("Memory allocation failed: %s\n", ucs_status_string(status));
        uct_md_close(md);
        uct_cleanup();
        return 1;
    }
    
    printf("Successfully allocated %zu bytes at %p\n", length, address);
    
    /* Write to the allocated memory */
    memset(address, 0xAB, length);
    printf("Successfully wrote to allocated memory\n");
    
    /* Free the memory */
    status = uct_md_mem_free(md, memh);
    
    if (status != UCS_OK) {
        printf("Memory free failed: %s\n", ucs_status_string(status));
        uct_md_close(md);
        uct_cleanup();
        return 1;
    }
    
    printf("Successfully freed memory\n");
    
    /* Close MD and cleanup */
    uct_md_close(md);
    uct_cleanup();
    
    printf("Test completed successfully\n");
    return 0;
}
EOL

# Test 10: Compile the hardware test
run_test "Compiling Gaudi hardware test" \
  "gcc -o /workspace/ucx/test_output/gaudi_hw_test /workspace/ucx/test_output/gaudi_hw_test.c -I/workspace/ucx/install/include -L/workspace/ucx/install/lib -luct -lucs"

# Test 11: Run the hardware test
run_test "Running Gaudi hardware test" \
  "LD_LIBRARY_PATH=/workspace/ucx/install/lib:/workspace/ucx/modules:$LD_LIBRARY_PATH /workspace/ucx/test_output/gaudi_hw_test" \
  "[ \$FAILED -gt 0 ]"

# SECTION 6: Final Hardware Check
echo -e "${BLUE}=== FINAL HARDWARE CHECK ===${NC}"
echo "Checking Gaudi hardware details:"

# Test 12: Detailed hardware check
run_test "Detailed hardware check" \
  "lspci -d 1da3: -vvv | grep -E 'Memory at|IRQ|Kernel driver|Region'" 

# Print summary
echo -e "${BLUE}=======================================${NC}"
echo -e "${BLUE}            TEST SUMMARY               ${NC}"
echo -e "${BLUE}=======================================${NC}"
echo -e "${GREEN}PASSED:  $PASSED${NC}"
echo -e "${RED}FAILED:  $FAILED${NC}"
echo -e "${YELLOW}SKIPPED: $SKIPPED${NC}"
echo

# Final verdict
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed! The Gaudi module is working properly.${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. Please check the output above for details.${NC}"
    exit 1
fi
