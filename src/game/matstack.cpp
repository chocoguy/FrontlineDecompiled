#include "game/matstack.h"

#include "ea/real.h"
#include "game/memory.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

CMatrixStack* g_matStack;
CMatrixStack* CMatrixStack::s_currentStack;

// Makes sure the CMatrix class statics exist before anything uses the stack
// (the original's static initializer only runs CMatrix::InitClass).
static CMatrix s_classInit;

inline void CMatrixStack::Free() {
    if (m_stack != NULL) {
        MEM_free(m_stack);
        m_stack = NULL;
        m_size = 0;
        m_top = 0;
    }
}

void CMatrixStack::InitClass() {
    g_matStack = new CMatrixStack;
    g_matStack->Allocate(8);
    g_matStack->SetCurrent();
}

void CMatrixStack::EndClass() {
    delete g_matStack;
}

CMatrixStack::CMatrixStack() {
    m_size = 0;
    m_top = 0;
    m_stack = NULL;
}

CMatrixStack::~CMatrixStack() {
    Free();
}

void CMatrixStack::Allocate(unsigned long size) {
    m_size = size;
    m_top = 0;
    m_stack = (CMatrix*)DWI_allocalign(NULL, size * sizeof(CMatrix), 0x10, 0x400);
}

void CMatrixStack::SetCurrent() {
    if (s_currentStack != this) {
        s_currentStack = this;
    }
}

void CMatrixStack::Reset() {
    m_top = 0;
    m_stack[m_top].Ident();
}

void CMatrixStack::Push() {
    m_stack[m_top + 1] = m_stack[m_top];
    m_top++;
}

void CMatrixStack::PushUnit() {
    m_stack[m_top + 1] = m_stack[m_top];
    m_top++;
    m_stack[m_top].Ident();
}

void CMatrixStack::Pop() {
    m_top--;
}

void CMatrixStack::Ident() {
    m_stack[m_top].Ident();
}

void CMatrixStack::Load(const CMatrix* m) {
    m_stack[m_top] = *m;
}

void CMatrixStack::Store(CMatrix* m) {
    *m = m_stack[m_top];
}

void CMatrixStack::ScaleRow(CVector3 scale) {
    CMatrix* m = &m_stack[m_top];
    m->m[0][0] *= scale.x;
    m->m[0][1] *= scale.x;
    m->m[0][2] *= scale.x;
    m->m[1][0] *= scale.y;
    m->m[1][1] *= scale.y;
    m->m[1][2] *= scale.y;
    m->m[2][0] *= scale.z;
    m->m[2][1] *= scale.z;
    m->m[2][2] *= scale.z;
}
