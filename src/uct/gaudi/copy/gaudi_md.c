#include <ucs/debug/memtrack.h>
#include <ucs/sys/sys.h>
#include <ucs/sys/string.h>
#include <ucs/debug/log.h>
#include <ucs/type/class.h>
#include <ucs/sys/math.h>
#include "gaudi_md.h"
#include "gaudi_iface.h"
#include <hlthunk.h>
#include <uct/base/uct_md.h>
#include <uct/api/v2/uct_v2.h>
/* Forward declaration*/
static uct_component_t uct_gaudi_component;

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

    {"ENABLE_DMABUF", "try",
     "Enable cross-device dmabuf file descriptor support",
     ucs_offsetof(uct_gaudi_md_config_t, enable_dmabuf),
     UCS_CONFIG_TYPE_TERNARY},

    {"REG_COST", "16us",
     "Memory registration cost",
     ucs_offsetof(uct_gaudi_md_config_t, uc_reg_cost.m),
     UCS_CONFIG_TYPE_TIME},

    {"REG_GROWTH", "0.06ns",
     "Memory registration growth rate",
     ucs_offsetof(uct_gaudi_md_config_t, uc_reg_cost.c),
     UCS_CONFIG_TYPE_TIME},

    {NULL}
};

void uct_gaudi_md_close(uct_md_h  uct_md)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    
    if (md->hlthunk_fd >= 0) {
        hlthunk_close(md->hlthunk_fd);
    }
    
    ucs_free(md);
}

ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    
    md_attr->flags = UCT_MD_FLAG_REG | 
                    UCT_MD_FLAG_ALLOC |
                    UCT_MD_FLAG_NEED_RKEY;
    
    md_attr->reg_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                            UCS_BIT(UCS_MEMORY_TYPE_UNKNOWN); /* Gaudi memory type */
    md_attr->reg_nonblock_mem_types = 0;
    md_attr->alloc_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                              UCS_BIT(UCS_MEMORY_TYPE_UNKNOWN);
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                               UCS_BIT(UCS_MEMORY_TYPE_UNKNOWN);
    md_attr->detect_mem_types = 0;
    md_attr->dmabuf_mem_types = gaudi_md->config.dmabuf_supported ? 
                               UCS_BIT(UCS_MEMORY_TYPE_UNKNOWN) : 0;
    md_attr->max_alloc = UINT64_MAX;
    md_attr->max_reg = UINT64_MAX;
    md_attr->rkey_packed_size = sizeof(uct_gaudi_key_t);
    
    memset(&md_attr->local_cpus, 0xff, sizeof(md_attr->local_cpus));
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_reg(uct_md_h md, void *address, size_t length,
                                         const uct_md_mem_reg_params_t *params,
                                         uct_mem_h *memh_p)
{
    //uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh;
    
    gaudi_memh = ucs_malloc(sizeof(*gaudi_memh), "gaudi_memh");
    if (gaudi_memh == NULL) {
        return UCS_ERR_NO_MEMORY;
    }
    
    /* For Gaudi, we need to check if this is already device memory or host memory */
    /* If it's host memory, we might need to map it for device access */
    /* For now, we'll store the address as-is */
    
    /* Note: The actual Gaudi memory registration would depend on:
     * 1. Whether this is host memory that needs to be made accessible to Gaudi
     * 2. Whether this is already Gaudi device memory
     * 3. The specific Gaudi APIs for memory mapping
     * 
     * Since hl-thunk doesn't provide a direct equivalent to the struct I used,
     * we'd need to use the actual hl-thunk memory mapping APIs such as:
     * - hlthunk_memory_map() for mapping host memory to device
     * - hlthunk_get_exported_dma_buf_fd() for getting DMA-BUF fd
     */
    
    gaudi_memh->vaddr = address;
    gaudi_memh->size = length;
    gaudi_memh->dev_addr = (uint64_t)address; /* Simplified - would need proper mapping */
    gaudi_memh->dmabuf_fd = -1; /* Would be set if using DMA-BUF export */
    
    *memh_p = gaudi_memh;
    
    ucs_trace("Registered memory %p length %zu",
              address, length);
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_dereg(uct_md_h md, 
                                           const uct_md_mem_dereg_params_t *params)
{
    //uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh = params->memh;
    
    UCT_MD_MEM_DEREG_CHECK_PARAMS(params, 0);
    
    /* If we have a DMA-BUF fd, close it */
    if (gaudi_memh->dmabuf_fd >= 0) {
        close(gaudi_memh->dmabuf_fd);
    }
    
    /* Note: Actual unmapping would depend on how the memory was mapped */
    
    ucs_free(gaudi_memh);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_alloc(uct_md_h md, size_t *length_p,
                                           void **address_p, ucs_memory_type_t mem_type,
                                           unsigned flags, const char *alloc_name, 
                                           uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh;
    uint64_t handle;
    uint64_t addr;
    
    /* Allocate device memory through hl-thunk */
    handle = hlthunk_device_memory_alloc(gaudi_md->hlthunk_fd, *length_p, 
                                      0, true, true);
    if (handle == 0) {
        ucs_debug("Failed to allocate device memory size %zu", *length_p);
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Map to host address space */
    addr = hlthunk_device_memory_map(gaudi_md->hlthunk_fd, handle, 0);
    if (addr == 0) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, handle);
        //hlthunk_close(gaudi_md->hlthunk_fd);
        ucs_error("Failed to map device memory handle 0x%lx", handle);
        return UCS_ERR_NO_MEMORY;
    }
    
    gaudi_memh = ucs_calloc(1, sizeof(*gaudi_memh), "gaudi_memh");
    if (gaudi_memh == NULL) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, handle);
        ucs_error("Failed to allocate Gaudi memory handle on host");
        return UCS_ERR_NO_MEMORY;
    }
    
    gaudi_memh->vaddr = (void *) addr;
    gaudi_memh->size = *length_p;
    gaudi_memh->handle = handle;
    gaudi_memh->dev_addr = (uint64_t)handle; /* device handle as address */
    
    *address_p = (void *) addr;
    *memh_p = gaudi_memh;
    
    ucs_trace("Allocated Gaudi memory %p size %zu handle 0x%lx",
              (void *) addr, *length_p, handle);
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh = memh;
    
    if (gaudi_memh->vaddr != NULL || gaudi_memh->handle != 0) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, gaudi_memh->handle);
    }
    
    ucs_free(gaudi_memh);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_mkey_pack(uct_md_h md, uct_mem_h memh, 
                                       void *address, size_t length,
                                       const uct_md_mkey_pack_params_t *params,
                                       void *mkey_buffer)
{
    uct_gaudi_mem_t *gaudi_memh = memh;
    uct_gaudi_key_t *packed = mkey_buffer;
    
    packed->vaddr = (uintptr_t)gaudi_memh->vaddr;
    packed->dev_addr = gaudi_memh->dev_addr;
    packed->size = gaudi_memh->size;
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_rkey_unpack(uct_component_t *component,
                                         const void *rkey_buffer,
                                         uct_rkey_t *rkey_p, void **handle_p)
{
    uct_gaudi_key_t *packed = (uct_gaudi_key_t *)rkey_buffer;
    uct_gaudi_key_t *key;
    
    key = ucs_malloc(sizeof(*key), "gaudi_rkey");
    if (key == NULL) {
        return UCS_ERR_NO_MEMORY;
    }
    
    *key = *packed;
    *handle_p = NULL;
    *rkey_p = (uintptr_t)key;
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_rkey_release(uct_component_t *component,
                                          uct_rkey_t rkey, void *handle)
{
    ucs_free((void *)rkey);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_detect_memory_type(uct_md_h md, const void *addr,
                                                   size_t length,
                                                   ucs_memory_type_t *mem_type_p)
{
    /* Simple detection - could be enhanced with actual Gaudi memory detection */
    *mem_type_p = UCS_MEMORY_TYPE_HOST;
    return UCS_OK;
}

static uct_md_ops_t uct_gaudi_md_ops = {
    .close              = uct_gaudi_md_close,
    .query              = uct_gaudi_md_query,
    .mem_alloc          = uct_gaudi_md_mem_alloc,
    .mem_free           = uct_gaudi_md_mem_free,
    .mkey_pack          = uct_gaudi_mkey_pack,
    .mem_reg            = uct_gaudi_md_mem_reg,
    .mem_dereg          = uct_gaudi_md_mem_dereg,
    .mem_attach         = ucs_empty_function_return_unsupported,
    .detect_memory_type = uct_gaudi_md_detect_memory_type
};

ucs_status_t uct_gaudi_query_md_resources(uct_component_t *component,
                                         uct_md_resource_desc_t **resources_p,
                                         unsigned *num_resources_p)
{
    uct_md_resource_desc_t *resources;
    int fd;
    int i;
    
    /* Check if Gaudi device is available */
    for (i = 0; i < 4; i++) {
        fd = hlthunk_open(devices[i], NULL);
        if (fd >= 0) {
            hlthunk_close(fd);
            break;
        }
    }
    
    if (fd < 0) {
        return uct_md_query_empty_md_resource(resources_p, num_resources_p);
    }
    
    resources = ucs_malloc(sizeof(*resources), "gaudi md resource");
    if (resources == NULL) {
        return UCS_ERR_NO_MEMORY;
    }
    
    ucs_snprintf_zero(resources->md_name, sizeof(resources->md_name), 
                      "%s", "gaudi");
    
    *resources_p = resources;
    *num_resources_p = 1;
    
    return UCS_OK;
}

ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                              const char *md_name,
                              const uct_md_config_t *config,
                              uct_md_h *md_p)
{
    const uct_gaudi_md_config_t *md_config = 
        ucs_derived_of(config, uct_gaudi_md_config_t);
    uct_gaudi_md_t *md;
    int i;
    int ret;
    
    md = ucs_calloc(1, sizeof(*md), "uct_gaudi_md");
    if (!md) {
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Try to open Gaudi device */
    md->hlthunk_fd = -1;
    for (i = 0; i < 4; i++) {
        md->hlthunk_fd = hlthunk_open(devices[i], NULL);
        if (md->hlthunk_fd >= 0) {
            md->device_type = devices[i];
            break;
        }
    }
    
    if (md->hlthunk_fd < 0) {
        ucs_error("Failed to open Gaudi device");
        ucs_free(md);
        return UCS_ERR_NO_DEVICE;
    }
    
    /* Get device info */
    ret = hlthunk_get_info(md->hlthunk_fd, &md->device_info);
    if (ret != 0) {
        ucs_error("Failed to get Gaudi device info");
        hlthunk_close(md->hlthunk_fd);
        ucs_free(md);
        return UCS_ERR_NO_DEVICE;
    }
    
    md->super.ops = &uct_gaudi_md_ops;
    md->super.component = &uct_gaudi_component;
    
    /* Copy configuration */
    //md->config.uc_reg_cost = md_config->uc_reg_cost;
    md->config.dmabuf_supported = (md_config->enable_dmabuf != UCS_NO);
    
    *md_p = &md->super;
    
    ucs_debug("Opened Gaudi MD device_type=%d", md->device_type);
    
    return UCS_OK;
}

static uct_component_t uct_gaudi_component = {
    .query_md_resources = uct_gaudi_query_md_resources,
    .md_open            = uct_gaudi_md_open,
    .cm_open            = ucs_empty_function_return_unsupported,
    .rkey_unpack        = uct_gaudi_rkey_unpack,
    .rkey_ptr           = ucs_empty_function_return_unsupported,
    .rkey_release       = uct_gaudi_rkey_release,
    .rkey_compare       = uct_base_rkey_compare,
    .name               = "gaudi",
    .md_config          = {
        .name           = "Gaudi memory domain",
        .prefix         = "GAUDI_",
        .table          = uct_gaudi_md_config_table,
        .size           = sizeof(uct_gaudi_md_config_t),
    },
    .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
    .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_component),
    .flags              = 0,
    .md_vfs_init        = (uct_component_md_vfs_init_func_t)ucs_empty_function
};

UCT_COMPONENT_REGISTER(&uct_gaudi_component)