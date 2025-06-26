#ifndef UCT_GAUDI_MD_H_
#define UCT_GAUDI_MD_H_


#include <uct/api/uct.h>
#include <uct/base/uct_md.h>
#include <ucs/type/status.h>

typedef struct uct_gaudi_md {
    uct_md_t super;
    int hlthunk_fd;   // Device handle obtained via hlthunk_open()
} uct_gaudi_md_t;

ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                               const char *md_name,
                               const uct_md_config_t *config,
                               uct_md_h *md_p);
void uct_gaudi_md_close(uct_md_h uct_md);
#endif
