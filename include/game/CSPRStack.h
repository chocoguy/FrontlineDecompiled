// A bump stack in the locked-cache scratch pad, for short-lived arrays.
#ifndef _GAME_CSPRSTACK_H_
#define _GAME_CSPRSTACK_H_

#include "types.h"
#include "game_types.h"
#include "game/gcscratchpad.h"

class CSPRStack {
public:
    CSPRStack() : m_begin(SPR_BEGIN), m_end(SPR_END), m_top(SPR_BEGIN) {}
    ~CSPRStack() {}  // 0x800775E0 (weak)
    u8* Alloc(int size) {
        u8* p = m_top;
        m_top += size;
        return p;
    }
    void Free(int size) { m_top -= size; }

    /* 0x0 */ u8* m_begin;
    /* 0x4 */ u8* m_end;
    /* 0x8 */ u8* m_top;
};

#endif
