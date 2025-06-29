#ifndef UCT_GAUDI_COPY_MD_H
#define UCT_GAUDI_COPY_MD_H

#include <uct/base/uct_md.h>
#include "../base/gaudi_md.h"


extern uct_component_t uct_gaudi_copy_component;

typedef enum {
    UCT_GAUDI_PREF_LOC_CPU,
    UCT_GAUDI_PREF_LOC_HPU,
    UCT_GAUDI_PREF_LOC_LAST
} uct_gaudi_pref_loc_t;


/**
 * @brief gaudi_copy MD descriptor
 */
typedef struct uct_gaudi_copy_md {
    struct uct_md               super;           /* Domain info */
    struct {
        ucs_on_off_auto_value_t alloc_whole_reg; /* force return of allocation
                                                    range even for small bar
                                                    GPUs*/
        double                  max_reg_ratio;
        int                     dmabuf_supported;
        uct_gaudi_pref_loc_t     pref_loc;
    } config;
    void * vaddr;
    void * dev_ptr;
    size_t reg_size;

} uct_gaudi_copy_md_t;

/**
 * gdr copy domain configuration.
 */
typedef struct uct_gaudi_copy_md_config {
    uct_md_config_t             super;
    ucs_on_off_auto_value_t     alloc_whole_reg;
    double                      max_reg_ratio;
    ucs_ternary_auto_value_t    enable_dmabuf;
    uct_gaudi_pref_loc_t       pref_loc;
} uct_gaudi_copy_md_config_t;


ucs_status_t uct_gaudi_copy_md_detect_memory_type(uct_md_h md,
                                                 const void *address,
                                                 size_t length,
                                                 ucs_memory_type_t *mem_type_p);

#endif
