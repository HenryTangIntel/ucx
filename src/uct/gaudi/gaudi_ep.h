#ifndef UCT_GAUDI_EP_H_
#define UCT_GAUDI_EP_H_

#include <uct/base/uct_base_ep.h>
#include <infiniband/verbs.h>
#include "gaudi_md.h"

typedef struct uct_gaudi_ep {
    uct_base_ep_t super;
    struct ibv_qp *qp;
    // Add remote address/rkey information as needed
} uct_gaudi_ep_t;

#endif