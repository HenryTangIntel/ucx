#include "gaudi_iface.h"
#include "gaudi_md.h"
#include <string.h>
#include <ucs/sys/math.h>
#include <ucs/type/class.h>
#include <ucs/sys/string.h>
#include <uct/base/uct_log.h>
#include <ucs/async/eventfd.h>
#include <ucs/sys/sys.h>


#define UCT_GAUDI_IFACE_ADDR_MAGIC   0xDEADBEEF12345678ULL
#define UCT_GAUDI_DEV_NAME            "gaudi"

extern uct_gaudi_base_info_t uct_gaudi_base_info;


ucs_status_t uct_gaudi_base_get_sys_dev(uct_md_h md,
                                        ucs_sys_device_t *sys_device_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    ucs_sys_bus_id_t bus_id;
    char busid_str[UCT_GAUDI_DEV_NAME_MAX_LEN];
    unsigned domain, bus, device, function;
    int fd;
    ucs_status_t status;

    if (gaudi_md->device_index < 0 || 
        gaudi_md->device_index >= uct_gaudi_base_info.num_devices) {
        goto err;
    }

    /* Get the file descriptor for this device */
    fd = uct_gaudi_base_info.device_fd[gaudi_md->device_index];
    if (fd < 0) {
        goto err;
    }

    /* Get the PCI bus ID string from the stored device info */
    strncpy(busid_str, uct_gaudi_base_info.device_busid[gaudi_md->device_index], 
            sizeof(busid_str) - 1);
    busid_str[sizeof(busid_str) - 1] = '\0';

    /* Parse the bus ID string into components and create bus_id structure */
    /* PCI bus ID format is typically "domain:bus:device.function" (e.g., "0000:00:02.0") */
    if (sscanf(busid_str, "%x:%x:%x.%x", &domain, &bus, &device, &function) != 4) {
        ucs_debug("Failed to parse PCI bus ID string: %s", busid_str);
        goto err;
    }

    bus_id.domain = (uint16_t)domain;
    bus_id.bus = (uint8_t)bus;
    bus_id.slot = (uint8_t)device;
    bus_id.function = (uint8_t)function;

    /* Find the system device by its bus ID */
    status = ucs_topo_find_device_by_bus_id(&bus_id, sys_device_p);
    if (status != UCS_OK) {
        ucs_debug("Failed to find system device for PCI bus ID %s", busid_str);
        goto err;
    }

    return UCS_OK;

err:
    *sys_device_p = UCS_SYS_DEVICE_ID_UNKNOWN;
    return UCS_ERR_NO_DEVICE;
}

ucs_status_t
uct_gaudi_base_query_devices_common(
        uct_md_h md, uct_tl_device_resource_t **tl_devices_p,
        unsigned *num_tl_devices_p)
{
    ucs_sys_device_t sys_device = UCS_SYS_DEVICE_ID_UNKNOWN;
    ucs_status_t status;
    status = uct_gaudi_base_get_sys_dev(md, &sys_device);
    if (status != UCS_OK) {
        ucs_debug("Failed to get system device ID for Gaudi device: %s", 
                  ucs_status_string(status));
        sys_device = UCS_SYS_DEVICE_ID_UNKNOWN;
    }

    
    return uct_single_device_resource(md, md->component->name,
                                     UCT_DEVICE_TYPE_ACC,
                                     sys_device, tl_devices_p,
                                     num_tl_devices_p);
}

ucs_status_t
uct_gaudi_base_query_devices(
        uct_md_h md, uct_tl_device_resource_t **tl_devices_p,
        unsigned *num_tl_devices_p)
{
    return uct_gaudi_base_query_devices_common(md, tl_devices_p, num_tl_devices_p);
}

ucs_status_t uct_gaudi_base_iface_event_fd_get(uct_iface_h tl_iface, int *fd_p)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    ucs_status_t status;

    if (iface->eventfd == UCS_ASYNC_EVENTFD_INVALID_FD) {
        status = ucs_async_eventfd_create(&iface->eventfd);
        if (status != UCS_OK) {
            return status;
        }
    }

    *fd_p = iface->eventfd;
    return UCS_OK;
}

ucs_status_t uct_gaudi_base_check_device_name(const uct_iface_params_t *params)
{
    const char *dev_name;
    
    UCT_CHECK_PARAM(params->field_mask & UCT_IFACE_PARAM_FIELD_DEVICE,
                    "UCT_IFACE_PARAM_FIELD_DEVICE is not defined");

    dev_name = params->mode.device.dev_name;
    
    /* For Gaudi copy transport, accept "gaudi_copy" as a valid device name */
    /* since this represents the logical device for the transport layer */
    if (strcmp(dev_name, "gaudi_copy") == 0) {
        return UCS_OK;
    }
    
    /* Also accept specific Gaudi device names if they match detected devices */
    /* This allows for more specific device selection in the future */
    if (strncmp(dev_name, "gaudi", 5) == 0) {
        return UCS_OK;
    }
    
    ucs_debug("Gaudi device not found: %s", dev_name);
    return UCS_ERR_NO_DEVICE;
}

UCS_CLASS_INIT_FUNC(uct_gaudi_iface_t, uct_iface_ops_t *tl_ops,
                    uct_iface_internal_ops_t *ops, uct_md_h md,
                    uct_worker_h worker, const uct_iface_params_t *params,
                    const uct_iface_config_t *tl_config,
                    const char *dev_name)
{
    UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, tl_ops, ops, md, worker, params,
                              tl_config UCS_STATS_ARG(params->stats_root)
                              UCS_STATS_ARG(dev_name));

    self->eventfd = UCS_ASYNC_EVENTFD_INVALID_FD;

    return UCS_OK;
}


static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_iface_t)
{
    ucs_async_eventfd_destroy(self->eventfd);
}

UCS_CLASS_DEFINE(uct_gaudi_iface_t, uct_base_iface_t);

