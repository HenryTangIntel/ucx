/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_ipc_md.h"

#include <uct/base/uct_md.h>
#include <ucs/arch/atomic.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/sys/sys.h>
#include <ucs/sys/string.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>


static ucs_config_field_t uct_gaudi_ipc_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_ipc_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {NULL}
};

static ucs_status_t uct_gaudi_ipc_md_query(uct_md_h md, uct_md_attr_t *md_attr)
{
    md_attr->cap.flags            = UCT_MD_FLAG_REG;
    md_attr->cap.reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.alloc_mem_types  = 0;  /* IPC doesn't allocate, only registers */
    md_attr->cap.access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.detect_mem_types = 0;
    md_attr->cap.max_alloc        = 0;
    md_attr->cap.max_reg          = ULONG_MAX;
    md_attr->cap.max_rkey_size    = sizeof(uct_gaudi_ipc_md_handle_t);
    md_attr->rkey_packed_size     = sizeof(uct_gaudi_ipc_md_handle_t);
    md_attr->reg_cost             = ucs_linear_func_make(1000e-9, 0.007e-9);
    md_attr->local_cpus           = UCS_CPU_SET_EMPTY;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_md_mem_reg(uct_md_h md, void *address,
                                             size_t length, unsigned flags,
                                             uct_mem_h *memh_p)
{
    uct_gaudi_ipc_memh_t *memh;
    int gaudi_fd, dmabuf_fd;
    uint64_t device_addr;

    /* For IPC registration, we need to export the existing Gaudi memory as dmabuf */
    memh = ucs_malloc(sizeof(*memh), "gaudi_ipc_memh");
    if (memh == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    /* Open Gaudi device */
    gaudi_fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (gaudi_fd < 0) {
        ucs_free(memh);
        return UCS_ERR_NO_DEVICE;
    }

    /* Map the host address to get the device handle */
    device_addr = hlthunk_host_memory_map(gaudi_fd, address, 0, length);
    if (device_addr == 0) {
        hlthunk_close(gaudi_fd);
        ucs_free(memh);
        return UCS_ERR_IO_ERROR;
    }

    /* Export as dmabuf for IPC sharing */
    dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(gaudi_fd, device_addr,
                                                              length, 0, 0);
    if (dmabuf_fd < 0) {
        hlthunk_memory_unmap(gaudi_fd, device_addr);
        hlthunk_close(gaudi_fd);
        ucs_free(memh);
        return UCS_ERR_IO_ERROR;
    }

    /* Store the handle information */
    memh->handle.dmabuf_fd  = dmabuf_fd;
    memh->handle.handle     = device_addr;
    memh->handle.length     = length;
    memh->handle.owner_pid  = getpid();
    memh->mapped_addr       = address;
    memh->gaudi_fd          = gaudi_fd;

    *memh_p = memh;

    ucs_debug("registered gaudi ipc memory %p length %zu dmabuf_fd %d",
              address, length, dmabuf_fd);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_md_mem_dereg(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_ipc_memh_t *gaudi_memh = memh;

    if (gaudi_memh == NULL) {
        return UCS_OK;
    }

    /* Clean up resources */
    if (gaudi_memh->handle.dmabuf_fd >= 0) {
        close(gaudi_memh->handle.dmabuf_fd);
    }

    if (gaudi_memh->gaudi_fd >= 0) {
        if (gaudi_memh->handle.handle != 0) {
            hlthunk_memory_unmap(gaudi_memh->gaudi_fd, gaudi_memh->handle.handle);
        }
        hlthunk_close(gaudi_memh->gaudi_fd);
    }

    ucs_free(gaudi_memh);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_md_mkey_pack(uct_md_h md, uct_mem_h memh,
                                               void *rkey_buffer)
{
    uct_gaudi_ipc_memh_t *gaudi_memh = memh;
    uct_gaudi_ipc_md_handle_t *packed_key = rkey_buffer;

    *packed_key = gaudi_memh->handle;

    ucs_debug("packed gaudi ipc rkey dmabuf_fd %d handle 0x%lx length %zu",
              packed_key->dmabuf_fd, packed_key->handle, packed_key->length);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_md_rkey_unpack(uct_component_t *component,
                                                 const void *rkey_buffer,
                                                 uct_rkey_t *rkey_p, void **handle_p)
{
    const uct_gaudi_ipc_md_handle_t *packed_key = rkey_buffer;
    uct_gaudi_ipc_md_handle_t *unpacked_key;
    int gaudi_fd;
    uint64_t mapped_addr;

    /* Create a copy of the handle for this process */
    unpacked_key = ucs_malloc(sizeof(*unpacked_key), "gaudi_ipc_rkey");
    if (unpacked_key == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    *unpacked_key = *packed_key;

    /* Open Gaudi device in this process */
    gaudi_fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (gaudi_fd < 0) {
        ucs_free(unpacked_key);
        return UCS_ERR_NO_DEVICE;
    }

    /* Import the dmabuf and map it in this process */
    /* Note: This would require dmabuf import support in hlthunk */
    /* For now, we'll store the dmabuf_fd for potential future use */
    mapped_addr = 0; /* TODO: implement dmabuf import mapping */

    *rkey_p   = (uintptr_t)unpacked_key;
    *handle_p = unpacked_key;

    ucs_debug("unpacked gaudi ipc rkey dmabuf_fd %d handle 0x%lx length %zu",
              unpacked_key->dmabuf_fd, unpacked_key->handle, unpacked_key->length);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_md_rkey_release(uct_component_t *component,
                                                  uct_rkey_t rkey, void *handle)
{
    uct_gaudi_ipc_md_handle_t *gaudi_handle = handle;

    if (gaudi_handle != NULL) {
        ucs_free(gaudi_handle);
    }
    return UCS_OK;
}

static uct_md_ops_t uct_gaudi_ipc_md_ops = {
    .close         = uct_md_close_empty,
    .query         = uct_gaudi_ipc_md_query,
    .mem_reg       = uct_gaudi_ipc_md_mem_reg,
    .mem_dereg     = uct_gaudi_ipc_md_mem_dereg,
    .mkey_pack     = uct_gaudi_ipc_md_mkey_pack,
};

static ucs_status_t uct_gaudi_ipc_md_open(uct_component_t *component,
                                          const char *md_name,
                                          const uct_md_config_t *config,
                                          uct_md_h *md_p)
{
    uct_gaudi_ipc_md_t *md;

    md = ucs_malloc(sizeof(*md), "gaudi_ipc_md");
    if (md == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &uct_gaudi_ipc_md_ops;
    md->super.component = component;

    *md_p = &md->super;
    return UCS_OK;
}

ucs_status_t uct_gaudi_ipc_query_md_resources(uct_component_t *component,
                                              uct_md_resource_desc_t **resources_p,
                                              unsigned *num_resources_p)
{
    int fd;

    /* Check if Gaudi device is available */
    fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (fd < 0) {
        return uct_md_query_empty_md_resource(resources_p, num_resources_p);
    }
    hlthunk_close(fd);

    return uct_md_query_single_md_resource(component, resources_p, num_resources_p);
}

static uct_gaudi_ipc_component_t uct_gaudi_ipc_component = {
    .super = {
        .query_md_resources = uct_gaudi_ipc_query_md_resources,
        .md_open            = uct_gaudi_ipc_md_open,
        .cm_open            = ucs_empty_function_return_unsupported,
        .rkey_unpack        = uct_gaudi_ipc_md_rkey_unpack,
        .rkey_ptr           = ucs_empty_function_return_unsupported,
        .rkey_release       = uct_gaudi_ipc_md_rkey_release,
        .rkey_compare       = uct_base_rkey_compare,
        .name               = "gaudi_ipc",
        .md_config          = {
            .name           = "Gaudi IPC memory domain",
            .prefix         = "GAUDI_IPC_",
            .table          = uct_gaudi_ipc_md_config_table,
            .size           = sizeof(uct_gaudi_ipc_md_config_t),
        },
        .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
        .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_ipc_component.super),
        .flags              = 0
    }
};

UCT_COMPONENT_REGISTER(&uct_gaudi_ipc_component.super);