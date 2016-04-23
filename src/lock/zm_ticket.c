#include "lock/zm_ticket.h"

int zm_ticket_init(zm_ticket_t *lock)
{
    atomic_store(&lock->next_ticket, 0);
    atomic_store(&lock->now_serving, 0);
    return 0;
}
