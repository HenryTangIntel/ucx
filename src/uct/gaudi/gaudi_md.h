#ifndef UCT_GAUDI_MD_H_
#define UCT_GAUDI_MD_H_

#include <uct/api/uct.h>
#include <infiniband/verbs.h>
#include <stdint.h>

typedef struct uct_gaudi_md {
    uct_md_t super;
    int fd;                 // Gaudi device file descriptor
    char pci_bus_id[32];    // PCI bus id string
    struct ibv_pd* pd;      // Verbs protection domain
    struct ibv_context* ctx; // Verbs context
} uct_gaudi_md_t;

typedef struct uct_gaudi_mem {
    int fd;
    uint64_t device_va;
    int dma_buf_fd;
    struct ibv_mr* mr;
    size_t length;
} uct_gaudi_mem_t;

#endif