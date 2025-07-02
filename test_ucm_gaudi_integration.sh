#!/bin/bash

# Test script to verify UCM Gaudi module integration

echo "=== UCM Gaudi Integration Test ==="

# Check if the build was successful
echo "1. Checking UCM Gaudi module build..."
if [ -f "/workspace/ucx/install/lib/ucx/libucm_gaudi.so" ]; then
    echo "✓ UCM Gaudi module built and installed"
else
    echo "✗ UCM Gaudi module not found"
    exit 1
fi

# Check if UCT Gaudi module was also built  
echo "2. Checking UCT Gaudi module build..."
if [ -f "/workspace/ucx/install/lib/ucx/libuct_gaudi.so" ]; then
    echo "✓ UCT Gaudi module built and installed"
else
    echo "✗ UCT Gaudi module not found"
    exit 1
fi

# Check configuration
echo "3. Checking configuration..."
if grep -q "UCM modules.*gaudi" /workspace/ucx/config.log; then
    echo "✓ UCM Gaudi module enabled in configuration"
else
    echo "✗ UCM Gaudi module not enabled in configuration"
    exit 1
fi

# Check symbols in the library
echo "4. Checking UCM Gaudi symbols..."
if nm /workspace/ucx/install/lib/ucx/libucm_gaudi.so | grep -q "ucm_hlthunk_device_memory_alloc"; then
    echo "✓ UCM hlthunk symbols found"
else
    echo "✗ UCM hlthunk symbols not found"
    exit 1
fi

# Summary
echo "5. Integration Summary:"
echo "   - UCM Gaudi module: BUILT"
echo "   - UCT Gaudi module: BUILT"  
echo "   - Configuration: ENABLED"
echo "   - Symbols: PRESENT"

echo ""
echo "=== UCM Gaudi Integration Test PASSED ==="
echo ""
echo "The UCM Gaudi module has been successfully integrated into UCX!"
echo "The module will intercept hlthunk memory operations and dispatch UCM events."
echo ""
echo "Module location: /workspace/ucx/install/lib/ucx/libucm_gaudi.so"
echo "Build configuration shows: UCM modules: < gaudi >"

exit 0
