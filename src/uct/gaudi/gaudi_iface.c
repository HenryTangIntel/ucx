/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "gaudi_iface.h"
#include <uct/base/uct_log.h>
#include <ucs/debug/memtrack.h>
#include <ucs/type/common.h>
#include <ucs/sys/string.h> // For ucs_strncpy_safe
#include <uct/base/uct_md.h> // For uct_md_query_tl_resources

// Configuration table for Gaudi IFACE
ucs_config_field_t uct_gaudi_iface_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_iface_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_iface_config_table)},

    // Add Gaudi-specific IFACE config options here
    // {"MAX_BCOPY", "1m", "Maximum bcopy size for Gaudi",
    //  ucs_offsetof(uct_gaudi_iface_config_t, max_bcopy), UCS_CONFIG_TYPE_MEMUNITS},

    {NULL}
};

// =============================================================================
// Placeholder EP operations
// =============================================================================
static ucs_status_t uct_gaudi_ep_put_short(uct_ep_h ep, const void *buffer,
                                           unsigned length, uint64_t remote_addr,
                                           uct_rkey_t rkey)
{
    // uct_gaudi_ep_t *gaudi_ep = ucs_derived_of(ep, uct_gaudi_ep_t);
    ucs_trace_data("uct_gaudi_ep_put_short: ep=%p buffer=%p length=%u remote_addr=0x%lx rkey=0x%lx",
                   ep, buffer, length, remote_addr, rkey);
    return UCS_ERR_NOT_IMPLEMENTED;
}

static ucs_status_t uct_gaudi_ep_am_short(uct_ep_h ep, uint8_t id, uint64_t header,
                                          const void *payload, unsigned length)
{
    // uct_gaudi_ep_t *gaudi_ep = ucs_derived_of(ep, uct_gaudi_ep_t);
    ucs_trace_data("uct_gaudi_ep_am_short: ep=%p id=%u header=0x%lx payload=%p length=%u",
                   ep, id, header, payload, length);
    return UCS_ERR_NOT_IMPLEMENTED;
}

static ucs_status_t uct_gaudi_ep_create(const uct_ep_params_t *params, uct_ep_h *ep_p)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(params->iface, uct_gaudi_iface_t);
    uct_gaudi_ep_t *ep;

    ep = ucs_calloc(1, sizeof(*ep), "gaudi_ep");
    if (ep == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    uct_base_ep_init(&iface->super, &ep->super);
    // ep->dummy_gaudi_conn_id =magic_number; // Initialize Gaudi specific EP fields

    ucs_debug("uct_gaudi_ep_create: iface=%p ep=%p", iface, ep);
    *ep_p = &ep->super;
    return UCS_OK;
}

static void uct_gaudi_ep_destroy(uct_ep_h ep)
{
    ucs_debug("uct_gaudi_ep_destroy: ep=%p", ep);
    ucs_free(ep);
}

static ucs_status_t uct_gaudi_ep_flush(uct_ep_h ep, unsigned flags, uct_completion_t *comp)
{
    // uct_gaudi_ep_t *gaudi_ep = ucs_derived_of(ep, uct_gaudi_ep_t);
    ucs_debug("uct_gaudi_ep_flush: ep=%p flags=%u", ep, flags);
    /* TODO: Implement actual flush. For now, assume immediate completion for placeholders. */
    return UCS_OK; /* or UCS_INPROGRESS if comp is not NULL */
}

static ucs_status_t uct_gaudi_ep_fence(uct_ep_h ep, unsigned flags)
{
    ucs_debug("uct_gaudi_ep_fence: ep=%p flags=%u", ep, flags);
    return UCS_OK;
}


// =============================================================================
// Placeholder IFACE operations
// =============================================================================

static ucs_status_t uct_gaudi_iface_get_address(uct_iface_h tl_iface, uct_iface_addr_t *addr)
{
    // uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    // *(int*)addr = iface->dummy_gaudi_queue_id; // Example: address is the queue ID
    ucs_debug("uct_gaudi_iface_get_address: iface=%p", tl_iface);
    return UCS_ERR_NOT_IMPLEMENTED; // Real implementation needed
}

static int uct_gaudi_iface_is_reachable(const uct_iface_h tl_iface, const uct_device_addr_t *dev_addr,
                                        const uct_iface_addr_t *iface_addr)
{
    // uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    // For placeholder, assume reachable if on the same "device" or always reachable
    ucs_debug("uct_gaudi_iface_is_reachable: iface=%p", tl_iface);
    return 1; // Assume reachable for now
}

static ucs_status_t uct_gaudi_iface_query_tl(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    ucs_debug("uct_gaudi_iface_query_tl: iface=%p", iface);

    uct_base_iface_query(&iface->super, iface_attr); // Fills in some defaults

    // TODO: Fill in Gaudi-specific attributes
    iface_attr->cap.flags = UCT_IFACE_FLAG_AM_SHORT |
                            UCT_IFACE_FLAG_PUT_SHORT |
                            UCT_IFACE_FLAG_CONNECT_TO_IFACE; // Basic capabilities

    // Example placeholder values
    iface_attr->iface_addr_len  = sizeof(int); // e.g. if address is dummy_gaudi_queue_id
    iface_attr->device_addr_len = 0; // No separate device address for this simple placeholder
    iface_attr->ep_addr_len     = 0;
    iface_attr->max_conn_priv   = 0;
    iface_attr->cap.put.max_short = 512;
    iface_attr->cap.am.max_short  = 512;
    iface_attr->latency.overhead  = 10e-9; /* 10 ns */
    iface_attr->latency.growth    = 0;
    iface_attr->bandwidth.shared  = 10000 * UCS_MBYTE; /* 10000 MB/s */
    iface_attr->bandwidth.dedicated = 0;
    iface_attr->overhead           = 0.5e-6; /* 0.5 us */

    return UCS_OK;
}

static void uct_gaudi_iface_close(uct_iface_h tl_iface)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    ucs_debug("uct_gaudi_iface_close: iface=%p", iface);
    // TODO: Release Gaudi-specific iface resources
    ucs_free(iface);
}

static ucs_status_t uct_gaudi_iface_flush(uct_iface_h tl_iface, unsigned flags, uct_completion_t *comp)
{
    ucs_debug("uct_gaudi_iface_flush: iface=%p flags=%u", tl_iface, flags);
    /* TODO: Implement. For now, assume immediate completion for placeholders. */
    return UCS_OK; /* or UCS_INPROGRESS if comp is not NULL */
}

static ucs_status_t uct_gaudi_iface_fence(uct_iface_h tl_iface, unsigned flags)
{
    ucs_debug("uct_gaudi_iface_fence: iface=%p flags=%u", tl_iface, flags);
    return UCS_OK;
}


static uct_iface_ops_t uct_gaudi_iface_ops = {
    .ep_put_short             = uct_gaudi_ep_put_short,
    .ep_am_short              = uct_gaudi_ep_am_short,
    .ep_create                = uct_gaudi_ep_create,
    .ep_destroy               = uct_gaudi_ep_destroy,
    .ep_flush                 = uct_gaudi_ep_flush,
    .ep_fence                 = uct_gaudi_ep_fence,
    // .ep_get_short          = (uct_ep_get_short_func_t)ucs_empty_function_return_unsupported,
    // .ep_am_zcopy           = (uct_ep_am_zcopy_func_t)ucs_empty_function_return_unsupported,
    // .ep_put_zcopy          = (uct_ep_put_zcopy_func_t)ucs_empty_function_return_unsupported,
    // .ep_get_zcopy          = (uct_ep_get_zcopy_func_t)ucs_empty_function_return_unsupported,
    // .ep_pending_add        = (uct_ep_pending_add_func_t)ucs_empty_function_return_unsupported,
    // .ep_pending_purge      = (uct_ep_pending_purge_func_t)ucs_empty_function_return_unsupported,
    .iface_flush              = uct_gaudi_iface_flush,
    .iface_fence              = uct_gaudi_iface_fence,
    .iface_progress_enable    = ucs_empty_function,
    .iface_progress_disable   = ucs_empty_function,
    .iface_progress           = ucs_empty_function_return_zero_unsigned,
    // .iface_event_fd_get    = (uct_iface_event_fd_get_func_t)ucs_empty_function_return_unsupported,
    // .iface_event_arm       = (uct_iface_event_arm_func_t)ucs_empty_function_return_unsupported,
    .iface_close              = uct_gaudi_iface_close,
    .iface_query              = uct_gaudi_iface_query_tl,
    .iface_get_address        = uct_gaudi_iface_get_address,
    // .iface_get_device_address = (uct_iface_get_device_address_func_t)ucs_empty_function_return_unsupported,
    .iface_is_reachable       = uct_gaudi_iface_is_reachable,
};

// This is a simplified IFACE component, often IFACEs are created by MDs.
// For now, we'll assume a single "gaudi" transport type that uses the MD.
// A more complete implementation would involve the MD component creating IFACEs.

// Placeholder for iface_query (now uct_gaudi_iface_query_tl for attributes)
// The uct_md_ops.iface_query is for discovering TL resources.
ucs_status_t uct_gaudi_iface_query(uct_md_h md, uct_tl_resource_desc_t **tl_resources_p,
                                   unsigned *num_tl_resources_p)
{
    // TODO: Query actual Gaudi transport resources.
    // This would typically be called by the MD to discover available interfaces.
    // For now, create a dummy transport resource if the MD is "gaudi".
    uct_md_resource_desc_t *md_resource;
    uct_tl_resource_desc_t *tl_resources;

    ucs_assert(md->component == &uct_gaudi_md_component);

    tl_resources = ucs_calloc(1, sizeof(*tl_resources), "gaudi tl resource");
    if (tl_resources == NULL) {
        ucs_error("failed to allocate gaudi tl resource");
        return UCS_ERR_NO_MEMORY;
    }

    // For this placeholder, we assume one "gaudi" device/transport.
    // A real implementation would iterate Gaudi devices/ports.
    ucs_snprintf_safe(tl_resources->tl_name, sizeof(tl_resources->tl_name), "gaudi0"); // Example name
    ucs_snprintf_safe(tl_resources->dev_name, sizeof(tl_resources->dev_name), md->component->name); // "gaudi_md"
    tl_resources->dev_type = UCT_DEVICE_TYPE_ACC;
    // tl_resources->sys_device = UCS_SYS_DEVICE_ID_UNKNOWN;

    *tl_resources_p = tl_resources;
    *num_tl_resources_p = 1;
    return UCS_OK;
}

// Placeholder: In a real scenario, the MD component would have a query_tl_resources op
// that calls something like uct_gaudi_iface_query.
// Then, uct_component_query would iterate these.
// For now, we'll modify uct_gaudi_md_ops in gaudi_md.c to include this.

// Placeholder for iface_open
ucs_status_t uct_gaudi_iface_open(uct_md_h md, uct_worker_h worker,
                                  const uct_iface_params_t *params,
                                  const uct_iface_config_t *generic_iface_config, // This is uct_iface_config_t
                                  uct_iface_h *iface_p)
{
    uct_gaudi_iface_t *iface;
    uct_gaudi_iface_config_t *config = ucs_derived_of(generic_iface_config, uct_gaudi_iface_config_t);

    iface = ucs_malloc(sizeof(*iface), "uct_gaudi_iface_t");
    if (NULL == iface) {
        ucs_error("Failed to allocate uct_gaudi_iface_t");
        return UCS_ERR_NO_MEMORY;
    }

    // Basic initialization from uct_base_iface_t
    uct_base_iface_init(&iface->super, &uct_gaudi_iface_ops,
                        (params->internal_ops == NULL) ? NULL : params->internal_ops, /* internal_ops */
                        md, worker, params,
                        generic_iface_config
                        UCS_STATS_ARG(NULL) /* TODO: stats */
                        UCS_STATS_ARG(NULL));

    // Initialize Gaudi-specific iface members here
    iface->dummy_gaudi_queue_id = 123; // Example ID
    iface->config               = *config; // Store the parsed config

    // TODO: Set actual iface attributes (latency, bandwidth, etc.)
    // For now, they are set in uct_gaudi_iface_query_tl as placeholders.
    // Some might be derived from config.

    ucs_debug("uct_gaudi_iface_open: md=%p worker=%p iface=%p queue_id=%d",
              md, worker, iface, iface->dummy_gaudi_queue_id);

    *iface_p = &iface->super;
    return UCS_OK;
}

// This structure would typically be part of a transport component (e.g., uct_gaudi_tl_component)
// or the MD component would be responsible for creating IFACEs.
// For simplicity in this placeholder stage, we'll assume the MD component
// can also create these IFACEs, or we'll add a new TL component later.
//
// Let's assume for now the MD component will have a uct_iface_open_func_t
// that points to uct_gaudi_iface_open.

// Add these to gaudi_md.c's uct_gaudi_md_ops:
// .iface_query = uct_gaudi_iface_query,
// .iface_open  = uct_gaudi_iface_open,

// And include "gaudi_iface.h" in gaudi_md.c
// Also need to add gaudi_iface.c and gaudi_iface.h to Makefile.am
// uct_gaudi_iface_sources = \
//    gaudi_iface.c \
//    gaudi_iface.h
// libuct_gaudi_la_SOURCES += $(uct_gaudi_iface_sources)
// EXTRA_libuct_gaudi_la_SOURCES += $(uct_gaudi_iface_sources)

// Note: The structure of UCT components means that usually an MD component
// discovers MD resources. Then, for each MD, TL (transport-level) components
// are queried to see if they can create IFACEs on that MD.
// This current placeholder mixes some of these roles for initial simplicity.
// A more correct structure would be:
// 1. uct_gaudi_md_component: Manages Gaudi memory domains.
// 2. uct_gaudi_tl_component (or multiple, e.g., uct_gaudi_copy_component): Manages
//    interfaces and endpoints for a specific way of communicating via Gaudi.
//    This TL component would have query_tl_resources and iface_open ops.

// For now, the goal is to get *something* to compile.
// We will refine the component structure later.
