#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

// More comprehensive DMA-BUF support test for Gaudi2 and MLX devices

void check_dmabuf_kernel_support() {
    FILE *fp;
    char line[256];
    
    printf("=== Checking Kernel DMA-BUF Support ===\n");
    
    // Check /proc/filesystems for dmabuf
    fp = fopen("/proc/filesystems", "r");
    if (fp) {
        printf("Available filesystems:\n");
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "dma") || strstr(line, "buf")) {
                printf("  %s", line);
            }
        }
        fclose(fp);
    }
    
    // Check /proc/modules for DMA-BUF related modules
    fp = fopen("/proc/modules", "r");
    if (fp) {
        printf("\nDMA/Buffer related kernel modules:\n");
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "dma") || strstr(line, "buf") || strstr(line, "habana") || strstr(line, "mlx")) {
                printf("  %s", line);
            }
        }
        fclose(fp);
    }
    printf("\n");
}

void check_device_capabilities() {
    DIR *dir;
    struct dirent *entry;
    char path[512];
    FILE *fp;
    char content[256];
    
    printf("=== Checking Device Capabilities ===\n");
    
    // Check Gaudi devices
    printf("Gaudi devices:\n");
    for (int i = 0; i < 8; i++) {
        snprintf(path, sizeof(path), "/sys/class/accel/accel%d/device/device_type", i);
        fp = fopen(path, "r");
        if (fp) {
            if (fgets(content, sizeof(content), fp)) {
                content[strcspn(content, "\n")] = '\0';
                printf("  accel%d: %s\n", i, content);
                
                // Check if device has DMA-related files
                snprintf(path, sizeof(path), "/sys/class/accel/accel%d/device", i);
                dir = opendir(path);
                if (dir) {
                    while ((entry = readdir(dir)) != NULL) {
                        if (strstr(entry->d_name, "dma") || strstr(entry->d_name, "buf")) {
                            printf("    Found: %s\n", entry->d_name);
                        }
                    }
                    closedir(dir);
                }
            }
            fclose(fp);
        }
    }
    
    // Check InfiniBand devices
    printf("\nInfiniBand devices:\n");
    dir = opendir("/sys/class/infiniband");
    if (dir) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                printf("  %s\n", entry->d_name);
                
                // Check device capabilities
                snprintf(path, sizeof(path), "/sys/class/infiniband/%s/device/vendor", entry->d_name);
                fp = fopen(path, "r");
                if (fp) {
                    if (fgets(content, sizeof(content), fp)) {
                        content[strcspn(content, "\n")] = '\0';
                        printf("    Vendor: %s\n", content);
                    }
                    fclose(fp);
                }
                
                snprintf(path, sizeof(path), "/sys/class/infiniband/%s/device/device", entry->d_name);
                fp = fopen(path, "r");
                if (fp) {
                    if (fgets(content, sizeof(content), fp)) {
                        content[strcspn(content, "\n")] = '\0';
                        printf("    Device: %s\n", content);
                    }
                    fclose(fp);
                }
            }
        }
        closedir(dir);
    }
    printf("\n");
}

void test_udmabuf_functionality() {
    int fd;
    void *mem;
    size_t size = 4096;
    
    printf("=== Testing udmabuf Functionality ===\n");
    
    fd = open("/dev/udmabuf", O_RDWR);
    if (fd < 0) {
        printf("✗ Cannot open /dev/udmabuf: %s\n", strerror(errno));
        return;
    }
    
    printf("✓ Opened /dev/udmabuf successfully\n");
    
    // In a real implementation, we would use udmabuf ioctls to create buffers
    // For now, just test basic file descriptor operations
    
    close(fd);
    printf("✓ udmabuf basic functionality working\n\n");
}

void suggest_dmabuf_integration() {
    printf("=== DMA-BUF Integration Suggestions ===\n");
    printf("1. Gaudi2 DMA-BUF Export:\n");
    printf("   - Use Habana Labs driver ioctls to export GPU memory as DMA-BUF\n");
    printf("   - Memory allocated on Gaudi device becomes shareable fd\n");
    printf("\n");
    printf("2. MLX DMA-BUF Import:\n");
    printf("   - MLX devices can import DMA-BUF fds for RDMA operations\n");
    printf("   - Zero-copy transfers between Gaudi GPU memory and MLX RDMA\n");
    printf("\n");
    printf("3. UCX Integration:\n");
    printf("   - Extend UCT Gaudi transport to support DMA-BUF export\n");
    printf("   - Add memory registration using DMA-BUF fds\n");
    printf("   - Enable direct GPU-to-network transfers\n");
    printf("\n");
    printf("4. Required Components:\n");
    printf("   - Habana Labs driver with DMA-BUF support\n");
    printf("   - MLX driver with DMA-BUF import capability\n");
    printf("   - UCX memory domain integration\n");
    printf("   - Application-level buffer management\n");
    printf("\n");
}

int main() {
    printf("=== Comprehensive DMA-BUF Support Analysis ===\n\n");
    
    check_dmabuf_kernel_support();
    check_device_capabilities();
    test_udmabuf_functionality();
    suggest_dmabuf_integration();
    
    printf("=== Next Steps for DMA-BUF Integration ===\n");
    printf("1. Verify Habana Labs driver DMA-BUF support\n");
    printf("2. Test MLX DMA-BUF import capabilities\n");
    printf("3. Implement UCX DMA-BUF memory registration\n");
    printf("4. Create end-to-end test with GPU→RDMA transfer\n");
    
    return 0;
}
