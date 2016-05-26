#include "mem/zm_hzdptr.h"

zm_atomic_ptr_t zm_hzdptr_list; /* head of the list*/
atomic_uint zm_hplist_length; /* N: correlates with the number
                                        of threads */

/* per-thread pointer to its own hazard pointer node */
zm_thread_local zm_hzdptr_lnode_t* zm_my_hplnode;


