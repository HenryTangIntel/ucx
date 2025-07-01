#ifndef UCT_GAUDI_IFACE_H_
#define UCT_GAUDI_IFACE_H_

#include <uct/api/uct.h>
#include <uct/base/uct_iface.h>
#include <ucs/sys/preprocessor.h>
#include <ucs/profile/profile.h>
#include <ucs/async/eventfd.h>
#include <ucs/type/status.h>
#include <ucs/datastruct/list.h>
#include <ucs/sys/compiler_def.h>
#include <uct/base/uct_iface.h>
#include <ucs/async/eventfd.h>


/* Gaudi interface address structure */

typedef struct uct_gaudi_iface_addr {
    uint64_t    magic;      /* Magic number for validation */
    uint64_t    iface_id;   /* Interface unique ID */
} uct_gaudi_iface_addr_t;

/* Gaudi interface configuration */
typedef struct uct_gaudi_iface_config {
    uct_iface_config_t    super;
    double                bandwidth;
    double                latency;
    size_t                max_short;
} uct_gaudi_iface_config_t;

typedef struct uct_gaudi_iface {
    uct_base_iface_t           super;   // Must be first for UCX base API compatibility
    uct_gaudi_iface_config_t   config;  // Configuration (bandwidth, latency, max_short)
    uint64_t                   id;      // Unique interface ID or other Gaudi-specific fields
    int        eventfd; // Event file descriptor for async operations
} uct_gaudi_iface_t;

ucs_status_t uct_gaudi_base_query_devices_common(
    uct_md_h md,
    uct_tl_device_resource_t **tl_devices_p,
    unsigned *num_tl_devices_p
);


ucs_status_t
uct_gaudi_base_query_devices(
        uct_md_h md, uct_tl_device_resource_t **tl_devices_p,
        unsigned *num_tl_devices_p);

ucs_status_t uct_gaudi_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr);
//ucs_status_t uct_gaudi_iface_get_address(uct_iface_h tl_iface, uct_iface_addr_t *iface_addr);
int          uct_gaudi_iface_is_reachable(const uct_iface_h tl_iface,
                                          const uct_device_addr_t *dev_addr,
                                          const uct_iface_addr_t *iface_addr);

ucs_status_t uct_gaudi_base_check_device_name(const uct_iface_params_t *params);

ucs_status_t uct_gaudi_base_iface_event_fd_get(uct_iface_h tl_iface, int *fd_p);

UCS_CLASS_INIT_FUNC(uct_gaudi_iface_t, uct_iface_ops_t *tl_ops,
                    uct_iface_internal_ops_t *internal_ops,
                    uct_md_h md, uct_worker_h worker,
                    const uct_iface_params_t *params,
                    const uct_iface_config_t *tl_config,
                    const char *dev_name);


#endif /* UCT_GAUDI_IFACE_H_ */
