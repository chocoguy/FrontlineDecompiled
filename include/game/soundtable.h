// soundtable: the level's sound tables (loaded from the level big file):
// script sounds keyed by (bank, variant, sound), and animation-triggered
// sounds keyed by id. Both files are little-endian and hold offsets that
// Init turns into pointers. Hand-maintained.
#ifndef _GAME_SOUNDTABLE_H_
#define _GAME_SOUNDTABLE_H_

#include "types.h"
#include "game_types.h"

class CSoundTable;

// size 0x8
class CSoundTable {
public:
    // A resolved sound event: how PlayScriptedSound plays it (size 0xE; the
    // table entries are an s16 id followed by one of these).
    struct SSoundParms {
        /* 0x0 */ s8 m_variantMin;  // event word 2 is random in [min, max]
        /* 0x1 */ s8 m_variantMax;
#ifdef FRONTLINE_NATIVE
        // clang allocates bitfields from the low bit, CodeWarrior from the
        // high bit: declared in reverse so the file's bits line up.
        /* 0x2 */ s8 m_category : 4;  // event word 1
        /* 0x2 */ s8 m_positional : 1;  // placed relative to the listener
        /* 0x2 */ s8 m_scheduled : 1;  // tracked by g_SoundSchedule
        /* 0x2 */ s8 m_flag40 : 1;
        /* 0x2 */ s8 m_altEvent : 1;  // AEMS event 0xE instead of 7
#else
        /* 0x2 */ s8 m_altEvent : 1;  // AEMS event 0xE instead of 7
        /* 0x2 */ s8 m_flag40 : 1;
        /* 0x2 */ s8 m_scheduled : 1;  // tracked by g_SoundSchedule
        /* 0x2 */ s8 m_positional : 1;  // placed relative to the listener
        /* 0x2 */ s8 m_category : 4;  // event word 1
#endif
        /* 0x4 */ s16 m_randomMin;  // event word 3 is random in [min, max]
        /* 0x6 */ s16 m_randomMax;
        /* 0x8 */ s16 m_volume;
        /* 0xA */ s16 m_rangeScale;  // distance scale, 1/256
        /* 0xC */ s16 m_looped;
    };
    // Sorted by bank, sound, then variant. size 0x8
    struct SKey {
        /* 0x0 */ s16 m_bank;
        /* 0x2 */ s16 m_variant;  // -1: any
        /* 0x4 */ s16 m_sound;
        /* 0x6 */ s16 m_parms;  // index into the parameters
    };
    // Sorted by id. size 0x10
    struct SAnimEntry {
        /* 0x0 */ s16 m_id;
        /* 0x2 */ SSoundParms m_parms;
    };
    // Header of the script sound file.
    struct SEventFile {
        /* 0x0 */ u32 m_magic;
        /* 0x4 */ u32 unk_0x4[3];
        /* 0x10 */ s32 m_numKeys;
        /* 0x14 */ s32 m_numParms;
        /* 0x18 */ PTR32(SKey) m_keys;
        /* 0x1C */ PTR32(SSoundParms) m_parms;
    };
    // Header of the animation sound file.
    struct SAnimFile {
        /* 0x0 */ u32 m_magic;
        /* 0x4 */ u32 unk_0x4[3];
        /* 0x10 */ s32 m_count;
        /* 0x14 */ PTR32(SAnimEntry) m_entries;
    };
    /* 0x0 */ SEventFile* m_events;
    /* 0x4 */ SAnimFile* m_anims;

    CSoundTable() : m_events(NULL) {}
    // The animation sound `id`.
    bool GetSoundEvent(int id, CSoundTable::SSoundParms* parms) const;  // 0x800EDE84
    // The script sound; a variant missing from the table falls back to the
    // wildcard entry, then to any variant.
    bool GetSoundEvent(int bank, int variant, int sound, CSoundTable::SSoundParms* parms) const;  // 0x800EDF78
    void Init(int mission, int stage);  // 0x800EE164
    void EndianSwap();  // 0x800EE474
    ~CSoundTable();  // 0x800EE6CC
};

extern CSoundTable g_soundMapSingleton;  // 0x8034ECE8

#endif
