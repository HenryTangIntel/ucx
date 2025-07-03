#include "gaudi_md.h"
#include "gaudi_iface.h"
#include <ucs/sys/module.h>
#include <ucs/sys/string.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64

uct_gaudi_base_info_t uct_gaudi_base_info;

enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2,
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
};


int uct_gaudi_base_get_device_fd(int device_index)
{
    if (device_index < 0 || device_index >= uct_gaudi_base_info.num_devices) {
        return -1; // Invalid device index
    }
    return uct_gaudi_base_info.device_fd[device_index];
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

static ucs_status_t uct_gaudi_base_init_devices(void)
{
    int count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    char busid[UCT_GAUDI_DEV_NAME_MAX_LEN];
    int ret;
    int i;
    int fd;
    
    if (count <= 0) {
        ucs_debug("No Gaudi devices found");
        return UCS_ERR_NO_DEVICE; // No devices found
    }

    for (i = 0; i < count; i++) {
        fd = hlthunk_open_by_module_id(i);
        if (fd < 0) {
            ucs_error("Failed to open Gaudi device %d", i);
            continue;
        }
        uct_gaudi_base_info.device_id[i] = hlthunk_get_device_index_from_module_id(i);
        uct_gaudi_base_info.device_fd[i] = fd;
        uct_gaudi_base_info.module_id[i] = i;
        
        ret = hlthunk_get_pci_bus_id_from_fd(fd, busid, sizeof(busid));
        if (ret == 0) {
            ucs_strncpy_zero(uct_gaudi_base_info.device_busid[i], busid,
                             UCT_GAUDI_DEV_NAME_MAX_LEN);
        } else {
            ucs_error("Failed to get PCI bus ID for Gaudi device module id: %d", i);
            uct_gaudi_base_info.device_busid[i][0] = '\0'; // Set empty if error
        }

        ucs_debug("Opened Gaudi device %d: %s", i, busid);
    }
    uct_gaudi_base_info.num_devices = count;

    return UCS_OK;
}


ucs_status_t uct_gaudi_base_init(void)
{
    static ucs_init_once_t init = UCS_INIT_ONCE_INITIALIZER;
    ucs_status_t status = UCS_OK;
    UCS_INIT_ONCE(&init) {
        status = uct_gaudi_base_init_devices();
    }
    return status;
}


int uct_gaudi_base_get_device(int device_num)
{
    if (device_num < 0 || device_num >= uct_gaudi_base_info.num_devices) {
        return -1; // Invalid device number
    }
    return uct_gaudi_base_info.device_fd[device_num];
}



ucs_status_t uct_gaudi_base_query_md_resources(
        uct_component_t *component,
        uct_md_resource_desc_t **resources_p,
        unsigned *num_resources_p)
{
    ucs_status_t status;
    status = uct_gaudi_base_init();
    if (status != UCS_OK) {
        ucs_error("Failed to initialize Gaudi base info: %s", ucs_status_string(status));
        return uct_md_query_empty_md_resource(resources_p, num_resources_p);
    }

    return uct_gaudi_query_md_resources(component, resources_p, num_resources_p);
}

UCS_STATIC_CLEANUP
{
    uct_gaudi_base_cleanup();
}



UCS_MODULE_INIT() {
    /* TODO make gdrcopy independent of cuda */
    UCS_MODULE_FRAMEWORK_DECLARE(uct_gaudi);
    UCS_MODULE_FRAMEWORK_LOAD(uct_gaudi, 0);
    return UCS_OK;
}
