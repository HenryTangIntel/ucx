#!/bin/bash

echo "=== Gaudi Hardware and Driver Check ==="

found_device=0
echo -n "Checking for Gaudi device (lspci)... "
if lspci | grep -iq "Habana"; then
    echo "Found."
    lspci | grep -i "Habana"
    found_device=1
else
    echo "Not found."
fi

echo -n "Checking for libhl-thunk.so... "
if ldconfig -p | grep -q "libhl-thunk.so"; then
    echo "Found."
    ldconfig -p | grep "libhl-thunk.so"
else
    echo "Not found. Make sure Habanalabs drivers and runtime are installed and ldconfig is updated."
fi

echo -n "Checking for habanalabs kernel module (lsmod)... "
if lsmod | grep -q "habanalabs"; then
    echo "Found."
    lsmod | grep "habanalabs"
else
    echo "Not found. Make sure the kernel module is loaded."
fi

echo ""
if [ $found_device -eq 1 ]; then
    echo "Basic hardware presence detected. Further checks for driver interaction needed for full confirmation."
else
    echo "Gaudi hardware not detected or basic driver components missing."
    exit 1
fi

# Add a simple test using a hlthunk utility if one exists and is safe to run
# For example, if hlthunk has a device query tool:
# echo -n "Attempting to query devices via hlthunk tool (if available)... "
# if command -v some_hlthunk_query_tool &> /dev/null; then
#    some_hlthunk_query_tool
# else
#    echo "No hlthunk query tool check performed."
# fi


echo "=== Check Complete ==="
exit 0
