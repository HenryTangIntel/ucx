/**
* Copyright (c) 2023-2024 Intel Corporation
*
* See file LICENSE for terms.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_iface.h"
#include "gaudi_copy_ep.h" // Will be created in the next step
#include <uct/gaudi/gaudi_md.h> // For UCT_MD_MEM_TYPE_GAUDI

#include <ucs/type/class.h>
#include <ucs/sys/string.h>
#include <ucs/arch/cpu.h>

// Default bandwidth if not configured - placeholder value
#define UCT_GAUDI_COPY_DEFAULT_BW (10000.0 * UCS_MBYTE)


static ucs_config_field_t uct_gaudi_copy_iface_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_copy_iface_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_iface_config_table)},

    {"MAX_BCOPY", "1m", /* Corresponds to UCS_MEMUNITS_AUTO */
     "Maximum size for bcopy operations.",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, max_bcopy), UCS_CONFIG_TYPE_MEMUNITS},

    {"BW", "10000MBs", /* Placeholder bandwidth */
     "Effective memory bandwidth for copy operations.",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, super.bandwidth), UCS_CONFIG_TYPE_BW},

    {NULL}
};

/* Forward declaration for the delete function */
static void UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_iface_t)(uct_iface_t*);


static ucs_status_t uct_gaudi_copy_iface_get_address(uct_iface_h tl_iface,
                                                     uct_iface_addr_t *iface_addr)
{
    /* Gaudi copy is typically within a single node, address might not be complex */
    *((int*)iface_addr) = 0xdeadbeef; // Dummy address
    return UCS_OK;
}

static int uct_gaudi_copy_iface_is_reachable(const uct_iface_h tl_iface,
                                             const uct_device_addr_t *dev_addr,
                                             const uct_iface_addr_t *iface_addr)
{
    /* Assuming reachable if on the same node/process */
    return 1;
}

static ucs_status_t uct_gaudi_copy_iface_query(uct_iface_h tl_iface,
                                               uct_iface_attr_t *iface_attr)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);

    uct_base_iface_query(&iface->super, iface_attr);

    iface_attr->iface_addr_len          = sizeof(int); /* Dummy address size */
    iface_attr->device_addr_len         = 0;
    iface_attr->ep_addr_len             = 0;
    iface_attr->cap.flags               = UCT_IFACE_FLAG_CONNECT_TO_IFACE |
                                          UCT_IFACE_FLAG_PUT_BCOPY        |
                                          UCT_IFACE_FLAG_GET_BCOPY        |
                                          UCT_IFACE_FLAG_PENDING; /* If operations can be pending */
                                          // UCT_IFACE_FLAG_AM_BCOPY (if we add AM later)

    iface_attr->cap.put.max_short       = 0;
    iface_attr->cap.put.max_bcopy       = iface->config.max_bcopy;
    iface_attr->cap.put.min_zcopy       = 0;
    iface_attr->cap.put.max_zcopy       = 0; // No ZCOPY for now
    iface_attr->cap.put.opt_zcopy_align = 1;
    iface_attr->cap.put.align_mtu       = 1;
    iface_attr->cap.put.max_iov         = 1;

    iface_attr->cap.get.max_short       = 0;
    iface_attr->cap.get.max_bcopy       = iface->config.max_bcopy;
    iface_attr->cap.get.min_zcopy       = 0;
    iface_attr->cap.get.max_zcopy       = 0; // No ZCOPY for now
    iface_attr->cap.get.opt_zcopy_align = 1;
    iface_attr->cap.get.align_mtu       = 1;
    iface_attr->cap.get.max_iov         = 1;

    iface_attr->cap.am.max_short        = 0; // No AM for now
    iface_attr->cap.am.max_bcopy        = 0;
    iface_attr->cap.am.min_zcopy        = 0;
    iface_attr->cap.am.max_zcopy        = 0;
    iface_attr->cap.am.opt_zcopy_align  = 1;
    iface_attr->cap.am.align_mtu        = 1;
    iface_attr->cap.am.max_hdr          = 0;
    iface_attr->cap.am.max_iov          = 1;

    iface_attr->latency.c               = 10e-9; /* Placeholder latency */
    iface_attr->latency.m               = 0;
    iface_attr->bandwidth.dedicated     = 0;
    iface_attr->bandwidth.shared        = iface->super.config.bandwidth;
    iface_attr->overhead                = 50e-9; /* Placeholder overhead */
    iface_attr->priority                = 0;

    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_iface_flush(uct_iface_h tl_iface, unsigned flags,
                                               uct_completion_t *comp)
{
    /* If operations are synchronous or use a progress mechanism that handles completions,
     * flush might just return UCS_OK or UCS_INPROGRESS if there's an internal queue.
     * For now, assume synchronous behavior for copies, so no pending operations to flush from iface.
     */
    if (comp != NULL) {
        return UCS_ERR_UNSUPPORTED; // Async flush not supported yet
    }
    UCT_TL_IFACE_STAT_FLUSH(ucs_derived_of(tl_iface, uct_base_iface_t));
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_iface_fence(uct_iface_h tl_iface, unsigned flags)
{
    /* TODO: Implement with hlthunk stream synchronization if applicable */
    return UCS_OK;
}


static unsigned uct_gaudi_copy_iface_progress(uct_iface_h tl_iface)
{
    /* If copy operations are asynchronous and use events/callbacks,
     * this function would poll/check them.
     * For now, assuming synchronous copies handled at EP level, or no specific iface progress needed.
     */
    return 0;
}

static uct_iface_ops_t uct_gaudi_copy_iface_ops = {
    .ep_put_bcopy             = uct_gaudi_copy_ep_put_bcopy, // To be implemented in gaudi_copy_ep.c
    .ep_get_bcopy             = uct_gaudi_copy_ep_get_bcopy, // To be implemented in gaudi_copy_ep.c
    // .ep_am_bcopy           = uct_gaudi_copy_ep_am_bcopy, // If AM is added
    .ep_pending_add           = ucs_empty_function_return_busy,
    .ep_pending_purge         = ucs_empty_function,
    .ep_flush                 = uct_base_ep_flush, // Can use base for now
    .ep_fence                 = uct_base_ep_fence, // Can use base for now
    .ep_create                = UCS_CLASS_NEW_FUNC_NAME(uct_gaudi_copy_ep_t), // To be implemented
    .ep_destroy               = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_ep_t), // To be implemented
    .iface_flush              = uct_gaudi_copy_iface_flush,
    .iface_fence              = uct_gaudi_copy_iface_fence,
    .iface_progress_enable    = uct_base_iface_progress_enable,
    .iface_progress_disable   = uct_base_iface_progress_disable,
    .iface_progress           = uct_gaudi_copy_iface_progress,
    .iface_event_fd_get       = ucs_empty_function_return_unsupported, // No event FD for now
    .iface_event_arm          = ucs_empty_function_return_unsupported, // No event FD for now
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_iface_t),
    .iface_query              = uct_gaudi_copy_iface_query,
    .iface_get_device_address = (uct_iface_get_device_address_func_t)ucs_empty_function_return_success,
    .iface_get_address        = uct_gaudi_copy_iface_get_address,
    .iface_is_reachable       = uct_gaudi_copy_iface_is_reachable
};


static UCS_CLASS_INIT_FUNC(uct_gaudi_copy_iface_t, uct_md_h md, uct_worker_h worker,
                           const uct_iface_params_t *params,
                           const uct_iface_config_t *tl_config)
{
    uct_gaudi_copy_iface_config_t *config = ucs_derived_of(tl_config,
                                                           uct_gaudi_copy_iface_config_t);

    UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, &uct_gaudi_copy_iface_ops, md, worker,
                              params, tl_config UCS_STATS_ARG(params->stats_root)
                              UCS_STATS_ARG(UCT_GAUDI_COPY_TL_NAME));

    self->md                 = ucs_derived_of(md, uct_gaudi_copy_md_t);
    self->config.max_bcopy   = config->max_bcopy;
    if (self->super.config.bandwidth == 0) {
        self->super.config.bandwidth = UCT_GAUDI_COPY_DEFAULT_BW;
    }


    ucs_debug("Gaudi Copy Iface initialized: max_bcopy %zu, bandwidth %.2f MB/s",
              self->config.max_bcopy, self->super.config.bandwidth / UCS_MBYTE);

    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_copy_iface_t)
{
    uct_base_iface_progress_disable(&self->super,
                                    UCT_PROGRESS_SEND | UCT_PROGRESS_RECV);
    /* Clean up any iface specific resources */
    ucs_debug("Gaudi Copy Iface cleaned up");
}

UCS_CLASS_DEFINE(uct_gaudi_copy_iface_t, uct_base_iface_t);
UCS_CLASS_DEFINE_NEW_FUNC(uct_gaudi_copy_iface_t, uct_iface_t, uct_md_h, uct_worker_h,
                          const uct_iface_params_t*, const uct_iface_config_t*);
static UCS_CLASS_DEFINE_DELETE_FUNC(uct_gaudi_copy_iface_t, uct_iface_t);


/*
 * This defines the "gaudi_copy" transport layer.
 * - &uct_gaudi_copy_component:  The MD component this transport belongs to.
 * - UCT_GAUDI_COPY_TL_NAME:     The name of the transport ("gaudi_copy").
 * - uct_gaudi_query_md_resources: Function to find underlying MDs.
 *                                 Using the main one from gaudi_md.c.
 * - uct_gaudi_copy_iface_t:     The class implementing this interface.
 * - "GAUDI_COPY_" :             Configuration prefix.
 * - uct_gaudi_copy_iface_config_table: The configuration table for this interface.
 * - uct_gaudi_copy_iface_config_t:   The configuration structure.
 */
UCT_TL_DEFINE(&uct_gaudi_copy_component, UCT_GAUDI_COPY_TL_NAME,
              uct_gaudi_query_md_resources, /* From gaudi_md.c, to find Gaudi devices */
              uct_gaudi_copy_iface_t,
              "GAUDI_COPY_", /* Config prefix for iface config */
              uct_gaudi_copy_iface_config_table,
              uct_gaudi_copy_iface_config_t);
