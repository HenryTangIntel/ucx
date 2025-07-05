#include <ucs/memory/memory_type.h>
#include <stdio.h>

int main() {
    printf("UCS_MEMORY_TYPE_HOST = %d\n", UCS_MEMORY_TYPE_HOST);
    printf("UCS_MEMORY_TYPE_CUDA = %d\n", UCS_MEMORY_TYPE_CUDA);
    printf("UCS_MEMORY_TYPE_GAUDI = %d\n", UCS_MEMORY_TYPE_GAUDI);
    printf("UCS_MEMORY_TYPE_LAST = %d\n", UCS_MEMORY_TYPE_LAST);
    printf("UCS_BIT(UCS_MEMORY_TYPE_GAUDI) = 0x%lx\n", UCS_BIT(UCS_MEMORY_TYPE_GAUDI));
    return 0;
}
