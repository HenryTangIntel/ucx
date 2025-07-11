/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_dma_iface.h"
#include <uct/gaudi/base/gaudi_md.h>

#include <uct/base/uct_md.h>
#include <ucs/arch/atomic.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/sys/sys.h>

#include <sys/types.h>
#include <unistd.h>
#include <string.h>


static ucs_config_field_t uct_gaudi_dma_iface_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_dma_iface_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_iface_config_table)},

    {"TX_QUEUE_LEN", "256",
     "Length of send queue in the DMA interface",
     ucs_offsetof(uct_gaudi_dma_iface_config_t, tx_queue_len), UCS_CONFIG_TYPE_UINT},

    {"RX_QUEUE_LEN", "256", 
     "Length of receive queue in the DMA interface",
     ucs_offsetof(uct_gaudi_dma_iface_config_t, rx_queue_len), UCS_CONFIG_TYPE_UINT},

    {NULL}
};

static ucs_status_t uct_gaudi_dma_submit_command(uct_gaudi_dma_iface_t *iface,
                                                 uint64_t src_addr, uint64_t dst_addr,
                                                 size_t length, uct_completion_t *comp)
{
    struct hlthunk_cs_in cs_in = {0};
    struct hlthunk_cs_out cs_out = {0};
    struct packet_lin_dma *dma_pkt;
    uint64_t cmd_seq;
    uct_gaudi_dma_cmd_t *cmd;
    ucs_status_t status;
    int ret;

    ucs_spin_lock(&iface->cmd_lock);

    /* Check if we have space in command buffer */
    if (iface->cmd_offset + sizeof(*dma_pkt) > iface->cmd_buffer_size) {
        ucs_spin_unlock(&iface->cmd_lock);
        return UCS_ERR_NO_RESOURCE;
    }

    /* Create DMA packet */
    dma_pkt = (struct packet_lin_dma*)((char*)iface->cmd_buffer_ptr + iface->cmd_offset);
    memset(dma_pkt, 0, sizeof(*dma_pkt));
    
    dma_pkt->tsize = length;
    dma_pkt->ctl = 0;
    dma_pkt->src_addr = src_addr;
    dma_pkt->dst_addr = dst_addr;
    
    /* Set DMA packet control fields */
    dma_pkt->lin = 1;           /* Linear DMA */
    dma_pkt->reg_barrier = 1;   /* Required */
    dma_pkt->opcode = 0x1;      /* DMA opcode */

    /* For now, use a simplified approach - actual implementation would need */
    /* proper command buffer management and submission via hlthunk APIs */
    
    /* Simulate DMA operation with immediate completion for demonstration */
    cmd_seq = ++iface->next_sequence;
    iface->cmd_offset += sizeof(*dma_pkt);
    
    /* Track pending command if completion is requested */
    if (comp != NULL) {
        cmd = ucs_malloc(sizeof(*cmd), "gaudi_dma_cmd");
        if (cmd == NULL) {
            ucs_spin_unlock(&iface->cmd_lock);
            return UCS_ERR_NO_MEMORY;
        }
        
        cmd->sequence = cmd_seq;
        cmd->comp     = comp;
        cmd->length   = length;
        ucs_queue_push(&iface->pending_cmds, &cmd->queue_elem);
        
        /* For demonstration, immediately complete the operation */
        uct_invoke_completion(comp, UCS_OK);
        ucs_queue_del_iter(&iface->pending_cmds, &cmd->queue_elem);
        ucs_free(cmd);
    }

    ucs_spin_unlock(&iface->cmd_lock);

    ucs_debug("submitted gaudi dma: src=0x%lx dst=0x%lx len=%zu seq=%lu",
              src_addr, dst_addr, length, cmd_seq);
    return UCS_OK;
}

static unsigned uct_gaudi_dma_iface_progress(uct_iface_h tl_iface)
{
    /* For simplified implementation, commands complete immediately */
    /* Real implementation would poll for DMA completion here */
    return 0;
}

static ucs_status_t uct_gaudi_dma_ep_put_zcopy(uct_ep_h tl_ep,
                                               const uct_iov_t *iov, size_t iovcnt,
                                               uint64_t remote_addr, uct_rkey_t rkey,
                                               uct_completion_t *comp)
{
    uct_gaudi_dma_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_dma_ep_t);
    uct_gaudi_dma_iface_t *iface = ucs_derived_of(ep->super.iface, uct_gaudi_dma_iface_t);
    uint64_t local_addr;

    if (iovcnt != 1) {
        return UCS_ERR_UNSUPPORTED;
    }

    /* Convert local buffer to device address */
    local_addr = hlthunk_host_memory_map(iface->gaudi_fd, iov[0].buffer, 0, iov[0].length);
    if (local_addr == 0) {
        return UCS_ERR_IO_ERROR;
    }

    return uct_gaudi_dma_submit_command(iface, local_addr, remote_addr, 
                                       iov[0].length, comp);
}

static ucs_status_t uct_gaudi_dma_ep_get_zcopy(uct_ep_h tl_ep,
                                               const uct_iov_t *iov, size_t iovcnt,
                                               uint64_t remote_addr, uct_rkey_t rkey,
                                               uct_completion_t *comp)
{
    uct_gaudi_dma_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_dma_ep_t);
    uct_gaudi_dma_iface_t *iface = ucs_derived_of(ep->super.iface, uct_gaudi_dma_iface_t);
    uint64_t local_addr;

    if (iovcnt != 1) {
        return UCS_ERR_UNSUPPORTED;
    }

    /* Convert local buffer to device address */
    local_addr = hlthunk_host_memory_map(iface->gaudi_fd, iov[0].buffer, 0, iov[0].length);
    if (local_addr == 0) {
        return UCS_ERR_IO_ERROR;
    }

    return uct_gaudi_dma_submit_command(iface, remote_addr, local_addr,
                                       iov[0].length, comp);
}

static ucs_status_t uct_gaudi_dma_iface_query(uct_iface_h tl_iface,
                                              uct_iface_attr_t *iface_attr)
{
    memset(iface_attr, 0, sizeof(*iface_attr));

    iface_attr->iface_addr_len          = 0;
    iface_attr->device_addr_len         = 0;
    iface_attr->ep_addr_len             = 0;
    iface_attr->max_conn_priv           = 0;
    iface_attr->cap.flags               = UCT_IFACE_FLAG_GET_ZCOPY |
                                          UCT_IFACE_FLAG_PUT_ZCOPY |
                                          UCT_IFACE_FLAG_CONNECT_TO_IFACE;

    iface_attr->cap.put.max_zcopy       = SIZE_MAX;
    iface_attr->cap.put.min_zcopy       = 1;
    iface_attr->cap.put.max_iov         = 1;
    iface_attr->cap.put.opt_zcopy_align = 1;
    iface_attr->cap.put.align_mtu       = iface_attr->cap.put.opt_zcopy_align;

    iface_attr->cap.get.max_zcopy       = SIZE_MAX;
    iface_attr->cap.get.min_zcopy       = 1;
    iface_attr->cap.get.max_iov         = 1;
    iface_attr->cap.get.opt_zcopy_align = 1;
    iface_attr->cap.get.align_mtu       = iface_attr->cap.get.opt_zcopy_align;

    iface_attr->latency                 = ucs_linear_func_make(1e-6, 0);
    iface_attr->bandwidth.dedicated     = 25600.0 * UCS_MBYTE; /* Gaudi DMA bandwidth estimate */
    iface_attr->bandwidth.shared        = 0;
    iface_attr->overhead                = 1e-6; /* Low DMA overhead */
    iface_attr->priority                = 0;

    return UCS_OK;
}

static ucs_status_t uct_gaudi_dma_ep_create(const uct_ep_params_t *params,
                                            uct_ep_h *ep_p)
{
    uct_gaudi_dma_iface_t *iface = ucs_derived_of(params->iface, uct_gaudi_dma_iface_t);
    uct_gaudi_dma_ep_t *ep;

    ep = ucs_malloc(sizeof(*ep), "gaudi_dma_ep");
    if (ep == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &ep->super, &iface->super);

    *ep_p = &ep->super.super;
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_dma_ep_t)
{
    /* Nothing to cleanup for now */
}

static uct_ep_ops_t uct_gaudi_dma_ep_ops = {
    .ep_put_zcopy             = uct_gaudi_dma_ep_put_zcopy,
    .ep_get_zcopy             = uct_gaudi_dma_ep_get_zcopy,
    .ep_pending_add           = ucs_empty_function_return_unsupported,
    .ep_pending_purge         = ucs_empty_function_return_unsupported,
    .ep_flush                 = uct_base_ep_flush,
    .ep_fence                 = uct_base_ep_fence,
    .ep_create                = UCS_CLASS_NEW_FUNC_NAME(uct_gaudi_dma_ep_t),
    .ep_destroy               = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_dma_ep_t),
};

UCS_CLASS_DEFINE(uct_gaudi_dma_ep_t, uct_base_ep_t)

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_dma_iface_t)
{
    uct_gaudi_dma_iface_t *iface = self;

    if (iface->cmd_buffer_addr != 0) {
        hlthunk_memory_unmap(iface->gaudi_fd, iface->cmd_buffer_addr);
    }
    if (iface->cmd_buffer_ptr != NULL) {
        ucs_free(iface->cmd_buffer_ptr);
    }
    if (iface->gaudi_fd >= 0) {
        hlthunk_close(iface->gaudi_fd);
    }
}

static uct_iface_ops_t uct_gaudi_dma_iface_ops = {
    .ep_create                = uct_gaudi_dma_ep_create,
    .ep_destroy               = ucs_empty_function_return_success,
    .iface_flush              = uct_base_iface_flush,
    .iface_fence              = uct_base_iface_fence,
    .iface_progress_enable    = ucs_empty_function,
    .iface_progress_disable   = ucs_empty_function,
    .iface_progress           = uct_gaudi_dma_iface_progress,
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_dma_iface_t),
    .iface_query              = uct_gaudi_dma_iface_query,
    .iface_get_device_address = ucs_empty_function_return_success,
    .iface_get_address        = ucs_empty_function_return_success,
    .iface_is_reachable       = ucs_empty_function_return_one,
};

static ucs_status_t uct_gaudi_dma_iface_open(uct_md_h md, uct_worker_h worker,
                                             const uct_iface_params_t *params,
                                             const uct_iface_config_t *tl_config,
                                             uct_iface_h *iface_p)
{
    uct_gaudi_dma_iface_config_t *config = ucs_derived_of(tl_config, uct_gaudi_dma_iface_config_t);
    uct_gaudi_dma_iface_t *iface;
    ucs_status_t status;

    iface = ucs_malloc(sizeof(*iface), "gaudi_dma_iface");
    if (iface == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    status = UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, &iface->super, &uct_gaudi_dma_iface_ops,
                                       &uct_gaudi_dma_ep_ops, md, worker, params,
                                       tl_config UCS_STATS_ARG(params->stats_root)
                                       UCS_STATS_ARG(UCT_GAUDI_DMA_TL_NAME));
    if (status != UCS_OK) {
        goto err_free;
    }

    /* Open Gaudi device */
    iface->gaudi_fd = hlthunk_open(HLTHUNK_DEVICE_GAUDI, NULL);
    if (iface->gaudi_fd < 0) {
        status = UCS_ERR_NO_DEVICE;
        goto err_cleanup_super;
    }

    /* Allocate command buffer */
    iface->cmd_buffer_size = 64 * 1024; /* 64KB command buffer */
    iface->cmd_buffer_ptr = ucs_memalign(4096, iface->cmd_buffer_size, "gaudi_cmd_buffer");
    if (iface->cmd_buffer_ptr == NULL) {
        status = UCS_ERR_NO_MEMORY;
        goto err_close_device;
    }

    /* Map command buffer */
    iface->cmd_buffer_addr = hlthunk_host_memory_map(iface->gaudi_fd, 
                                                    iface->cmd_buffer_ptr, 0,
                                                    iface->cmd_buffer_size);
    if (iface->cmd_buffer_addr == 0) {
        status = UCS_ERR_IO_ERROR;
        goto err_free_buffer;
    }

    iface->cmd_offset = 0;
    iface->next_sequence = 1;
    ucs_queue_head_init(&iface->pending_cmds);
    ucs_spinlock_init(&iface->cmd_lock, 0);

    *iface_p = &iface->super.super;
    return UCS_OK;

err_free_buffer:
    ucs_free(iface->cmd_buffer_ptr);
err_close_device:
    hlthunk_close(iface->gaudi_fd);
err_cleanup_super:
    UCS_CLASS_CLEANUP_CALL_SUPER(uct_base_iface_t, &iface->super);
err_free:
    ucs_free(iface);
    return status;
}

UCS_CLASS_DEFINE(uct_gaudi_dma_iface_t, uct_base_iface_t);

static uct_tl_t uct_gaudi_dma_tl = {
    .name           = UCT_GAUDI_DMA_TL_NAME,
    .iface_open     = uct_gaudi_dma_iface_open,
    .query_devices  = uct_gaudi_base_query_tl_devices,
};

UCT_TL_REGISTER(&uct_gaudi_dma_tl, &uct_gaudi_md_component,
                UCT_GAUDI_DMA_TL_NAME,
                uct_gaudi_dma_iface_config_table, uct_gaudi_dma_iface_config_t);