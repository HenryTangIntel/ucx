#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

/* hlthunk device types */
#define HLTHUNK_DEVICE_GAUDI 1

/* hlthunk function type */
typedef int (*hlthunk_open_func_t)(int device_type, int minor);
typedef void (*hlthunk_close_func_t)(int fd);

int main() {
    void *handle;
    hlthunk_open_func_t hlthunk_open_func;
    hlthunk_close_func_t hlthunk_close_func;
    char *error;
    int fd;
    
    printf("Loading hlthunk library...\n");
    handle = dlopen("/usr/lib/habanalabs/libhl-thunk.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Cannot open hlthunk library: %s\n", dlerror());
        return 1;
    }
    
    /* Load the hlthunk_open function */
    dlerror(); /* Clear any existing error */
    hlthunk_open_func = (hlthunk_open_func_t)dlsym(handle, "hlthunk_open");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Cannot find hlthunk_open: %s\n", error);
        dlclose(handle);
        return 1;
    }
    
    /* Load the hlthunk_close function */
    hlthunk_close_func = (hlthunk_close_func_t)dlsym(handle, "hlthunk_close");
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "Cannot find hlthunk_close: %s\n", error);
        dlclose(handle);
        return 1;
    }
    
    printf("Trying to open Gaudi device...\n");
    fd = hlthunk_open_func(HLTHUNK_DEVICE_GAUDI, 0);
    if (fd < 0) {
        printf("Failed to open Gaudi device. Return code: %d\n", fd);
        perror("hlthunk_open error");
        dlclose(handle);
        return 1;
    }
    
    printf("Successfully opened Gaudi device. File descriptor: %d\n", fd);
    
    /* Close the device */
    hlthunk_close_func(fd);
    printf("Closed Gaudi device\n");
    
    /* Cleanup */
    dlclose(handle);
    
    return 0;
}
