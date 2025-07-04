/**
 * Copyright (c) 2024 Habana Labs Ltd. ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <tools/perf/lib/libperf_int.h>
#include <habanalabs/hlthunk.h>
#include <ucs/sys/compiler.h>
#include <ucs/sys/ptr_arith.h>
#include <uct/api/v2/uct_v2.h>
#include <stddef.h>
#include <stdbool.h>

static int gaudi_fd = -1;

static ucs_status_t ucx_perf_gaudi_init(ucx_perf_context_t *perf)
{
    int num_devices;

    fprintf(stderr, "DEBUG: ucx_perf_gaudi_init called\n");
    fflush(stderr);

    /* Get number of Gaudi devices using generic detection */
    num_devices = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    fprintf(stderr, "DEBUG: Number of Gaudi devices: %d\n", num_devices);
    fflush(stderr);
    
    if (num_devices <= 0) {
        fprintf(stderr, "DEBUG: No Gaudi devices found\n");
        fflush(stderr);
        return UCS_ERR_NO_DEVICE;
    }

    /* Open the Gaudi device using generic type */
    gaudi_fd = hlthunk_open(HLTHUNK_DEVICE_DONT_CARE, NULL);
    if (gaudi_fd < 0) {
        fprintf(stderr, "DEBUG: Failed to open Gaudi device\n");
        fflush(stderr);
        return UCS_ERR_IO_ERROR;
    }

    fprintf(stderr, "DEBUG: Successfully opened Gaudi device (fd=%d)\n", gaudi_fd);
    fflush(stderr);
    return UCS_OK;
}


static inline ucs_status_t ucx_perf_gaudi_alloc(size_t length,
                                                ucs_memory_type_t mem_type,
                                                void **address_p)
{
    uint64_t device_addr;

    fprintf(stderr, "DEBUG: ucx_perf_gaudi_alloc called with length=%zu, mem_type=%d\n", length, mem_type);
    fflush(stderr);

    ucs_assert(mem_type == UCS_MEMORY_TYPE_GAUDI);

    if (gaudi_fd < 0) {
        fprintf(stderr, "DEBUG: Gaudi device not opened (fd=%d)\n", gaudi_fd);
        fflush(stderr);
        ucs_error("Gaudi device not opened");
        return UCS_ERR_NO_DEVICE;
    }

    fprintf(stderr, "DEBUG: Attempting to allocate %zu bytes on Gaudi device (fd=%d)\n", length, gaudi_fd);
    fflush(stderr);

    /* Allocate device memory */
    device_addr = hlthunk_device_memory_alloc(gaudi_fd, length, 0, true, false);
    if (device_addr == 0) {
        fprintf(stderr, "DEBUG: hlthunk_device_memory_alloc failed\n");
        fflush(stderr);
        ucs_error("failed to allocate Gaudi device memory");
        return UCS_ERR_NO_MEMORY;
    }

    fprintf(stderr, "DEBUG: Successfully allocated device memory at address 0x%lx\n", device_addr);
    fflush(stderr);

    *address_p = (void*)device_addr;
    return UCS_OK;
}

static inline ucs_status_t ucx_perf_gaudi_free(void *address)
{
    if (gaudi_fd < 0) {
        return UCS_ERR_NO_DEVICE;
    }

    /* Free device memory */
    hlthunk_device_memory_free(gaudi_fd, (uint64_t)address);
    return UCS_OK;
}

static void ucx_perf_gaudi_cleanup(void)
{
    if (gaudi_fd >= 0) {
        hlthunk_close(gaudi_fd);
        gaudi_fd = -1;
    }
}


static inline ucs_status_t
uct_perf_gaudi_alloc_reg_mem(const ucx_perf_context_t *perf,
                             size_t length,
                             ucs_memory_type_t mem_type,
                             unsigned flags,
                             uct_allocated_memory_t *alloc_mem)
{
    uct_md_attr_v2_t md_attr = {.field_mask = UCT_MD_ATTR_FIELD_REG_ALIGNMENT};
    void *reg_address;
    ucs_status_t status;

    status = uct_md_query_v2(perf->uct.md, &md_attr);
    if (status != UCS_OK) {
        ucs_error("uct_md_query_v2() returned %d", status);
        return status;
    }

    status = ucx_perf_gaudi_alloc(length, mem_type, &alloc_mem->address);
    if (status != UCS_OK) {
        return status;
    }

    /* Register memory respecting MD reg_alignment */
    reg_address = alloc_mem->address;
    ucs_align_ptr_range(&reg_address, &length, md_attr.reg_alignment);

    status = uct_md_mem_reg(perf->uct.md, reg_address, length, flags,
                            &alloc_mem->memh);
    if (status != UCS_OK) {
        ucx_perf_gaudi_free(alloc_mem->address);
        ucs_error("failed to register memory");
        return status;
    }

    alloc_mem->mem_type = mem_type;
    alloc_mem->md       = perf->uct.md;

    return UCS_OK;
}

static ucs_status_t uct_perf_gaudi_alloc(const ucx_perf_context_t *perf,
                                         size_t length, unsigned flags,
                                         uct_allocated_memory_t *alloc_mem)
{
    return uct_perf_gaudi_alloc_reg_mem(perf, length, UCS_MEMORY_TYPE_GAUDI,
                                        flags, alloc_mem);
}

static void uct_perf_gaudi_free(const ucx_perf_context_t *perf,
                                uct_allocated_memory_t *alloc_mem)
{
    ucs_status_t status;

    ucs_assert(alloc_mem->md == perf->uct.md);

    status = uct_md_mem_dereg(perf->uct.md, alloc_mem->memh);
    if (status != UCS_OK) {
        ucs_error("failed to deregister memory");
    }

    ucx_perf_gaudi_free(alloc_mem->address);
}

static void ucx_perf_gaudi_memcpy_func(void *dst, ucs_memory_type_t dst_mem_type,
                                       const void *src, ucs_memory_type_t src_mem_type,
                                       size_t count)
{
    /* For now, fall back to regular memcpy until we implement proper DMA operations.
     * In a full implementation, this would use Gaudi's command submission interface
     * to perform DMA operations between host and device memory. */
    memcpy(dst, src, count);
}

static void* ucx_perf_gaudi_memset(void *dst, int value, size_t count)
{
    /* For now, fall back to regular memset until we implement proper device operations.
     * In a full implementation, this would use Gaudi's command submission interface
     * to perform memset operations on device memory. */
    return memset(dst, value, count);
}

UCS_STATIC_INIT {
    static ucx_perf_allocator_t gaudi_allocator = {
        .mem_type  = UCS_MEMORY_TYPE_GAUDI,
        .init      = ucx_perf_gaudi_init,
        .uct_alloc = uct_perf_gaudi_alloc,
        .uct_free  = uct_perf_gaudi_free,
        .memcpy    = ucx_perf_gaudi_memcpy_func,
        .memset    = ucx_perf_gaudi_memset
    };

    ucx_perf_mem_type_allocators[UCS_MEMORY_TYPE_GAUDI] = &gaudi_allocator;
}

UCS_STATIC_CLEANUP {
    ucx_perf_mem_type_allocators[UCS_MEMORY_TYPE_GAUDI] = NULL;
    ucx_perf_gaudi_cleanup();
}
