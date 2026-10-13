// Byte-order helpers: the game's data files are little-endian.
#ifndef _GAME_ENDIAN_H_
#define _GAME_ENDIAN_H_

#include "types.h"

// Reverse the bytes of a 4-byte value in place.
template <typename T>
inline void EndianSwap(T& value) {
    T tmp = value;
    u8* b = (u8*)&tmp;
    u8 t = b[0];
    b[0] = b[3];
    b[3] = t;
    t = b[1];
    b[1] = b[2];
    b[2] = t;
    value = tmp;
}

// 2-byte values.
inline void EndianSwap(u16& value) {
    u16 tmp = value;
    u8* b = (u8*)&tmp;
    u8 t = b[0];
    b[0] = b[1];
    b[1] = t;
    value = tmp;
}

inline void EndianSwap(s16& value) {
    s16 tmp = value;
    u8* b = (u8*)&tmp;
    u8 t = b[0];
    b[0] = b[1];
    b[1] = t;
    value = tmp;
}

#endif
