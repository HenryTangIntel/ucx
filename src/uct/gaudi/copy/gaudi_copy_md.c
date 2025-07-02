#include "gaudi_copy_md.h"
#include "../base/gaudi_iface.h"
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ucs/debug/log.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/memory/memtype_cache.h>
#include <ucs/profile/profile.h>
#include <ucs/type/class.h>
#include <ucs/sys/math.h>
#include <uct/api/v2/uct_v2.h>

/* Conditional include for Habana Labs driver */
#ifdef HAVE_HLTHUNK_H
#include <hlthunk.h>
#endif

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64
#define UCT_GAUDI_MAX_DEVICES      32

uct_component_t uct_gaudi_copy_component;

#ifdef HAVE_HLTHUNK_H
enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2,
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
};
#endif

static const char *uct_gaudi_pref_loc[] = {
    [UCT_GAUDI_PREF_LOC_CPU]  = "cpu",
    [UCT_GAUDI_PREF_LOC_HPU]  = "gaudi",
    [UCT_GAUDI_PREF_LOC_LAST] = NULL,
};

static ucs_config_field_t uct_gaudi_copy_md_config_table[] = {
    {"", "", NULL,
        ucs_offsetof(uct_gaudi_copy_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {"REG_WHOLE_ALLOC", "auto",
     "Allow registration of whole allocation\n"
     " auto - Let runtime decide where whole allocation registration is turned on.\n"
     "        By default this will be turned off for limited BAR GPUs (eg. T4)\n"
     " on   - Whole allocation registration is always turned on.\n"
     " off  - Whole allocation registration is always turned off.",
     ucs_offsetof(uct_gaudi_copy_md_config_t, alloc_whole_reg),
     UCS_CONFIG_TYPE_ON_OFF_AUTO},

    {"MAX_REG_RATIO", "0.1",
     "If the ratio of the length of the allocation to which the user buffer belongs to"
     " to the total GPU memory capacity is below this ratio, then the whole allocation"
     " is registered. Otherwise only the user specified region is registered.",
     ucs_offsetof(uct_gaudi_copy_md_config_t, max_reg_ratio), UCS_CONFIG_TYPE_DOUBLE},

    {"DMABUF", "try",
     "Enable using cross-device dmabuf file descriptor",
     ucs_offsetof(uct_gaudi_copy_md_config_t, enable_dmabuf),
                  UCS_CONFIG_TYPE_TERNARY},

    {"PREF_LOC", "cpu",
     "System device designation of a gaudi managed memory buffer"
     " whose preferred location attribute is not set.\n"
     " cpu - Assume buffer is on the CPU.\n"
     " gaudi - Assume buffer is on the HPU corresponding to buffer's HPU context.",
     ucs_offsetof(uct_gaudi_copy_md_config_t, pref_loc),
     UCS_CONFIG_TYPE_ENUM(uct_gaudi_pref_loc)},

    {NULL}
};

/* Forward declarations */
static ucs_status_t uct_gaudi_copy_md_mem_query(uct_md_h uct_md, const void *address,
                                               size_t length,
                                               uct_md_mem_attr_t *mem_attr_p);

static ucs_status_t uct_gaudi_copy_mem_reg_internal(
        uct_md_h uct_md, void *address, size_t length,
        int export_dmabuf, uct_gaudi_mem_t *mem_hndl);

#ifdef HAVE_HLTHUNK_H
static int uct_gaudi_export_dmabuf(uct_gaudi_md_t *gaudi_md, 
                                   uct_gaudi_mem_t *gaudi_memh);
static ucs_status_t uct_gaudi_import_dmabuf(uct_gaudi_md_t *gaudi_md,
                                           int dmabuf_fd, size_t offset,
                                           size_t size, uct_gaudi_mem_t *gaudi_memh);
#endif

ucs_status_t uct_gaudi_copy_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    
    md_attr->flags = UCT_MD_FLAG_REG | 
                    UCT_MD_FLAG_ALLOC |
                    UCT_MD_FLAG_NEED_RKEY;
                    
    if (gaudi_md->config.dmabuf_supported) {
        md_attr->flags |= UCT_MD_FLAG_REG_DMABUF;
    }
    
    md_attr->reg_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                            UCS_BIT(UCS_MEMORY_TYPE_GAUDI); /* Gaudi device memory */
    md_attr->reg_nonblock_mem_types = 0;
    md_attr->alloc_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                              UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                               UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->detect_mem_types = 0;
    md_attr->dmabuf_mem_types = gaudi_md->config.dmabuf_supported ? 
                               UCS_BIT(UCS_MEMORY_TYPE_GAUDI) : 0;
    md_attr->max_alloc = UINT64_MAX;
    md_attr->max_reg = UINT64_MAX;
    md_attr->rkey_packed_size = sizeof(uct_gaudi_key_t);
    
    memset(&md_attr->local_cpus, 0xff, sizeof(md_attr->local_cpus));
    
    return UCS_OK;
}

static ucs_status_t
uct_gaudi_copy_mkey_pack(uct_md_h md, uct_mem_h memh, void *address,
                        size_t length, const uct_md_mkey_pack_params_t *params,
                        void *mkey_buffer)
{
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_mem_alloc(uct_md_h md, size_t *length_p,
                                           void **address_p, ucs_memory_type_t mem_type,
                                           unsigned flags, const char *alloc_name, 
                                           uct_mem_h *memh_p)
{
    uct_gaudi_mem_t *gaudi_memh;
    
#ifdef HAVE_HLTHUNK_H
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
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
        ucs_error("Failed to map device memory handle 0x%lx", handle);
        return UCS_ERR_NO_MEMORY;
    }
    
    gaudi_memh = ucs_calloc(1, sizeof(*gaudi_memh), "gaudi_memh");
    if (gaudi_memh == NULL) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, handle);
        ucs_error("Failed to allocate Gaudi memory handle on host");
        return UCS_ERR_NO_MEMORY;
    }
     gaudi_memh->vaddr = (void *)addr;
    gaudi_memh->size = *length_p;
    gaudi_memh->handle = handle;
    gaudi_memh->dev_addr = addr;
    gaudi_memh->dmabuf_fd = -1;

    /* Optionally export as DMA-BUF if flags indicate it */
    if (flags & UCT_MD_MEM_FLAG_FIXED) {
        /* Export device memory as DMA-BUF for sharing */
        int dmabuf_fd = uct_gaudi_export_dmabuf(gaudi_md, gaudi_memh);
        if (dmabuf_fd >= 0) {
            gaudi_memh->dmabuf_fd = dmabuf_fd;
            ucs_debug("Exported allocated memory as DMA-BUF fd %d", dmabuf_fd);
        } else {
            ucs_warn("Failed to export allocated memory as DMA-BUF");
        }
    }

    *address_p = (void *)addr;
    *memh_p = gaudi_memh;
    
    return UCS_OK;
#else
    /* Without hlthunk, use regular host memory allocation */
    (void)md; /* Suppress unused parameter warning */
    
    gaudi_memh = ucs_calloc(1, sizeof(*gaudi_memh), "gaudi_memh");
    if (gaudi_memh == NULL) {
        return UCS_ERR_NO_MEMORY;
    }
    
    gaudi_memh->vaddr = ucs_malloc(*length_p, "gaudi_mem");
    if (gaudi_memh->vaddr == NULL) {
        ucs_free(gaudi_memh);
        return UCS_ERR_NO_MEMORY;
    }
    
    gaudi_memh->size = *length_p;
    gaudi_memh->handle = 0;
    gaudi_memh->dev_addr = (uint64_t)gaudi_memh->vaddr;
    gaudi_memh->dmabuf_fd = -1;
    
    *address_p = gaudi_memh->vaddr;
    *memh_p = gaudi_memh;
    
    return UCS_OK;
#endif
}

static ucs_status_t uct_gaudi_copy_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_mem_t *gaudi_memh = memh;
#ifdef HAVE_HLTHUNK_H
    uct_gaudi_md_t *gaudi_md;
#endif
    
    /* Close DMA-BUF file descriptor if it was created */
    if (gaudi_memh->dmabuf_fd >= 0) {
        close(gaudi_memh->dmabuf_fd);
        ucs_debug("Closed DMA-BUF fd %d during memory free", 
                  gaudi_memh->dmabuf_fd);
    }
    
#ifdef HAVE_HLTHUNK_H
    gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    if (gaudi_md->hlthunk_fd >= 0 && gaudi_memh->handle != 0) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, gaudi_memh->handle);
    }
#else
    /* Without hlthunk, just free the regular memory */
    (void)md; /* Suppress unused parameter warning */
    if (gaudi_memh->vaddr != NULL) {
        ucs_free(gaudi_memh->vaddr);
    }
#endif
    
    ucs_free(gaudi_memh);
    return UCS_OK;
}

void uct_gaudi_copy_md_close(uct_md_h  uct_md)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    
    if (md->hlthunk_fd >= 0) {
#ifdef HAVE_HLTHUNK_H
        hlthunk_close(md->hlthunk_fd);
#else
        close(md->hlthunk_fd);
#endif
    }
    
    ucs_free(md);
}

static ucs_status_t uct_gaudi_copy_mem_reg_internal(
        uct_md_h uct_md, void *address, size_t length,
        int export_dmabuf, uct_gaudi_mem_t *mem_hndl)
{
    void *dev_addr = NULL;
#ifdef HAVE_HLTHUNK_H
    uct_gaudi_md_t *gaudi_md;
#endif

    ucs_assert((address != NULL) && (length != 0));

    mem_hndl->vaddr    = address;
    mem_hndl->dev_addr = (uint64_t)dev_addr;
    mem_hndl->size     = length;
    mem_hndl->handle   = 0;
    mem_hndl->dmabuf_fd = -1;

#ifdef HAVE_HLTHUNK_H
    gaudi_md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    
    /* Try to export memory as DMA-BUF if requested and supported */
    if (export_dmabuf && gaudi_md->config.dmabuf_supported && 
        gaudi_md->hlthunk_fd >= 0) {
        
        ucs_trace("Attempting DMA-BUF export for address %p, length %zu", 
                  address, length);
        
        /* Export the registered memory as DMA-BUF */
        mem_hndl->dmabuf_fd = uct_gaudi_export_dmabuf(gaudi_md, mem_hndl);
        if (mem_hndl->dmabuf_fd >= 0) {
            ucs_debug("Exported registered memory as DMA-BUF fd %d for addr %p", 
                      mem_hndl->dmabuf_fd, address);
        } else {
            ucs_debug("Failed to export registered memory as DMA-BUF for addr %p", 
                      address);
        }
    }
#else
    /* Suppress unused parameter warning when hlthunk is not available */
    (void)uct_md;
    (void)export_dmabuf;
#endif

    ucs_trace("Registered addr %p len %zu dev addr %p dmabuf_fd %d", 
              address, length, dev_addr, mem_hndl->dmabuf_fd);
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_mem_reg,
                 (md, address, length, params, memh_p),
                 uct_md_h md, void *address, size_t length,
                 const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh = NULL;
    ucs_status_t status;
    int export_dmabuf = 0;

    /* Check if DMA-BUF export is requested via parameters */
    if (params != NULL) {
        /* Check for DMA-BUF related parameters in the future API */
        export_dmabuf = gaudi_md->config.dmabuf_supported;
    }

    gaudi_memh = ucs_malloc(sizeof(uct_gaudi_mem_t), "gaudi_mem_handle");
    if (NULL == gaudi_memh) {
        ucs_error("failed to allocate memory for gaudi_mem_t");
        return UCS_ERR_NO_MEMORY;
    }

    status = uct_gaudi_copy_mem_reg_internal(md, address, length, 
                                           export_dmabuf, gaudi_memh);
    if (status != UCS_OK) {
        ucs_free(gaudi_memh);
        return status;
    }

    /* Additional DMA-BUF functionality can be added here */
    if (gaudi_memh->dmabuf_fd >= 0) {
        ucs_debug("Memory registration created DMA-BUF fd %d", 
                  gaudi_memh->dmabuf_fd);
    }

    *memh_p = gaudi_memh;
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_mem_dereg,
                 (md, params),
                 uct_md_h md, const uct_md_mem_dereg_params_t *params)
{
    uct_gaudi_mem_t *gaudi_memh;
    
    if (params == NULL || params->memh == NULL) {
        return UCS_ERR_INVALID_PARAM;
    }
    
    gaudi_memh = (uct_gaudi_mem_t *)params->memh;
    
    /* Close DMA-BUF file descriptor if it was created */
    if (gaudi_memh->dmabuf_fd >= 0) {
        close(gaudi_memh->dmabuf_fd);
        ucs_debug("Closed DMA-BUF fd %d for address %p", 
                  gaudi_memh->dmabuf_fd, gaudi_memh->vaddr);
    }
    
    ucs_trace("Deregistering addr %p len %zu", 
              gaudi_memh->vaddr, gaudi_memh->size);
    
    /* Free the memory handle */
    ucs_free(gaudi_memh);
    
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_md_detect_memory_type,
                 (md, address, length, mem_type_p), uct_md_h md,
                 const void *address, size_t length,
                 ucs_memory_type_t *mem_type_p)
{
    return UCS_OK;
}

#ifdef HAVE_HLTHUNK_H

static int uct_gaudi_export_dmabuf(uct_gaudi_md_t *gaudi_md, 
                                   uct_gaudi_mem_t *gaudi_memh)
{
    int dmabuf_fd = -1;
    
    if (gaudi_md->hlthunk_fd < 0 || gaudi_memh->handle == 0) {
        ucs_debug("Cannot export DMA-BUF: invalid device or memory handle");
        return -1;
    }
    
    /* 
     * Use hlthunk API to export device memory as DMA-BUF
     * This creates a file descriptor that can be shared with other devices like MLX NICs
     */
    dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
        gaudi_md->hlthunk_fd, gaudi_memh->dev_addr, gaudi_memh->size, 
        0, (O_RDWR | O_CLOEXEC));
    
    if (dmabuf_fd < 0) {
        ucs_debug("hlthunk_device_mapped_memory_export_dmabuf_fd failed for "
                  "handle 0x%lx, addr 0x%lx, size %zu: %m", 
                  gaudi_memh->handle, gaudi_memh->dev_addr, gaudi_memh->size);
        return -1;
    }
    
    ucs_debug("Exported Gaudi memory as DMA-BUF: handle=0x%lx addr=0x%lx "
              "size=%zu fd=%d", gaudi_memh->handle, gaudi_memh->dev_addr, 
              gaudi_memh->size, dmabuf_fd);
    
    return dmabuf_fd;
}

static ucs_status_t __attribute__((unused)) uct_gaudi_import_dmabuf(uct_gaudi_md_t *gaudi_md,
                                           int dmabuf_fd, size_t offset,
                                           size_t size, uct_gaudi_mem_t *gaudi_memh)
{
    uint64_t handle = 0;
    uint64_t dev_addr = 0;
    
    if (gaudi_md->hlthunk_fd < 0 || dmabuf_fd < 0) {
        return UCS_ERR_INVALID_PARAM;
    }
    
    /*
     * Placeholder for actual hlthunk DMA-BUF import implementation
     */
    
    gaudi_memh->handle = handle;
    gaudi_memh->dev_addr = dev_addr;
    gaudi_memh->size = size;
    gaudi_memh->dmabuf_fd = dmabuf_fd;  /* Keep reference to original fd */
    gaudi_memh->vaddr = NULL;  /* DMA-BUF imports may not have host mapping */
    
    ucs_debug("DMA-BUF import prepared for fd %d, size %zu "
              "(awaiting hlthunk API implementation)", dmabuf_fd, size);
    
    return UCS_OK;
}

#endif /* HAVE_HLTHUNK_H */

static uct_md_ops_t uct_gaudi_md_ops = {
    .close              = uct_gaudi_copy_md_close,
    .query              = uct_gaudi_copy_md_query,
    .mem_alloc          = uct_gaudi_copy_mem_alloc,
    .mem_free           = uct_gaudi_copy_mem_free,
    .mkey_pack          = uct_gaudi_copy_mkey_pack,
    .mem_reg            = uct_gaudi_copy_mem_reg,
    .mem_dereg          = uct_gaudi_copy_mem_dereg,
    .mem_attach         = ucs_empty_function_return_unsupported,
    .mem_query          = uct_gaudi_copy_md_mem_query,
    .detect_memory_type = uct_gaudi_copy_md_detect_memory_type
};

ucs_status_t uct_gaudi_copy_md_open(uct_component_t *component,
                              const char *md_name,
                              const uct_md_config_t *config,
                              uct_md_h *md_p)
{
    const uct_gaudi_copy_md_config_t *md_config =
        ucs_derived_of(config, uct_gaudi_copy_md_config_t);
    uct_gaudi_md_t *md;
    ucs_status_t status;
    int device_index = 0; /* Use first device by default */
#ifdef HAVE_HLTHUNK_H
    int i, ret;
#endif

    ucs_info("Opening Gaudi MD");
    
    md = ucs_calloc(1, sizeof(*md), "uct_gaudi_md");
    if (!md) {
        return UCS_ERR_NO_MEMORY;
    }
    
    /* Initialize base device info */
    status = uct_gaudi_base_init();
    if (status != UCS_OK) {
        ucs_debug("No Gaudi devices found, using fallback mode");
        md->hlthunk_fd = -1;
        md->device_index = -1;
    } else {
        /* Use the first available device */
        md->hlthunk_fd = uct_gaudi_base_get_device_fd(device_index);
        md->device_index = device_index;
        
#ifdef HAVE_HLTHUNK_H
        /* Try to open using hlthunk API */
        for (i = 0; i < 4; i++) {
            int fd = hlthunk_open(devices[i], NULL);
            if (fd >= 0) {
                if (md->hlthunk_fd >= 0) {
                    close(md->hlthunk_fd); /* Close the raw fd */
                }
                md->hlthunk_fd = fd;
                md->device_type = devices[i];
                break;
            }
        }
        
        if (md->hlthunk_fd >= 0) {
            /* Get device info for real hardware */
            ret = hlthunk_get_hw_ip_info(md->hlthunk_fd, &md->hw_info);
            if (ret != 0) {
                ucs_error("Failed to get Gaudi device info");
                hlthunk_close(md->hlthunk_fd);
                ucs_free(md);
                return UCS_ERR_NO_DEVICE;
            }
        }
#endif
    }

    md->super.ops = &uct_gaudi_md_ops;
    md->super.component = &uct_gaudi_copy_component;
    
    /* Copy configuration */
    md->config.dmabuf_supported = (md_config->enable_dmabuf != UCS_NO);
    
    *md_p = &md->super;
    
    ucs_debug("Opened Gaudi MD device_index=%d", md->device_index);
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_md_mem_query(uct_md_h uct_md, const void *address,
                                               size_t length,
                                               uct_md_mem_attr_t *mem_attr_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    ucs_memory_type_t mem_type;
    ucs_status_t status;
    
    if (!(mem_attr_p->field_mask & (UCT_MD_MEM_ATTR_FIELD_DMABUF_FD |
                                    UCT_MD_MEM_ATTR_FIELD_DMABUF_OFFSET))) {
        /* No DMA-BUF attributes requested */
        return UCS_OK;
    }
    
    /* Check if this is Gaudi device memory */
    status = uct_gaudi_copy_md_detect_memory_type(uct_md, address, length, &mem_type);
    if (status != UCS_OK) {
        return status;
    }
    
    if (mem_type != UCS_MEMORY_TYPE_GAUDI) {
        /* Not Gaudi memory, set invalid DMA-BUF fd */
        if (mem_attr_p->field_mask & UCT_MD_MEM_ATTR_FIELD_DMABUF_FD) {
            mem_attr_p->dmabuf_fd = UCT_DMABUF_FD_INVALID;
        }
        if (mem_attr_p->field_mask & UCT_MD_MEM_ATTR_FIELD_DMABUF_OFFSET) {
            mem_attr_p->dmabuf_offset = 0;
        }
        return UCS_OK;
    }
    
    /* For Gaudi memory, we need to export it as DMA-BUF */
    if (mem_attr_p->field_mask & UCT_MD_MEM_ATTR_FIELD_DMABUF_FD) {
        if (!gaudi_md->config.dmabuf_supported) {
            mem_attr_p->dmabuf_fd = UCT_DMABUF_FD_INVALID;
        } else {
#ifdef HAVE_HLTHUNK_H
            /* Try to export this memory region as DMA-BUF */
            int dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
                gaudi_md->hlthunk_fd, (uint64_t)address, length, 
                0, (O_RDWR | O_CLOEXEC));
            
            if (dmabuf_fd >= 0) {
                mem_attr_p->dmabuf_fd = dmabuf_fd;
                ucs_debug("Exported Gaudi memory region as DMA-BUF: "
                          "addr=%p len=%zu fd=%d", address, length, dmabuf_fd);
            } else {
                mem_attr_p->dmabuf_fd = UCT_DMABUF_FD_INVALID;
                ucs_debug("Failed to export Gaudi memory as DMA-BUF: "
                          "addr=%p len=%zu: %m", address, length);
            }
#else
            mem_attr_p->dmabuf_fd = UCT_DMABUF_FD_INVALID;
#endif
        }
    }
    
    if (mem_attr_p->field_mask & UCT_MD_MEM_ATTR_FIELD_DMABUF_OFFSET) {
        /* For Gaudi, offset is typically 0 as we export exact regions */
        mem_attr_p->dmabuf_offset = 0;
    }
    
    return UCS_OK;
}

uct_component_t uct_gaudi_copy_component = {
    .query_md_resources = uct_gaudi_query_md_resources,
    .md_open            = uct_gaudi_copy_md_open,
    .cm_open            = (uct_component_cm_open_func_t)ucs_empty_function_return_unsupported,
    .rkey_unpack        = uct_md_stub_rkey_unpack,
    .rkey_ptr           = (uct_component_rkey_ptr_func_t)ucs_empty_function_return_unsupported,
    .rkey_release       = (uct_component_rkey_release_func_t)ucs_empty_function_return_success,
    .rkey_compare       = uct_base_rkey_compare,
    .name               = "gaudi_copy",
    .md_config          = {
        .name           = "Gaudi-copy memory domain",
        .prefix         = "GAUDI_COPY_",
        .table          = uct_gaudi_copy_md_config_table,
        .size           = sizeof(uct_gaudi_copy_md_config_t),
    },
    .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
    .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_copy_component),
    .flags              = 0,
    .md_vfs_init        = (uct_component_md_vfs_init_func_t)ucs_empty_function
};

UCT_COMPONENT_REGISTER(&uct_gaudi_copy_component);
