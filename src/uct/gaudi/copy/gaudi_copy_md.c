#include "gaudi_copy_md.h"
extern ucs_config_field_t uct_gaudi_md_config_table[];
#include <ucs/sys/math.h>
#include <ucs/sys/math.h>
#include <ucs/sys/string.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/log.h>
#include <ucs/type/class.h>
#include <hlthunk.h>

#define SZ_128				0x00000080
#define SZ_1K				0x00000400
#define SZ_2K				0x00000800
#define SZ_4K				0x00001000
#define SZ_8K				0x00002000
#define SZ_16K				0x00004000
#define SZ_32K				0x00008000
#define SZ_64K				0x00010000
#define NOT_CONTIGUOUS      0

 ucs_status_t uct_gaudi_mem_reg_internal(uct_md_h uct_md, void *address, size_t length,
                                             int pg_align_addr, uct_gaudi_mem_t *mem_hndl);

 void uct_gaudi_md_close(uct_md_h md)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    
    if (gaudi_md->fd >= 0) {
        hlthunk_close(gaudi_md->fd);
    }
    ucs_free(gaudi_md);
}


 ucs_status_t uct_gaudi_md_query(uct_md_h uct_md, uct_md_attr_v2_t *md_attr)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);

    md_attr->flags            = UCT_MD_FLAG_REG | UCT_MD_FLAG_NEED_RKEY;
    md_attr->reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_HOST) | 
                                    UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) | 
                                    UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->detect_mem_types = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->max_alloc        = 0;
    md_attr->max_reg          = UINT64_MAX;
    md_attr->rkey_packed_size     = sizeof(uct_gaudi_key_t);
    //md_attr->reg_cost.overhead    = 0;
    //md_attr->reg_cost.growth      = 0;
    
    // Support DMA buf if the driver supports it
    if (md->have_dmabuf) {
        md_attr->dmabuf_mem_types = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    }
    
    return UCS_OK;
}

 ucs_status_t
uct_gaudi_mem_reg_internal(uct_md_h uct_md, void *address, size_t length,
                         int pg_align_addr, uct_gaudi_mem_t *mem_hndl)
{
    void *dev_addr = NULL;
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    uint64_t status;

    if (pg_align_addr) {
        uct_gaudi_pg_align_addr(&address, &length);
    }

    // For host memory, use hlthunk to pin/register it with the Gaudi device
    status = hlthunk_host_memory_map(md->fd, dev_addr, 0, length);
    if (status != 0) { // Assuming 0 is success for hlthunk API
        ucs_error("Failed to register memory with Gaudi: error %ld", status);
        return UCS_ERR_IO_ERROR;
    }

    mem_hndl->vaddr    = address;
    mem_hndl->dev_ptr  = dev_addr;
    mem_hndl->reg_size = length;

    ucs_trace("Registered addr %p len %zu dev addr %p", address, length, dev_addr);
    return UCS_OK;
}

 ucs_status_t
uct_gaudi_mem_reg(uct_md_h md, void *address, size_t length,
                const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p)
{
    uct_gaudi_mem_t *mem_hndl = NULL;
    ucs_status_t status;

    mem_hndl = ucs_malloc(sizeof(uct_gaudi_mem_t), "gaudi handle");
    if (NULL == mem_hndl) {
        ucs_error("Failed to allocate memory for gaudi_mem_t");
        return UCS_ERR_NO_MEMORY;
    }

    status = uct_gaudi_mem_reg_internal(md, address, length, 1, mem_hndl);
    if (status != UCS_OK) {
        ucs_free(mem_hndl);
        return status;
    }

    *memh_p = mem_hndl;
    return UCS_OK;
}

 ucs_status_t uct_gaudi_mem_dereg(uct_md_h md, const uct_md_mem_dereg_params_t *params)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *mem_hndl = params->memh;
    int status;

    status = hlthunk_memory_unmap(gaudi_md->fd, (uint64_t) mem_hndl->dev_ptr);
    if (status != 0) {
        ucs_error("Failed to unregister memory from Gaudi: error %d", status);
        return UCS_ERR_IO_ERROR;
    }

    ucs_free(mem_hndl);
    return UCS_OK;
}

 ucs_status_t
uct_gaudi_mem_alloc(uct_md_h md, size_t *length_p, void **address_p,
                  ucs_memory_type_t mem_type, ucs_sys_device_t sys_dev,
                  unsigned flags, const char *alloc_name,
                  uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *mem_hndl;
    void *dev_addr = NULL;
    uint64_t device_mem_handler;
    uint64_t size = *length_p;
    
    if (mem_type != UCS_MEMORY_TYPE_GAUDI) {
        return UCS_ERR_UNSUPPORTED;
    }

    // Allocate memory handle
    mem_hndl = ucs_malloc(sizeof(uct_gaudi_mem_t), "gaudi_alloc_handle");
    if (mem_hndl == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    // Use Gaudi's API to allocate device memory
    // Note: Adapting to hlthunk API - actual implementation depends on hlthunk version
    device_mem_handler = hlthunk_device_memory_alloc(gaudi_md->fd, size, SZ_4K, NOT_CONTIGUOUS, false);

    if (device_mem_handler == 0) {
        ucs_debug("Failed to allocate Gaudi device memory");
        ucs_free(mem_hndl);
        return UCS_ERR_NO_MEMORY;
    }


    dev_addr = (void *)hlthunk_device_memory_map(gaudi_md->fd, device_mem_handler, 0);
    if (dev_addr == NULL) {
        ucs_debug("Failed to map Gaudi device memory");
        ucs_free(mem_hndl);
        return UCS_ERR_NO_MEMORY;
    }


    mem_hndl->vaddr = NULL;  // No host mapping for device memory
    mem_hndl->dev_ptr = dev_addr;
    mem_hndl->reg_size = size;

    *address_p = dev_addr;
    *memh_p = mem_hndl;
    
    return UCS_OK;
}

 ucs_status_t uct_gaudi_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *mem_hndl = memh;
    int status;

    status = hlthunk_device_memory_free(gaudi_md->fd, (uint64_t) mem_hndl->dev_ptr);
    if (status != 0) {
        ucs_error("Failed to free Gaudi device memory: error %d", status);
        return UCS_ERR_IO_ERROR;
    }

    ucs_free(mem_hndl);
    return UCS_OK;
}

 ucs_status_t
uct_gaudi_mkey_pack(uct_md_h uct_md, uct_mem_h memh, void *address,
                  size_t length, const uct_md_mkey_pack_params_t *params,
                  void *mkey_buffer)
{
    uct_gaudi_key_t *packed = mkey_buffer;
    uct_gaudi_mem_t *mem_hndl = memh;

    packed->vaddr = (uint64_t)mem_hndl->vaddr;
    packed->dev_ptr = mem_hndl->dev_ptr;

    return UCS_OK;
}

/*
 ucs_status_t
uct_gaudi_rcache_mem_reg_cb(void *context, ucs_rcache_t *rcache,
                          void *arg, ucs_rcache_region_t *rregion,
                          uint16_t rcache_mem_reg_flags)
{
    uct_gaudi_md_t *md = context;
    uct_gaudi_rcache_region_t *region;

    region = ucs_derived_of(rregion, uct_gaudi_rcache_region_t);
    return uct_gaudi_mem_reg_internal(&md->super, (void*)region->super.super.start,
                                    region->super.super.end -
                                    region->super.super.start,
                                    0, &region->memh);
}
                                    */

ucs_status_t uct_gaudi_query_md_resources(uct_component_t *component,
                                        uct_md_resource_desc_t **resources_p,
                                        unsigned *num_resources_p)
{
    uct_md_resource_desc_t *resources;
    int device_count;
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

 ucs_status_t uct_gaudi_rkey_unpack(uct_component_t *component, const void *rkey_buffer,
                                   const uct_rkey_unpack_params_t *params,
                                   uct_rkey_t *rkey_p, void **handle_p)
{
    uct_gaudi_key_t *packed = (uct_gaudi_key_t *)rkey_buffer;
    uct_gaudi_key_t *key = ucs_malloc(sizeof(uct_gaudi_key_t), "gaudi_rkey");
    if (key == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    *key = *packed;
    *handle_p = key;
    *rkey_p = (uintptr_t)key;

    return UCS_OK;
}

/* Define uct_gaudi_rkey_release */
ucs_status_t uct_gaudi_rkey_release(uct_component_t *component, uct_rkey_t rkey, void *handle)
{
    ucs_free(handle);
    return UCS_OK;
}


ucs_status_t uct_gaudi_md_open(uct_component_t *component, const char *md_name,
                            const uct_md_config_t *config, uct_md_h *md_p)
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
    md->ops.detect_memory_type  = NULL;
    md->have_dmabuf             = 0;  /* Set based on device capabilities */
    
    *md_p = &md->super;
    return UCS_OK;
    
err_free_md:
    ucs_free(md);
    return status;
}

uct_component_t uct_gaudi_component = {
    .query_md_resources = uct_gaudi_query_md_resources,
    .md_open            = uct_gaudi_md_open,
    .cm_open            = NULL,
    .rkey_unpack        = uct_gaudi_rkey_unpack,
    .rkey_ptr           = NULL,
    .rkey_release       = uct_gaudi_rkey_release,
    .rkey_compare       = uct_base_rkey_compare,
    .name               = "gaudi",
    .md_config          = {
        .name           = "Habana Gaudi memory domain",
        .prefix         = "GAUDI_",
        .table          = uct_gaudi_md_config_table,
        .size           = sizeof(uct_gaudi_md_config_t),
    },
    .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
    .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_component),
    .flags              = 0,
    .md_vfs_init        = NULL
};
UCT_COMPONENT_REGISTER(&uct_gaudi_component);
