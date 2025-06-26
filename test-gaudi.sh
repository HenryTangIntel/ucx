gcc -o gaudi_direct_test gaudi_direct_test.c -I/workspace/ucx/src -L/workspace/ucx/src/ucs/.libs -L/usr/lib/habanalabs -lucs -ldl
LD_LIBRARY_PATH=/workspace/ucx/src/ucs/.libs:/workspace/ucx/src/uct/.libs:/usr/lib/habanalabs ./gaudi_direct_test
