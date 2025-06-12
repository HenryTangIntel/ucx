#include "gaudi_md.h"
//#include <uct/base/uct_md_device.h>
//#include <uct/base/uct_md_mem.h>
#include <ucs/sys/string.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/log.h>
#include <hlthunk.h>



static uct_md_ops_t md_ops = {
    .close               = (uct_md_close_func_t) ucs_empty_function,
    .query               = uct_gaudi_md_query,
    .mem_alloc           = uct_gaudi_mem_alloc,
    .mem_free            = uct_gaudi_mem_free,
    .mem_reg             = uct_gaudi_mem_reg,
    .mem_dereg           = uct_gaudi_mem_dereg,
    .mkey_pack           = uct_gaudi_mkey_pack,
    .mem_attach          = (uct_md_mem_attach_func_t)ucs_empty_function_return_unsupported,
    .detect_memory_type  = ucs_empty_function_return_unsupported,
    .is_sockaddr_accessible = ucs_empty_function_return_zero_int,
    .is_mem_type_owned   = ucs_empty_function_return_zero
};



ucs_status_t uct_gaudi_query_md_resources(uct_component_t *component, uct_md_resource_desc_t **resources_p,
                                          unsigned *num_resources_p)
{
    uct_md_resource_desc_t *resources;
    int device_count;
    //ucs_status_t status = UCS_OK;
    int i, fd;
    int gaudi_found = 0;
    enum hlthunk_device_name device_type;

    // Initialize default return values
    *num_resources_p = 0;
    *resources_p = NULL;

    // Check if any Gaudi devices are available
    device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    if (device_count <= 0) {
        return UCS_OK; // Not an error, just no devices
    }
    
    for (i = 0; i < device_count && i < HLTHUNK_MAX_MINOR; i++) {
        fd = hlthunk_open_control(i, NULL);
        if (fd < 0) {
            continue;
        }
        
        device_type = hlthunk_get_device_name_from_fd(fd);
        if ((device_type == HLTHUNK_DEVICE_GAUDI) || 
            (device_type == HLTHUNK_DEVICE_GAUDI2)) {
            gaudi_found = 1;
            hlthunk_close(fd);
            break;
        }
        
        hlthunk_close(fd);
    }

    if (!gaudi_found) {
        ucs_debug("No Gaudi devices found");
        return UCS_OK; // Not an error, just no devices
    }

    // Allocate a resource descriptor
    resources = ucs_malloc(sizeof(*resources), "gaudi_md_resources");
    if (resources == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    // Fill in the resource descriptor
    ucs_strncpy_safe(resources[0].md_name, "gaudi", UCT_MD_NAME_MAX);
    
    // Pass the resources back to the caller
    *resources_p = resources;
    *num_resources_p = 1;
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_open(const char *md_name, const uct_md_config_t *config,
                                      uct_md_h *md_p)
{
    uct_gaudi_md_t *md;
    int device_count;
    ucs_status_t status;
    int i, fd = -1;
    enum hlthunk_device_name device_type;
    
    /* Allocate MD object */
    md = ucs_calloc(1, sizeof(*md), "gaudi_md");
    if (md == NULL) {
        ucs_error("Failed to allocate memory for Gaudi MD");
        return UCS_ERR_NO_MEMORY;
    }

    /* Try to find and open a Gaudi device */
    device_count = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    if (device_count <= 0) {
        ucs_debug("No Habana devices found");
        status = UCS_ERR_NO_DEVICE;
        goto err_free_md;
    }
    
    /* Open a Gaudi device */
    for (i = 0; i < device_count && i < HLTHUNK_MAX_MINOR; i++) {
        fd = hlthunk_open_control(i, NULL);
        if (fd < 0) {
            continue;
        }
        
        device_type = hlthunk_get_device_name_from_fd(fd);
        if ((device_type == HLTHUNK_DEVICE_GAUDI) || 
            (device_type == HLTHUNK_DEVICE_GAUDI2)) {
            hlthunk_close(fd);
            fd = hlthunk_open(device_type, NULL);
            if (fd >= 0) {
                break;
            }
        }
        
        hlthunk_close(fd);
        fd = -1;
    }
    
    if (fd < 0) {
        ucs_debug("Failed to open Gaudi device");
        status = UCS_ERR_NO_DEVICE;
        goto err_free_md;
    }
    
    /* Store device file descriptor */
    md->fd = fd;
    
    /* Set MD operations */
    md->super.ops = &md->ops;
    
    md->ops.close               = uct_gaudi_md_close;
    md->ops.query               = uct_gaudi_md_query;
    md->ops.mem_alloc           = uct_gaudi_mem_alloc;
    md->ops.mem_free            = uct_gaudi_mem_free;
    md->ops.mem_reg             = uct_gaudi_mem_reg;
    md->ops.mem_dereg           = uct_gaudi_mem_dereg;
    md->ops.mkey_pack           = uct_gaudi_mkey_pack;
    md->ops.detect_memory_type  = ucs_empty_function_return_unsupported;
    md->ops.is_sockaddr_accessible = ucs_empty_function_return_zero_int;
    md->ops.is_mem_type_owned   = ucs_empty_function_return_zero;
    
    /* Set capabilities */
    md->super.cap.flags         = UCT_MD_FLAG_REG;
    md->super.cap.reg_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md->super.cap.access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md->super.cap.max_alloc     = 0;
    md->super.cap.max_reg       = UINT64_MAX;
    
    *md_p = &md->super;
    return UCS_OK;
    
err_free_md:
    ucs_free(md);
    return status;
}

uct_component_t uct_gaudi_md_component = {
    .query_md_resources = uct_gaudi_query_md_resources,
    .md_open            = uct_gaudi_md_open,
    .cm_open            = (uct_component_cm_open_func_t)ucs_empty_function_return_unsupported,
    .rkey_unpack        = (uct_component_rkey_unpack_func_t)ucs_empty_function_return_unsupported,
    .rkey_ptr           = (uct_component_rkey_ptr_func_t)ucs_empty_function_return_unsupported,
    .rkey_release       = (uct_component_rkey_release_func_t)ucs_empty_function_return_unsupported,
    .rkey_compare       = uct_base_rkey_compare,
    .name               = "gaudi",
    .md_config          = {
        .name   = "Gaudi memory domain",
        .prefix = "GAUDI_",
        .table  = NULL, // No specific config table for now
        .size   = 0
    },
    .flags              = 0,
    .md_vfs_init        = (uct_component_md_vfs_init_func_t)ucs_empty_function_return_unsupported
};

UCT_COMPONENT_REGISTER(&uct_gaudi_md_component)



