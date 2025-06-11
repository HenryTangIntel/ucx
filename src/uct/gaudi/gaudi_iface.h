#ifndef UCT_GAUDI_IFACE_H_
#define UCT_GAUDI_IFACE_H_

#include <uct/base/uct_iface.h>
#include <infiniband/verbs.h>
#include "gaudi_md.h"

typedef struct uct_gaudi_iface {
    uct_base_iface_t super;
    struct ibv_cq *cq;
    // Add QP pool, remote address book, etc.
} uct_gaudi_iface_t;

#endif