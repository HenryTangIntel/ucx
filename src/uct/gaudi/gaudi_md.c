#include <ucs/debug/memtrack.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/log.h>
#include "gaudi_md.h"
#include <hlthunk.h>
#include <uct/base/uct_md.h>

// Assumed hlthunk function for device memory allocation
#define hlthunk_device_memory_alloc(fd, size, type, address_p) UCS_ERR_NOT_IMPLEMENTED
// Assumed hlthunk function for freeing memory
#define hlthunk_memory_free(fd, address) UCS_ERR_NOT_IMPLEMENTED
// Assumed hlthunk function for registering memory
#define hlthunk_memory_register(fd, address, length, memh_p) UCS_ERR_NOT_IMPLEMENTED
// Assumed hlthunk function for deregistering memory
#define hlthunk_memory_deregister(fd, memh) UCS_ERR_NOT_IMPLEMENTED


enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2,
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
};

static ucs_config_field_t uct_gaudi_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_md_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {NULL}
};

void uct_gaudi_md_close(uct_md_h uct_md)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    if (md->hlthunk_fd >= 0) {
        hlthunk_close(md->hlthunk_fd);
    }
    ucs_free(md);
}

ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr)
{
    md_attr->flags = UCT_MD_FLAG_REG | UCT_MD_FLAG_ALLOC | UCT_MD_FLAG_NEED_RKEY;
    /* Assuming Gaudi can allocate and register its own device memory (UCS_MEMORY_TYPE_ROCM for ROCm, UCS_MEMORY_TYPE_CUDA for CUDA)
     * and also register host memory.
     * For now, let's use a placeholder UCS_MEMORY_TYPE_GAUDI, which would need to be defined.
     * Or, if hlthunk uses a more generic device memory concept, we might use UCS_MEMORY_TYPE_DEVICE.
     * Let's assume a UCS_MEMORY_TYPE_GAUDI for now.
     */
    md_attr->reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_HOST) | UCS_BIT(UCT_MD_MEM_TYPE_GAUDI);
    md_attr->alloc_mem_types  = UCS_BIT(UCT_MD_MEM_TYPE_GAUDI);
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) | UCS_BIT(UCT_MD_MEM_TYPE_GAUDI);
    md_attr->detect_mem_types = 0; // TODO: Implement memory type detection if possible
    md_attr->max_alloc        = UINT64_MAX; // Placeholder
    md_attr->max_reg          = UINT64_MAX; // Placeholder
    md_attr->rkey_packed_size = sizeof(uint64_t); // Placeholder, dependent on actual rkey structure
    md_attr->reg_cost.c       = 100e-9; // Placeholder
    md_attr->reg_cost.m       = 0;
    memset(&md_attr->local_cpus, 0xff, sizeof(md_attr->local_cpus)); // Assume accessible from all CPUs

    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_alloc(uct_md_h md, size_t *length_p,
                                           void **address_p, ucs_memory_type_t mem_type,
                                           unsigned flags, const char *alloc_name, uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    ucs_status_t status;

    if (mem_type != UCT_MD_MEM_TYPE_GAUDI) {
        ucs_error("Unsupported memory type %d for allocation by gaudi MD", mem_type);
        return UCS_ERR_UNSUPPORTED;
    }

    // Placeholder: Using a generic type for allocation. This might need specific flags.
    status = hlthunk_device_memory_alloc(gaudi_md->hlthunk_fd, *length_p, 0, address_p);
    if (status != UCS_OK) {
        ucs_error("hlthunk_device_memory_alloc(size=%zu) failed: %s", *length_p, ucs_status_string(status));
        return status;
    }

    // For alloc, memh_p is an opaque handle. Here, we might store the address or a registration handle.
    // If hlthunk_device_memory_alloc also implicitly registers, we might get a handle.
    // For now, let's assume it's just the pointer, and it needs separate registration if used for RMA.
    // However, UCT_MD_FLAG_ALLOC often implies it's ready for use by the transport.
    // Let's assume for now that alloc implies it's usable by the local device,
    // and registration is for exposing it to remote operations (which needs an rkey).
    *memh_p = (uct_mem_h)(uintptr_t)(*address_p); // Simplified: using address as handle. This is likely insufficient.
                                                // A proper implementation would register this memory and store the hlthunk handle.
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    void *address = (void*)(uintptr_t)memh; // Assuming memh was the address
    ucs_status_t status;

    // We need to distinguish between memory allocated by us and memory registered by us.
    // This simple conversion from memh to address is problematic if memh is a complex structure.
    // For now, assuming it's directly the address from a previous alloc.
    status = hlthunk_memory_free(gaudi_md->hlthunk_fd, address);
    if (status != UCS_OK) {
        ucs_error("hlthunk_memory_free(address=%p) failed: %s", address, ucs_status_string(status));
        return status;
    }
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_reg(uct_md_h md, void *address, size_t length,
                                         const uct_md_mem_reg_params_t *params,
                                         uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uint64_t *gaudi_memh; // Placeholder for what hlthunk_memory_register returns
    ucs_status_t status;

    // UCT_MD_MEM_REG_FIELD_MEM_TYPE should be checked if provided in params
    // ucs_memory_type_t mem_type = UCT_MD_MEM_REG_FIELD_VALUE(params, mem_type, UCS_MEMORY_TYPE_UNKNOWN);

    // This is a placeholder. The actual registration might return a more complex handle.
    gaudi_memh = ucs_malloc(sizeof(*gaudi_memh), "gaudi_memh");
    if (gaudi_memh == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    status = hlthunk_memory_register(gaudi_md->hlthunk_fd, address, length, gaudi_memh);
    if (status != UCS_OK) {
        ucs_error("hlthunk_memory_register(address=%p, length=%zu) failed: %s",
                  address, length, ucs_status_string(status));
        ucs_free(gaudi_memh);
        return status;
    }

    *memh_p = (uct_mem_h)gaudi_memh;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_dereg(uct_md_h md, const uct_md_mem_dereg_params_t *params)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_mem_h memh = UCT_MD_MEM_DEREG_FIELD_VALUE(params, memh, NULL, NULL);
    uint64_t *gaudi_memh = (uint64_t*)memh; // Assuming memh is the pointer to our stored handle
    ucs_status_t status;

    if (gaudi_memh == NULL) {
        ucs_error("Invalid memory handle for deregistration");
        return UCS_ERR_INVALID_PARAM;
    }

    status = hlthunk_memory_deregister(gaudi_md->hlthunk_fd, *gaudi_memh);
    if (status != UCS_OK) {
        ucs_error("hlthunk_memory_deregister() failed: %s", ucs_status_string(status));
        // We still free our memh structure
    }

    ucs_free(gaudi_memh);
    return status;
}

static uct_md_ops_t uct_gaudi_md_ops = {
    .close        = uct_gaudi_md_close,
    .query        = uct_gaudi_md_query,
    .mem_alloc    = uct_gaudi_md_mem_alloc,
    .mem_free     = uct_gaudi_md_mem_free,
    .mem_reg      = uct_gaudi_md_mem_reg,
    .mem_dereg    = uct_gaudi_md_mem_dereg,
    .mkey_pack    = NULL, // To be implemented: pack rkey for network transfer
    .is_mem_type_owned = (uct_md_is_mem_type_owned_func_t)ucs_empty_function_return_zero, // TODO
    .detect_memory_type = (uct_md_detect_memory_type_func_t)ucs_empty_function_return_unsupported //TODO
};

ucs_status_t uct_gaudi_query_md_resources(uct_component_h component,
                                          uct_md_resource_desc_t **resources_p,
                                          unsigned *num_resources_p)
{
    uct_md_resource_desc_t *resources = NULL;
    unsigned num_devices = 0;
    int fd = -1;
    int i, current_device;
    ucs_status_t status = UCS_OK;

    // Try to open any available Gaudi device to confirm presence
    for (i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
        fd = hlthunk_open(devices[i], NULL);
        if (fd >= 0) {
            // For simplicity, assume one MD resource if any device is found.
            // A more robust implementation would query hlthunk for number of devices
            // and potentially create one MD per device or a single MD managing all.
            num_devices = 1; // Simplified
            hlthunk_close(fd);
            break;
        }
    }

    if (num_devices == 0) {
        ucs_debug("No Gaudi devices found");
        *resources_p     = NULL;
        *num_resources_p = 0;
        return UCS_OK; // No error, just no resources
    }
    
    resources = ucs_calloc(num_devices, sizeof(*resources), "gaudi md resource");
    if (resources == NULL) {
        ucs_error("Failed to allocate Gaudi MD resources");
        return UCS_ERR_NO_MEMORY;
    }
    
    // For now, creating one MD resource named "gaudi0" if a device is found
    // This part would need to enumerate actual devices if multiple MDs are supported
    snprintf(resources[0].md_name, UCT_MD_NAME_MAX, "%s%d", UCT_GAUDI_MD_NAME, 0);
    // resources[0].mem_type should list supported memory types, e.g. UCT_MD_MEM_TYPE_GAUDI
    // This is usually not set here but queried from the MD itself later.

    *resources_p     = resources;
    *num_resources_p = num_devices;
    
    return UCS_OK;
}

// This component is for the MD itself. Transports like gaudi_copy will have their own components.
UCT_COMPONENT_DEFINE(uct_gaudi_component, "gaudi",
                     uct_gaudi_query_md_resources, uct_gaudi_md_open,
                     NULL, NULL, /* cm_open, rkey_unpack */
                     NULL, NULL, /* rkey_ptr, rkey_release */
                     "GAUDI", uct_gaudi_md_config_table,
                     sizeof(uct_gaudi_md_config_t),
                     ucs_empty_function_return_success, /* vfs_init */
                     0, NULL);


ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                               const char *md_name,
                               const uct_md_config_t *config,
                               uct_md_h *md_p)
{
    uct_gaudi_md_t *md;
    int i;

    md = ucs_malloc(sizeof(*md), "uct_gaudi_md");
    if (md == NULL) {
        ucs_error("Failed to allocate uct_gaudi_md_t");
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &uct_gaudi_md_ops;
    md->super.component = component; // Use the passed component
    md->hlthunk_fd      = -1;

    // Attempt to open a Gaudi device.
    // md_name might indicate a specific device, e.g., "gaudi0", "gaudi1"
    // For now, try opening any available one.
    // A more robust solution would parse md_name for a device index.
    for (i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
        md->hlthunk_fd = hlthunk_open(devices[i], NULL);
        if (md->hlthunk_fd >= 0) {
            ucs_debug("Opened Gaudi device with fd %d using device enum %d", md->hlthunk_fd, devices[i]);
            break;
        }
    }

    if (md->hlthunk_fd < 0) {
        ucs_error("Failed to open any Gaudi device for MD %s", md_name);
        ucs_free(md);
        return UCS_ERR_NO_DEVICE;
    }

    *md_p = &md->super;
    ucs_debug("Gaudi MD %s opened successfully", md_name);
    return UCS_OK;
}

// UCT_COMPONENT_REGISTER is deprecated, use UCT_COMPONENT_DEFINE
// UCT_COMPONENT_REGISTER(&uct_gaudi_component)
