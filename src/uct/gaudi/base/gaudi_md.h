#ifndef UCT_GAUDI_MD_H_
#define UCT_GAUDI_MD_H_


#include <uct/api/uct.h>
#include <uct/base/uct_md.h>
#include <ucs/type/status.h>
#include <hlthunk.h>


typedef struct uct_gaudi_md {
    uct_md_t super;
    struct hl_info_args device_info; /* Device info obtained via hlthunk_get_info() */
    int hlthunk_fd;   // Device handle obtained via hlthunk_open()
    enum hlthunk_device_name device_type; /*Type of Gaudi device*/
    struct {
        int dmabuf_supported; /* DMA_BUF support flag */
    } config;
} uct_gaudi_md_t;

/* Gaudi MD configuration */
typedef struct uct_gaudi_md_config {
    uct_md_config_t             super;
    ucs_ternary_auto_value_t    enable_dmabuf;
    ucs_linear_func_t           uc_reg_cost;
} uct_gaudi_md_config_t;

/* Gaudi memory handle */
typedef struct uct_gaudi_mem {
    void        *vaddr;      /* Virtual address */
    size_t      size;        /* Size of allocation */
    uint64_t    handle;      /* Device memory handle */
    uint64_t    dev_addr;    /* Device address */
    int         dmabuf_fd;   /* DMA-BUF file descriptor */
} uct_gaudi_mem_t;

/* Gaudi remote key */
typedef struct uct_gaudi_key {
    uint64_t    vaddr;       /* Virtual address */
    uint64_t    dev_addr;    /* Device address */
    size_t      size;        /* Size */
} uct_gaudi_key_t;


ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                               const char *md_name,
                               const uct_md_config_t *config,
                               uct_md_h *md_p);
void uct_gaudi_md_close(uct_md_h uct_md);

/* Forward declarations */
ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr);
ucs_status_t uct_gaudi_query_md_resources(uct_component_h component,
                                              uct_md_resource_desc_t **resources_p,
                                              unsigned *num_resources_p);

#endif
