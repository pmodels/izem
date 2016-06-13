#include <stdlib.h>
#include "lock/zm_csvmcs.h"

int zm_csvmcs_init(zm_csvmcs_t *L)
{
    atomic_store(&L->lock, (zm_ptr_t)NULL);
    L->cur_ctx = NULL;
    return 0;
}
