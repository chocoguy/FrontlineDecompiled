// AEMS: EA's event-driven sound effect system. A system file maps game events (with
// parameters) onto modules in module banks; each module is a graph of components (inputs,
// triggers, math, envelopes, players...) instantiated per event and updated at 100 Hz or
// when an event changes. All files are relocated in place when added.
//
// Pointer fields are PTR32: in the files they hold offsets, and the runtime
// structs are allocated and laid out with GameCube sizes (aemscompinfo's sizes,
// SNDMEMI_allocz(0xC), `count << 2`). Natively the files are big-endian; see
// AEMSSystemFromBigEndian and AEMSBankFromBigEndian.
#ifndef _EA_SND_AEMS_H_
#define _EA_SND_AEMS_H_

#include "ea/snd/sndi.h"

// Component kinds (index into aemscompinfo).
enum {
    AEMSCOMP_INPARAM = 0x1,
    AEMSCOMP_TRIGGER = 0x2,
    AEMSCOMP_TRIGGER2 = 0x3,
    AEMSCOMP_PLAYER = 0x4,
    AEMSCOMP_RANGETRIG = 0x5,
    AEMSCOMP_ENVELOPE = 0x6,
    AEMSCOMP_COUNTER = 0x7,
    AEMSCOMP_RANDOM = 0x8,
    AEMSCOMP_MUX = 0x9,
    AEMSCOMP_DEMUX = 0xA,
    AEMSCOMP_ADD = 0xB,
    AEMSCOMP_SUBTRACT = 0xC,
    AEMSCOMP_SCALE = 0xD,
    AEMSCOMP_MULTIPLY = 0xE,
    AEMSCOMP_DIVIDE = 0xF,
    AEMSCOMP_MODULO = 0x10,
    AEMSCOMP_TABLE = 0x11,
    AEMSCOMP_DELAYTRIG = 0x12,
    AEMSCOMP_DELAYLINE = 0x13,
    AEMSCOMP_STATIC = 0x14,
    AEMSCOMP_OSCILLATOR = 0x15,
    AEMSCOMP_WARP = 0x16,
    AEMSCOMP_MAX = 0x17,
    AEMSCOMP_MIN = 0x18,
    AEMSCOMP_STATEGEN = 0x19,
    AEMSCOMP_DESTROY = 0x1C,
    AEMSCOMP_SEND = 0x1D,
    AEMSCOMP_TABLE2 = 0x1E,
    AEMSCOMP_RAMP = 0x1F,
    AEMSCOMP_MERGE = 0x20,
};

struct MODULE;
struct AEMSCOMPDYNAMIC;

// Per kind: update function, static and dynamic sizes, and where outputs come from
// (0: the static value at +0xC, 1: the dynamic out[] at +0x10, 2: the array *(+0x10)).
struct AEMSCOMPINFO {
    void (*update)(AEMSCOMPDYNAMIC* d, MODULE* module);
    u8 staticSize;
    u8 dynamicSize;
    u16 outputKind;
};

extern AEMSCOMPINFO aemscompinfo[33];

// An output's connections: component and input numbers.
struct AEMSTARGET {
    u16 comp;
    u16 input;
};
struct AEMSOUTPUT {
    /* 0x0 */ PTR32(AEMSTARGET) targets;
    /* 0x4 */ s16 count;
};

// A component in a module bank (aemscompinfo[type].staticSize bytes).
struct AEMSCOMP {
    /* 0x00 */ u32 hash;
    /* 0x04 */ u8 type;
    /* 0x05 */ u8 numInputs;
    /* 0x06 */ u8 numOutputs;
    /* 0x08 */ PTR32(AEMSOUTPUT) outputs;
    /* 0x0C */ s32 p[5];  // kind specific
};

// Live copy of one connection: *dst = *src after the component updates.
struct AEMSLINK {
    PTR32(s32) src;
    PTR32(s32) dst;
};

// A component's state in an instance (aemscompinfo[type].dynamicSize bytes).
struct AEMSCOMPDYNAMIC {
    /* 0x00 */ PTR32(AEMSCOMP) comp;
    /* 0x04 */ u8 connected;  // inputs wired so far (sorting); warps start at 1
    /* 0x05 */ u8 scheduled;
    /* 0x06 */ u16 numOutputs;
    /* 0x08 */ PTR32(AEMSLINK) outputs;
    /* 0x0C */ PTR32(s32) in;
    /* 0x10 */ s32 out;
    /* 0x14 */ s32 s[10];  // kind specific
};

struct MODULEBANK;

// Players: start/stop sounds (bank patches 0x4000, streams 0xC000, 0x8000 not linked here).
struct AEMSSTREAMREQ;
struct AEMSCOMPDYNAMICPLAYER {
    /* 0x00 */ PTR32(AEMSCOMP) comp;
    /* 0x04 */ u8 connected;
    /* 0x05 */ u8 scheduled;
    /* 0x06 */ u16 numOutputs;
    /* 0x08 */ PTR32(AEMSLINK) outputs;
    /* 0x0C */ PTR32(s32) in;  // pitch, timemult, vol, azimuth, distance, fx, index, gate, lowpass, highpass, dry
    /* 0x10 */ s32 out;  // playing
    /* 0x14 */ s32 timeLeft;  // ms
    /* 0x18 */ s32 position;  // ms
    /* 0x1C */ s32 handle;
    /* 0x20 */ s32 handle2;  // second stream request
    /* 0x24 */ s32 stream;
    /* 0x28 */ PTR32(AEMSSTREAMREQ) req;
    /* 0x2C */ u16 entry;  // kind | index
    /* 0x2E */ u8 lastGate;
    /* 0x30 */ u16 pitch;
    /* 0x32 */ u16 timemult;
    /* 0x34 */ u16 azimuth;
    /* 0x36 */ s16 distance;
    /* 0x38 */ u16 vol;
    /* 0x3A */ u16 fx;
    /* 0x3C */ u16 lowpass;
    /* 0x3E */ u16 highpass;
    /* 0x40 */ u16 dry;
};

// Player component data: which sound to play for each index.
struct AEMSPLAYERTABLE {
    PTR32(u32) entries;
    s16 count;
};

// An instance of a module (one per event handle); the component states follow at 0x1C.
struct MODULEINSTANCE {
    /* 0x00 */ s32 handle;
    /* 0x04 */ u8 dirty;  // update on the next pass
    /* 0x08 */ PTR32(u8) dyn;
    /* 0x0C */ s32 numExec;
    /* 0x10 */ PTR32(PTR32(AEMSCOMPDYNAMIC)) exec;  // components in update order (the links follow)
    /* 0x14 */ PTR32(AEMSLINK) links;
    /* 0x18 */ PTR32(AEMSCOMPDYNAMIC) destroy;  // its input > 0 destroys the instance
};

struct MODULERT {
    /* 0x0 */ PTR32(PTR32(MODULEINSTANCE)) instances;
    /* 0x4 */ s16 count;
    /* 0x8 */ PTR32(s32) compOffsets;  // dynamic block offset per component
};

// A module (0x14 bytes; 0x1C from bank version 8, already in update order).
struct MODULE {
    /* 0x00 */ u16 id;
    /* 0x02 */ s16 numInstances;
    /* 0x04 */ s16 numComps;
    /* 0x06 */ s16 updateType;  // 0 timer, 1 events
    /* 0x08 */ s32 event;
    /* 0x0C */ PTR32(AEMSCOMP) comps;
    /* 0x10 */ PTR32(MODULERT) rt;
    /* 0x14 */ s32 numLinks;  // version 8+
    /* 0x18 */ s32 numExec;
};

struct MODULEBANKRT {
    /* 0x0 */ s32 sfxBank;
    /* 0x4 */ s32 midiBank;
    /* 0x8 */ PTR32(char) streamFile;
    /* 0xC */ s32 streamOffset;
};

// Stream player entries (version 5+).
struct AEMSSTREAMENTRY {
    /* 0x0 */ u16 hold;
    /* 0x2 */ u8 priority;
    /* 0x3 */ u8 flags;  // 1: a second part follows
    /* 0x4 */ s32 offset;
    /* 0x8 */ s32 offset2;
};

struct AEMSBANKENTRY {
    /* 0x0 */ s32 unk_0x0;
    /* 0x4 */ s32 unk_0x4;
    /* 0x8 */ PTR32(void) data;
};

struct MODULEBANK {
    /* 0x00 */ u16 unk_0x0;
    /* 0x02 */ u8 version;
    /* 0x04 */ s32 size;
    /* 0x08 */ s32 unk_0x8;
    /* 0x0C */ s32 numModules;
    /* 0x10 */ PTR32(MODULE) modules;
    /* 0x14 */ u16 bankId;
    /* 0x16 */ u16 unk_0x16;
    /* 0x18 */ s16 numEntries;
    /* 0x1C */ s32 sfxBankOffset;  // sample banks inside the file
    /* 0x20 */ s32 sfxBankSize;
    /* 0x24 */ s32 midiBankOffset;
    /* 0x28 */ s32 midiBankSize;
    /* 0x2C */ PTR32(AEMSBANKENTRY) entries;
    /* 0x30 */ PTR32(MODULEBANKRT) rt;
    /* 0x34 */ PTR32(AEMSSTREAMENTRY) streams;
};

// System file: events, their parameter and trigger links, globals.
struct AEMSLINKLIST {
    PTR32(s32) indices;
    s16 count;
};

struct AEMSPARAM {
    /* 0x00 */ s32 min;
    /* 0x04 */ s32 max;
    /* 0x08 */ u16 bankId;
    /* 0x0A */ u16 moduleId;
    /* 0x0C */ u32 compHash;
    /* 0x10 */ u16 resolved;  // bankId once found, 0xFFFF otherwise
    /* 0x12 */ u16 unk_0x12;
    /* 0x14 */ u16 moduleIndex;
    /* 0x16 */ u16 compIndex;
};

struct AEMSTRIG {
    /* 0x00 */ u16 bankId;
    /* 0x02 */ u16 moduleId;
    /* 0x04 */ u32 compHash;
    /* 0x08 */ u16 resolved;
    /* 0x0A */ u16 unk_0xA;
    /* 0x0C */ u16 moduleIndex;
    /* 0x0E */ u16 compIndex;  // 0: creates instances on begin
};

struct AEMSEVENT {
    /* 0x00 */ s16 numParams;
    /* 0x04 */ PTR32(AEMSLINKLIST) params;
    /* 0x08 */ AEMSLINKLIST begin;
    /* 0x10 */ AEMSLINKLIST update;
    /* 0x18 */ AEMSLINKLIST end;
};

struct AEMSSYSTEM {
    /* 0x00 */ u8 unk_0x0[0x8];
    /* 0x08 */ s16 numParams;
    /* 0x0A */ s16 numTrigs;
    /* 0x0C */ s16 numEvents;
    /* 0x0E */ s16 numGlobals;
    /* 0x10 */ PTR32(AEMSPARAM) params;
    /* 0x14 */ PTR32(AEMSTRIG) trigs;
    /* 0x18 */ PTR32(AEMSEVENT) events;
    /* 0x1C */ PTR32(AEMSLINKLIST) globals;  // parameters each global drives
};

// Stream players (SNDAEMS_streaminit).
struct AEMSSTREAMREQ {
    /* 0x00 */ PTR32(AEMSCOMPDYNAMICPLAYER) player;
    /* 0x04 */ s32 hold;
    /* 0x08 */ PTR32(char) file;
    /* 0x0C */ s32 offset;
    /* 0x10 */ s32 offset2;  // -1: none
    /* 0x14 */ u8 state;  // 0 idle, 1 queue, 2 queue with a second part, 3 second part pending
};

struct AEMSSTREAMSTATUS {
    /* 0x00 */ PTR32(u8) buffer;
    /* 0x04 */ s32 stream;
    /* 0x08 */ s32 priority;
    /* 0x0C */ PTR32(AEMSCOMPDYNAMICPLAYER) owner;
    /* 0x10 */ AEMSSTREAMREQ req;
};

struct AEMSSTREAMSYS {
    /* 0x00 */ PTR32(u8) buffer;
    /* 0x04 */ s32 size;
    /* 0x08 */ s32 numStreams;
    /* 0x0C */ s32 frames;
    /* 0x10 */ PTR32(PTR32(AEMSSTREAMSTATUS)) streams;
};

typedef void (*AEMSPLAYERFN)(AEMSCOMPDYNAMICPLAYER* d);
typedef void (*AEMSSENDCLIENT)(int handle, s32* event);

struct SNDAEMSSTATE {
    /* 0x00 */ s32 counter;  // next event handle
    /* 0x04 */ s32 tickMs;
    /* 0x08 */ AEMSSYSTEM* system;
    /* 0x0C */ PTR32(MODULEBANK)* banks;  // 16
    /* 0x10 */ AEMSSTREAMSYS* streams;
    /* 0x14 */ s32* globals;
    /* 0x18 */ AEMSPLAYERFN updatePlayer8000;
    /* 0x1C */ AEMSPLAYERFN stopPlayer8000;
    /* 0x20 */ AEMSPLAYERFN updatePlayerBank;
    /* 0x24 */ AEMSPLAYERFN updatePlayerStream;
    /* 0x28 */ u8 loading;  // async module bank load in progress
    /* 0x29 */ u8 unk_0x29[2];
    /* 0x2B */ u8 numSendClients;
    /* 0x2C */ AEMSSENDCLIENT sendClients[6];
};

// File data, or allocated with these sizes: checked on both builds.
FILE_SIZE_ASSERT(AEMSOUTPUT, 0x8);
FILE_SIZE_ASSERT(AEMSCOMP, 0x20);
FILE_SIZE_ASSERT(AEMSLINK, 0x8);
FILE_SIZE_ASSERT(AEMSCOMPDYNAMIC, 0x3C);
FILE_SIZE_ASSERT(AEMSCOMPDYNAMICPLAYER, 0x44);
FILE_SIZE_ASSERT(AEMSPLAYERTABLE, 0x8);
FILE_SIZE_ASSERT(MODULEINSTANCE, 0x1C);
FILE_SIZE_ASSERT(MODULERT, 0xC);
FILE_SIZE_ASSERT(MODULE, 0x1C);
FILE_SIZE_ASSERT(MODULEBANKRT, 0x10);
FILE_SIZE_ASSERT(AEMSSTREAMENTRY, 0xC);
FILE_SIZE_ASSERT(AEMSBANKENTRY, 0xC);
FILE_SIZE_ASSERT(MODULEBANK, 0x38);
FILE_SIZE_ASSERT(AEMSLINKLIST, 0x8);
FILE_SIZE_ASSERT(AEMSPARAM, 0x18);
FILE_SIZE_ASSERT(AEMSTRIG, 0x10);
FILE_SIZE_ASSERT(AEMSEVENT, 0x20);
FILE_SIZE_ASSERT(AEMSSYSTEM, 0x20);
FILE_SIZE_ASSERT(AEMSSTREAMREQ, 0x18);
FILE_SIZE_ASSERT(AEMSSTREAMSTATUS, 0x28);
FILE_SIZE_ASSERT(AEMSSTREAMSYS, 0x14);
FILE_OFFSET_ASSERT(MODULEBANK, rt, 0x30);
FILE_OFFSET_ASSERT(AEMSPARAM, compIndex, 0x16);
FILE_OFFSET_ASSERT(AEMSTRIG, compIndex, 0xE);
SIZE_ASSERT(SNDAEMSSTATE, 0x44);
OFFSET_ASSERT(SNDAEMSSTATE, numSendClients, 0x2B);

extern SNDAEMSSTATE sndaems;

#ifdef FRONTLINE_NATIVE
#include "native/swap.h"

// AEMS files are big-endian; the native build swaps them in place before the
// relocation in SNDAEMSI_addsystem and SNDAEMSI_resolvemodulebank. Offsets are
// still file-relative here. Records and arrays can be shared, so every field
// goes through NativeSwapOnce.

// Swaps a system file. Returns false if it was already swapped (the 'AE' magic
// reads back as 'EA').
inline bool AEMSSystemFromBigEndian(AEMSSYSTEM* sys) {
    u8* base = (u8*)sys;
    if (base[0] != 'A' || base[1] != 'E') {
        return false;
    }
    NativeSwapOnce once;
    NativeSwap16s(base, 2);
    NativeSwap32(base + 4);
    NativeSwap16s(base + 8, 4);
    NativeSwap32s(base + 0x10, 4);
    for (int i = 0; i < sys->numParams; i++) {
        u8* p = base + sys->params.addr() + i * sizeof(AEMSPARAM);
        once.swap32s(p, 2);
        once.swap16s(p + 8, 2);
        once.swap32s(p + 0xC, 1);
        once.swap16s(p + 0x10, 4);
    }
    for (int i = 0; i < sys->numTrigs; i++) {
        u8* t = base + sys->trigs.addr() + i * sizeof(AEMSTRIG);
        once.swap16s(t, 2);
        once.swap32s(t + 4, 1);
        once.swap16s(t + 8, 4);
    }
    // A link list and its index array.
    struct Lists {
        static void Swap(u8* base, AEMSLINKLIST* list, NativeSwapOnce& once) {
            once.swap32s(&list->indices, 1);
            once.swap16s(&list->count, 1);
            once.swap32s(base + list->indices.addr(), list->count);
        }
    };
    for (int i = 0; i < sys->numEvents; i++) {
        AEMSEVENT* ev = (AEMSEVENT*)(base + sys->events.addr()) + i;
        once.swap16s(&ev->numParams, 1);
        once.swap32s(&ev->params, 1);
        for (int j = 0; j < ev->numParams; j++) {
            Lists::Swap(base, (AEMSLINKLIST*)(base + ev->params.addr()) + j, once);
        }
        Lists::Swap(base, &ev->begin, once);
        Lists::Swap(base, &ev->update, once);
        Lists::Swap(base, &ev->end, once);
    }
    for (int i = 0; i < sys->numGlobals; i++) {
        Lists::Swap(base, (AEMSLINKLIST*)(base + sys->globals.addr()) + i, once);
    }
    return true;
}

// Swaps a module bank's 0x38-byte header (the async loader reads it on its own).
// Returns false if it was already swapped (the 'AB' magic reads back as 'BA').
inline bool AEMSBankHeaderFromBigEndian(MODULEBANK* bank) {
    u8* base = (u8*)bank;
    if (base[0] != 'A' || base[1] != 'B') {
        return false;
    }
    NativeSwap16(base);
    NativeSwap32s(base + 4, 4);
    NativeSwap16s(base + 0x14, 3);
    NativeSwap32s(base + 0x1C, 7);
    return true;
}

// Swaps the rest of a module bank: modules, components, and the tables they
// point to. Each kind's parameters p[] are swapped the way its update function
// reads them: as words, as two s16 halves, or (flags) as single bytes.
inline void AEMSBankFromBigEndian(MODULEBANK* bank) {
    // aemscompinfo[type].staticSize (that table also holds the update functions).
    static const u8 kStaticSize[33] = {0,    0x0C, 0x0C, 0x0C, 0x14, 0x1C, 0x18, 0x18, 0x1C, 0x10, 0x10,
                                       0x14, 0x10, 0x14, 0x10, 0x0C, 0x0C, 0x18, 0x10, 0x10, 0x10, 0x10,
                                       0x0C, 0x10, 0x10, 0x14, 0,    0,    0x0C, 0x14, 0x20, 0x0C, 0x0C};
    u8* base = (u8*)bank;
    NativeSwapOnce once;
    int maxStream = -1;
    for (int m = 0; m < bank->numModules; m++) {
        MODULE* module = (MODULE*)(base + bank->modules.addr() + m * (bank->version >= 8 ? 0x1C : 0x14));
        NativeSwap16s(module, 4);
        NativeSwap32s(&module->event, bank->version >= 8 ? 5 : 3);
        u8* c = base + module->comps.addr();
        for (int i = 0; i < module->numComps; i++) {
            AEMSCOMP* comp = (AEMSCOMP*)c;
            NativeSwap32(&comp->hash);
            NativeSwap32(&comp->outputs);
            for (int k = 0; k < comp->numOutputs; k++) {
                AEMSOUTPUT* out = (AEMSOUTPUT*)(base + comp->outputs.addr()) + k;
                once.swap32s(&out->targets, 1);
                once.swap16s(&out->count, 1);
                once.swap16s(base + out->targets.addr(), out->count * 2);
            }
            int numParams = (kStaticSize[comp->type] - 0xC) / 4;
            s32* p = comp->p;
            switch (comp->type) {
            case AEMSCOMP_OSCILLATOR:  // wave, unipolar
                NativeSwap16s(&p[0], 2);
                break;
            case AEMSCOMP_ENVELOPE:  // points, (count, sustain), start level
                NativeSwap32(&p[0]);
                NativeSwap16s(&p[1], 2);
                NativeSwap32(&p[2]);
                once.swap32s(base + p[0], ((s16*)p)[2] * 2);
                break;
            case AEMSCOMP_COUNTER:  // min, max, up (byte)
                NativeSwap32s(&p[0], 2);
                break;
            case AEMSCOMP_RANDOM:  // min, max, byte weights, mode (byte)
                NativeSwap32s(&p[0], 3);
                break;
            case AEMSCOMP_TABLE:  // s16 values, (size, min), (max, -)
                NativeSwap32(&p[0]);
                NativeSwap16s(&p[1], 4);
                once.swap16s(base + p[0], ((s16*)p)[2]);
                break;
            case AEMSCOMP_TABLE2:  // values, count, min, max, value size
                NativeSwap32s(&p[0], 5);
                if (p[4] == 4) {
                    once.swap32s(base + p[0], p[1]);
                } else if (p[4] == 2) {
                    once.swap16s(base + p[0], p[1]);
                }
                break;
            case AEMSCOMP_STATEGEN:  // count, s16 states
                NativeSwap32s(&p[0], 2);
                once.swap16s(base + p[1], p[0]);
                break;
            case AEMSCOMP_PLAYER: {  // table, (bank)
                NativeSwap32s(&p[0], 2);
                AEMSPLAYERTABLE* table = (AEMSPLAYERTABLE*)(base + p[0]);
                once.swap32s(&table->entries, 1);
                once.swap16s(&table->count, 1);
                once.swap32s(base + table->entries.addr(), table->count);
                for (int k = 0; k < table->count; k++) {
                    u32 entry = ((u32*)(base + table->entries.addr()))[k] & 0xFFFF;
                    if (entry >= 0xC000 && (int)(entry & 0x3FFF) > maxStream) {
                        maxStream = entry & 0x3FFF;
                    }
                }
                break;
            }
            default:
                NativeSwap32s(p, numParams);
                break;
            }
            c += kStaticSize[comp->type];
        }
    }
    for (int i = 0; i < bank->numEntries; i++) {
        NativeSwap32s(base + bank->entries.addr() + i * sizeof(AEMSBANKENTRY), 3);
    }
    // No count in the file: as many as the players index.
    if (bank->version >= 5 && bank->streams.addr() != 0) {
        for (int i = 0; i <= maxStream; i++) {
            u8* s = base + bank->streams.addr() + i * sizeof(AEMSSTREAMENTRY);
            NativeSwap16(s);
            NativeSwap32s(s + 4, 2);
        }
    }
}
#endif

// Module i of a bank (the module size depends on the version).
inline MODULE* AEMS_module(MODULEBANK* bank, int i) {
    if (bank->version >= 8) {
        return (MODULE*)((u8*)bank->modules + i * 0x1C);
    }
    return (MODULE*)((u8*)bank->modules + i * 0x14);
}

void SNDAEMSI_restore();
int SNDAEMSI_addsystem(void* system);
int SNDAEMSI_removesystem();
void SNDAEMSI_resolvemodulebank(MODULEBANK* bank, char* streamFile, int streamOffset);
int SNDAEMSI_removemodulebank(int bank);
int SNDAEMSI_beginevent(void* event);
int SNDAEMSI_updateevent(int handle, void* event);
int SNDAEMSI_endevent(int handle, void* event);
int SNDAEMSI_setglobal(int global, int value);
int SNDAEMSI_stuffparam(int value, int event, int param, int handle);
int SNDAEMSI_stufftrigger(int event, int type, int handle);
void AEMSI_timerupdate();
void SNDAEMSI_updatemodules(int type);
void SNDAEMSI_updatemodule(int bank, int module, int type);
int SNDAEMSI_createinstance(MODULEBANK* bank, MODULE* module, int handle);
int SNDAEMSI_destroyinstance(MODULE* module, int handle);
void SNDAEMSI_reordermodule(MODULE* module, MODULEINSTANCE* inst);
void SNDAEMSI_reordermodule2(MODULE* module, MODULEINSTANCE* inst);
void SNDAEMS_linkbank();
void SNDAEMSI_linkstream();
void SNDAEMSI_streamupdatestatus(AEMSCOMPDYNAMICPLAYER* d);
int SNDAEMSI_addmodulebank(void* data, char* streamFile, int streamOffset, void* (*alloc)(void*, int, int));
int SNDAEMSI_asyncloadmodulebank(char* file, int offset, char* streamFile, int streamOffset, void* buffer,
                                 int size, void* (*alloc)(int));

extern "C" {
int SNDAEMS_asyncloadmodulebankdone(void);
int SNDBANK_asyncload(const char* name, int offset, void* buffer, int size, void* (*alloc)(int));
int SNDBANK_asyncdone(void);
int SNDSTRM_3dpos(int stream, int azimuth, int distance);
int SNDSTRM_fxlevel(int stream, int bus, int level);
int SNDSTRM_lowpass(int stream, int freq);
int SNDSTRM_highpass(int stream, int freq);
int SNDSTRM_timemult(int stream, int mult);
int SNDSTRM_drylevel(int stream, int level);
}

#endif
