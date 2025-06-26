
#!/bin/bash
echo "Testing Gaudi module"
echo
echo "1. Checking Gaudi hardware"
lspci -d 1da3: | wc -l
echo
echo "2. Checking kernel driver"
lsmod | grep habanalabs
echo
echo "3. Checking Gaudi module"
ls -la modules/libuct_gaudi.so
file modules/libuct_gaudi.so
echo
echo "4. Checking module target"
ls -la src/uct/gaudi/.libs/libuct_gaudi.so*
echo
echo "5. Module validation result"
if [ -f "modules/libuct_gaudi.so" ] && lspci -d 1da3: | grep -q ""; then
    echo "SUCCESS: Gaudi module has been successfully built and hardware is available"
else
    echo "ERROR: Issues detected with Gaudi module or hardware"
fi

