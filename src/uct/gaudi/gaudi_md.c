/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */


#include "gaudi_md.h"
 #include "gaudi_iface.h" // Include the iface header
#include <uct/base/uct_md.h> // For uct_mem_alloc_params_t etc.
#include <uct/base/uct_log.h>
#include <ucs/debug/memtrack.h>
#include <ucs/type/common.h>
#include <ucs/sys/string.h> // For ucs_strncpy_safe


// Configuration table for Gaudi MD
ucs_config_field_t uct_gaudi_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_md_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    // Add Gaudi-specific MD config options here in the future
    // {"MAX_REGISTRATIONS", "1024", "Maximum number of memory registrations",
    //  ucs_offsetof(uct_gaudi_md_config_t, max_regs), UCS_CONFIG_TYPE_UINT},

    {NULL}
};


// Helper function to query MD resources (placeholder)
static ucs_status_t uct_gaudi_md_query_resources(uct_md_resource_desc_t **resources_p,
                                                 unsigned *num_resources_p)
{
    // TODO: Implement actual resource discovery for Gaudi devices.
    // This might involve using a Gaudi API to list available devices.
    // For now, we can return a single dummy resource if Gaudi is "detected".

    // Example: creating a single dummy resource
    uct_md_resource_desc_t *resources;
    resources = ucs_calloc(1, sizeof(*resources), "gaudi md resource");
    if (resources == NULL) {
        ucs_error("failed to allocate gaudi md resource");
        return UCS_ERR_NO_MEMORY;
    }

    ucs_snprintf_safe(resources->md_name, sizeof(resources->md_name), "gaudi");
    resources->dev_type = UCT_DEVICE_TYPE_ACC; // Accelerator type
    // resources->sys_device = UCS_SYS_DEVICE_ID_UNKNOWN; // Or a specific ID if available

    *resources_p = resources;
    *num_resources_p = 1;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_open(uct_component_h component, const char *md_name,
                                      const uct_md_config_t *md_config,
                                      uct_md_h *md_p)
{
    uct_gaudi_md_t *md;

    // TODO: Proper initialization with Gaudi context/device.
    // For now, just allocate the structure.
    md = ucs_malloc(sizeof(*md), "uct_gaudi_md_t");
    if (NULL == md) {
        ucs_error("failed to allocate uct_gaudi_md_t");
        return UCS_ERR_NO_MEMORY;
    }

    ucs_debug("uct_gaudi_md_open: component=%p md_name=%s", component, md_name);

    md->super.ops       = &uct_gaudi_md_ops; // Assign actual ops table
    md->super.component = component;

    // Initialize Gaudi-specific members here
    md->dummy_gaudi_device_id = 0; // Example initialization
    ucs_debug("Gaudi MD %p opened with dummy_gaudi_device_id: %d", md, md->dummy_gaudi_device_id);

    *md_p = &md->super;
    return UCS_OK;
}

static void uct_gaudi_md_close(uct_md_h md)
{
    // TODO: Release Gaudi resources if any were acquired in md_open
    ucs_free(md);
}


static uct_md_ops_t uct_gaudi_md_ops = {
    .close              = uct_gaudi_md_close,
    .query_md_resources = uct_gaudi_md_query_resources,
    .iface_query        = uct_gaudi_iface_query,  // Placeholder from gaudi_iface.c
    .iface_open         = uct_gaudi_iface_open,   // Placeholder from gaudi_iface.c
    .mem_alloc          = uct_md_mem_alloc,       // Using generic alloc for now
    .mem_free           = uct_md_mem_free,        // Using generic free for now
    .mkey_pack          = uct_md_stub_mkey_pack,  // Stub, Gaudi will need a real one
    .rkey_unpack        = uct_md_stub_rkey_unpack,// Stub
    .rkey_release       = uct_md_stub_rkey_release, // Stub
    .is_mem_type_owned  = uct_md_is_mem_type_owned_stub, // Stub
};

static ucs_status_t uct_gaudi_mem_alloc(uct_md_h md, size_t *length_p, void **address_p,
                                        unsigned flags, const char *alloc_name,
                                        uct_mem_alloc_params_t *params)
{
    // uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    // uct_gaudi_mem_alloc_params_t *gaudi_params = ucs_derived_of(params, uct_gaudi_mem_alloc_params_t);

    // TODO: Implement Gaudi specific memory allocation if needed.
    // This might involve gaudiMalloc, or registering host memory.
    // For now, this function could be a stub or call a generic allocator.
    ucs_debug("uct_gaudi_mem_alloc: md=%p length=%zu address=%p flags=%u name=%s",
              md, *length_p, *address_p, flags, alloc_name);

    // If Gaudi has specific alignment or memory type requirements, handle them here.
    // For basic placeholder, we can try to use system malloc or return not implemented.
    // *address_p = malloc(*length_p);
    // if (*address_p == NULL) {
    //     return UCS_ERR_NO_MEMORY;
    // }
    // return UCS_OK;
    return UCS_ERR_NOT_IMPLEMENTED; // Better to be explicit for placeholder
}

static ucs_status_t uct_gaudi_mem_free(uct_md_h md, void *address, uct_mem_free_params_t *params)
{
    // uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    ucs_debug("uct_gaudi_mem_free: md=%p address=%p", md, address);
    // TODO: Implement Gaudi specific memory free.
    // free(address);
    // return UCS_OK;
    return UCS_ERR_NOT_IMPLEMENTED; // Better to be explicit
}

static ucs_status_t uct_gaudi_mkey_pack(uct_md_h md, uct_mem_h memh,
                                        const uct_md_mkey_pack_params_t *params,
                                        void *mkey_buffer)
{
    // uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    // TODO: Implement Gaudi specific mkey packing.
    // This involves serializing information about the memory registration
    // so it can be sent to and used by a remote peer.
    // For a placeholder, this can be a no-op or copy a dummy handle.
    ucs_debug("uct_gaudi_mkey_pack: md=%p memh=%p mkey_buffer=%p", md, memh, mkey_buffer);
    // Example: *(uint64_t*)mkey_buffer = (uint64_t)memh; // Highly simplified, likely incorrect for real hw
    return UCS_ERR_NOT_IMPLEMENTED; // Better to be explicit
}

// Gaudi MD component definition
UCT_COMPONENT_DEFINE(uct_gaudi_md_component, "gaudi_md",
                     uct_gaudi_md_query_resources, uct_gaudi_md_open,
                     uct_gaudi_md_config_table, uct_gaudi_md_config_t,
                     &uct_gaudi_md_ops,
                     0, "GAUDI_MD_"); // Prefix for environment variables

// Helper to get component
uct_component_t *uct_gaudi_component_get()
{
    return &uct_gaudi_md_component;
}
