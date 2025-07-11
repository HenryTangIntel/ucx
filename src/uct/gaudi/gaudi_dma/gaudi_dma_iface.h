/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_DMA_IFACE_H
#define UCT_GAUDI_DMA_IFACE_H

#include <uct/base/uct_iface.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <ucs/datastruct/queue.h>
#include <ucs/type/spinlock.h>
#include <hlthunk.h>
#include <drm/habanalabs_accel.h>

/* Gaudi DMA packet structure */
struct packet_lin_dma {
    uint32_t tsize;
    union {
        struct {
            uint32_t wr_comp_en :1;
            uint32_t transpose:1;
            uint32_t dtype :1;
            uint32_t lin :1;        /* must be 1 for linear DMA */
            uint32_t mem_set :1;
            uint32_t compress :1;
            uint32_t decompress :1;
            uint32_t reserved: 9;
            uint32_t context_id_low :8;
            uint32_t opcode :5;
            uint32_t eng_barrier :1;
            uint32_t reg_barrier :1;    /* must be 1 */
            uint32_t msg_barrier :1;
        };
        uint32_t ctl;
    };
    uint64_t src_addr;
    uint64_t dst_addr;
};

#define UCT_GAUDI_DMA_TL_NAME "gaudi_dma"

typedef struct uct_gaudi_dma_iface_config {
    uct_iface_config_t super;
    size_t             tx_queue_len;
    size_t             rx_queue_len;
} uct_gaudi_dma_iface_config_t;

typedef struct uct_gaudi_dma_cmd {
    ucs_queue_elem_t    queue_elem;
    uint64_t            sequence;
    void               *user_data;
    uct_completion_t   *comp;
    size_t              length;
} uct_gaudi_dma_cmd_t;

typedef struct uct_gaudi_dma_iface {
    uct_base_iface_t    super;
    int                 gaudi_fd;
    uint64_t            cmd_buffer_addr;
    void               *cmd_buffer_ptr;
    size_t              cmd_buffer_size;
    size_t              cmd_offset;
    uint64_t            next_sequence;
    ucs_queue_head_t    pending_cmds;
    ucs_spinlock_t      cmd_lock;
} uct_gaudi_dma_iface_t;

typedef struct uct_gaudi_dma_ep {
    uct_base_ep_t super;
} uct_gaudi_dma_ep_t;

extern uct_component_t uct_gaudi_dma_component;

#endif