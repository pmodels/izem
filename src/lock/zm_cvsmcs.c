#include <stdlib.h>
#include "lock/zm_cvsmcs.h"

int zm_cvsmcs_init(zm_cvsmcs_t *L)
{
    atomic_store(&L->lock, (zm_ptr_t)NULL);
    L->cur_ctx = NULL;
    return 0;
}
