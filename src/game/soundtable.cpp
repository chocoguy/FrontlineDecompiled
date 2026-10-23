// soundtable: the level's sound tables, looked up by binary search.
#include "game/soundtable.h"
#include "game/toilet.h"

CSoundTable g_soundMapSingleton;

static inline void Swap16(s16& v) {
#ifdef FRONTLINE_NATIVE
    return;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    u16 u = (u16)v;
    v = (s16)((u >> 8) | (u << 8));
}

template <class T>
static inline void Swap32(T& v) {
#ifndef FRONTLINE_NATIVE  // little-endian data on a little-endian CPU: nothing to swap
    u32 u = (u32)v;
    v = (T)((u >> 24) | ((u >> 8) & 0xFF00) | ((u << 8) & 0xFF0000) | (u << 24));
#endif
}

// File offsets to pointers (NULL stays NULL).
#ifdef FRONTLINE_NATIVE
template <class T>
static void offsetPtr(ptr32<T>& p, int base) {
    if (p.addr() != 0) {
        p.setAddr(p.addr() + base);
    }
}
#else
template <class T>
static void offsetPtr(T*& p, int base) {
    if (p != NULL) {
        p = (T*)((char*)p + base);
    }
}
#endif

static inline void SwapParms(CSoundTable::SSoundParms& parms) {
    Swap16(parms.m_randomMin);
    Swap16(parms.m_randomMax);
    Swap16(parms.m_volume);
    Swap16(parms.m_rangeScale);
    Swap16(parms.m_looped);
}

CSoundTable::~CSoundTable() {
    TLT_CloseFile(m_events);
}

// The headers' words (the entries are swapped by Init).
void CSoundTable::EndianSwap() {
    if (m_events != NULL) {
        Swap32(m_events->unk_0x4[0]);
        Swap32(m_events->unk_0x4[1]);
        Swap32(m_events->unk_0x4[2]);
        Swap32(m_events->m_numKeys);
        Swap32(m_events->m_numParms);
        Swap32(m_events->m_keys);
        Swap32(m_events->m_parms);
    }
    if (m_anims != NULL) {
        Swap32(m_anims->unk_0x4[1]);
        Swap32(m_anims->unk_0x4[2]);
        Swap32(m_anims->m_count);
        Swap32(m_anims->m_entries);
    }
}

void CSoundTable::Init(int mission, int stage) {
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xC, NULL);
    m_events = (SEventFile*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(const char, level->fields[11].value), NULL);
    m_anims = (SAnimFile*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(const char, level->fields[12].value), NULL);
    EndianSwap();
    if (m_events != NULL) {
        offsetPtr(m_events->m_keys, (int)PTR_TO_ADDR(m_events));
        offsetPtr(m_events->m_parms, (int)PTR_TO_ADDR(m_events));
        for (int i = 0; i < m_events->m_numKeys; i++) {
            SKey& key = m_events->m_keys[i];
            Swap16(key.m_bank);
            Swap16(key.m_variant);
            Swap16(key.m_sound);
            Swap16(key.m_parms);
        }
        for (int i = 0; i < m_events->m_numParms; i++) {
            SwapParms(m_events->m_parms[i]);
        }
    }
    if (m_anims != NULL) {
        offsetPtr(m_anims->m_entries, (int)PTR_TO_ADDR(m_anims));
        for (int i = 0; i < m_anims->m_count; i++) {
            SAnimEntry& entry = m_anims->m_entries[i];
            Swap16(entry.m_id);
            SwapParms(entry.m_parms);
        }
    }
}

// Pass 0 matches the variant exactly, pass 1 also takes wildcard (-1)
// entries, pass 2 ignores it.
static inline int CompareKey(const CSoundTable::SKey& entry, const CSoundTable::SKey& key, int pass) {
    if (entry.m_bank < key.m_bank) {
        return -1;
    }
    if (key.m_bank < entry.m_bank) {
        return 1;
    }
    if (entry.m_sound < key.m_sound) {
        return -1;
    }
    if (key.m_sound < entry.m_sound) {
        return 1;
    }
    if (pass <= 1 && (pass <= 0 || entry.m_variant != -1)) {
        if (entry.m_variant < key.m_variant) {
            return -1;
        }
        if (key.m_variant < entry.m_variant) {
            return 1;
        }
    }
    return 0;
}

bool CSoundTable::GetSoundEvent(int bank, int variant, int sound, CSoundTable::SSoundParms* parms) const {
    if (m_events == NULL) {
        return false;
    }
    SKey key;
    key.m_bank = bank;
    key.m_variant = variant;
    key.m_sound = sound;
    for (int pass = 0; pass < 3; pass++) {
        s16 lo = 0;
        s16 mid = 0;
        s16 hi = m_events->m_numKeys - 1;
        while (lo <= hi) {
            mid = (lo + hi) / 2;
            int cmp = CompareKey(m_events->m_keys[mid], key, pass);
            if (cmp < 0) {
                lo = mid + 1;
            } else if (cmp > 0) {
                hi = mid - 1;
            } else {
                break;
            }
        }
        if (lo <= hi) {
            *parms = m_events->m_parms[m_events->m_keys[mid].m_parms];
            return true;
        }
    }
    return false;
}

bool CSoundTable::GetSoundEvent(int id, CSoundTable::SSoundParms* parms) const {
    SAnimFile* anims = m_anims;
    if (anims == NULL) {
        return false;
    }
    s16 lo = 0;
    s16 mid = 0;
    s16 hi = anims->m_count - 1;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        int cmp = anims->m_entries[mid].m_id - id;
        if (cmp < 0) {
            lo = mid + 1;
        } else if (cmp > 0) {
            hi = mid - 1;
        } else {
            break;
        }
    }
    if (lo <= hi) {
        *parms = anims->m_entries[mid].m_parms;
        return true;
    }
    return false;
}
