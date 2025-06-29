#include "gaudi_md.h"
#include "gaudi_iface.h"
#include <ucs/sys/module.h>
#include <ucs/sys/string.h>

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64

static struct {
    //gaudi_driver driver;
    int device_fd [8];
    char device_name[8][UCT_GAUDI_DEV_NAME_MAX_LEN];
    int num_devices;
} uct_gaudi_base_info;


int uct_gaudi_base_get_device_fd(int device_index)
{
    if (device_index < 0 || device_index >= uct_gaudi_base_info.num_devices) {
        return -1; // Invalid device index
    }
    return uct_gaudi_base_info.device_fd[device_index];
}

ucs_status_t uct_gaudi_base_info_init(void)
{
    uct_gaudi_base_info.num_devices = 0;
    // Initialize the Gaudi base info structure
    // This is a placeholder for actual initialization logic
    // For example, you might want to query the system for available Gaudi devices
    // and populate the device_fd and device_name arrays accordingly.
    
    // Example initialization (replace with actual logic):
    uct_gaudi_base_info.device_fd[0] = 0; // Example file descriptor
    ucs_strncpy_zero(uct_gaudi_base_info.device_name[0], "gaudi_device_0", UCT_GAUDI_DEV_NAME_MAX_LEN);
    uct_gaudi_base_info.num_devices = 1;

    return UCS_OK;
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
    uct_gaudi_base_info.num_devices = 0;
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
