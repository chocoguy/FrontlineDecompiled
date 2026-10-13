// CRenderList: the frame's render buffer and the registered render bins
// (sorted by type). Each frame bins render into the buffer as tagged
// chunks; Finish links each bin's chunks, Draw then re-inits the bins.
// Hand-maintained.
#ifndef _GAME_RENDERLIST_H_
#define _GAME_RENDERLIST_H_

#include "types.h"
#include "game_types.h"
#include "game/renderbin.h"

class CRenderBin;
class CRenderList;

// size 0x1C
class CRenderList : public CDmaPacket {
public:
    /* 0xC */ CDmaTag* m_tag;  // tag being written by Render
    /* 0x10 */ u32 m_capacity;  // in 16-byte units
    /* 0x14 */ u32 m_maxUsed;  // bytes, high-water mark
    /* 0x18 */ CRenderBin* m_bins;
    // Returns true (non-empty).
    bool Finish();  // 0x800F0B80
    void FinishBin(CRenderBin*);  // 0x800F0BF0
    void Wait();  // 0x800F0ED4
    void Kick();  // 0x800F0ED8
    void Restart();  // 0x800F0FD4
    void Draw();  // 0x800F10FC
    void Render(CRenderBin&, void*);  // 0x800F1168
    void UnRegister(CRenderBin&);  // 0x800F1230
    void Register(CRenderBin&);  // 0x800F127C
    ~CRenderList();  // 0x800F1338
    CRenderList(unsigned long, unsigned long);  // 0x800F138C
};

SIZE_ASSERT(CRenderList, 0x1C);

extern bool g_bEmptyList;  // 0x8034E318
// Fill the buffer with 0xBEEFCACE on each restart (debugging).
extern bool g_bClearRenderList;  // 0x8034E319

#endif
