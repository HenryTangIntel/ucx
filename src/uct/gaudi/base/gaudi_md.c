#include "gaudi_md.h"
#include "gaudi_iface.h"
#include <ucs/sys/module.h>
#include <ucs/sys/string.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64

struct uct_gaudi_base_info uct_gaudi_base_info;


int uct_gaudi_base_get_device_fd(int device_index)
{
    if (device_index < 0 || device_index >= uct_gaudi_base_info.num_devices) {
        return -1; // Invalid device index
    }
    return uct_gaudi_base_info.device_fd[device_index];
}

ucs_status_t uct_gaudi_base_info_init(void)
{
    DIR *dir;
    struct dirent *entry;
    int dev_idx = 0;
    char device_type_path[320];
    char device_path[320];
    char device_type[32];
    FILE *type_file;
    size_t len;
    int fd;

    uct_gaudi_base_info.num_devices = 0;

    dir = opendir("/sys/class/accel/");
    if (!dir) {
        ucs_error("Failed to open /sys/class/accel/ directory");
        return UCS_ERR_IO_ERROR;
    }

    while ((entry = readdir(dir)) != NULL && dev_idx < 8) {
        // Skip "." and ".."
        if (entry->d_name[0] == '.')
            continue;

        memset(device_type, 0, sizeof(device_type));
        snprintf(device_type_path, sizeof(device_type_path), 
                "/sys/class/accel/%s/device/device_type", entry->d_name);
        type_file = fopen(device_type_path, "r");
        if (!type_file) {
            // Not a Gaudi device if attribute missing
            continue;
        }
        if (!fgets(device_type, sizeof(device_type), type_file)) {
            fclose(type_file);
            continue;
        }
        fclose(type_file);
        
        // Remove trailing newline
        len = strlen(device_type);
        if (len > 0 && device_type[len-1] == '\n') {
            device_type[len-1] = '\0';
        }
        if (strcmp(device_type, "GAUDI") != 0 && strcmp(device_type, "GAUDI2") != 0) {
            // Not a Gaudi device
            continue;
        }

        // Try to open the device for actual hardware access
        snprintf(device_path, sizeof(device_path), "/dev/accel/%s", entry->d_name);
        fd = open(device_path, O_RDWR);
        if (fd < 0) {
            // Alternative path
            snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
            fd = open(device_path, O_RDWR);
        }
        
        if (fd < 0) {
            ucs_debug("Cannot open Gaudi device %s: %m", entry->d_name);
            continue;
        }

        uct_gaudi_base_info.device_fd[dev_idx] = fd;
        ucs_strncpy_zero(uct_gaudi_base_info.device_name[dev_idx], 
                        entry->d_name, UCT_GAUDI_DEV_NAME_MAX_LEN);
        dev_idx++;
    }
    closedir(dir);

    uct_gaudi_base_info.num_devices = dev_idx;

    if (dev_idx == 0) {
        ucs_debug("No accessible Gaudi devices found");
        return UCS_ERR_NO_DEVICE;
    }

    ucs_info("Found %d Gaudi device(s)", dev_idx);
    return UCS_OK;
}

void uct_gaudi_base_cleanup(void)
{
    int i;
    
    for (i = 0; i < uct_gaudi_base_info.num_devices; i++) {
        if (uct_gaudi_base_info.device_fd[i] >= 0) {
            close(uct_gaudi_base_info.device_fd[i]);
            uct_gaudi_base_info.device_fd[i] = -1;
        }
    }
    uct_gaudi_base_info.num_devices = 0;
}

int uct_gaudi_base_init(void)
{
    static ucs_init_once_t init = UCS_INIT_ONCE_INITIALIZER;
    ucs_status_t status;
    UCS_INIT_ONCE(&init) {
        status = uct_gaudi_base_info_init();
        if (status != UCS_OK) {
            ucs_error("Failed to initialize Gaudi base info: %s", ucs_status_string(status));
            continue;
        }
    }
    return UCS_OK;

}


ucs_status_t
uct_gaudi_query_md_resources(uct_component_t *component,
                                 uct_md_resource_desc_t **resources_p,
                                 unsigned *num_resources_p)
{
    if (uct_gaudi_base_init() != UCS_OK) {
        ucs_error("Failed to initialize Gaudi base info");
        return uct_md_query_empty_md_resource(resources_p, num_resources_p);
    }
    return uct_md_query_single_md_resource(component, resources_p,
                                           num_resources_p);
}
