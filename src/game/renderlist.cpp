// renderlist: the frame's render buffer and the registered render bins.
#include "game/renderlist.h"
#include "game/renderbin.h"
#include "game/memory.h"
#include "ea/real.h"
#include "dolphin/gx.h"
#ifdef FRONTLINE_NATIVE
#include "native/hooks.h"
#endif

bool g_bEmptyList = true;
bool g_bClearRenderList = true;
static u32 g_ClearValue = 0xBEEFCACE;

// Fills the buffer with the debug pattern and rewinds it.
static inline void Rewind(CRenderList* list) {
    if (g_bClearRenderList) {
        u32* p = (u32*)list->m_begin;
        for (u32 i = 0; i < list->m_capacity; i++) {
            p[0] = g_ClearValue;
            p[1] = g_ClearValue;
            p[2] = g_ClearValue;
            p[3] = g_ClearValue;
            p += 4;
        }
    }
    list->m_ptr = list->m_begin;
}

bool CRenderList::Finish() {
#ifdef FRONTLINE_NATIVE
    NativeDrawClass(NATIVE_DRAW_WORLD);
#endif
    for (CRenderBin* bin = m_bins; bin != NULL; bin = bin->m_pNext) {
        FinishBin(bin);
    }
    u32 used = m_ptr - m_begin;
    if (m_maxUsed < used) {
        m_maxUsed = used;
    }
    return true;
}

// Links the chunks each used bin wrote (its first tag is in m_count).
void CRenderList::FinishBin(CRenderBin* bin) {
    if (bin->IsUsed()) {
        bin->Link(ADDR_TO_PTR(CDmaTag, bin->m_count), *this);
    }
    for (CRenderBin* child = bin->m_pChildren; child != NULL; child = child->m_pNext) {
        FinishBin(child);
    }
}

void CRenderList::Wait() {}

void CRenderList::Kick() {
    if (!g_bEmptyList) {
        Rewind(this);
    }
}

void CRenderList::Restart() {
    Draw();
    if (!g_bEmptyList) {
        Rewind(this);
    }
    GXDrawDone();
}

void CRenderList::Draw() {
    g_bEmptyList = !Finish();
    if (!g_bEmptyList) {
        for (CRenderBin* bin = m_bins; bin != NULL; bin = bin->m_pNext) {
            bin->Init();
        }
    }
}

// Lets `bin` write a chunk: a tag (next, data) then its data. The bin's
// tags are chained from m_count (first) to field_0xC (last); a chunk with
// no data is dropped.
void CRenderList::Render(CRenderBin& bin, void* context) {
    CDmaTag* tag = (CDmaTag*)m_ptr;
    m_tag = tag;
    m_tag->m_next = NULL;
    m_tag->m_data = NULL;
    m_ptr += sizeof(CDmaTag);
    m_tag->m_data = m_ptr;
    CDmaTag* last = ADDR_TO_PTR(CDmaTag, bin.field_0xC);
    if (last != NULL) {
        last->m_next = m_tag;
    }
    bin.field_0xC = (u32)PTR_TO_ADDR(m_tag);
    u8* start = m_ptr;
    bin.Render(*this, context);
    if (start == m_ptr) {
        m_ptr = (u8*)tag;
        bin.field_0xC = (u32)PTR_TO_ADDR(last);
    } else if (bin.m_count == 0) {
        bin.m_count = (u32)PTR_TO_ADDR(m_tag);
    }
    m_tag = NULL;
}

void CRenderList::UnRegister(CRenderBin& bin) {
    if (m_bins == &bin) {
        m_bins = bin.m_pNext;
    }
    if (bin.m_pPrev != NULL) {
        bin.m_pPrev->m_pNext = bin.m_pNext;
    }
    if (bin.m_pNext != NULL) {
        bin.m_pNext->m_pPrev = bin.m_pPrev;
    }
    bin.m_pNext = NULL;
    bin.m_pPrev = NULL;
}

// Inserts `bin` before the first bin of a higher type.
void CRenderList::Register(CRenderBin& bin) {
    if (m_bins == NULL) {
        m_bins = &bin;
        return;
    }
    CRenderBin* head = m_bins;
    for (CRenderBin* cur = head; cur != NULL;) {
        if (bin.m_type < cur->m_type) {
            if (cur == head) {
                m_bins = &bin;
            }
            bin.m_pPrev = cur->m_pPrev;
            if (cur->m_pPrev != NULL) {
                cur->m_pPrev->m_pNext = &bin;
            }
            bin.m_pNext = cur;
            cur->m_pPrev = &bin;
            return;
        }
        if (cur->m_pNext == NULL) {
            bin.m_pNext = cur->m_pNext;
            if (cur->m_pNext != NULL) {
                cur->m_pNext->m_pPrev = &bin;
            }
            bin.m_pPrev = cur;
            cur->m_pNext = &bin;
            return;
        }
        cur = cur->m_pNext;
    }
}

CRenderList::~CRenderList() {
    MEM_free(m_begin);
}

CRenderList::CRenderList(unsigned long capacity, unsigned long) {
    m_begin = NULL;
    m_end = NULL;
    m_ptr = NULL;
    m_capacity = capacity;
    m_maxUsed = 0;
    m_bins = NULL;
    m_tag = NULL;
    m_begin = (u8*)DWI_allocalign("Renderlist", m_capacity * 0x10, 0x80, 0x400);
    m_end = m_begin + m_capacity * 0x10;
    Rewind(this);
}
