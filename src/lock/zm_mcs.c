#include <stdlib.h>
#include "lock/zm_mcs.h"

int zm_mcs_init(zm_mcs_t *L)
{
    zm_mcs_qnode_t* node = malloc(sizeof *node);
    atomic_store(&node->next, NULL);
    atomic_store(&node->status, ZM_UNLOCKED);
    atomic_store(L, (zm_ptr_t)&node);
    return 0;
}
