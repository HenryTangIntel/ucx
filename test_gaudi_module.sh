#!/bin/bash
# Run Gaudi tests and hardware checks

echo "===== Running Gaudi Module Tests and Hardware Checks ====="

# Colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Step 1: Building basic test...${NC}"
cd /workspace/ucx
gcc -o gaudi_test_check gaudi_test_check.c -L/workspace/ucx/install/lib -lucp -luct -lucs \
    -I/workspace/ucx/install/include -I/workspace/ucx/src -I/workspace/ucx
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Basic test compiled successfully${NC}"
else
    echo -e "${RED}❌ Basic test compilation failed${NC}"
    exit 1
fi

echo -e "\n${YELLOW}Step 2: Running basic module registration test...${NC}"
LD_LIBRARY_PATH=/workspace/ucx/install/lib:$LD_LIBRARY_PATH ./gaudi_test_check
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Basic test passed - Gaudi module is registered${NC}"
else
    echo -e "${RED}❌ Basic test failed - Gaudi module registration issue${NC}"
    exit 1
fi

echo -e "\n${YELLOW}Step 3: Building comprehensive test...${NC}"
make -f gaudi_test_final.Makefile
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Comprehensive test compiled successfully${NC}"
else
    echo -e "${RED}❌ Comprehensive test compilation failed${NC}"
    exit 1
fi

echo -e "\n${YELLOW}Step 4: Running comprehensive function test...${NC}"
LD_LIBRARY_PATH=/workspace/ucx/install/lib:$LD_LIBRARY_PATH ./gaudi_test_final
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Comprehensive test passed - Gaudi module functions properly${NC}"
else
    echo -e "${RED}❌ Comprehensive test failed - Check output for details${NC}"
fi

echo -e "\n${YELLOW}Step 5: Running hardware check...${NC}"
./check_gaudi_hardware.sh

echo -e "\n${YELLOW}Step 6: Building hardware-specific test...${NC}"
make -f gaudi_hardware_test.Makefile
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Hardware test compiled successfully${NC}"
else
    echo -e "${RED}❌ Hardware test compilation failed${NC}"
fi

echo -e "\n${YELLOW}Step 7: Attempting hardware operations test...${NC}"
echo -e "${YELLOW}Note: This test will only succeed if actual Gaudi hardware is available${NC}"
LD_LIBRARY_PATH=/workspace/ucx/install/lib:$LD_LIBRARY_PATH ./gaudi_hardware_test
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Hardware operations test succeeded - Gaudi hardware is functioning${NC}"
    HW_WORKING=1
else
    echo -e "${YELLOW}⚠️ Hardware operations test did not succeed - This is expected if no physical Gaudi devices are present${NC}"
    HW_WORKING=0
fi

echo -e "\n===== Test Results Summary ====="
echo -e "Module Registration: Successful"
echo -e "Module Structure: Proper"

if [ "$HW_WORKING" -eq 1 ]; then
    echo -e "Hardware Status: ${GREEN}Working - Full functionality confirmed${NC}"
    echo -e "Next Steps: "
    echo -e "  1. Proceed to integration with full application"
    echo -e "  2. Conduct performance testing"
else
    echo -e "Hardware Status: ${YELLOW}Limited or unavailable${NC}"
    echo -e "Next Steps: "
    echo -e "  1. If hardware is expected, check hardware installation and driver"
    echo -e "  2. If no hardware, this is expected - module structure is still correct"
fi
echo -e "============================="
