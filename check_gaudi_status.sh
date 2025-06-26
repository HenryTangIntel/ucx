#!/bin/bash
# Script to check Gaudi hardware and driver status

echo "Checking Gaudi hardware and driver status..."

# Check if hlthunk library is available
echo -e "\n=== Checking hlthunk library ==="
if [ -f "/usr/lib/habanalabs/libhl-thunk.so" ]; then
  echo "hlthunk library found at /usr/lib/habanalabs/libhl-thunk.so"
  
  # Check library dependencies
  echo -e "\nLibrary dependencies:"
  ldd /usr/lib/habanalabs/libhl-thunk.so
else
  echo "hlthunk library not found at /usr/lib/habanalabs/libhl-thunk.so"
  
  # Try to find it elsewhere
  echo -e "\nSearching for hlthunk library in common locations:"
  find /lib /usr/lib /usr/local/lib -name "libhl-thunk.so*" 2>/dev/null || echo "Not found"
fi

# Check for Gaudi devices in /dev
echo -e "\n=== Checking for Gaudi devices ==="
ls -la /dev/dri/* /dev/habanalabs/* 2>/dev/null || echo "No Gaudi devices found in /dev"

# Check if kernel modules are loaded
echo -e "\n=== Checking for Habanalabs kernel modules ==="
lsmod | grep -E 'habanalabs|gaudi' || echo "No habanalabs/gaudi kernel modules found"

# Check dmesg for any Gaudi related messages
echo -e "\n=== Checking dmesg for Gaudi messages ==="
dmesg | grep -E 'habanalabs|gaudi|hl' | tail -n 20 || echo "No Gaudi messages found in dmesg"

# Print system information
echo -e "\n=== System Information ==="
uname -a

# Check PCI devices for Gaudi hardware
echo -e "\n=== Checking PCI devices ==="
lspci | grep -i habanalabs || echo "No Habanalabs PCI devices found"

echo -e "\nCheck complete."
