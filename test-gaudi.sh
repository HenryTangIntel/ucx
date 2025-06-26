gcc -o gaudi_direct_test gaudi_direct_test.c -L/usr/lib/habanalabs -ldl
LD_LIBRARY_PATH=/workspace/ucx/src/ucs/.libs:/workspace/ucx/src/uct/.libs:/usr/lib/habanalabs ./gaudi_direct_test
