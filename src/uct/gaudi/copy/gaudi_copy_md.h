#ifndef UCT_GAUDI_COPY_MD_H_
#define UCT_GAUDI_COPY_MD_H_

#include <uct/gaudi/gaudi_md.h> // Reuse main Gaudi MD config and types for now

#define UCT_GAUDI_COPY_MD_NAME "gaudi_copy"

/**
 * @brief Gaudi Copy MD configuration.
 *
 * For now, it can just wrap the main uct_gaudi_md_config_t.
 * If gaudi_copy needs specific MD configurations later, they can be added here.
 */
typedef uct_gaudi_md_config_t uct_gaudi_copy_md_config_t;


/**
 * @brief Gaudi Copy memory domain.
 *
 * This MD will be specific to the gaudi_copy transport.
 * It might wrap or directly use the underlying uct_gaudi_md_t,
 * or it could have its own logic if needed.
 * For simplicity, let's assume it has a pointer to the main Gaudi MD.
 */
typedef struct uct_gaudi_copy_md {
    uct_md_t      super;
    uct_md_h      gaudi_md; /* Underlying Gaudi MD for device access */
    /* Add other gaudi_copy specific MD fields if needed */
} uct_gaudi_copy_md_t;

extern uct_component_t uct_gaudi_copy_component;

ucs_status_t uct_gaudi_copy_md_query(uct_md_h md, uct_md_attr_v2_t *attr);

#endif /* UCT_GAUDI_COPY_MD_H_ */
