// AEMS: EA's event-driven sound effect system. A system file maps game events (with
// parameters) onto modules in module banks; each module is a graph of components (inputs,
// triggers, math, envelopes, players...) instantiated per event and updated at 100 Hz or
// when an event changes. All files are relocated in place when added.
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
    /* 0x0 */ AEMSTARGET* targets;
    /* 0x4 */ s16 count;
};

// A component in a module bank (aemscompinfo[type].staticSize bytes).
struct AEMSCOMP {
    /* 0x00 */ u32 hash;
    /* 0x04 */ u8 type;
    /* 0x05 */ u8 numInputs;
    /* 0x06 */ u8 numOutputs;
    /* 0x08 */ AEMSOUTPUT* outputs;
    /* 0x0C */ s32 p[5];  // kind specific
};

// Live copy of one connection: *dst = *src after the component updates.
struct AEMSLINK {
    s32* src;
    s32* dst;
};

// A component's state in an instance (aemscompinfo[type].dynamicSize bytes).
struct AEMSCOMPDYNAMIC {
    /* 0x00 */ AEMSCOMP* comp;
    /* 0x04 */ u8 connected;  // inputs wired so far (sorting); warps start at 1
    /* 0x05 */ u8 scheduled;
    /* 0x06 */ u16 numOutputs;
    /* 0x08 */ AEMSLINK* outputs;
    /* 0x0C */ s32* in;
    /* 0x10 */ s32 out;
    /* 0x14 */ s32 s[10];  // kind specific
};

struct MODULEBANK;

// Players: start/stop sounds (bank patches 0x4000, streams 0xC000, 0x8000 not linked here).
struct AEMSSTREAMREQ;
struct AEMSCOMPDYNAMICPLAYER {
    /* 0x00 */ AEMSCOMP* comp;
    /* 0x04 */ u8 connected;
    /* 0x05 */ u8 scheduled;
    /* 0x06 */ u16 numOutputs;
    /* 0x08 */ AEMSLINK* outputs;
    /* 0x0C */ s32* in;  // pitch, timemult, vol, azimuth, distance, fx, index, gate, lowpass, highpass, dry
    /* 0x10 */ s32 out;  // playing
    /* 0x14 */ s32 timeLeft;  // ms
    /* 0x18 */ s32 position;  // ms
    /* 0x1C */ s32 handle;
    /* 0x20 */ s32 handle2;  // second stream request
    /* 0x24 */ s32 stream;
    /* 0x28 */ AEMSSTREAMREQ* req;
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
    u32* entries;
    s16 count;
};

// An instance of a module (one per event handle); the component states follow at 0x1C.
struct MODULEINSTANCE {
    /* 0x00 */ s32 handle;
    /* 0x04 */ u8 dirty;  // update on the next pass
    /* 0x08 */ u8* dyn;
    /* 0x0C */ s32 numExec;
    /* 0x10 */ AEMSCOMPDYNAMIC** exec;  // components in update order (the links follow)
    /* 0x14 */ AEMSLINK* links;
    /* 0x18 */ AEMSCOMPDYNAMIC* destroy;  // its input > 0 destroys the instance
};

struct MODULERT {
    /* 0x0 */ MODULEINSTANCE** instances;
    /* 0x4 */ s16 count;
    /* 0x8 */ s32* compOffsets;  // dynamic block offset per component
};

// A module (0x14 bytes; 0x1C from bank version 8, already in update order).
struct MODULE {
    /* 0x00 */ u16 id;
    /* 0x02 */ s16 numInstances;
    /* 0x04 */ s16 numComps;
    /* 0x06 */ s16 updateType;  // 0 timer, 1 events
    /* 0x08 */ s32 event;
    /* 0x0C */ AEMSCOMP* comps;
    /* 0x10 */ MODULERT* rt;
    /* 0x14 */ s32 numLinks;  // version 8+
    /* 0x18 */ s32 numExec;
};

struct MODULEBANKRT {
    /* 0x0 */ s32 sfxBank;
    /* 0x4 */ s32 midiBank;
    /* 0x8 */ char* streamFile;
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
    /* 0x8 */ void* data;
};

struct MODULEBANK {
    /* 0x00 */ u16 unk_0x0;
    /* 0x02 */ u8 version;
    /* 0x04 */ s32 size;
    /* 0x08 */ s32 unk_0x8;
    /* 0x0C */ s32 numModules;
    /* 0x10 */ MODULE* modules;
    /* 0x14 */ u16 bankId;
    /* 0x16 */ u16 unk_0x16;
    /* 0x18 */ s16 numEntries;
    /* 0x1C */ s32 sfxBankOffset;  // sample banks inside the file
    /* 0x20 */ s32 sfxBankSize;
    /* 0x24 */ s32 midiBankOffset;
    /* 0x28 */ s32 midiBankSize;
    /* 0x2C */ AEMSBANKENTRY* entries;
    /* 0x30 */ MODULEBANKRT* rt;
    /* 0x34 */ AEMSSTREAMENTRY* streams;
};

// System file: events, their parameter and trigger links, globals.
struct AEMSLINKLIST {
    s32* indices;
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
    /* 0x04 */ AEMSLINKLIST* params;
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
    /* 0x10 */ AEMSPARAM* params;
    /* 0x14 */ AEMSTRIG* trigs;
    /* 0x18 */ AEMSEVENT* events;
    /* 0x1C */ AEMSLINKLIST* globals;  // parameters each global drives
};

// Stream players (SNDAEMS_streaminit).
struct AEMSSTREAMREQ {
    /* 0x00 */ AEMSCOMPDYNAMICPLAYER* player;
    /* 0x04 */ s32 hold;
    /* 0x08 */ char* file;
    /* 0x0C */ s32 offset;
    /* 0x10 */ s32 offset2;  // -1: none
    /* 0x14 */ u8 state;  // 0 idle, 1 queue, 2 queue with a second part, 3 second part pending
};

struct AEMSSTREAMSTATUS {
    /* 0x00 */ u8* buffer;
    /* 0x04 */ s32 stream;
    /* 0x08 */ s32 priority;
    /* 0x0C */ AEMSCOMPDYNAMICPLAYER* owner;
    /* 0x10 */ AEMSSTREAMREQ req;
};

struct AEMSSTREAMSYS {
    /* 0x00 */ u8* buffer;
    /* 0x04 */ s32 size;
    /* 0x08 */ s32 numStreams;
    /* 0x0C */ s32 frames;
    /* 0x10 */ AEMSSTREAMSTATUS** streams;
};

typedef void (*AEMSPLAYERFN)(AEMSCOMPDYNAMICPLAYER* d);
typedef void (*AEMSSENDCLIENT)(int handle, s32* event);

struct SNDAEMSSTATE {
    /* 0x00 */ s32 counter;  // next event handle
    /* 0x04 */ s32 tickMs;
    /* 0x08 */ AEMSSYSTEM* system;
    /* 0x0C */ MODULEBANK** banks;  // 16
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

SIZE_ASSERT(AEMSCOMPDYNAMICPLAYER, 0x44);
SIZE_ASSERT(MODULE, 0x1C);
SIZE_ASSERT(AEMSPARAM, 0x18);
SIZE_ASSERT(AEMSTRIG, 0x10);
SIZE_ASSERT(AEMSEVENT, 0x20);
SIZE_ASSERT(AEMSSTREAMSTATUS, 0x28);
SIZE_ASSERT(SNDAEMSSTATE, 0x44);
OFFSET_ASSERT(MODULEBANK, rt, 0x30);
OFFSET_ASSERT(AEMSPARAM, compIndex, 0x16);
OFFSET_ASSERT(AEMSTRIG, compIndex, 0xE);
OFFSET_ASSERT(SNDAEMSSTATE, numSendClients, 0x2B);

extern SNDAEMSSTATE sndaems;

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
