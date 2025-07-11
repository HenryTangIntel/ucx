/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_md.h"

#include <uct/base/uct_md.h>
#include <ucs/arch/atomic.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/sys/sys.h>
#include <ucs/sys/string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


static ucs_config_field_t uct_gaudi_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {NULL}
};

static ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_t *md_attr)
{
    md_attr->cap.flags            = UCT_MD_FLAG_ALLOC | UCT_MD_FLAG_REG | UCT_MD_FLAG_REG_DMABUF;
    md_attr->cap.reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                    UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.alloc_mem_types  = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                    UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.detect_mem_types = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.dmabuf_min_size  = 1;
    md_attr->cap.max_alloc        = ULONG_MAX;
    md_attr->cap.max_reg          = ULONG_MAX;
    md_attr->reg_cost             = ucs_linear_func_make(1000e-9, 0.007e-9);
    md_attr->local_cpus           = UCS_CPU_SET_EMPTY;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_alloc(uct_md_h md, size_t *length_p,
                                           void **address_p, unsigned flags,
                                           const char *alloc_name,
                                           uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_md_t);
    uct_gaudi_mem_t *memh;
    ucs_status_t status;
    uint64_t device_addr;
    int fd;

    if (*length_p == 0) {
        *address_p = NULL;
        *memh_p    = NULL;
        return UCS_OK;
    }

    memh = ucs_malloc(sizeof(*memh), "gaudi_memh");
    if (memh == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (fd < 0) {
        ucs_free(memh);
        return UCS_ERR_NO_DEVICE;
    }

    device_addr = hlthunk_device_memory_alloc(fd, *length_p, 0, false, false);
    if (device_addr == 0) {
        hlthunk_close(fd);
        ucs_free(memh);
        return UCS_ERR_NO_MEMORY;
    }

    memh->address   = (void*)device_addr;
    memh->length    = *length_p;
    memh->fd        = fd;
    memh->handle    = device_addr;
    memh->dmabuf_fd = -1;

    *address_p = memh->address;
    *memh_p    = memh;

    ucs_debug("allocated gaudi memory %p length %zu", *address_p, *length_p);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_free(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_mem_t *gaudi_memh = memh;
    int ret;

    if (gaudi_memh == NULL) {
        return UCS_OK;
    }

    /* Close dmabuf fd if it was exported */
    if (gaudi_memh->dmabuf_fd >= 0) {
        close(gaudi_memh->dmabuf_fd);
    }

    ret = hlthunk_device_memory_free(gaudi_memh->fd, gaudi_memh->handle);
    if (ret != 0) {
        ucs_warn("failed to free gaudi memory %p", gaudi_memh->address);
    }

    hlthunk_close(gaudi_memh->fd);
    ucs_free(gaudi_memh);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_reg(uct_md_h md, void *address,
                                         size_t length, unsigned flags,
                                         uct_mem_h *memh_p)
{
    uct_gaudi_mem_t *memh;

    memh = ucs_malloc(sizeof(*memh), "gaudi_reg_memh");
    if (memh == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    memh->address   = address;
    memh->length    = length;
    memh->fd        = -1;
    memh->handle    = 0;
    memh->dmabuf_fd = -1;

    *memh_p = memh;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_dereg(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_mem_t *gaudi_memh = memh;

    if (gaudi_memh != NULL) {
        ucs_free(gaudi_memh);
    }
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_mem_export_to_dmabuf(uct_md_h md, uct_mem_h memh,
                                                      int *dmabuf_fd_p)
{
    uct_gaudi_mem_t *gaudi_memh = memh;
    int dmabuf_fd;

    if (gaudi_memh == NULL || gaudi_memh->fd < 0 || gaudi_memh->handle == 0) {
        return UCS_ERR_UNSUPPORTED;
    }

    /* Export Gaudi device memory to dmabuf */
    dmabuf_fd = hlthunk_device_memory_export_dmabuf_fd(gaudi_memh->fd,
                                                       gaudi_memh->handle,
                                                       gaudi_memh->length, 0);
    if (dmabuf_fd < 0) {
        ucs_debug("failed to export gaudi memory %p to dmabuf", gaudi_memh->address);
        return UCS_ERR_IO_ERROR;
    }

    gaudi_memh->dmabuf_fd = dmabuf_fd;
    *dmabuf_fd_p = dmabuf_fd;

    ucs_debug("exported gaudi memory %p to dmabuf fd %d", gaudi_memh->address, dmabuf_fd);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_md_detect_memory_type(uct_md_h md, const void *addr,
                                                    size_t length,
                                                    ucs_memory_type_t *mem_type_p)
{
    /* TODO: implement proper Gaudi memory detection */
    *mem_type_p = UCS_MEMORY_TYPE_UNKNOWN;
    return UCS_ERR_UNSUPPORTED;
}

static uct_md_ops_t uct_gaudi_md_ops = {
    .close                = uct_md_close_empty,
    .query                = uct_gaudi_md_query,
    .mem_alloc            = uct_gaudi_md_mem_alloc,
    .mem_free             = uct_gaudi_md_mem_free,
    .mem_reg              = uct_gaudi_md_mem_reg,
    .mem_dereg            = uct_gaudi_md_mem_dereg,
    .mem_export_to_dmabuf = uct_gaudi_md_mem_export_to_dmabuf,
    .detect_memory_type   = uct_gaudi_md_detect_memory_type,
};

static ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                                      const char *md_name,
                                      const uct_md_config_t *config,
                                      uct_md_h *md_p)
{
    uct_gaudi_md_t *md;

    md = ucs_malloc(sizeof(*md), "gaudi_md");
    if (md == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &uct_gaudi_md_ops;
    md->super.component = component;

    *md_p = &md->super;
    return UCS_OK;
}

ucs_status_t uct_gaudi_base_query_md_resources(uct_component_t *component,
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

ucs_status_t uct_gaudi_base_query_tl_devices(uct_md_h md,
                                             uct_tl_device_resource_t **tl_devices_p,
                                             unsigned *num_tl_devices_p)
{
    uct_tl_device_resource_t *devices;
    int fd;

    /* Check if Gaudi device is available */
    fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (fd < 0) {
        *tl_devices_p     = NULL;
        *num_tl_devices_p = 0;
        return UCS_OK;
    }
    hlthunk_close(fd);

    devices = ucs_calloc(1, sizeof(*devices), "gaudi device");
    if (devices == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    ucs_snprintf_zero(devices[0].name, sizeof(devices[0].name), "gaudi:0");
    devices[0].type       = UCT_DEVICE_TYPE_ACC;
    devices[0].sys_device = UCS_SYS_DEVICE_ID_UNKNOWN;

    *tl_devices_p     = devices;
    *num_tl_devices_p = 1;
    return UCS_OK;
}

UCT_MD_COMPONENT_DEFINE(uct_gaudi_md_component, "gaudi",
                        uct_gaudi_base_query_md_resources, uct_gaudi_md_open,
                        NULL, uct_gaudi_md_config_table, "GAUDI_");