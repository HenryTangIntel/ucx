/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_iface.h"
#include "gaudi_copy_ep.h"

#include <uct/gaudi/base/gaudi_md.h>
#include <ucs/type/class.h>
#include <ucs/sys/string.h>
#include <ucs/arch/cpu.h>

#define UCT_GAUDI_COPY_IFACE_OVERHEAD 0
#define UCT_GAUDI_COPY_IFACE_LATENCY  ucs_linear_func_make(5e-6, 0)

static ucs_config_field_t uct_gaudi_copy_iface_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_copy_iface_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_iface_config_table)},

    {"BANDWIDTH", "25600MB/s", "Effective memory bandwidth",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, bandwidth), UCS_CONFIG_TYPE_BW},

    {"OVERHEAD", "0", "Software overhead in seconds",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, overhead), UCS_CONFIG_TYPE_TIME},

    {"LATENCY", "5us", "Software latency in seconds", 
     ucs_offsetof(uct_gaudi_copy_iface_config_t, latency), UCS_CONFIG_TYPE_TIME},

    {"MAX_SHORT", "256", "Maximum short message size",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, max_short), UCS_CONFIG_TYPE_MEMUNITS},

    {"MAX_BCOPY", "32768", "Maximum bcopy message size",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, max_bcopy), UCS_CONFIG_TYPE_MEMUNITS},

    {"MAX_ZCOPY", "1GB", "Maximum zcopy message size",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, max_zcopy), UCS_CONFIG_TYPE_MEMUNITS},

    {NULL}
};

static ucs_status_t uct_gaudi_copy_iface_query(uct_iface_h tl_iface,
                                               uct_iface_attr_t *iface_attr)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);

    memset(iface_attr, 0, sizeof(*iface_attr));

    /* Interface capabilities */
    iface_attr->iface_addr_len      = 0;
    iface_attr->device_addr_len     = 0;
    iface_attr->ep_addr_len         = 0;
    iface_attr->max_conn_priv       = 0;
    iface_attr->cap.flags           = UCT_IFACE_FLAG_PUT_SHORT |
                                      UCT_IFACE_FLAG_PUT_BCOPY |
                                      UCT_IFACE_FLAG_PUT_ZCOPY |
                                      UCT_IFACE_FLAG_GET_BCOPY |
                                      UCT_IFACE_FLAG_GET_ZCOPY |
                                      UCT_IFACE_FLAG_CONNECT_TO_IFACE;

    /* Short message capabilities */
    iface_attr->cap.put.max_short   = iface->config.max_short;
    iface_attr->cap.put.min_zcopy   = 1;
    iface_attr->cap.put.max_zcopy   = iface->config.max_zcopy;
    iface_attr->cap.put.max_bcopy   = iface->config.max_bcopy;
    iface_attr->cap.put.min_zcopy   = 1;
    iface_attr->cap.put.max_iov     = 1;
    iface_attr->cap.put.opt_zcopy_align = 1;
    iface_attr->cap.put.align_mtu   = iface_attr->cap.put.opt_zcopy_align;

    /* Get capabilities */
    iface_attr->cap.get.max_bcopy   = iface->config.max_bcopy;
    iface_attr->cap.get.min_zcopy   = 1;
    iface_attr->cap.get.max_zcopy   = iface->config.max_zcopy;
    iface_attr->cap.get.max_iov     = 1;
    iface_attr->cap.get.opt_zcopy_align = 1;
    iface_attr->cap.get.align_mtu   = iface_attr->cap.get.opt_zcopy_align;

    /* Performance characteristics */
    iface_attr->latency             = ucs_linear_func_make(iface->config.latency, 0);
    iface_attr->bandwidth.dedicated = iface->config.bandwidth;
    iface_attr->bandwidth.shared    = 0;
    iface_attr->overhead            = iface->config.overhead;
    iface_attr->priority            = 0;

    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_iface_get_address(uct_iface_h tl_iface,
                                                     uct_iface_addr_t *iface_addr)
{
    return UCS_OK; /* No addressing needed for copy operations */
}

static int uct_gaudi_copy_iface_is_reachable(const uct_iface_h tl_iface,
                                             const uct_device_addr_t *dev_addr,
                                             const uct_iface_addr_t *iface_addr)
{
    return 1; /* Always reachable for local copy operations */
}

static ucs_status_t uct_gaudi_copy_ep_create(const uct_ep_params_t *params,
                                             uct_ep_h *ep_p)
{
    return uct_gaudi_copy_ep_create_connected(params, ep_p);
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_copy_iface_t)
{
    /* Nothing to cleanup */
}

static uct_iface_ops_t uct_gaudi_copy_iface_ops = {
    .ep_create                = uct_gaudi_copy_ep_create,
    .ep_destroy               = ucs_empty_function_return_success,
    .iface_flush              = uct_base_iface_flush,
    .iface_fence              = uct_base_iface_fence,
    .iface_progress_enable    = ucs_empty_function,
    .iface_progress_disable   = ucs_empty_function,
    .iface_progress           = ucs_empty_function_return_zero,
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_iface_t),
    .iface_query              = uct_gaudi_copy_iface_query,
    .iface_get_device_address = ucs_empty_function_return_success,
    .iface_get_address        = uct_gaudi_copy_iface_get_address,
    .iface_is_reachable       = uct_gaudi_copy_iface_is_reachable,
};

static ucs_status_t uct_gaudi_copy_iface_open(uct_md_h md, uct_worker_h worker,
                                              const uct_iface_params_t *params,
                                              const uct_iface_config_t *tl_config,
                                              uct_iface_h *iface_p)
{
    uct_gaudi_copy_iface_config_t *config = ucs_derived_of(tl_config, uct_gaudi_copy_iface_config_t);
    uct_gaudi_copy_iface_t *iface;
    ucs_status_t status;

    iface = ucs_malloc(sizeof(*iface), "gaudi_copy_iface");
    if (iface == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    status = UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, &iface->super, &uct_gaudi_copy_iface_ops,
                                       &uct_gaudi_copy_ep_ops, md, worker, params,
                                       tl_config UCS_STATS_ARG(params->stats_root)
                                       UCS_STATS_ARG(UCT_GAUDI_COPY_TL_NAME));
    if (status != UCS_OK) {
        goto err_free;
    }

    /* Copy configuration */
    iface->config.bandwidth  = config->bandwidth;
    iface->config.overhead   = config->overhead;
    iface->config.latency    = config->latency;
    iface->config.max_short  = config->max_short;
    iface->config.max_bcopy  = config->max_bcopy;
    iface->config.max_zcopy  = config->max_zcopy;

    *iface_p = &iface->super.super;
    return UCS_OK;

err_free:
    ucs_free(iface);
    return status;
}

UCS_CLASS_DEFINE(uct_gaudi_copy_iface_t, uct_base_iface_t);

static uct_tl_t uct_gaudi_copy_tl = {
    .name           = UCT_GAUDI_COPY_TL_NAME,
    .iface_open     = uct_gaudi_copy_iface_open,
    .query_devices  = uct_gaudi_base_query_tl_devices,
};

UCT_TL_REGISTER(&uct_gaudi_copy_tl, &uct_gaudi_copy_component.super,
                UCT_GAUDI_COPY_TL_NAME,
                uct_gaudi_copy_iface_config_table, uct_gaudi_copy_iface_config_t);