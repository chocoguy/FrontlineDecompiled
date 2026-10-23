// EA sound library internals: the global state (sndgs) and the internal calls shared by the
// library's units. Library sources are C++ (mangled internal names); the public API is C.
#ifndef _EA_SND_SNDI_H_
#define _EA_SND_SNDI_H_

#include "types.h"
#include "ea/real.h"

// The sound library keeps functions out of line unless they are declared inline.
#pragma auto_inline off

// 64-bit value as two words (iSNDmulu64/iSNDdivu64).
struct SINT64 {
    u32 lo;
    u32 hi;
};

// Doubly linked list with a count (slinklst).
struct SNDLINKNODE {
    SNDLINKNODE* next;
    SNDLINKNODE* prev;
};
struct SNDLINKLIST {
    SNDLINKNODE* head;
    SNDLINKNODE* tail;
    s32 count;
};

// Sound memory pool (smemman): allocations grow up from `base`, their table of
// {offset, size} entries grows down from the end, sorted by offset. Entry 0 is the top word
// pair; entries are numbered 0, -1, ... and `count` holds minus the number of entries.
struct SNDMEMENTRY {
    s32 offset;
    s32 size;
};
// The header of the sound memory block (SNDMEMI_init): the allocations start
// 0x18 bytes in, so the pointers are 32-bit natively too.
struct SNDMEMPOOL {
    /* 0x00 */ PTR32(u8) base;
    /* 0x04 */ PTR32(SNDMEMENTRY) table;
    /* 0x08 */ s32 size;
    /* 0x0C */ s32 limit;     // end of allocatable space (shrinks as the table grows)
    /* 0x10 */ s32 lowwater;  // least free space seen
    /* 0x14 */ s32 count;     // -(number of allocations)
};
FILE_SIZE_ASSERT(SNDMEMPOOL, 0x18);

// Envelope segment: reach `level` after `time` 100 Hz ticks.
struct SNDENVSEG {
    s32 time;
    s32 level;
};

// A playing sound (sndgs.voices[], 0x80 bytes).
struct CHANPUB {
    /* 0x00 */ s32 handle;  // < 0: free
    /* 0x04 */ s16 chanVoice[4];  // platform voice per channel
    /* 0x0C */ s16 patch;
    /* 0x0E */ s16 bank;  // -1: not from a bank
    /* 0x10 */ s32 endFrame;
    /* 0x14 */ s32 unk_0x14;  // > 0: not started on the platform yet
    /* 0x18 */ u16 azimuth;
    /* 0x1A */ s16 distance;
    /* 0x1C */ u16 rate;
    /* 0x1E */ u8 unk_0x1E;
    /* 0x1F */ u8 channels;
    /* 0x20 */ u16 renderMode;
    /* 0x22 */ u8 unk_0x22;  // patch group: voices sharing it are controlled together
    /* 0x23 */ u8 unk_0x23;  // set on the group's master voice
    /* 0x24 */ s16 master;  // first voice of a multichannel sound; -1 on the first itself
    /* 0x26 */ u8 priority;
    /* 0x28 */ u32 allocTime;  // sndgs.serverTicks when allocated or freed
    /* 0x2C */ s32 volStep;  // autovol ramp per tick
    /* 0x30 */ s32 volTarget;
    /* 0x34 */ s32 vol;  // 16.16
    /* 0x38 */ s32 envStep;
    /* 0x3C */ s32 envLevel;  // 16.16
    /* 0x40 */ s32 envTicks;  // ticks left in the segment
    /* 0x44 */ s8 unk_0x44;  // volume from the play options
    /* 0x45 */ s8 unk_0x45;
    /* 0x46 */ u16 azimuthBase;  // added to SND3dpos azimuths
    /* 0x48 */ u16 chanAzimuth[4];  // per channel
    /* 0x50 */ s8 envNumSegs;
    /* 0x51 */ s8 envSeg;
    /* 0x52 */ u8 unk_0x52;
    /* 0x53 */ u8 unk_0x53;
    /* 0x54 */ s8 fxSend[1];   // per bus, from the patch
    /* 0x55 */ s8 fxLevel[1];  // per bus, SNDfxlevel
    /* 0x56 */ s16 fxOut[1];   // per bus: bus level * send * level >> 6
    /* 0x58 */ u8 unk_0x58;
    /* 0x59 */ u8 volLfoLen;
    /* 0x5A */ u8 pitchLfoLen;
    /* 0x5B */ u8 volLfoPos;
    /* 0x5C */ u8 pitchLfoPos;
    /* 0x5D */ u8 state;  // 0 free, 1 playing, 2 released (master kept for its other channels)
    /* 0x5E */ u16 timeMult;
    /* 0x60 */ SNDENVSEG* env;
    /* 0x64 */ u8* volCurve;     // maps the computed volume
    /* 0x68 */ u8* unk_0x68;     // maps unk_0x58 for the detune
    /* 0x6C */ s8* volLfo;
    /* 0x70 */ s8* pitchLfo;
    /* 0x74 */ s16 pitchLfoDepth;  // cents at full swing (<< 6)
    /* 0x76 */ s16 unk_0x76;     // cents per unit of unk_0x58 - 64 (<< 6)
    /* 0x78 */ s16 detune;       // cents
    /* 0x7A */ u16 linearPitch;  // 0: recompute from the detune (4.12)
    /* 0x7C */ u16 pitchMult;
    /* 0x7E */ u16 pitchMult2;  // AEMS players
};
typedef CHANPUB SNDVOICE;


// Tag stream reader (SNDI_gettag): tags are a byte id, a byte size (0xFF: a 4-byte size
// follows) and the data. 0xFC is padding, 0xFD/0xFE have no data, 0xFF ends the stream.
struct SNDTAGINFO {
    /* 0x00 */ u8* ptr;
    /* 0x04 */ s32 tag;
    /* 0x08 */ s32 value;  // the data as a big-endian number, if size <= 4
    /* 0x0C */ u8* data;
    /* 0x10 */ u32 size;
};

// Sample bank (.bnk version 5) header.
struct BANKVER5 {
    /* 0x00 */ u8 unk_0x0[0x6];
    /* 0x06 */ u16 numPatches;
    /* 0x08 */ s32 headerSize;  // sample data follows the header
    /* 0x0C */ s32 sampleSize;  // bytes of sample data (downloaded to ARAM)
    /* 0x10 */ u8 unk_0x10[0x4];
    /* 0x14 */ s32 patchOffsets[1];  // patch i at &patchOffsets[i] + patchOffsets[i]; 0: none
};

struct TAGGEDPATCH;

// A loaded bank (sndgs.banks[]).
struct SNDBANKSLOT {
    /* 0x0 */ BANKVER5* data;
    /* 0x4 */ void* aram;  // sample data in ARAM
    /* 0x8 */ s8 busy;     // being added
};

// One timbre of a patch, parsed from its tags (SNDI_parsetimbre). Field comments give the
// source tag; pointer fields are the tag's data address plus its value. size 0xC0
struct SNDIPATCHHEADER {
    /* 0x00 */ s16 unk_0x0;    // tag 0x24
    /* 0x02 */ u8 unk_0x2;     // tag 0x80 (default 2)
    /* 0x03 */ u8 channels;    // tag 0x82 (default 1)
    /* 0x04 */ s16 unk_0x4;    // tag 0x10
    /* 0x06 */ s16 unk_0x6;    // tag 0x11
    /* 0x08 */ s8 velLo;   // tag 0x01: plays for velocities velLo..velHi
    /* 0x09 */ s8 velHi;   // tag 0x02 (default 0x7F)
    /* 0x0A */ s8 keyLo;   // tag 0x03: and keys keyLo..keyHi
    /* 0x0B */ s8 keyHi;   // tag 0x04 (default 0x7F)
    /* 0x0C */ u8 unk_0xC;     // tag 0x06
    /* 0x0D */ u8 rootKey;   // tag 0x07 (default 60)
    /* 0x0E */ u8 unk_0xE;     // tag 0x08 (default -1)
    /* 0x0F */ u8 unk_0xF;     // tag 0x09 (default 1)
    /* 0x10 */ u8 unk_0x10;    // tag 0x0A
    /* 0x11 */ u8 pan;       // tag 0x0C (default 0x40)
    /* 0x12 */ u8 panRand;   // tag 0x0D
    /* 0x13 */ u8 unk_0x13;    // tag 0xA0 (default 7)
    /* 0x14 */ u8 vol;       // tag 0x0E (default 0x7F)
    /* 0x15 */ u8 volRand;   // tag 0x0F
    /* 0x16 */ u8 unk_0x16;    // tag 0x13
    /* 0x17 */ s8 numUserData; // tag 0x14, repeatable
    /* 0x18 */ u16 renderMode; // tag 0x8C (default 0x200); also the resolve mask in SNDBANKI_asyncresolvepatch
    /* 0x1A */ u8 unk_0x1A;    // tag 0x1C (default 0x7F)
    /* 0x1B */ u8 unk_0x1B;    // tag 0x1E
    /* 0x1C */ u8 unk_0x1C;    // tag 0x1F
    /* 0x1D */ u8 unk_0x1D;    // tag 0x21
    /* 0x1E */ u8 unk_0x1E;    // tag 0x23
    /* 0x1F */ u8 unk_0x1F;    // tag 0x25 (default 1)
    /* 0x20 */ s32 unk_0x20;   // tag 0x12
    /* 0x24 */ s32 unk_0x24;   // tag 0x17
    /* 0x28 */ SNDENVSEG* env; // tag 0x19 (default snddefaultenvelope)
    /* 0x2C */ s32 unk_0x2C;   // tag 0x1D
    /* 0x30 */ s32 unk_0x30;   // tag 0x20
    /* 0x34 */ void* userData[4];
    /* 0x44 */ s32 userDataSize[4];
    /* 0x54 */ s16 unk_0x54;   // tag 0x22
    /* 0x56 */ u16 rate;       // tag 0x84 (default 24000)
    /* 0x58 */ s32 samples;    // tag 0x85
    /* 0x5C */ s32 loopStart;  // tag 0x86 (default -1)
    /* 0x60 */ s32 loopEnd;    // tag 0x87 (default -1): < 0 one-shot
    /* 0x64 */ s32* aramAddr;  // tag 0x8A data: where the bank's sample data was put
    /* 0x68 */ s32 offset[4];  // tags 0x88, 0x89, 0x94, 0x95: per channel sample data
    /* 0x78 */ s32 unk_0x78;   // tag 0x1A (default -1)
    /* 0x7C */ void* data[4];  // tags 0x98-0x9B: per channel (time stretch tables)
    /* 0x8C */ s16 dataSize[4];
    /* 0x94 */ s16 azimuth[4];   // per channel: speaker layout + tags 0x9C-0x9F
    /* 0x9C */ void* adpcm[4];      // tags 0x8F, 0x90, 0x91: DSP ADPCM coefficients
    /* 0xAC */ void* adpcmLoop[4];  // tags 0x8D, 0x8E, 0x8B: loop context
    /* 0xBC */ void* start;     // the timbre's tags
};
struct SNDSAMPLEFORMAT {
    /* 0x0 */ u16 rate;
    /* 0x2 */ u8 channels;
    /* 0x3 */ u8 unk_0x3;
};
// Sample attributes of a timbre/stream. size 0x54
struct SNDSAMPLEATTR {
    /* 0x00 */ s16 unk_0x0;
    /* 0x02 */ u8 unk_0x2;
    /* 0x03 */ u8 unk_0x3;
    /* 0x04 */ u8 unk_0x4;
    /* 0x05 */ u8 unk_0x5;
    /* 0x06 */ s8 unk_0x6;
    /* 0x07 */ u8 unk_0x7;
    /* 0x08 */ u16 renderMode;
    /* 0x0A */ u8 unk_0xA[0x2];
    /* 0x0C */ u16 unk_0xC[4];
    /* 0x14 */ void* data[4];
    /* 0x24 */ s32 size[4];
    /* 0x34 */ void* userData[4];  // zero terminated
    /* 0x44 */ s32 userDataSize[4];
};
// size 0x14
struct SNDSAMPLEDESC {
    /* 0x0 */ u32 samples;
    /* 0x4 */ s32 offset[4];  // per channel sample data (bank relative for RAM sounds)
};

struct SNDFILTERDEF {
    /* 0x00 */ u8 unk_0x0[0x10];
    /* 0x10 */ void* unk_0x10;  // set: the filter is in use
    /* 0x14 */ u8 unk_0x14[0x4];
};

// Effects bus settings (SNDfxinitbus). size 0x14
struct SNDFXBUS {
    /* 0x00 */ u16 type;
    /* 0x02 */ u8 level;  // master level
    /* 0x04 */ s32 param1;
    /* 0x08 */ s32 param2;
    /* 0x0C */ u8 unk_0xC[0x8];
};

typedef void (*SNDCLIENT)(void);
typedef void (*SNDUSERDATACLIENT)(SNDUSERDATACBINFO* info);

// Library state. size 0x238
struct SNDGS {
    /* 0x000 */ SNDSYSOPTS opts;
    /* 0x0D0 */ SNDSYSCAPS caps;
    /* 0x168 */ s8 inited;
    /* 0x169 */ u8 masterVol;
    /* 0x16A */ u8 unk_0x16A;
    /* 0x16B */ u8 critical;  // entercritical depth
    /* 0x16C */ s8 num100hzClients;
    /* 0x16D */ s8 numServerClients;
    /* 0x16E */ s8 numUserDataClients;
    /* 0x16F */ u8 unk_0x16F;  // mixer resampling quality
    /* 0x170 */ s16 numVoices;
    /* 0x174 */ s32 serverTicks;
    /* 0x178 */ SNDCLIENT client100hz[6];
    /* 0x190 */ SNDCLIENT serverClients[6];
    /* 0x1A8 */ SNDUSERDATACLIENT userDataClients[4];
    /* 0x1B8 */ void (*restoreHook0)(void);
    /* 0x1BC */ void (*restoreHook1)(void);
    /* 0x1C0 */ void (*restoreHook2)(void);
    /* 0x1C4 */ void (*restoreHook3)(int);
    /* 0x1C8 */ void (*restoreHook4)(void);
    /* 0x1CC */ void (*restoreHook5)(void);
    /* 0x1D0 */ void (*restoreHook6)(void);
    /* 0x1D4 */ SNDVOICE* voices;
    /* 0x1D8 */ SNDBANKSLOT* banks;  // caps.maxBanks entries
    /* 0x1DC */ SNDFXBUS fxbus[2];  // bus 0: type 4 at [0], type 0x200 at [1]
    /* 0x204 */ SNDMEMPOOL* mem;
    /* 0x208 */ u16 chanAzimuth[4][4];  // [channels - 1][channel]: default speaker positions
    /* 0x228 */ void (*premix)(void);   // called by the driver thread around each slice
    /* 0x22C */ void (*postmix)(void);
    /* 0x230 */ u8 unk_0x230[0x8];
};
SIZE_ASSERT(SNDGS, 0x238);
OFFSET_ASSERT(SNDGS, opts.caps.speakers, 0x48);
OFFSET_ASSERT(SNDGS, opts.caps.renderModes, 0x50);
OFFSET_ASSERT(SNDGS, opts.abortmsg, 0xC4);
OFFSET_ASSERT(SNDGS, caps.speakerAz, 0x128);
OFFSET_ASSERT(SNDGS, voices, 0x1D4);
OFFSET_ASSERT(SNDGS, fxbus[1], 0x1F0);
OFFSET_ASSERT(SNDGS, mem, 0x204);
SIZE_ASSERT(SNDVOICE, 0x80);
OFFSET_ASSERT(SNDVOICE, allocTime, 0x28);
OFFSET_ASSERT(SNDSAMPLEATTR, data, 0x14);
SIZE_ASSERT(SNDSAMPLEATTR, 0x54);
SIZE_ASSERT(SNDIPATCHHEADER, 0xC0);
OFFSET_ASSERT(SNDIPATCHHEADER, userData, 0x34);
OFFSET_ASSERT(SNDIPATCHHEADER, azimuth, 0x94);
OFFSET_ASSERT(SNDIPATCHHEADER, start, 0xBC);
OFFSET_ASSERT(SNDGS, chanAzimuth, 0x208);
OFFSET_ASSERT(SNDVOICE, env, 0x60);
OFFSET_ASSERT(SNDVOICE, state, 0x5D);

extern SNDGS sndgs;            // sdata.c
extern s16 sndvoicereserved[4];  // salloc.c
extern SNDENVSEG snddefaultenvelope[1];  // stpparse.c
extern char sndlibauthor[];    // sauthor.c

// Internal calls (C++ linkage).
int SNDPLATFORM_outputcaps();
int SNDPLATFORM_outputset();
int SNDPLATFORM_init();
int SNDPLATFORM_restore();
int SNDPLATFORM_outputlatency();
void SNDPLATFORM_setvol(int voice);
int SNDPLATFORM_setpitch(int voice);
void SNDI_randomseed(unsigned int seed);
void SNDI_mutexalloc();
void SNDI_mutexfree();
void SNDI_mutexlock();
void SNDI_mutexunlock();
void SNDI_precalcaztospkrvol();
void iSNDserve();
void iSNDcalcpitch(int voice);
int iSNDdetunetolinear(int cents);
void iSNDcalcvol(int voice);
void SNDREAL_exithandler();
int SNDI_getb(void* data, int size);
int SNDI_gettag(SNDTAGINFO* info);
unsigned int iSNDrandom();
int randrange(int range);
int iSNDsin(int angle);
int SNDI_pantoazimuth(int pan);
float SNDI_sin(float x);
float SNDI_cos(float x);
float SNDI_rootof1plusx(float x);
int SNDI_findprime(int ms, int rate);
int SNDI_findmult16(int ms, int rate);
void SNDMEMI_init(void* mem, int size);
int SNDMEMI_restore();
void* SNDMEMI_allocz(int size);
void SNDMEMI_free(void* ptr);
void SNDLINKI_init(SNDLINKLIST* list);
void SNDLINKI_push(SNDLINKLIST* list, SNDLINKNODE* node);
void SNDLINKI_pushtail(SNDLINKLIST* list, SNDLINKNODE* node);
SNDLINKNODE* SNDLINKI_pop(SNDLINKLIST* list);
void SNDLINKI_remove(SNDLINKLIST* list, SNDLINKNODE* node);
SINT64 iSNDmulu64(unsigned int a, unsigned int b);
unsigned int iSNDdivu64(SINT64 value, int divisor);
void iSNDserveraddclient(void (*client)());
void iSNDserverremoveclient(void (*client)());
void SNDSYSI_100hzserver();
int SNDI_validrendermode(int* mode, SNDIPATCHHEADER* header);
int SNDBANKI_alloc();
void* SNDBANKI_getppatch(BANKVER5* bank, int patch);
void SNDBANKI_userdatacallback(SNDIPATCHHEADER* header, int type);
int SNDBANKI_valid(int bank);
int SNDBANKI_asyncresolvepatch(int mask, TAGGEDPATCH* patch, char* aram, int* state);
int SNDBANKI_playpatch(void* bank, TAGGEDPATCH* patch, int bankIndex, int patchIndex, SNDPLAYOPTS* opts);
int SNDBANKI_playtimbre(int bankIndex, int patchIndex, void* bank, SNDPLAYOPTS* opts, SNDIPATCHHEADER* header,
                        int key, int vel, int detune);
int SNDPLATFORM_playtimbre(SNDIPATCHHEADER* header, void* bank, int voice, int timemult, int a, int lowpass,
                           int highpass);
void SNDI_aztospkrvol(int azimuth, s16* vols);
void SNDARAM_init(int max);
void SNDARAM_setpool(unsigned int base, int size);
int SNDARAM_alloc(int size);
void SNDARAM_free(unsigned int addr);
void SNDARAM_restore();
int SNDDRV_getmastervoice(int voice);
int SNDDRV_getsamplechan(int voice);
int SNDI_parsetimbre(void** data, SNDIPATCHHEADER* header);
void SNDI_patchtohdr(void* bank, TAGGEDPATCH* patch, SNDSAMPLEFORMAT* format, SNDSAMPLEATTR* attr,
                     SNDSAMPLEDESC* desc);
void* SNDPLATFORM_memalloc(int type, int size);
int SNDPLATFORM_memfree(int type, unsigned int addr);
int SNDPLATFORM_download(int type, void* src, void* dest, int size);
int SNDPLATFORM_downloadcomplete(int handle);
int SNDPLATFORM_asyncresolvetimbre(SNDIPATCHHEADER* header, char* aram, int* state);
int SNDVOICEI_get(int handle);
int SNDVOICEI_alloc(int channels, int priority, int* handle, int lo, int hi);
void SNDVOICEI_free(int voice);
void SNDI_calcfxlevel(int bus, int voice);
int iSNDpatchkey(int voice, int* key);
int SNDPLATFORM_fxinit(int bus, int type);
int SNDPLATFORM_setfxlevel(int voice, int bus);
void SNDPLATFORM_setdrylevel(int voice);
SNDFXBUS* SNDCTRLI_getfxbus(int bus, int type);
int SNDPLATFORM_packetoverhead();
int SNDPLATFORM_packetplaycreate(int player, void* buffer);
int SNDPLATFORM_packetplay(int player, int voice, int a, int b, int c, int d, SNDSAMPLEFORMAT* format,
                           void** data);
int SNDPLATFORM_packetplaydestroy(int player);
void SNDPLATFORM_getvoicerange(int renderMode, int* lo, int* hi);
void SNDPLATFORM_set3dpos(int voice);
void SNDPLATFORM_lowpass(int voice, int freq);
void SNDPLATFORM_highpass(int voice, int freq);
int SNDPLATFORM_timemult(int voice, int mult);
int SNDPLATFORM_getcurframe(int voice);
int SNDPLATFORM_stop(int voice);
int SNDPLATFORM_filteradd(int voice, SNDFILTERDEF* filter);

extern "C" {
void SNDSYS_entercritical(void);
void SNDSYS_leavecritical(void);
void SNDSYS_service(void);
int SNDMEM_gethighwater(void);
int SNDfxinitbus(int bus, int level, int type, int param1, int param2);
int SNDfxmasterlevel(int bus, int level);
int SNDfxlevel(int handle, int bus, int level);
void SNDstopall(void);
int SNDover(int handle);
int SNDCTRL_filteradd(int handle, SNDFILTERDEF* filter);
int SNDCTRL_lowpass(int handle, int freq);
int SNDCTRL_highpass(int handle, int freq);
int SNDCTRL_timemult(int handle, int mult);
int SNDCTRL_drylevel(int handle, int level);
int SND3dpos(int handle, int azimuth, int distance);
int SND_attrsetdef(SNDSAMPLEATTR* attr);
void SNDI_memcpy(void* dest, const void* src, int size);
}

#ifdef FRONTLINE_NATIVE
#include "native/swap.h"

// Sample banks are big-endian ('BNKb'). Patches are tag streams, read a byte
// at a time (SNDI_gettag), so they stay as they are; the native build swaps the
// header and the patch data the CPU reads as numbers: envelopes, and the DSP
// ADPCM coefficients and loop context the AX voices are given. Sample data goes
// to ARAM as is (the platform's DSP decoder reads it big-endian).

// The fixed header (the async loader reads the first block on its own).
// Returns false if it was already swapped: the magic becomes EA's little-endian
// 'BNKl'.
inline bool SNDBankHeaderFromBigEndian(BANKVER5* bank) {
    u8* b = bank->unk_0x0;
    if (b[0] != 'B' || b[1] != 'N' || b[2] != 'K' || b[3] != 'b') {
        return false;
    }
    b[3] = 'l';
    NativeSwap16(&bank->numPatches);
    NativeSwap32s(&bank->headerSize, 3);
    return true;
}

// SNDI_getb, here so the swap needs nothing linked.
inline int SNDBankGetb(const u8* p, int size) {
    int value = 0;
    for (int n = 0; n < size; n++) {
        value = (value << 8) + p[n];
    }
    if (size > 0 && size < 4 && value >= (1 << (size * 8 - 1))) {
        value -= 1 << (size * 8);
    }
    return value;
}

// The rest of the header. Call once per load, after SNDBankHeaderFromBigEndian
// and with the whole header in memory.
inline void SNDBankFromBigEndian(BANKVER5* bank) {
    NativeSwapOnce once;
    NativeSwap32s(bank->patchOffsets, bank->numPatches);
    for (int i = 0; i < bank->numPatches; i++) {
        if (bank->patchOffsets[i] == 0) {
            continue;
        }
        // The tags after the patch's 4-byte header, timbre by timbre (as SNDI_parsetimbre).
        u8* p = (u8*)&bank->patchOffsets[i] + bank->patchOffsets[i] + 4;
        u8* env = 0;
        int envSegs = 1;
        for (;;) {
            while (*p == 0xFC) {
                p++;
            }
            u8 tag = *p++;
            if (tag == 0xFE || tag == 0xFF) {
                if (env != 0) {
                    once.swap32s(env, envSegs * 2);
                }
                if (tag == 0xFF) {
                    break;
                }
                env = 0;
                envSegs = 1;
                continue;
            }
            if (tag == 0xFD) {
                continue;
            }
            u32 size = *p;
            if (size == 0xFF) {
                size = SNDBankGetb(p + 1, 4);
                p += 4;
            }
            p++;
            if (tag == 0x19) {  // envelope: at the data plus its value
                env = p + SNDBankGetb(p, size);
            } else if (tag == 0x09) {  // its segments
                envSegs = SNDBankGetb(p, size);
            } else if (tag == 0x8F || tag == 0x90 || tag == 0x91) {  // ADPCM coefficients, then a byte
                once.swap16s(p, 16);
            } else if (tag == 0x8D || tag == 0x8E || tag == 0x8B) {  // loop yn1, yn2, then a byte
                once.swap16s(p, 2);
            }
            p += size;
        }
    }
}
#endif

#endif
