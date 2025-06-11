#include <libhlthunk.h>
#include <infiniband/verbs.h>
#include <ucs/type/status.h>
#include <ucs/sys/string.h>
#include <unistd.h>
#include "gaudi_md.h"

static struct ibv_context* gaudi_map_verbs_ctx(const char *gaudi_busid)
{
    int num;
    struct ibv_device **dev_list = ibv_get_device_list(&num);
    if (!dev_list) return NULL;
    struct ibv_context *ctx = NULL;
    for (int i = 0; i < num; ++i) {
        // TODO: Match PCI busid using sysfs or other methods for production
        ctx = ibv_open_device(dev_list[i]);
        break;
    }
    ibv_free_device_list(dev_list);
    return ctx;
}

ucs_status_t uct_gaudi_md_open(const char *md_name, const uct_md_config_t *md_config,
                               uct_md_h *md_p)
{
    int dev_idx = 0;
    sscanf(md_name, "gaudi%d", &dev_idx);

    int fd = hlthunk_open_original(HLTHUNK_DEVICE_PCIE, NULL); // TODO: open correct device
    if (fd < 0) return UCS_ERR_IO_ERROR;

    char busid[32] = {0};
    if (hlthunk_get_pci_bus_id_from_fd(fd, busid, sizeof(busid)) < 0) {
        hlthunk_close_original(fd);
        return UCS_ERR_IO_ERROR;
    }

    struct ibv_context *ctx = gaudi_map_verbs_ctx(busid);
    if (!ctx) {
        hlthunk_close_original(fd);
        return UCS_ERR_IO_ERROR;
    }

    struct ibv_pd *pd = ibv_alloc_pd(ctx);
    if (!pd) {
        ibv_close_device(ctx);
        hlthunk_close_original(fd);
        return UCS_ERR_IO_ERROR;
    }

    uct_gaudi_md_t *md = ucs_malloc(sizeof(*md), "gaudi-md");
    if (!md) {
        ibv_dealloc_pd(pd);
        ibv_close_device(ctx);
        hlthunk_close_original(fd);
        return UCS_ERR_NO_MEMORY;
    }
    md->fd = fd;
    strncpy(md->pci_bus_id, busid, sizeof(md->pci_bus_id));
    md->pd = pd;
    md->ctx = ctx;
    *md_p = (uct_md_h)md;
    return UCS_OK;
}

ucs_status_t uct_gaudi_md_mem_reg(uct_md_h md, void *address, size_t length,
                                  unsigned flags, uct_mem_h *memh_p)
{
    uct_gaudi_md_t *gmd = (uct_gaudi_md_t*)md;
    uct_gaudi_mem_t *mem = ucs_malloc(sizeof(*mem), "gaudi-mem");
    if (!mem) return UCS_ERR_NO_MEMORY;
    mem->fd = gmd->fd;

    uint64_t device_va = 0;
    if (hlthunk_device_memory_alloc(mem->fd, length, &device_va)) goto err_free;

    int dma_buf_fd = hlthunk_device_memory_export_dmabuf_fd(mem->fd, device_va);
    if (dma_buf_fd < 0) goto err_alloc;

    struct ibv_mr *mr = ibv_reg_dmabuf_mr(
        gmd->pd, dma_buf_fd, 0, length,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ, 0);
    if (!mr) goto err_dmabuf;

    mem->device_va = device_va;
    mem->dma_buf_fd = dma_buf_fd;
    mem->mr = mr;
    mem->length = length;

    *memh_p = mem;
    return UCS_OK;

err_dmabuf:
    close(dma_buf_fd);
err_alloc:
    hlthunk_device_memory_free(mem->fd, device_va);
err_free:
    ucs_free(mem);
    return UCS_ERR_IO_ERROR;
}

ucs_status_t uct_gaudi_md_mem_dereg(uct_md_h md, uct_mem_h memh)
{
    uct_gaudi_mem_t *mem = (uct_gaudi_mem_t*)memh;
    ibv_dereg_mr(mem->mr);
    close(mem->dma_buf_fd);
    hlthunk_device_memory_free(mem->fd, mem->device_va);
    ucs_free(mem);
    return UCS_OK;
}

ucs_status_t uct_gaudi_md_detect_mem_type(uct_md_h md, const void *address,
                                          size_t length, ucs_memory_type_t *mem_type_p)
{
    *mem_type_p = UCS_MEMORY_TYPE_HOST; // TODO: thunk pointer check if available
    return UCS_OK;
}