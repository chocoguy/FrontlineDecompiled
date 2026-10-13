// CMatrixStack: a fixed-size stack of matrices (the 2D/UI and skinning
// transform stack). Hand-maintained.
#ifndef _GAME_MATSTACK_H_
#define _GAME_MATSTACK_H_

#include "types.h"
#include "game_types.h"
#include "game/matrix.h"

class CVector3;

// size 0xC (operator new)
class CMatrixStack {
public:
    /* 0x0 */ u32 m_size;  // capacity in matrices
    /* 0x4 */ int m_top;
    /* 0x8 */ CMatrix* m_stack;
    CMatrix* Top() { return &m_stack[m_top]; }
    void ScaleRow(CVector3);  // 0x80090C84
    void Store(CMatrix*);  // 0x80090D28
    void Load(const CMatrix*);  // 0x80090D60
    void Ident();  // 0x80090D90
    void Pop();  // 0x80090DC0
    void PushUnit();  // 0x80090DD0
    void Push();  // 0x80090E38
    void Reset();  // 0x80090E8C
    void SetCurrent();  // 0x80090EC4
    void Allocate(unsigned long);  // 0x80090ED8
    ~CMatrixStack();  // 0x80090F24
    CMatrixStack();  // 0x80090F90
    static void EndClass();  // 0x80090FA4
    static void InitClass();  // 0x80090FFC

    static CMatrixStack* s_currentStack;  // 0x8034EABC

private:
    void Free();
};

extern CMatrixStack* g_matStack;  // 0x8034EAB8

#endif
