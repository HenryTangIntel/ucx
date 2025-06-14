#ifndef UCT_GAUDI_IFACE_H
#define UCT_GAUDI_IFACE_H

#include <uct/base/uct_iface.h>


typedef struct uct_gaudi_iface {
    uct_base_iface_t super;
    // Add Gaudi-specific fields here
} uct_gaudi_iface_t;

extern ucs_class_t uct_gaudi_iface_t_class;

ucs_status_t uct_gaudi_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr);

#endif

