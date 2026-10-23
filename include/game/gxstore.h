// Loads from and stores into memory the graphics chip reads (display lists,
// vertex arrays, textures). That memory is big-endian on the GameCube, and the
// native GX layer reads it the same way, so CPU code that reads or fills it
// uses these instead of plain accesses. On GameCube they are plain accesses.
// (Plain byte-for-byte copies need nothing.)
#ifndef _GAME_GXSTORE_H_
#define _GAME_GXSTORE_H_

#include "types.h"

#ifdef FRONTLINE_NATIVE
#include <string.h>
static inline void GXStoreBE32(void* p, u32 v) {
    v = __builtin_bswap32(v);
    memcpy(p, &v, 4);
}
static inline void GXStoreBE16(void* p, u16 v) {
    v = __builtin_bswap16(v);
    memcpy(p, &v, 2);
}
static inline u16 GXLoadBE16(const void* p) {
    u16 v;
    memcpy(&v, p, 2);
    return __builtin_bswap16(v);
}
#define GX_STORE32(p, v) GXStoreBE32((p), (v))
#define GX_STORE16(p, v) GXStoreBE16((p), (v))
#define GX_LOAD16(p) GXLoadBE16(p)
#define GX_LOADS16(p) ((s16)GXLoadBE16(p))
#else
#define GX_STORE32(p, v) (*(u32*)(p) = (v))
#define GX_STORE16(p, v) (*(u16*)(p) = (v))
#define GX_LOAD16(p) (*(u16*)(p))
#define GX_LOADS16(p) (*(s16*)(p))
#endif

#endif
