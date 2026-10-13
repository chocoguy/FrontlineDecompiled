// CRenderBin: a node in the render list's tree of bins. Each frame the render
// list walks registered bins; derived bins override Render/Link/IsUsed.
// Hand-maintained.
#ifndef _GAME_RENDERBIN_H_
#define _GAME_RENDERBIN_H_

#include "types.h"
#include "game_types.h"

class CRenderBin;

// A bin's queued draw: Render() writes data into the packet; the render list
// hands Link() the chain of tags pointing at what each Render() wrote.
struct CDmaTag {
    /* 0x0 */ CDmaTag* m_next;
    /* 0x4 */ void* m_data;
};

// The render list's buffer (CRenderList is one).
struct CDmaPacket {
    /* 0x0 */ u8* m_begin;
    /* 0x4 */ u8* m_end;
    /* 0x8 */ u8* m_ptr;  // write position
};

// size 0x1C (no virtuals)
class CRenderBinData {
public:
    /* 0x00 */ CRenderBin* m_pPrev;      // previous sibling
    /* 0x04 */ CRenderBin* m_pNext;      // next sibling
    /* 0x08 */ CRenderBin* m_pChildren;  // first child
    /* 0x0C */ u32 field_0xC;
    /* 0x10 */ u32 m_count;              // non-zero when the bin has something to draw
    /* 0x14 */ int m_type;               // ordering key in the render list
    /* 0x18 */ u32 field_0x18;
};

// size 0x20; vtable pointer at 0x1C (after the base's data); all methods inline
class CRenderBin : public CRenderBinData {
public:
    CRenderBin() {
        m_pPrev = NULL;
        m_pNext = NULL;
        m_pChildren = NULL;
        field_0xC = 0;
        m_count = 0;
        m_type = 3;
    }
    virtual ~CRenderBin() {}  // 0x80074290
    virtual int Render(CDmaPacket&, void*) { return 0; }  // 0x80076E18
    virtual void Init() {  // 0x80073B2C
        field_0xC = 0;
        m_count = 0;
        for (CRenderBin* bin = m_pChildren; bin != NULL; bin = bin->m_pNext) {
            bin->Init();
        }
    }
    virtual void Link(CDmaTag*, CDmaPacket&) {}  // 0x80076E20
    virtual bool IsUsed() { return m_count != 0; }  // 0x80073B84
};

#endif
