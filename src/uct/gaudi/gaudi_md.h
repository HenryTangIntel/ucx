#ifndef UCT_GAUDI_MD_H
#define UCT_GAUDI_MD_H

#include <uct/base/uct_md.h>
#include <uct/api/v2/uct_v2.h>

typedef struct uct_gaudi_md {
    uct_md_t            super;
    uct_md_ops_t        ops;
    int                 fd;
} uct_gaudi_md_t;

ucs_status_t uct_gaudi_query_md_resources(uct_component_t *component,
                                          uct_md_resource_desc_t **resources_p,
                                          unsigned *num_resources_p);

ucs_status_t uct_gaudi_md_close(uct_md_h md);
ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_t *md_attr);
ucs_status_t uct_gaudi_mem_alloc(uct_md_h md, size_t *length_p, void **address_p,
                                ucs_memory_type_t mem_type, unsigned flags,
                                const char *alloc_name, uct_mem_h *memh_p);
ucs_status_t uct_gaudi_mem_free(uct_md_h md, uct_mem_h memh);
ucs_status_t uct_gaudi_mem_reg(uct_md_h md, void *address, size_t length,
                               const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p);
ucs_status_t uct_gaudi_mem_dereg(uct_md_h md, uct_md_mem_dereg_params_t  *params);
ucs_status_t uct_gaudi_mkey_pack(uct_md_h md, uct_mem_h memh, void *address, size_t length,
                                const uct_md_mkey_pack_params_t *params,
                                void *rkey_buffer);

#endif
