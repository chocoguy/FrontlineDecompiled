// Script built-in functions (BIFunc_*): called by the BS machine's BIFNCALL
// opcode with the script stack and the scene node of the running object.
// Arguments are on the stack (the last one on top); the function pops
// m_popCount words and writes its result, if any, to the new top.
#ifndef _GAME_BSBIF_H_
#define _GAME_BSBIF_H_

#include "types.h"

typedef void (*BSBuiltInFunc)(int** sp, void* self);

// size 0x10
struct BSBuiltInFunction_struct {
    /* 0x0 */ BSBuiltInFunc m_func;
    /* 0x4 */ u32 m_crc;  // CRC of the function name the script imports
    /* 0x8 */ s16 field_0x8;
    /* 0xA */ s16 m_popCount;
    /* 0xC */ s16 m_numArgs;
    /* 0xE */ u8 unk_0xE[0x2];
};

extern BSBuiltInFunction_struct* g_pBuiltInFunctions;
extern int g_iCurrentBIFIndex;

inline BSBuiltInFunction_struct& BIFCurrent() {
    return g_pBuiltInFunctions[g_iCurrentBIFIndex];
}

// Argument `i` (0 = first) of the current call.
inline int BIFArg(int** sp, int i) {
    return (*sp - (BIFCurrent().m_numArgs - 1 - i))[0];
}

inline void BIFPop(int** sp) {
    *sp -= BIFCurrent().m_popCount;
}

inline void BIFReturn(int** sp, int value) {
    **sp = value;
}

#endif
