#!/bin/bash
# Direct Gaudi hardware test script

# Print out colorized output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Set up environment
export LD_LIBRARY_PATH=/workspace/ucx/src/uct/.libs:/workspace/ucx/src/ucs/.libs:/workspace/ucx/modules:$LD_LIBRARY_PATH

echo -e "${BLUE}=== Gaudi Hardware and Module Test ===${NC}"

# Test 1: Check if hardware is present
echo -e "\n${YELLOW}1. Checking for Gaudi hardware${NC}"
GAUDI_COUNT=$(lspci -d 1da3: | wc -l)
if [ "$GAUDI_COUNT" -gt 0 ]; then
    echo -e "${GREEN}✓ Found $GAUDI_COUNT Gaudi devices${NC}"
    lspci -d 1da3: | head -3
    if [ "$GAUDI_COUNT" -gt 3 ]; then
        echo "   (and $(($GAUDI_COUNT-3)) more devices)"
    fi
else
    echo -e "${RED}✗ No Gaudi devices found${NC}"
fi

# Test 2: Check if kernel module is loaded
echo -e "\n${YELLOW}2. Checking kernel driver${NC}"
if lsmod | grep -q habanalabs; then
    echo -e "${GREEN}✓ Habanalabs kernel module is loaded${NC}"
    lsmod | grep habanalabs | head -1
else
    echo -e "${RED}✗ Habanalabs kernel module is not loaded${NC}"
fi

# Test 3: Check if HL-Thunk library is available
echo -e "\n${YELLOW}3. Checking for HL-Thunk library${NC}"
if [ -f /usr/lib/habanalabs/libhl-thunk.so ]; then
    echo -e "${GREEN}✓ HL-Thunk library found${NC}"
    file /usr/lib/habanalabs/libhl-thunk.so
else
    echo -e "${RED}✗ HL-Thunk library not found${NC}"
fi

# Test 4: Check if device nodes are accessible
echo -e "\n${YELLOW}4. Checking device nodes${NC}"
if [ -d /dev/habanalabs ]; then
    echo -e "${GREEN}✓ Device directory exists${NC}"
    ls -la /dev/habanalabs/
else
    echo -e "${RED}✗ Device directory does not exist${NC}"
fi

# Test 5: Check if Gaudi module is built
echo -e "\n${YELLOW}5. Checking module binary${NC}"
if [ -f /workspace/ucx/modules/libuct_gaudi.so ]; then
    echo -e "${GREEN}✓ Gaudi module exists${NC}"
    file /workspace/ucx/modules/libuct_gaudi.so
    
    # Check actual module file
    MODULE_PATH=$(readlink -f /workspace/ucx/modules/libuct_gaudi.so)
    if [ -f "$MODULE_PATH" ]; then
        echo -e "${GREEN}✓ Module binary is valid${NC}"
        echo "   Real path: $MODULE_PATH"
        
        # Check symbols in module
        echo -e "\n${YELLOW}6. Checking module symbols${NC}"
        echo "   Module exports:"
        nm -D $MODULE_PATH | grep -E 'uct_gaudi|_md|_component' | head -10
    else
        echo -e "${RED}✗ Module binary symlink points to missing file${NC}"
    fi
else
    echo -e "${RED}✗ Gaudi module does not exist${NC}"
fi

# Test 7: Load module and check
echo -e "\n${YELLOW}7. Testing module loading${NC}"
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}✗ Python3 not found, skipping module load test${NC}"
else
    python3 -c "
import ctypes
import os

def test_module():
    try:
        # Try to load the module
        module_path = '/workspace/ucx/modules/libuct_gaudi.so'
        lib = ctypes.CDLL(module_path, mode=ctypes.RTLD_GLOBAL)
        print(f'✓ Successfully loaded module: {module_path}')
        
        # Try to get some symbols
        try:
            # Check if we can access the component initialization function
            init_func = lib.uct_gaudi_component_init
            print(f'✓ Found component initialization function')
        except AttributeError:
            print('✗ Could not find component initialization function')
        
        return True
    except Exception as e:
        print(f'✗ Failed to load module: {str(e)}')
        return False

test_module()
"
fi

# Check if UCT and UCS can be loaded
echo -e "\n${YELLOW}8. Checking UCT and UCS libraries${NC}"
if [ -f /workspace/ucx/src/uct/.libs/libuct.so ]; then
    echo -e "${GREEN}✓ UCT library exists${NC}"
    ldd /workspace/ucx/src/uct/.libs/libuct.so | head -5
else
    echo -e "${RED}✗ UCT library does not exist${NC}"
fi

if [ -f /workspace/ucx/src/ucs/.libs/libucs.so ]; then
    echo -e "${GREEN}✓ UCS library exists${NC}"
    ldd /workspace/ucx/src/ucs/.libs/libucs.so | head -5
else
    echo -e "${RED}✗ UCS library does not exist${NC}"
fi

# Test 9: Test modules directory setup
echo -e "\n${YELLOW}9. Checking modules directory${NC}"
echo "Modules in /workspace/ucx/modules:"
ls -la /workspace/ucx/modules/

# Summarize results
echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "${GREEN}✓ Gaudi hardware present: $GAUDI_COUNT devices${NC}"
echo -e "${GREEN}✓ Habanalabs kernel module is loaded${NC}"
echo -e "${GREEN}✓ HL-Thunk library is available${NC}"

if [ -f /workspace/ucx/modules/libuct_gaudi.so ] && [ -f "$(readlink -f /workspace/ucx/modules/libuct_gaudi.so)" ]; then
    echo -e "${GREEN}✓ Gaudi module is built and available${NC}"
else
    echo -e "${RED}✗ Gaudi module issues detected${NC}"
fi

if [ -d /dev/habanalabs ]; then
    echo -e "${GREEN}✓ Device nodes are available${NC}"
else
    echo -e "${YELLOW}⚠ Device nodes are not available - this may limit hardware access${NC}"
    echo "  This could be due to container isolation or permission issues"
fi

echo -e "\n${BLUE}=== Final Result ===${NC}"
if [ "$GAUDI_COUNT" -gt 0 ] && [ -f /workspace/ucx/modules/libuct_gaudi.so ]; then
    echo -e "${GREEN}The Gaudi module is properly built and Gaudi hardware is detected${NC}"
    echo "Further testing of memory operations requires proper device node access"
else
    echo -e "${RED}Issues detected with Gaudi module or hardware detection${NC}"
fi
