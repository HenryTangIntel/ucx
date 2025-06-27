/**
* Copyright (c) 2023-2024 Intel Corporation
*
* See file LICENSE for terms.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_md.h"
#include <uct/gaudi/gaudi_md.h> // For uct_gaudi_query_md_resources and UCT_MD_MEM_TYPE_GAUDI

#include <string.h>
#include <limits.h>
#include <ucs/debug/log.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/memory/memtype_cache.h> // If we implement memtype detection
#include <ucs/type/class.h>


static ucs_config_field_t uct_gaudi_copy_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_copy_md_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    /* Add gaudi_copy specific MD configurations here if any in the future */
    /* For example, similar to cuda_copy_md_config_table's REG_WHOLE_ALLOC */

    {NULL}
};


/**
 * The gaudi_copy MD itself doesn't allocate/register memory in the Gaudi device space directly.
 * That's the job of the main "gaudi" MD.
 * This MD is more about declaring capabilities for the gaudi_copy transport.
 * So, mem_alloc/free/reg/dereg might be minimal or even stubs if all memory
 * is expected to be handled by the main "gaudi" MD and host MD.
 *
 * However, to mirror cuda_copy, it might be useful for it to be able to register
 * host memory if the underlying Gaudi API has an equivalent to cudaHostRegister.
 * For now, these will be stubs or return UCS_ERR_UNSUPPORTED.
 */

static ucs_status_t uct_gaudi_copy_md_mem_alloc(uct_md_h md, size_t *length_p,
                                                void **address_p, ucs_memory_type_t mem_type,
                                                unsigned flags, const char *alloc_name,
                                                uct_mem_h *memh_p)
{
    /* gaudi_copy MD does not allocate memory directly; uses underlying gaudi MD for device memory */
    ucs_debug("uct_gaudi_copy_md_mem_alloc: not supported");
    return UCS_ERR_UNSUPPORTED;
}

static ucs_status_t uct_gaudi_copy_md_mem_free(uct_md_h md, uct_mem_h memh)
{
    ucs_debug("uct_gaudi_copy_md_mem_free: not supported");
    return UCS_ERR_UNSUPPORTED;
}

static ucs_status_t uct_gaudi_copy_md_mem_reg(uct_md_h md, void *address, size_t length,
                                              const uct_md_mem_reg_params_t *params,
                                              uct_mem_h *memh_p)
{
    /* Placeholder: If hlthunk has something like cudaHostRegister for host memory,
     * it could be used here. For now, assume main gaudi MD handles device memory
     * registration and host memory is either pre-registered or handled by a general host MD.
     * Returning a dummy handle for now.
     */
    ucs_trace("uct_gaudi_copy_md_mem_reg: address %p, length %zu (dummy registration)",
              address, length);
    *memh_p = (void*)address; // Dummy handle, same as input address
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_md_mem_dereg(uct_md_h md,
                                                const uct_md_mem_dereg_params_t *params)
{
    uct_mem_h memh = UCT_MD_MEM_DEREG_FIELD_VALUE(params, memh, NULL, NULL);
    ucs_trace("uct_gaudi_copy_md_mem_dereg: memh %p (dummy deregistration)", memh);
    return UCS_OK;
}

static void uct_gaudi_copy_md_close(uct_md_h uct_md) {
    uct_gaudi_copy_md_t *md = ucs_derived_of(uct_md, uct_gaudi_copy_md_t);
    ucs_debug("uct_gaudi_copy_md_close: md %p", md);
    /* If gaudi_md was opened and owned by this MD, close it here.
     * Assuming for now it's a reference to an MD opened elsewhere or by the main component.
     */
    ucs_free(md);
}

ucs_status_t uct_gaudi_copy_md_query(uct_md_h uct_md, uct_md_attr_v2_t *md_attr)
{
    /* The gaudi_copy transport can copy between host memory and Gaudi device memory.
     * It registers host memory (potentially, via uct_gaudi_copy_md_mem_reg if needed)
     * and uses memory registered by the main "gaudi" MD.
     */
    md_attr->flags                  = UCT_MD_FLAG_REG; /* Can register (host) memory */
                                      /* No ALLOC flag as it doesn't allocate directly */
    md_attr->reg_mem_types          = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                      UCS_BIT(UCT_MD_MEM_TYPE_GAUDI);
    md_attr->alloc_mem_types        = 0; /* Does not allocate */
    md_attr->access_mem_types       = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                      UCS_BIT(UCT_MD_MEM_TYPE_GAUDI);
    md_attr->detect_mem_types       = UCS_BIT(UCT_MD_MEM_TYPE_GAUDI); // Can detect Gaudi memory
    md_attr->dmabuf_mem_types       = 0; // No dmabuf support for now
    md_attr->max_alloc              = 0;
    md_attr->max_reg                = ULONG_MAX;
    md_attr->rkey_packed_size       = 0; /* No remote keys needed for copy transport itself */
    md_attr->reg_cost.c             = 10e-9; /* Placeholder for host registration cost */
    md_attr->reg_cost.m             = 0;
    memset(&md_attr->local_cpus, 0xff, sizeof(md_attr->local_cpus));
    return UCS_OK;
}

// Forward declaration for detect_memory_type
static ucs_status_t uct_gaudi_copy_detect_memory_type(uct_md_h md,
                                                      const void *address,
                                                      size_t length,
                                                      ucs_memory_type_t *mem_type_p);


static uct_md_ops_t uct_gaudi_copy_md_ops = {
    .close              = uct_gaudi_copy_md_close,
    .query              = uct_gaudi_copy_md_query,
    .mem_alloc          = uct_gaudi_copy_md_mem_alloc,
    .mem_free           = uct_gaudi_copy_md_mem_free,
    .mem_reg            = uct_gaudi_copy_md_mem_reg,
    .mem_dereg          = uct_gaudi_copy_md_mem_dereg,
    .mkey_pack          = NULL, /* No mkey packing for copy transport */
    .is_mem_type_owned  = (uct_md_is_mem_type_owned_func_t)ucs_empty_function_return_zero,
    .detect_memory_type = uct_gaudi_copy_detect_memory_type
};


static ucs_status_t uct_gaudi_copy_md_open(uct_component_t *component,
                                           const char *md_name,
                                           const uct_md_config_t *md_config,
                                           uct_md_h *md_p)
{
    uct_gaudi_copy_md_t *md;
    ucs_status_t status;
    uct_md_h main_gaudi_md;
    uct_component_t *main_gaudi_component = &uct_gaudi_component; // from gaudi_md.h
    uct_md_config_t *main_gaudi_md_config; // Use default config for the main MD

    md = ucs_malloc(sizeof(uct_gaudi_copy_md_t), "uct_gaudi_copy_md_t");
    if (NULL == md) {
        ucs_error("failed to allocate memory for uct_gaudi_copy_md_t");
        return UCS_ERR_NO_MEMORY;
    }

    // Initialize the superclass fields
    md->super.ops       = &uct_gaudi_copy_md_ops;
    md->super.component = component;

    // Try to open the main 'gaudi' MD to get a handle to it.
    // This is needed for operations like detect_memory_type.
    // We use the default configuration for the main Gaudi MD.
    status = uct_md_config_read(main_gaudi_component, NULL, NULL, &main_gaudi_md_config);
    if (status != UCS_OK) {
        ucs_error("Failed to read default config for main gaudi MD: %s", ucs_status_string(status));
        ucs_free(md);
        return status;
    }

    status = uct_md_open(main_gaudi_component, UCT_GAUDI_MD_NAME, main_gaudi_md_config, &main_gaudi_md);
    uct_config_release(main_gaudi_md_config); // Release config after use

    if (status != UCS_OK) {
        ucs_warn("uct_gaudi_copy_md_open: could not open main gaudi MD '%s' (%s). "
                 "Memory type detection for Gaudi memory might be limited.",
                 UCT_GAUDI_MD_NAME, ucs_status_string(status));
        md->gaudi_md = NULL; // Mark that we don't have the main MD
    } else {
        ucs_debug("uct_gaudi_copy_md_open: successfully opened main gaudi MD '%s'", UCT_GAUDI_MD_NAME);
        md->gaudi_md = main_gaudi_md;
    }

    *md_p = (uct_md_h)md;
    ucs_debug("Gaudi Copy MD %s opened successfully", md_name);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_detect_memory_type(uct_md_h md,
                                                      const void *address,
                                                      size_t length,
                                                      ucs_memory_type_t *mem_type_p)
{
    uct_gaudi_copy_md_t *copy_md = ucs_derived_of(md, uct_gaudi_copy_md_t);
    uct_md_attr_v2_t md_attr;
    ucs_status_t status;

    if (copy_md->gaudi_md != NULL) {
        // Query the main gaudi_md to detect memory type
        if (copy_md->gaudi_md->ops->detect_memory_type != NULL) {
            status = copy_md->gaudi_md->ops->detect_memory_type(copy_md->gaudi_md,
                                                                address, length,
                                                                mem_type_p);
            if (status == UCS_OK && *mem_type_p == UCT_MD_MEM_TYPE_GAUDI) {
                return UCS_OK;
            }
        } else {
            // Fallback: Check if address is within ranges known to gaudi_md (not implemented here)
            // For now, if detect_memory_type is not on main MD, we can't detect GAUDI type via it.
        }
    }

    // Default to host memory if not detected as Gaudi
    // A more robust solution would query system services or use address range analysis
    *mem_type_p = UCS_MEMORY_TYPE_HOST;
    return UCS_OK;
}


/*
 * The uct_gaudi_copy_component is a UCT component that represents the "gaudi_copy"
 * transport. It does not manage Gaudi hardware resources directly but relies on
 * the main "gaudi" component/MD for that.
 *
 * Its query_md_resources uses the main uct_gaudi_query_md_resources. This implies
 * that for every Gaudi device found by the main MD component, a gaudi_copy MD
 * will also be available.
 */
UCT_COMPONENT_DEFINE(uct_gaudi_copy_component, UCT_GAUDI_COPY_MD_NAME,
                     uct_gaudi_query_md_resources, /* Use main gaudi MD's resource query function */
                     uct_gaudi_copy_md_open,
                     NULL, NULL, /* cm_open, rkey_unpack */
                     NULL, NULL, /* rkey_ptr, rkey_release */
                     "GAUDI_COPY_", uct_gaudi_copy_md_config_table, /* Note: GAUDI_COPY_ prefix */
                     sizeof(uct_gaudi_copy_md_config_t),
                     ucs_empty_function_return_success, /* vfs_init */
                     0,             /* flags */
                     NULL           /* tl_list - will be populated by IFACE component part */
                     );

/*
The tl_list (transport list) for uct_gaudi_copy_component should be populated
when the corresponding gaudi_copy *interface* is defined.
Typically, a UCT_TL_DEFINE macro is used for that, which adds the transport
to its component's tl_list.
*/
