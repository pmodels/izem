#include <stdlib.h>
#include "lock/zm_mcs.h"

int zm_mcs_init(zm_mcs_t *L)
{
    atomic_store(L, (zm_ptr_t)NULL);
    return 0;
}
