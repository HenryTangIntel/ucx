#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *device_path = "/dev/habanalabs/hl0";
    int fd;
    
    printf("Attempting to open Gaudi device: %s\n", device_path);
    fd = open(device_path, O_RDWR);
    
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }
    
    printf("Successfully opened Gaudi device (fd=%d)\n", fd);
    close(fd);
    
    return 0;
}
