# Gaudi Module for UCX - Test Program

This test program verifies that the Gaudi memory domain module has been correctly
compiled and registered with the UCX framework.

## Usage

1. Make sure UCX has been built with Gaudi support:
```
cd /workspace/ucx
./autogen.sh
./configure --enable-debug --with-gaudi
make -j$(nproc)
```

2. Run the Gaudi test program:
```
cd /workspace/ucx
./gaudi_test_final
```

## What the test verifies

- The Gaudi component has been successfully registered with UCX
- The component has the correct configuration
- The module can be discovered by UCX applications

## Expected output

If the Gaudi module has been successfully compiled and registered, you should see:
```
Found 8 UCT components:
Component[0]: self
Component[1]: tcp
...
Component[7]: gaudi

Gaudi component successfully registered!
Details for Gaudi component:
  Name: gaudi
  MD config name: Gaudi memory domain
  MD config prefix: GAUDI_

Success: The Gaudi module has been successfully compiled and registered with UCX.
The module should be functional when Gaudi hardware is present.
```

## Notes

- This test only verifies that the Gaudi module is correctly registered
- To fully test the functionality, you'll need actual Gaudi hardware
- The hlthunk library is required when Gaudi hardware is present

## Testing with hardware

When Gaudi hardware is available:

1. Make sure the hlthunk library is installed
2. Create a more comprehensive test that exercises the memory registration
   and data transfer capabilities of the Gaudi hardware
3. Test communication between multiple Gaudi devices
