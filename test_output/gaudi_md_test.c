#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uct/api/uct.h>
#include <uct/api/uct_def.h>

extern ucs_status_t uct_init(void);
extern ucs_status_t uct_cleanup(void);
extern ucs_status_t uct_query_components(uct_component_h **components_p, unsigned *num_components_p);
extern void uct_release_component_list(uct_component_h *components);
extern ucs_status_t uct_md_config_read(uct_component_h component, const char *env_prefix,
                                    const char *filename, uct_md_config_t **config_p);
extern void uct_config_release(void *config);
extern ucs_status_t uct_md_open(uct_component_h component, const char *name, 
                              const uct_md_config_t *config, uct_md_h *md_p);
extern void uct_md_close(uct_md_h md);

static uct_component_h find_gaudi_component() {
    ucs_status_t status;
    uct_component_h *components, gaudi_comp = NULL;
    unsigned num_components;
    
    status = uct_query_components(&components, &num_components);
    if (status != UCS_OK) {
        return NULL;
    }
    
    for (unsigned i = 0; i < num_components; i++) {
        if (strcmp(components[i]->name, "gaudi") == 0) {
            gaudi_comp = components[i];
            break;
        }
    }
    
    uct_release_component_list(components);
    return gaudi_comp;
}

int main(int argc, char **argv) {
    ucs_status_t status;
    uct_component_h gaudi_comp;
    uct_md_config_t *md_config;
    uct_md_h md;
    
    /* Initialize UCT */
    status = uct_init();
    if (status != UCS_OK) {
        printf("Failed to initialize UCT\n");
        return 1;
    }
    
    /* Find Gaudi component */
    gaudi_comp = find_gaudi_component();
    if (gaudi_comp == NULL) {
        printf("Gaudi component not found\n");
        uct_cleanup();
        return 1;
    }
    
    printf("Found Gaudi component\n");
    
    /* Read MD config */
    status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
    if (status != UCS_OK) {
        printf("Failed to read Gaudi MD config\n");
        uct_cleanup();
        return 1;
    }
    
    printf("Successfully read Gaudi MD config\n");
    
    /* Open MD */
    status = uct_md_open(gaudi_comp, "gaudi", md_config, &md);
    uct_config_release(md_config);
    
    if (status != UCS_OK) {
        printf("Failed to open Gaudi memory domain: %s\n", ucs_status_string(status));
        uct_cleanup();
        return 1;
    }
    
    printf("Successfully opened Gaudi memory domain\n");
    
    /* Close MD and cleanup */
    uct_md_close(md);
    uct_cleanup();
    
    printf("Test completed successfully\n");
    return 0;
}
