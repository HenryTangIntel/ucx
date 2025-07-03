/**
 * Copyright (c) 2024, Habana Labs Ltd. an Intel Company
 */

#ifndef UCT_GAUDI_COPY_MD_H
#define UCT_GAUDI_COPY_MD_H

#include <uct/base/uct_md.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <hlthunk.h>


extern uct_component_t uct_gaudi_copy_component;

/**
 * @brief gaudi_copy MD descriptor
 */
typedef struct uct_gaudi_copy_md {
    struct uct_md                super;           /* Domain info */
} uct_gaudi_copy_md_t;

/**
 * gaudi_copy MD configuration.
 */
typedef struct uct_gaudi_copy_md_config {
    uct_md_config_t             super;
} uct_gaudi_copy_md_config_t;

ucs_status_t uct_gaudi_copy_md_detect_memory_type(uct_md_h md,
                                                 const void *address,
                                                 size_t length,
                                                 ucs_memory_type_t *mem_type_p);

#endif
