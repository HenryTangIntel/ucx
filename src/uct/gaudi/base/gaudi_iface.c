#include "gaudi_iface.h"
#include "gaudi_md.h"
#include <string.h>
#include <ucs/sys/math.h>
#include <ucs/type/class.h>
#include <ucs/sys/string.h>
#include <uct/base/uct_log.h>
#include <ucs/async/eventfd.h>


#define UCT_GAUDI_IFACE_ADDR_MAGIC   0xDEADBEEF12345678ULL
#define UCT_GAUDI_DEV_NAME            "gaudi"


ucs_status_t
uct_gaudi_base_query_devices_common(
        uct_md_h md, uct_tl_device_resource_t **tl_devices_p,
        unsigned *num_tl_devices_p)
{
    return uct_single_device_resource(md, "gaudi", UCT_DEVICE_TYPE_ACC,
                                      UCS_SYS_DEVICE_ID_UNKNOWN, tl_devices_p,
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
    UCT_CHECK_PARAM(params->field_mask & UCT_IFACE_PARAM_FIELD_DEVICE,
                    "UCT_IFACE_PARAM_FIELD_DEVICE is not defined");

    if (strncmp(params->mode.device.dev_name, UCT_GAUDI_DEV_NAME,
                strlen(UCT_GAUDI_DEV_NAME)) != 0) {
        ucs_error("no device was found: %s", params->mode.device.dev_name);
        return UCS_ERR_NO_DEVICE;
    }

    return UCS_OK;
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

