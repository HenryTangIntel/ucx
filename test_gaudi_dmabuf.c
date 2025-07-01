#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

// Basic DMA-BUF test to check if Gaudi devices support DMA-BUF export/import

int test_udmabuf_support() {
    int fd;
    
    printf("Testing udmabuf support...\n");
    
    fd = open("/dev/udmabuf", O_RDWR);
    if (fd < 0) {
        printf("Cannot open /dev/udmabuf: %s\n", strerror(errno));
        return -1;
    }
    
    printf("Successfully opened /dev/udmabuf (fd=%d)\n", fd);
    close(fd);
    return 0;
}

int test_gaudi_device_access() {
    int fd;
    char device_path[256];
    
    printf("Testing Gaudi device access for DMA-BUF support...\n");
    
    // Try to open a working Gaudi device
    for (int i = 1; i <= 7; i++) {
        snprintf(device_path, sizeof(device_path), "/dev/accel/accel%d", i);
        fd = open(device_path, O_RDWR);
        if (fd >= 0) {
            printf("Successfully opened %s (fd=%d)\n", device_path, fd);
            
            // In a real implementation, we would try DMA-BUF specific ioctls here
            // For now, just verify basic access
            printf("Device %s is accessible for potential DMA-BUF operations\n", device_path);
            
            close(fd);
            return 0;
        }
    }
    
    printf("No accessible Gaudi devices found for DMA-BUF testing\n");
    return -1;
}

int test_memory_mapping() {
    void *mem;
    size_t size = 4096; // 4KB page
    
    printf("Testing memory mapping capabilities...\n");
    
    // Test if we can create mappable memory
    mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        printf("Failed to map memory: %s\n", strerror(errno));
        return -1;
    }
    
    printf("Successfully mapped %zu bytes at %p\n", size, mem);
    
    // Write test pattern
    memset(mem, 0xAB, size);
    printf("Memory mapping test successful\n");
    
    munmap(mem, size);
    return 0;
}

int main() {
    printf("=== Gaudi DMA-BUF Support Test ===\n\n");
    
    // Test 1: Check if udmabuf is available
    if (test_udmabuf_support() == 0) {
        printf("✓ udmabuf support available\n\n");
    } else {
        printf("✗ udmabuf not available\n\n");
    }
    
    // Test 2: Check Gaudi device access
    if (test_gaudi_device_access() == 0) {
        printf("✓ Gaudi device access working\n\n");
    } else {
        printf("✗ Gaudi device access failed\n\n");
    }
    
    // Test 3: Test basic memory mapping
    if (test_memory_mapping() == 0) {
        printf("✓ Memory mapping support working\n\n");
    } else {
        printf("✗ Memory mapping failed\n\n");
    }
    
    printf("=== DMA-BUF Test Notes ===\n");
    printf("- DMA-BUF requires kernel driver support\n");
    printf("- Gaudi devices may support DMA-BUF export/import through specific ioctls\n");
    printf("- For full DMA-BUF testing, need access to Gaudi driver documentation\n");
    printf("- UCX integration would use DMA-BUF fds for zero-copy transfers\n");
    
    return 0;
}
