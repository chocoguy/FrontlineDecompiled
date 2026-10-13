#include "ea/snd/smix.h"

// Inserts f into the chain, keeping it sorted by order.
extern "C" void SFILTER_add(SFILTER** chain, SFILTER* f) {
    SFILTER* cur = *chain;
    SFILTER* prev = NULL;
    while (cur != NULL && cur->order < f->order) {
        prev = cur;
        cur = cur->next;
    }
    f->next = cur;
    if (cur != NULL) {
        cur->prev = f;
    }
    if (prev == NULL) {
        *chain = f;
        return;
    }
    prev->next = f;
    f->prev = prev;
}

extern "C" void SFILTER_remove(SFILTER** chain, SFILTER* f) {
    SFILTER* p = *chain;
    if (f == p) {
        *chain = p->next;
        return;
    }
    while (p->next != NULL && p->next != f) {
        p = p->next;
    }
    if (p->next != NULL && p->next == f) {
        if (f->next != NULL) {
            f->next->prev = p;
        }
        p->next = p->next->next;
    }
}

// Feeds output `out` (1 or 2) of `from` into input `in` (1 or 2) of `to`.
extern "C" int SFILTER_connect(SFILTER* from, SFILTER* to, int out, int in) {
    if (out == 1 && in == 1 && to->next == NULL) {
        from->prev = to;
        to->next = from;
        to->port = 1;
        return 0;
    }
    if (out == 1 && in == 2 && to->in2 == NULL) {
        from->prev = to;
        to->in2 = from;
        to->port = 1;
        return 0;
    }
    if (out == 2 && in == 1 && to->next == NULL) {
        from->out2 = to;
        to->next = from;
        to->port = 2;
        return 0;
    }
    if (out == 2 && in == 2 && to->in2 == NULL) {
        from->out2 = to;
        to->in2 = from;
        to->port = 2;
        return 0;
    }
    return -1;
}
