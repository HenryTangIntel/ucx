/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_ipc_iface.h"

#include <uct/base/uct_md.h>
#include <ucs/arch/atomic.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/sys/sys.h>

#include <sys/types.h>
#include <unistd.h>


static ucs_config_field_t uct_gaudi_ipc_iface_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_ipc_iface_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_iface_config_table)},

    {NULL}
};

static ucs_status_t uct_gaudi_ipc_iface_query(uct_iface_h tl_iface,
                                              uct_iface_attr_t *iface_attr)
{
    memset(iface_attr, 0, sizeof(*iface_attr));

    iface_attr->iface_addr_len          = sizeof(pid_t);
    iface_attr->device_addr_len         = 0;
    iface_attr->ep_addr_len             = 0;
    iface_attr->max_conn_priv           = 0;
    iface_attr->cap.flags               = UCT_IFACE_FLAG_GET_ZCOPY |
                                          UCT_IFACE_FLAG_PUT_ZCOPY |
                                          UCT_IFACE_FLAG_CONNECT_TO_IFACE;

    iface_attr->cap.put.max_zcopy       = SIZE_MAX;
    iface_attr->cap.put.min_zcopy       = 1;
    iface_attr->cap.put.max_iov         = 1;
    iface_attr->cap.put.opt_zcopy_align = 1;
    iface_attr->cap.put.align_mtu       = iface_attr->cap.put.opt_zcopy_align;

    iface_attr->cap.get.max_zcopy       = SIZE_MAX;
    iface_attr->cap.get.min_zcopy       = 1;
    iface_attr->cap.get.max_iov         = 1;
    iface_attr->cap.get.opt_zcopy_align = 1;
    iface_attr->cap.get.align_mtu       = iface_attr->cap.get.opt_zcopy_align;

    iface_attr->latency                 = ucs_linear_func_make(1e-6, 0);
    iface_attr->bandwidth.dedicated     = 0;
    iface_attr->bandwidth.shared        = 12800.0 * UCS_MBYTE; /* Estimate */
    iface_attr->overhead                = 0;
    iface_attr->priority                = 0;

    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_iface_get_address(uct_iface_h tl_iface,
                                                    uct_iface_addr_t *iface_addr)
{
    *(pid_t*)iface_addr = getpid();
    return UCS_OK;
}

static int uct_gaudi_ipc_iface_is_reachable(const uct_iface_h tl_iface,
                                            const uct_device_addr_t *dev_addr,
                                            const uct_iface_addr_t *iface_addr)
{
    /* Gaudi IPC is only reachable within the same host */
    return 1;
}

static ucs_status_t uct_gaudi_ipc_ep_create(const uct_ep_params_t *params,
                                            uct_ep_h *ep_p)
{
    uct_gaudi_ipc_iface_t *iface = ucs_derived_of(params->iface, uct_gaudi_ipc_iface_t);
    uct_gaudi_ipc_ep_t *ep;

    ep = ucs_malloc(sizeof(*ep), "gaudi_ipc_ep");
    if (ep == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &ep->super, &iface->super);

    if (params->field_mask & UCT_EP_PARAM_FIELD_IFACE_ADDR) {
        ep->remote_pid = *(pid_t*)params->iface_addr;
    } else {
        ep->remote_pid = 0;
    }

    *ep_p = &ep->super.super;
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_ipc_ep_t)
{
    /* Nothing to cleanup for now */
}

static uct_ep_ops_t uct_gaudi_ipc_ep_ops = {
    .ep_put_zcopy             = ucs_empty_function_return_unsupported,
    .ep_get_zcopy             = ucs_empty_function_return_unsupported,
    .ep_pending_add           = ucs_empty_function_return_unsupported,
    .ep_pending_purge         = ucs_empty_function_return_unsupported,
    .ep_flush                 = uct_base_ep_flush,
    .ep_fence                 = uct_base_ep_fence,
    .ep_create                = UCS_CLASS_NEW_FUNC_NAME(uct_gaudi_ipc_ep_t),
    .ep_destroy               = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_ipc_ep_t),
};

UCS_CLASS_DEFINE(uct_gaudi_ipc_ep_t, uct_base_ep_t)

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_ipc_iface_t)
{
    /* Nothing to cleanup for now */
}

static uct_iface_ops_t uct_gaudi_ipc_iface_ops = {
    .ep_create                = uct_gaudi_ipc_ep_create,
    .ep_destroy               = ucs_empty_function_return_success,
    .iface_flush              = uct_base_iface_flush,
    .iface_fence              = uct_base_iface_fence,
    .iface_progress_enable    = ucs_empty_function,
    .iface_progress_disable   = ucs_empty_function,
    .iface_progress           = ucs_empty_function_return_zero,
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_ipc_iface_t),
    .iface_query              = uct_gaudi_ipc_iface_query,
    .iface_get_device_address = ucs_empty_function_return_success,
    .iface_get_address        = uct_gaudi_ipc_iface_get_address,
    .iface_is_reachable       = uct_gaudi_ipc_iface_is_reachable,
};

static ucs_status_t uct_gaudi_ipc_iface_open(uct_md_h md, uct_worker_h worker,
                                             const uct_iface_params_t *params,
                                             const uct_iface_config_t *tl_config,
                                             uct_iface_h *iface_p)
{
    uct_gaudi_ipc_iface_t *iface;
    ucs_status_t status;

    iface = ucs_malloc(sizeof(*iface), "gaudi_ipc_iface");
    if (iface == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    status = UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, &iface->super, &uct_gaudi_ipc_iface_ops,
                                       &uct_gaudi_ipc_ep_ops, md, worker, params,
                                       tl_config UCS_STATS_ARG(params->stats_root)
                                       UCS_STATS_ARG(UCT_GAUDI_IPC_TL_NAME));
    if (status != UCS_OK) {
        goto err_free;
    }

    *iface_p = &iface->super.super;
    return UCS_OK;

err_free:
    ucs_free(iface);
    return status;
}

UCS_CLASS_DEFINE(uct_gaudi_ipc_iface_t, uct_base_iface_t);

static uct_tl_t uct_gaudi_ipc_tl = {
    .name           = UCT_GAUDI_IPC_TL_NAME,
    .iface_open     = uct_gaudi_ipc_iface_open,
    .query_devices  = uct_gaudi_base_query_tl_devices,
};

UCT_TL_REGISTER(&uct_gaudi_ipc_tl, &uct_gaudi_ipc_component.super,
                UCT_GAUDI_IPC_TL_NAME,
                uct_gaudi_ipc_iface_config_table, uct_gaudi_ipc_iface_config_t);