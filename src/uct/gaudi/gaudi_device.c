#include <libhlthunk.h>
#include <uct/base/uct_md.h>
#include <ucs/sys/string.h>
#include "gaudi_md.h"

#define MAX_GAUDI_DEVICES 16

// Try to open devices with different PCI busids (in production, enumerate properly)
static int gaudi_get_device_count()
{
    int count = 0;
    for (int i = 0; i < MAX_GAUDI_DEVICES; ++i) {
        int fd = hlthunk_open_original(HLTHUNK_DEVICE_PCIE, NULL); // TODO: open specific device
        if (fd < 0) break;
        hlthunk_close_original(fd);
        count++;
    }
    return count;
}

ucs_status_t uct_gaudi_query_devices(uct_md_h uct_md,
                                     uct_tl_device_resource_t **tl_devices_p,
                                     unsigned *num_tl_devices_p)
{
    int num = gaudi_get_device_count();
    if (num == 0) {
        *num_tl_devices_p = 0;
        *tl_devices_p = NULL;
        return UCS_OK;
    }
    *tl_devices_p = ucs_calloc(num, sizeof(**tl_devices_p), "gaudi-devices");
    if (!*tl_devices_p)
        return UCS_ERR_NO_MEMORY;

    for (int i = 0; i < num; ++i) {
        ucs_snprintf_zero((*tl_devices_p)[i].name, UCT_DEVICE_NAME_MAX, "gaudi%d", i);
        (*tl_devices_p)[i].type = UCT_DEVICE_TYPE_ACC;
        // sys_device_id can be added for NUMA awareness
    }
    *num_tl_devices_p = num;
    return UCS_OK;
}