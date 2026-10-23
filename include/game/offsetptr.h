// Turns a file offset into a pointer (NULL stays NULL); used when loaded
// level files are relocated in place.
#ifndef _GAME_OFFSETPTR_H_
#define _GAME_OFFSETPTR_H_

#ifdef FRONTLINE_NATIVE
// Natively the field must be a PTR32 (4 bytes, as in the file); `base` is the
// file's address (PTR_TO_ADDR).
template <class T>
void offsetPtr(ptr32<T>& ptr, int base) {
    if (ptr.addr() != 0) {
        ptr.setAddr(ptr.addr() + base);
    }
}
#else
template <class T>
void offsetPtr(T*& ptr, int base) {
    if (ptr != NULL) {
        ptr = (T*)((int)ptr + base);
    }
}
#endif

#endif
