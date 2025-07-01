#include "gaudi_copy_md.h"
#include "../base/gaudi_iface.h"
#include <string.h>
#include <limits.h>
#include <ucs/debug/log.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/memory/memtype_cache.h>
#include <ucs/profile/profile.h>
#include <ucs/type/class.h>
#include <ucs/sys/math.h>
#include <uct/api/v2/uct_v2.h>
#include <hlthunk.h>

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64
#define UCT_GAUDI_MAX_DEVICES      32

uct_component_t uct_gaudi_copy_component;

enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2,
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
};

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




ucs_status_t uct_gaudi_copy_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr)
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



static ucs_status_t uct_gaudi_copy_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh = memh;
    
    if (gaudi_memh->vaddr != NULL || gaudi_memh->handle != 0) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, gaudi_memh->handle);
    }
    
    ucs_free(gaudi_memh);
    return UCS_OK;
}

void uct_gaudi_copy_md_close(uct_md_h  uct_md)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    
    if (md->hlthunk_fd >= 0) {
        hlthunk_close(md->hlthunk_fd);
    }
    
    ucs_free(md);
}

static ucs_status_t uct_gaudi_copy_mem_reg_internal(
        uct_md_h uct_md, void *address, size_t length,
        int pg_align_addr, uct_gaudi_copy_md_t *mem_hndl)
{
    void *dev_addr = NULL;

    ucs_assert((address != NULL) && (length != 0));

 

    mem_hndl->vaddr    = address;
    mem_hndl->dev_ptr  = dev_addr;
    mem_hndl->reg_size = length;

    ucs_trace("Registered addr %p len %zu dev addr %p", address, length, dev_addr);
    return UCS_OK;
}


UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_mem_reg,
                 (md, address, length, params, memh_p),
                 uct_md_h md, void *address, size_t length,
                 const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p)
{

    uct_gaudi_copy_md_t *mem_hndl = NULL;
    ucs_status_t status;

    mem_hndl = ucs_malloc(sizeof(uct_gaudi_copy_md_t), "gaudi_copy handle");
    if (NULL == mem_hndl) {
        ucs_error("failed to allocate memory for gaudi_copy_md_t");
        return UCS_ERR_NO_MEMORY;
    }

    status = uct_gaudi_copy_mem_reg_internal(md, address, length, 1, mem_hndl);
    if (status != UCS_OK) {
        ucs_free(mem_hndl);
        return status;
    }

    *memh_p = mem_hndl;
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_mem_dereg,
                 (md, params),
                 uct_md_h md, const uct_md_mem_dereg_params_t *params)
{
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_md_detect_memory_type,
                 (md, address, length, mem_type_p), uct_md_h md,
                 const void *address, size_t length,
                 ucs_memory_type_t *mem_type_p)
{
    return UCS_OK;
}


static uct_md_ops_t uct_gaudi_md_ops = {
    .close              = uct_gaudi_copy_md_close,
    .query              = uct_gaudi_copy_md_query,
    .mem_alloc          = uct_gaudi_copy_mem_alloc,
    .mem_free           = uct_gaudi_copy_mem_free,
    .mkey_pack          = uct_gaudi_copy_mkey_pack,
    .mem_reg            = uct_gaudi_copy_mem_reg,
    .mem_dereg          = uct_gaudi_copy_mem_dereg,
    .mem_attach         = ucs_empty_function_return_unsupported,
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
    int i;
    int ret;

    ucs_info("Opening Gaudi MD");
    
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
    ret = hlthunk_get_hw_ip_info(md->hlthunk_fd, &md->hw_info);
    if (ret != 0) {
        ucs_error("Failed to get Gaudi device info");
        hlthunk_close(md->hlthunk_fd);
        ucs_free(md);
        return UCS_ERR_NO_DEVICE;
    }

    //hlthunk_close(md->hlthunk_fd);

    
    md->super.ops = &uct_gaudi_md_ops;
    md->super.component = &uct_gaudi_copy_component;
    
    /* Copy configuration */
    //md->config.uc_reg_cost = md_config->uc_reg_cost;
    md->config.dmabuf_supported = (md_config->enable_dmabuf != UCS_NO);
    
    *md_p = &md->super;
    
    ucs_debug("Opened Gaudi MD device_type=%d", md->device_type);
    
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
    .name               = "gaudi_cpy",
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
