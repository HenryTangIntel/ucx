
#ifndef UCT_GAUDI_IFACE_H_
#define UCT_GAUDI_IFACE_H_

#include <uct/api/uct.h>
#include <ucs/type/status.h>
#include <ucs/datastruct/list.h>
#include <ucs/sys/compiler_def.h>
#include <uct/base/uct_iface.h>

typedef struct uct_gaudi_iface {
    uct_iface_t           super;
    // Add Gaudi-specific fields here
} uct_gaudi_iface_t;

typedef struct uct_gaudi_iface_config {
    uct_iface_config_t    super;
    // Add Gaudi-specific config fields here
} uct_gaudi_iface_config_t;

ucs_status_t uct_gaudi_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr);
ucs_status_t uct_gaudi_iface_get_address(uct_iface_h tl_iface, uct_iface_addr_t *iface_addr);
int          uct_gaudi_iface_is_reachable(const uct_iface_h tl_iface,
                                          const uct_device_addr_t *dev_addr,
                                          const uct_iface_addr_t *iface_addr);

#endif /* UCT_GAUDI_IFACE_H_ */
