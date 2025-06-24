#ifndef UCT_GAUDI_MD_H
#define UCT_GAUDI_MD_H

#include <uct/base/uct_md.h>
#include <ucs/type/class.h>
#include <ucs/memory/rcache.h>

/* Define Gaudi memory type, use next available value */
#define UCS_MEMORY_TYPE_GAUDI UCS_MEMORY_TYPE_LAST

/* Gaudi memory handle */
typedef struct uct_gaudi_mem {
    void      *vaddr;     /* User virtual address */
    void      *dev_ptr;   /* Device pointer or address */
    size_t    reg_size;   /* Registered memory size */
} uct_gaudi_mem_t;

/* Gaudi packed key for remote access */
typedef struct uct_gaudi_key {
    uint64_t  vaddr;      /* Virtual address */
    void      *dev_ptr;   /* Device pointer */
} uct_gaudi_key_t;

/* Gaudi memory domain */
typedef struct uct_gaudi_md {
    uct_md_t        super;
    uct_md_ops_t    ops;
    int             fd;
    int             have_dmabuf;
} uct_gaudi_md_t;

/* Gaudi MD config */
typedef struct uct_gaudi_md_config {
    uct_md_config_t      super;  /* Base MD config */
    int                  enable_rcache; /* Enable registration cache */
} uct_gaudi_md_config_t;

void uct_gaudi_md_close(uct_md_h md);
ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr);
ucs_status_t uct_gaudi_mem_alloc(uct_md_h md, size_t *length_p, void **address_p,
                                 ucs_memory_type_t mem_type, ucs_sys_device_t sys_dev,
                                 unsigned flags, const char *alloc_name, uct_mem_h *memh_p);
ucs_status_t uct_gaudi_mem_free(uct_md_h md, uct_mem_h memh);
ucs_status_t uct_gaudi_mem_reg(uct_md_h md, void *address, size_t length,
                               const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p);
ucs_status_t uct_gaudi_mem_dereg(uct_md_h md, const uct_md_mem_dereg_params_t *params);
ucs_status_t uct_gaudi_mkey_pack(uct_md_h md, uct_mem_h memh, void *address, size_t length,
                                 const uct_md_mkey_pack_params_t *params, void *mkey_buffer);
ucs_status_t uct_gaudi_rkey_unpack(uct_component_t *component, const void *rkey_buffer,
                                   const uct_rkey_unpack_params_t *params,
                                   uct_rkey_t *rkey_p, void **handle_p);
ucs_status_t uct_gaudi_rkey_release(uct_component_t *component, uct_rkey_t rkey, void *handle);

void uct_gaudi_pg_align_addr(void **addr, size_t *length);


#endif