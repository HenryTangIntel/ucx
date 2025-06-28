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

static const char *uct_gaudi_pref_loc[] = {
    [UCT_GAUDI_PREF_LOC_CPU]  = "cpu",
    [UCT_GAUDI_PREF_LOC_HPU]  = "hpu",
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
     "System device designation of a CUDA managed memory buffer"
     " whose preferred location attribute is not set.\n"
     " cpu - Assume buffer is on the CPU.\n"
     " gpu - Assume buffer is on the GPU corresponding to buffer's GPU context.",
     ucs_offsetof(uct_gaudi_copy_md_config_t, pref_loc),
     UCS_CONFIG_TYPE_ENUM(uct_gaudi_pref_loc)},

    {NULL}
};



static int uct_gaudi_copy_md_is_dmabuf_supported()
{
    int dmabuf_supported = 1;

   
    return dmabuf_supported;
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



static ucs_status_t
uct_gaudi_copy_mkey_pack(uct_md_h md, uct_mem_h memh, void *address,
                        size_t length, const uct_md_mkey_pack_params_t *params,
                        void *mkey_buffer)
{
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_rkey_unpack(uct_component_t *component,
                                              const void *rkey_buffer,
                                              uct_rkey_t *rkey_p,
                                              void **handle_p)
{
    *rkey_p   = 0xdeadbeef;
    *handle_p = NULL;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_rkey_release(uct_component_t *component,
                                               uct_rkey_t rkey, void *handle)
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



static ucs_status_t uct_gaudi_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *gaudi_memh = memh;
    
    if (gaudi_memh->vaddr != NULL || gaudi_memh->handle != 0) {
        hlthunk_device_memory_free(gaudi_md->hlthunk_fd, gaudi_memh->handle);
    }
    
    ucs_free(gaudi_memh);
    return UCS_OK;
}

void uct_gaudi_md_close(uct_md_h  uct_md)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    
    if (md->hlthunk_fd >= 0) {
        hlthunk_close(md->hlthunk_fd);
    }
    
    ucs_free(md);
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
    .detect_memory_type = uct_gaudi_md_detect_memory_type
};




