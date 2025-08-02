/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. All rights reserved.
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <tools/perf/lib/libperf_int.h>
#include <ucs/sys/compiler.h>
#include <ucs/sys/ptr_arith.h>
#include <uct/api/v2/uct_v2.h>
#include <hlthunk.h>

static ucs_status_t ucx_perf_gaudi_init(ucx_perf_context_t *perf)
{
    unsigned group_index;
    int num_devices;
    int device_index;
    int fd;

    group_index = rte_call(perf, group_index);

    num_devices = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    if (num_devices <= 0) {
        ucs_error("no Gaudi devices available");
        return UCS_ERR_NO_DEVICE;
    }

    device_index = group_index % num_devices;
    
    fd = hlthunk_open(device_index, NULL);
    if (fd < 0) {
        ucs_error("failed to open Gaudi device %d", device_index);
        return UCS_ERR_NO_DEVICE;
    }

    // Store fd in perf context for later use
    // You may need to extend the perf context structure
    
    return UCS_OK;
}

static ucs_status_t ucx_perf_gaudi_alloc(size_t length, void **address_p)
{
    // Use the hlthunk APIs for device memory allocation
    uint64_t handle = hlthunk_device_memory_alloc(fd, length, 0, true, true);
    if (handle == 0) {
        ucs_error("failed to allocate Gaudi device memory size %zu", length);
        return UCS_ERR_NO_MEMORY;
    }

    uint64_t device_addr = hlthunk_device_memory_map(fd, handle, 0);
    if (device_addr == 0) {
        hlthunk_device_memory_free(fd, handle);
        ucs_error("failed to map Gaudi device memory");
        return UCS_ERR_NO_MEMORY;
    }

    *address_p = (void*)device_addr;
    return UCS_OK;
}

static ucs_status_t uct_perf_gaudi_alloc_reg_mem(const ucx_perf_context_t *perf,
                                                 size_t length,
                                                 unsigned flags,
                                                 uct_allocated_memory_t *alloc_mem)
{
    uct_md_attr_v2_t md_attr = {.field_mask = UCT_MD_ATTR_FIELD_REG_ALIGNMENT};
    void *reg_address;
    ucs_status_t status;

    status = uct_md_query_v2(perf->uct.md, &md_attr);
    if (status != UCS_OK) {
        return status;
    }

    status = ucx_perf_gaudi_alloc(length, &alloc_mem->address);
    if (status != UCS_OK) {
        return status;
    }

    reg_address = alloc_mem->address;
    ucs_align_ptr_range(&reg_address, &length, md_attr.reg_alignment);

    status = uct_md_mem_reg(perf->uct.md, reg_address, length, flags,
                            &alloc_mem->memh);
    if (status != UCS_OK) {
        // Free Gaudi memory here
        ucs_error("failed to register Gaudi memory");
        return status;
    }

    alloc_mem->mem_type = UCS_MEMORY_TYPE_GAUDI;
    alloc_mem->md       = perf->uct.md;

    return UCS_OK;
}

static ucs_status_t uct_perf_gaudi_alloc(const ucx_perf_context_t *perf,
                                         size_t length, unsigned flags,
                                         uct_allocated_memory_t *alloc_mem)
{
    return uct_perf_gaudi_alloc_reg_mem(perf, length, flags, alloc_mem);
}

static void uct_perf_gaudi_free(const ucx_perf_context_t *perf,
                               uct_allocated_memory_t *alloc_mem)
{
    ucs_status_t status;

    status = uct_md_mem_dereg(perf->uct.md, alloc_mem->memh);
    if (status != UCS_OK) {
        ucs_error("failed to deregister Gaudi memory");
    }

    // Free Gaudi device memory using hlthunk APIs
    // You'll need to track handles to properly free
}

static void ucx_perf_gaudi_memcpy(void *dst, ucs_memory_type_t dst_mem_type,
                                 const void *src, ucs_memory_type_t src_mem_type,
                                 size_t count)
{
    // Implement copy between host and Gaudi memory
    // This can use hlthunk_host_memory_map/unmap or 
    // direct memory copy operations
}

static void* ucx_perf_gaudi_memset(void *dst, int value, size_t count)
{
    // Implement memset for Gaudi device memory
    // May require mapping to host or using device operations
    return dst;
}

UCS_STATIC_INIT {
    static ucx_perf_allocator_t gaudi_allocator = {
        .mem_type  = UCS_MEMORY_TYPE_GAUDI,
        .init      = ucx_perf_gaudi_init,
        .uct_alloc = uct_perf_gaudi_alloc,
        .uct_free  = uct_perf_gaudi_free,
        .memcpy    = ucx_perf_gaudi_memcpy,
        .memset    = ucx_perf_gaudi_memset
    };

    ucx_perf_mem_type_allocators[UCS_MEMORY_TYPE_GAUDI] = &gaudi_allocator;
}

UCS_STATIC_CLEANUP {
    ucx_perf_mem_type_allocators[UCS_MEMORY_TYPE_GAUDI] = NULL;
}