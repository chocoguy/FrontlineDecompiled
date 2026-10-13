// Turns a file offset into a pointer (NULL stays NULL); used when loaded
// level files are relocated in place.
#ifndef _GAME_OFFSETPTR_H_
#define _GAME_OFFSETPTR_H_

template <class T>
void offsetPtr(T*& ptr, int base) {
    if (ptr != NULL) {
        ptr = (T*)((int)ptr + base);
    }
}

#endif
