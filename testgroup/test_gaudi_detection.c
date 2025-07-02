#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64

int main() {
    DIR *dir;
    struct dirent *entry;
    int dev_idx = 0;
    char device_type_path[320];
    char device_path[320];
    char device_type[32];
    FILE *type_file;
    size_t len;
    int fd;

    printf("Scanning for Gaudi devices...\n");

    dir = opendir("/sys/class/accel/");
    if (!dir) {
        printf("Failed to open /sys/class/accel/ directory\n");
        return 1;
    }

    while ((entry = readdir(dir)) != NULL && dev_idx < 8) {
        // Skip "." and ".."
        if (entry->d_name[0] == '.')
            continue;

        printf("Found entry: %s\n", entry->d_name);

        memset(device_type, 0, sizeof(device_type));
        snprintf(device_type_path, sizeof(device_type_path), 
                "/sys/class/accel/%s/device/device_type", entry->d_name);
        
        printf("Checking device type at: %s\n", device_type_path);
        
        type_file = fopen(device_type_path, "r");
        if (!type_file) {
            printf("Cannot open device_type file for %s\n", entry->d_name);
            continue;
        }
        
        if (!fgets(device_type, sizeof(device_type), type_file)) {
            printf("Cannot read device_type for %s\n", entry->d_name);
            fclose(type_file);
            continue;
        }
        fclose(type_file);
        
        // Remove trailing newline
        len = strlen(device_type);
        if (len > 0 && device_type[len-1] == '\n') {
            device_type[len-1] = '\0';
        }
        
        printf("Device %s has type: '%s'\n", entry->d_name, device_type);
        
        if (strcmp(device_type, "GAUDI") != 0 && strcmp(device_type, "GAUDI2") != 0) {
            printf("Not a Gaudi device: %s\n", entry->d_name);
            continue;
        }

        // Try to open the device for actual hardware access
        snprintf(device_path, sizeof(device_path), "/dev/accel/%s", entry->d_name);
        printf("Trying to open: %s\n", device_path);
        fd = open(device_path, O_RDWR);
        if (fd < 0) {
            printf("Cannot open %s, trying alternative path\n", device_path);
            // Alternative path
            snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
            printf("Trying to open: %s\n", device_path);
            fd = open(device_path, O_RDWR);
        }
        
        if (fd < 0) {
            printf("Cannot open Gaudi device %s: %s\n", entry->d_name, strerror(errno));
            continue;
        }

        printf("Successfully opened device %s (fd=%d)\n", entry->d_name, fd);
        close(fd);
        dev_idx++;
    }
    closedir(dir);

    printf("Found %d Gaudi devices\n", dev_idx);
    return 0;
}
