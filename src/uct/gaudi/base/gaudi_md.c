/**
 * Copyright (c) 2025, Habana Labs Ltd. an Intel Company. All rights reserved.
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_md.h"
#include "gaudi_iface.h"

#include <ucs/sys/module.h>
#include <ucs/sys/string.h>
#include <ucs/sys/topo/base/topo.h>
#include <hlthunk.h>
#include <errno.h>
#include <cjson/cJSON.h>

// Returns 0 on success, -1 on failure
int gaudi_lookup_busid_from_env(int device_index, char *bus_id, size_t bus_id_len)
{
    const char *env = getenv("GAUDI_MAPPING_TABLE");
    cJSON *root;
    int found = 0;
    int count, i;

    if (!env) {
        ucs_warn("GAUDI_MAPPING_TABLE not set");
        return -1;
    }

    root = cJSON_Parse(env);
    if (!root || !cJSON_IsArray(root)) {
        ucs_warn("Failed to parse GAUDI_MAPPING_TABLE as JSON array");
        if (root) cJSON_Delete(root);
        return -1;
    }

    count = cJSON_GetArraySize(root);
    for (i = 0; i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        cJSON *idx = cJSON_GetObjectItem(item, "index");
        cJSON *bus = cJSON_GetObjectItem(item, "bus_id");
        if (cJSON_IsNumber(idx) && cJSON_IsString(bus) && idx->valueint == device_index) {
            strncpy(bus_id, bus->valuestring, bus_id_len - 1);
            bus_id[bus_id_len - 1] = '\0';
            found = 1;
            break;
        }
    }
    cJSON_Delete(root);

    if (!found) {
        ucs_warn("Device index %d not found in GAUDI_MAPPING_TABLE", device_index);
        return -1;
    }
    return 0;
}


void uct_gaudi_base_get_sys_dev(int gaudi_device,
                               ucs_sys_device_t *sys_dev_p)
{
    ucs_sys_bus_id_t bus_id;
    ucs_status_t status;
    char pci_bus_id_str[64] = {0};
    int domain = 0, bus = 0, device = 0, function = 0;

    /* Use gaudi_lookup_busid_from_env to get the PCI bus id string */
    if (gaudi_lookup_busid_from_env(gaudi_device, pci_bus_id_str, sizeof(pci_bus_id_str)) != 0) {
        ucs_debug("GAUDI_MAPPING_TABLE did not provide a mapping for Gaudi device %d", gaudi_device);
        goto err;
    }

    /* Parse PCI bus ID string: format is [domain]:[bus]:[device].[function] */
    if (sscanf(pci_bus_id_str, "%x:%x:%x.%x", &domain, &bus, &device, &function) != 4) {
        ucs_debug("Failed to parse PCI bus ID '%s' for Gaudi device %d", pci_bus_id_str, gaudi_device);
        goto err;
    }

    /* Convert to UCX bus ID format */
    bus_id.domain   = domain;
    bus_id.bus      = bus;
    bus_id.slot     = device;
    bus_id.function = function;

    /* Find the system device by PCI bus ID */
    status = ucs_topo_find_device_by_bus_id(&bus_id, sys_dev_p);
    if (status != UCS_OK) {
        ucs_debug("Failed to find system device by PCI bus ID %s for Gaudi device %d: %s", 
                  pci_bus_id_str, gaudi_device, ucs_status_string(status));
        goto err;
    }

    /* Associate the system device with the Gaudi device index */
    status = ucs_topo_sys_device_set_user_value(*sys_dev_p, gaudi_device);
    if (status != UCS_OK) {
        ucs_debug("Failed to set user value for system device for Gaudi device %d: %s", 
                  gaudi_device, ucs_status_string(status));
        goto err;
    }

    ucs_debug("Successfully mapped Gaudi device %d to system device (PCI: %s, domain=%d, bus=%d, slot=%d, func=%d)", 
              gaudi_device, pci_bus_id_str, domain, bus, device, function);
    return;

err:
    ucs_debug("System device detection failed for Gaudi device %d, will use unknown", gaudi_device);
    *sys_dev_p = UCS_SYS_DEVICE_ID_UNKNOWN;
}




// Open a Gaudi device by index, using busid from env if available
int uct_gaudi_md_open_device(int device_index) {
    char bus_id[64] = {0};
    int fd = -1;
    if (gaudi_lookup_busid_from_env(device_index, bus_id, sizeof(bus_id)) == 0) {
        fd = hlthunk_open(device_index, bus_id);
        if (fd < 0) {
            ucs_warn("Failed to open Gaudi device %d (busid=%s)", device_index, bus_id);
        }
    } else {
        // fallback: try open by index only
        fd = hlthunk_open(device_index, NULL);
        if (fd < 0) {
            ucs_warn("Failed to open Gaudi device %d (no busid)", device_index);
        }
    }
    return fd;
}

// Close a Gaudi device
void uct_gaudi_md_close_device(int fd) {
    if (fd >= 0) {
        hlthunk_close(fd);
    }
}

ucs_status_t
uct_gaudi_base_get_gaudi_device(ucs_sys_device_t sys_dev, int *device)
{
    uintptr_t user_value;

    user_value = ucs_topo_sys_device_get_user_value(sys_dev);
    if (user_value == UINTPTR_MAX) {
        return UCS_ERR_NO_DEVICE;
    }

    *device = user_value;
    if (*device == -1) {
        return UCS_ERR_NO_DEVICE;
    }

    return UCS_OK;
}

ucs_status_t
uct_gaudi_base_query_md_resources(uct_component_t *component,
                                 uct_md_resource_desc_t **resources_p,
                                 unsigned *num_resources_p)
{
    const unsigned sys_device_priority = 10;
    uct_md_resource_desc_t *resources;
    ucs_sys_device_t sys_dev;
    ucs_status_t status;
    char device_name[10];
    int i, num_gpus;

    /* Try different Gaudi device types */
    num_gpus = hlthunk_get_device_count(HLTHUNK_DEVICE_DONT_CARE);
    if (num_gpus <= 0) {
        return uct_md_query_empty_md_resource(resources_p, num_resources_p);
    }

    for (i = 0; i < num_gpus; ++i) {
        uct_gaudi_base_get_sys_dev(i, &sys_dev);
        if (sys_dev != UCS_SYS_DEVICE_ID_UNKNOWN) {
            ucs_snprintf_safe(device_name, sizeof(device_name), "GAUDI%d", i);
            status = ucs_topo_sys_device_set_name(sys_dev, device_name,
                                                  sys_device_priority);
            ucs_assert_always(status == UCS_OK);
        } else {
            ucs_debug("System device detection failed for Gaudi device %d, "
                      "transport will still be available but device name will be unknown", i);
        }
    }

    ucs_debug("Successfully detected Gaudi devices");
    resources = calloc(num_gpus, sizeof(*resources));
    if (resources == NULL) {
        ucs_error("Failed to allocate memory for Gaudi MD resources");
        return UCS_ERR_NO_MEMORY;
    }

    for (i = 0; i < num_gpus; ++i) {
        snprintf(resources[i].md_name, sizeof(resources[i].md_name),
                 "gaudi%d", i);
    }
    *num_resources_p = num_gpus;
    *resources_p = resources;
    return UCS_OK;

}

ucs_status_t uct_gaudi_md_mem_reg(uct_md_h md, void *address, size_t length,
                                  const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p)
{
   
   
    uct_gaudi_memh_t *memh = NULL;
    int gaudi_fd = -1;
    uint64_t gaudi_handle = 0;
    uint64_t device_va = 0;
    int dmabuf_fd = -1;
    
    /*
     * Provider-specific: get gaudi_fd from registration parameters if present.
     * This requires the user to set a custom field and mask bit.
     * Example:
     *   #define UCT_MD_MEM_REG_FIELD_GAUDI_FD UCS_BIT(16)
     *   typedef struct {
     *       uct_md_mem_reg_params_t super;
     *       int gaudi_fd;
     *   } uct_gaudi_mem_reg_params_t;
     *
     *   // In user code:
     *   uct_gaudi_mem_reg_params_t params = {0};
     *   params.super.field_mask = UCT_MD_MEM_REG_FIELD_GAUDI_FD;
     *   params.gaudi_fd = ...;
     *
     *   // In provider:
     */

    if (params && (params->field_mask & UCT_MD_MEM_REG_FIELD_GAUDI_FD)) {
        gaudi_fd = ((uct_gaudi_mem_reg_params_t*)params)->gaudi_fd;
    } else {
        ucs_error("Gaudi device fd must be provided in params with field_mask UCT_MD_MEM_REG_FIELD_GAUDI_FD");
        return UCS_ERR_INVALID_PARAM;
    }

    memh = ucs_malloc(sizeof(*memh), "uct_gaudi_memh_t");
    if (!memh) {
        ucs_error("Failed to allocate memory for Gaudi memh");
        return UCS_ERR_NO_MEMORY;
    }
    *memh_p = (uct_mem_h)memh;
    memh->gaudi_handle = hlthunk_device_memory_alloc(gaudi_fd, length, 0, true, true);
    if (memh->gaudi_handle == 0) {
        ucs_error("Failed to allocate Gaudi device memory of size %zu", length);
        ucs_free(memh);
        return UCS_ERR_NO_MEMORY;
    }
    memh->gaudi_fd = gaudi_fd;
    memh->length = length;
    memh->device_va = hlthunk_device_memory_map(gaudi_fd, memh->gaudi_handle, 0);
    if (memh->device_va == 0) {
        ucs_error("Failed to map Gaudi device memory handle 0x%lx", memh->gaudi_handle);
        hlthunk_device_memory_free(gaudi_fd, memh->gaudi_handle);
        ucs_free(memh);
        return UCS_ERR_NO_MEMORY;
    }   
    // If params->field_mask contains DMABUF_FD, export the memory as DMA-BUF
    if (params->field_mask & UCT_MD_MEM_REG_FIELD_DMABUF_FD) {
        dmabuf_fd = hlthunk_device_mapped_memory_export_dmabuf_fd(
            gaudi_fd, memh->device_va, length, 0, (O_RDWR | O_CLOEXEC));
        if (dmabuf_fd < 0) {
            ucs_error("Failed to export Gaudi device memory as DMA-BUF fd");
            hlthunk_device_memory_free(gaudi_fd, memh->gaudi_handle);
            ucs_free(memh);
            return UCS_ERR_NO_MEMORY;
        }
        memh->dmabuf_fd = dmabuf_fd;
        ucs_debug("Exported Gaudi device memory as DMA-BUF fd %d", dmabuf_fd);
    } else {
        memh->dmabuf_fd = -1; // Not exporting as DMA-BUF
    }

    memh->gaudi_fd = gaudi_fd;
    memh->gaudi_handle = gaudi_handle;
    memh->device_va = device_va;
    memh->dmabuf_fd = dmabuf_fd;
    memh->length = length;
    memh->host_ptr = NULL; // Initialize host pointer to NULL

    *memh_p = memh;

   
    ucs_error("Gaudi mem_reg: implementation required");
    return UCS_ERR_UNSUPPORTED;
}

UCS_MODULE_INIT() {
    UCS_MODULE_FRAMEWORK_DECLARE(uct_gaudi);
    UCS_MODULE_FRAMEWORK_LOAD(uct_gaudi, 0);
    return UCS_OK;
}