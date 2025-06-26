#!/bin/bash
# Comprehensive Gaudi hardware check script

echo "===== Gaudi Hardware Status Check ====="
echo

# Check for Habana Labs kernel module
echo "=== Kernel Driver Status ==="
if lsmod | grep -q habanalabs; then
    echo "✅ Habanalabs kernel module is loaded"
    modinfo habanalabs | grep -E "version|description"
else
    echo "❌ Habanalabs kernel module not found"
fi
echo

# Check for Habana Labs devices
echo "=== PCI Device Check ==="
DEVICES=$(lspci -d 1da3: 2>/dev/null)
if [ -n "$DEVICES" ]; then
    echo "✅ Found Habana Labs PCI devices:"
    echo "$DEVICES"
    echo
    echo "Device details:"
    lspci -vvv -d 1da3: | grep -E "Subsystem|Memory|Region|Kernel|driver"
else
    echo "❌ No Habana Labs PCI devices detected"
fi
echo

# Check for hlthunk library
echo "=== Library Check ==="
for path in /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64 /usr/lib/habanalabs; do
    if [ -f "$path/libhl-thunk.so" ]; then
        echo "✅ Found libhl-thunk.so at $path/libhl-thunk.so"
        ls -la "$path/libhl-thunk.so"*
        echo
        echo "Library symbols:"
        nm -D "$path/libhl-thunk.so" | grep -E "hlthunk_open|hlthunk_close" | head -5
        LD_FOUND=1
        break
    fi
done

if [ -z "$LD_FOUND" ]; then
    echo "❌ libhl-thunk.so not found in standard library paths"
fi
echo

# Check for device nodes
echo "=== Device Nodes ==="
if [ -d "/dev/habanalabs" ]; then
    echo "✅ Found Habana device nodes:"
    ls -la /dev/habanalabs/
else
    echo "❌ No Habana device nodes found in /dev/habanalabs/"
fi
echo

# Check environment variables
echo "=== Environment Variables ==="
env | grep -i "HABANA\|HL_" || echo "No Habana-related environment variables found"
echo

echo "===== Hardware Check Complete ====="
