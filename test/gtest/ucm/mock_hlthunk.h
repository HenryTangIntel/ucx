#ifndef MOCK_HLTHUNK_H
#define MOCK_HLTHUNK_H

#include <stdlib.h>

/* Mock implementation for hlthunk functions */
static inline int hlthunk_allocate_device_memory(int device_id, void **dptr, size_t size) {
    *dptr = malloc(size);
    return (*dptr != NULL) ? 0 : -1;
}

static inline int hlthunk_free_device_memory(int device_id, void *dptr) {
    free(dptr);
    return 0;
}

#endif /* MOCK_HLTHUNK_H */
