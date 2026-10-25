// EA sound library: the main CPU mixer (smixer) and its filter chains (sfilter, sf*).
#ifndef _EA_SND_SMIX_H_
#define _EA_SND_SMIX_H_

#include "ea/snd/sndi.h"
#include "ea/snd/eaxadecf.h"
#include "ea/snd/sdecmut.h"

// A 16-bit sample (or decoder state) from sample data, which is big-endian:
// banks and streams come from the disc as they are.
#ifdef FRONTLINE_NATIVE
#define SAMPLE_S16(p) ((s16)__builtin_bswap16(*(const u16*)(p)))
#else
#define SAMPLE_S16(p) (*(const s16*)(p))
#endif

// A processing node. Voices run a chain of these, sorted by `order`, from the decoder (the
// source) to the output: process() reads `in`, writes `out` and returns the frames produced
// (< 0 when the source has ended). Node state follows the header. size 0x1C
struct SFILTER;
typedef int (*SFILTERPROC)(void* self, int frames, void* in, void* out, int flag);
struct SFILTER {
    /* 0x00 */ SFILTERPROC process;
    /* 0x04 */ void (*destroy)(SFILTER* self);
    /* 0x08 */ SFILTER* next;  // also input 1 of a graph node
    /* 0x0C */ SFILTER* in2;
    /* 0x10 */ SFILTER* prev;  // also output 1 of a graph node
    /* 0x14 */ SFILTER* out2;
    /* 0x18 */ u16 order;
    /* 0x1A */ u8 port;  // which output of its source feeds this node
    /* 0x1B */ u8 unk_0x1B;
};

// Time stretch (stretch.c): plays the input period by period, dropping or repeating periods
// (crossfaded) to follow the ratio. size 0x1828
struct TIMESTRETCHSTATE {
    /* 0x0000 */ SFILTER f;
    /* 0x001C */ u8* table;    // per period: extra shift for the crossfade
    /* 0x0020 */ s32 player;   // packet player feeding it, or -1
    /* 0x0024 */ f32 ratio;    // 0.5 .. 2.0
    /* 0x0028 */ f32 err;      // accumulated timing error
    /* 0x002C */ s32 period;
    /* 0x0030 */ s32 remaining;  // input samples left
    /* 0x0034 */ s32 carry;    // samples held in carryBuf
    /* 0x0038 */ s32 outAvail;
    /* 0x003C */ s32 outPos;
    /* 0x0040 */ f32 carryBuf[510];
    /* 0x0838 */ f32 outBuf[1020];
};
// Resampler (sfrsf). size 0x3C
typedef void (*RSFKERNEL)(int frames, f32* src, f32* dest, int* ipos, u32* frac, int stepInt, u32 stepFrac);
struct RSFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ u32 pitch;  // 16.16
    /* 0x20 */ u32 phase;  // 16.16
    /* 0x24 */ RSFKERNEL kernel;
    /* 0x28 */ s16 primed;
    /* 0x2A */ s16 delay;
    /* 0x2C */ f32 hist[4];  // the last input samples, needed by the next call
};
struct HPFSTATE;

// One-pole low-pass (sflpf). size 0x28
struct LPFRCSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ f32 y;
    /* 0x20 */ f32 a;  // feedback
    /* 0x24 */ f32 b;  // input gain
};
// Float to 24-bit output conversion (sfft24). size 0x1C
struct FT24_32STATE {
    /* 0x00 */ SFILTER f;
};

// Data source node (sfsrc): copies from a buffer. size 0x20
struct SRCSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ u8* data;
};

// A mixer voice. size 0x50
struct SNDMIXVOICE {
    /* 0x00 */ u8 state;        // 0 free, 1 set up, 2 playing
    /* 0x01 */ s8 rampPending;  // gains changed: ramp over the next 16 frames
    /* 0x04 */ f32 dryCur[4];   // per output, as last mixed
    /* 0x14 */ f32 dry[4];
    /* 0x24 */ f32 wetCur;
    /* 0x28 */ f32 wet[1];      // per effects bus
    /* 0x2C */ f32 last;        // last sample produced (faded out on stop)
    /* 0x30 */ SFILTER* chain;
    /* 0x34 */ int (*getframe)(SFILTER* decoder);
    /* 0x38 */ SFILTER* decoder;
    /* 0x3C */ TIMESTRETCHSTATE* stretch;
    /* 0x40 */ RSFSTATE* rsf;
    /* 0x44 */ u8 unk_0x44[0x4];
    /* 0x48 */ LPFRCSTATE* lpf;
    /* 0x4C */ HPFSTATE* hpf;
};

// Parameters for a decoder's init function (MIX_playinit).
struct UNPACKINITPARAMS {
    /* 0x00 */ void* data;
    /* 0x04 */ s32 unk_0x4;
    /* 0x08 */ s32 unk_0x8;
    /* 0x0C */ s32 samples;
    /* 0x10 */ s32 loopStart;
    /* 0x14 */ s32 loopEnd;  // > 0: looped
    /* 0x18 */ s32 decode;   // 0 (format 9): produce silence
    /* 0x1C */ s32 voice;
    /* 0x20 */ s32 quality;
    /* 0x24 */ int (*getframe)(SFILTER* decoder);  // set by the decoder
};
typedef void (*MIXDECODERINITFN)(void* decoder, UNPACKINITPARAMS* init);

// Mixer setup (MIX_create).
struct MIXOPTS {
    /* 0x0 */ s32 rate;
    /* 0x4 */ u8 numVoices;
    /* 0x5 */ u8 numOutputs;
    /* 0x6 */ u8 unk_0x6;
    /* 0x7 */ u8 unk_0x7;
    /* 0x8 */ void (*stopcb)(int voice);
};

typedef void (*MIXFN)(int frames, f32* src, f32* dest, f32 gain);

// Mixer state. size 0x200
struct SNDMIX {
    /* 0x000 */ s32 rate;
    /* 0x004 */ u8 numVoices;
    /* 0x005 */ u8 numOutputs;
    /* 0x006 */ u8 unk_0x6;
    /* 0x007 */ u8 unk_0x7;
    /* 0x008 */ void (*stopcb)(int voice);  // a voice's source ended
    /* 0x00C */ u16 frames;  // per slice
    /* 0x00E */ u8 quality;
    /* 0x00F */ s8 fxMode[1];  // per effects bus; 0: off, 2: gains * 1.5
    /* 0x010 */ void (*fxinit)(struct MIXFXHEADER* hdr);
    /* 0x014 */ void (*postmix)(int frames);
    /* 0x018 */ MIXDECODERINITFN decoderInit[33];  // by format (MIX_playinit)
    /* 0x09C */ s32 decoderSize[33];
    /* 0x120 */ void* bufMem[2];
    /* 0x128 */ f32* buf[2];  // 64-byte aligned work buffers
    /* 0x130 */ void* outMem[4];
    /* 0x140 */ f32* out[4];  // per output mix
    /* 0x150 */ f32 outTail[4];  // levels of stopped voices to fade out
    /* 0x160 */ f32 fxTail;
    /* 0x164 */ SFILTER* outChain[4];
    /* 0x174 */ SFILTER* outPre[4];
    /* 0x184 */ FT24_32STATE ft[4];
    /* 0x1F4 */ SNDMIXVOICE* voices;
    /* 0x1F8 */ MIXFN mix;
    /* 0x1FC */ void (*decode)(int frames, s16* src, f32* dest);  // 16-bit samples to float
};

// Effects (reverb) send buffers (smixfx): the send, then two module outputs. size 0x784
struct SNDFXSEND {
    /* 0x000 */ f32 buf[0x1E0];  // 3 x 0xA0
    /* 0x780 */ s32 silent;  // slices since anything was sent
};

// 16-bit PCM decoders (supf, suplf, suppf).
struct UNPACKFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ s16* data;
    /* 0x20 */ u32 pos;
    /* 0x24 */ u32 end;
    /* 0x28 */ s32 decode;
};
struct UNPACKLFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ s16* data;
    /* 0x20 */ u32 pos;
    /* 0x24 */ u32 loopStart;
    /* 0x28 */ u32 loopEnd;
    /* 0x2C */ s32 decode;
};
// Fed by the packet player.
struct UNPACKPFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ s16* data;  // current packet
    /* 0x20 */ s32 len;
    /* 0x24 */ s32 pos;
    /* 0x28 */ s32 player;
    /* 0x2C */ u32 done;  // frames produced last call, freed on the next
    /* 0x30 */ u8 decode;
    /* 0x31 */ u8 chan;
};

// XA ADPCM decoders (supxaf, supxalf, supxapf).
struct UNPACKXAFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ SND::CEAXABLKDecf* dec;
    /* 0x20 */ u8* data;
    /* 0x24 */ s32 samples;
    /* 0x28 */ s32 pos;
};
struct UNPACKXALFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ SND::CEAXABLKDecf* dec;
    /* 0x20 */ u8* data;
    /* 0x24 */ s32 samples;
    /* 0x28 */ s32 pos;
    /* 0x2C */ s32 looped;  // loopState is valid
    /* 0x30 */ s32 started;
    /* 0x34 */ s32 loopStart;
    /* 0x38 */ s32 loopEnd;
    /* 0x3C */ SND::XAFSTATE loopState;  // decoder history at the loop's first block
};
// Fed by the packet player: each packet starts with the s16 history, then blocks.
struct UNPACKXAPFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ SND::CEAXABLKDecf* dec;
    /* 0x20 */ u8* data;
    /* 0x24 */ int samples;
    /* 0x28 */ s32 pos;
    /* 0x2C */ s32 player;
    /* 0x30 */ u32 done;  // frames produced last call, freed on the next
    /* 0x34 */ u8 chan;
};

// MicroTalk decoders (supmutf, supmutpf).
struct UNPACKMTFSTATE {
    /* 0x000 */ SFILTER f;
    /* 0x01C */ u32 samples;
    /* 0x020 */ u32 pos;
    /* 0x024 */ s32 avail;  // decoded samples left at the end of ut.frame
    /* 0x028 */ UTALKSTATE ut;
};
// Fed by the packet player: a packet starts with a byte, nonzero for a new stream header.
struct UNPACKMTPFSTATE {
    /* 0x000 */ SFILTER f;
    /* 0x01C */ u8* data;
    /* 0x020 */ int len;
    /* 0x024 */ s32 player;
    /* 0x028 */ u32 done;  // frames produced last call, freed on the next
    /* 0x02C */ u16 pos;
    /* 0x02E */ u16 avail;
    /* 0x030 */ UTALKSTATE ut;
    /* 0xD70 */ u8 chan;
};

// 9-tap symmetric FIR (sfir8, sfir). size 0x3C
struct SNDFIRSTATE {
    /* 0x00 */ f32 hist[8];  // hist[0] newest
    /* 0x20 */ f32 c[5];     // c[4] is the centre tap
    /* 0x34 */ f32 f1;       // cutoffs as fractions of the Nyquist rate
    /* 0x38 */ f32 f2;
};
// FIR filter nodes (sflpffir8, sfhpffir8, sfbpffir8). size 0x58
struct LPFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ SNDFIRSTATE fir;
};
struct HPFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ SNDFIRSTATE fir;
};
struct BPFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ SNDFIRSTATE fir;
};

// Gain (sfamplf). size 0x20
struct AMPLFSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ f32 gain;
};
// Sums inputs 1 and 2 (sfmixer). size 0x24
struct MIXERSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ f32* buf;
    /* 0x20 */ s32 size;
};
// Feeds one input to two outputs (sfsplit): the second pull returns a copy. size 0x28
struct SPLITSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ f32* buf;
    /* 0x20 */ s32 size;
    /* 0x24 */ s16 served;
    /* 0x26 */ s16 fresh;
};

// All-pass delay (sfecho). size 0x34
struct ECHOSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ f32* buf;  // 16-byte aligned in mem
    /* 0x20 */ void* mem;
    /* 0x24 */ s32 pos;
    /* 0x28 */ s32 len;  // multiple of 16 samples
    /* 0x2C */ f32 fb;
    /* 0x30 */ f32 negfb;
};
// Two-pole resonator (sfreson). size 0x40
struct RESONSTATE {
    /* 0x00 */ SFILTER f;
    /* 0x1C */ s32 rate;
    /* 0x20 */ f32 freq;
    /* 0x24 */ f32 bw;
    /* 0x28 */ f32 r;     // pole radius
    /* 0x2C */ f32 c;     // cos term
    /* 0x30 */ f32 gain;
    /* 0x34 */ f32 unk_0x34;
    /* 0x38 */ f32 y1;
    /* 0x3C */ f32 y2;
};

// Classic reverb (sfxrevc/sfxrev): up to 11 damped all-pass delays in three groups.
// size 0x24
struct SNDFBSTATE {
    /* 0x00 */ s32 wpos;  // write and read positions count down from len
    /* 0x04 */ s32 rpos;
    /* 0x08 */ s32 len;
    /* 0x0C */ f32* buf;
    /* 0x10 */ f32 g;
    /* 0x14 */ f32 negg;
    /* 0x18 */ f32 lp;    // damping state
    /* 0x1C */ f32 lpa;
    /* 0x20 */ f32 lpb;
};
// Reverb settings. type 0: classic (modules[] follow), 10: a filter graph file
// (MIXFXFILTHEADER, the same first 8 bytes).
struct MIXFXMODULE {
    /* 0x0 */ u8 delay[2];  // ms, big-endian
    /* 0x2 */ s8 feedback;  // /127
    /* 0x3 */ s8 damping;   // /127
};
struct MIXFXHEADER {
    /* 0x0 */ u8 unk_0x0[2];
    /* 0x2 */ u8 type;
    /* 0x3 */ u8 unk_0x3;
    /* 0x4 */ u8 numModules[3];  // per group
    /* 0x7 */ u8 damping;        // (damping + 100) / 350 scales the damping filters
    /* 0x8 */ MIXFXMODULE modules[1];
};
// Filter graph file: per output-channel count, the number of modules and connections,
// then the module records and connection records of each configuration.
struct MIXFXFILTHEADER {
    /* 0x00 */ u8 unk_0x0[8];
    /* 0x08 */ u8 numModules[6][4];      // big-endian s32
    /* 0x20 */ u8 numConnections[6][4];  // big-endian s32
};

extern SNDMIX sndmix;
extern SNDFBSTATE sndfb[11];
extern s32 sndfxnummodules[3];
extern u8 filehead[0x38];
extern u8* pheaderinfo;
extern SFILTER** pfilterlist;
extern SFILTER** phead;
extern u8* plisthead;
extern u8* pconhead;
extern s32 lastoutputchannels;
extern s32 validstructoutputs;
extern u8* modlist;
extern u8* conlist;
extern s32 SNDMIXfxv2inited;
extern s32 filtermodsizetable[10];
extern s8 tablecopyto[6][6][6][6];
extern SNDFXSEND sndfx;
extern void (*MIXinitfn)(MIXOPTS* opts);
extern void (*MIXrestorefn)();
extern void (*MIXaudioslicefn)(f32** dest, int frames);
extern void (*MIXplayinitfn)(int voice, int format, int mode, int a, int b, u8* stretch, int c, int d, int e,
                             int f, int g, int h, int i);
extern void (*MIXplayfn)(int voice);
extern void (*MIXstopfn)(int voice);
extern void (*MIXsetpitchfn)(int voice, int pitch);

int SFILTER_timestretchinit(TIMESTRETCHSTATE* ts, unsigned char* data, int voice);
void SFILTER_timestretchsetratio(TIMESTRETCHSTATE* ts, int ratio);
void SFILTER_rsfinit(RSFSTATE* rsf, int quality, int hq);
extern "C" void rsflc(int frames, f32* src, f32* dest, int* ipos, u32* frac, int stepInt, u32 stepFrac);
void SFILTER_rsfsetpitch(RSFSTATE* rsf, int pitch);
int SFILTER_createHPFFIR8(HPFSTATE* hpf);
void SFILTER_modifyHPFFIR8(HPFSTATE* hpf, int* params);
int SFILTER_createLPFFIR8(LPFSTATE* lpf);
void SFILTER_modifyLPFFIR8(LPFSTATE* lpf, int* params);
int SFILTER_createBPFFIR8(BPFSTATE* bpf);
void SFILTER_modifyBPFFIR8(BPFSTATE* bpf, int* params);
int SFILTER_createSOURCE(SRCSTATE* src);
void SFILTER_initSOURCE(SRCSTATE* src, void* data);
int SFILTER_createAMPLF(AMPLFSTATE* s);
void SFILTER_modifyAMPLF(AMPLFSTATE* s, int* params);
int SFILTER_createMIX(MIXERSTATE* s);
int SFILTER_createSPLIT(SPLITSTATE* s);
int SFILTER_createECHO(ECHOSTATE* s);
void SFILTER_modifyECHO(ECHOSTATE* s, int* params);
int SFILTER_createRESON(RESONSTATE* s);
void SFILTER_modifyRESON(RESONSTATE* s, int* params);
void SNDI_fir8(SNDFIRSTATE* fir, int frames, void* in, void* out);
void SNDI_fir8init(SNDFIRSTATE* fir);
void calcFIRCoeffs(SNDFIRSTATE* fir, int type);
int SFILTER_createLPFRC(LPFRCSTATE* lpf);
void SFILTER_modifyLPFRC(LPFRCSTATE* lpf, int* params);
void SFILTER_ft24_32init(FT24_32STATE* ft);
void SFILTER_unpackfinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpacklfinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpackpfinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpackxafinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpackxalfinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpackxapfinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpackmtfinit(void* state, UNPACKINITPARAMS* init);
void SFILTER_unpackmtpfinit(void* state, UNPACKINITPARAMS* init);
void MIXI_interpolatemix(f32 from, f32 to, f32* src, f32* dest);
void MIXI_interpolateto0(f32* level, f32* dest);

extern "C" {
void SFILTER_add(SFILTER** chain, SFILTER* f);
void SFILTER_remove(SFILTER** chain, SFILTER* f);
int SFILTER_connect(SFILTER* from, SFILTER* to, int out, int in);
void MIX_reset(void);
void MIXI_initunpack16(void);
void MIXI_initunpackxa(void);
void MIXI_initunpackmt(void);
void mixc(int frames, f32* src, f32* dest, f32 gain);
void decode16x87(int frames, s16* src, f32* dest);
void MIX_create(MIXOPTS* opts);
void MIX_destroy(void);
void MIX_audioslice(f32** dest, int frames);
void MIX_playinit(int voice, int format, int mode, int a, int b, u8* stretch, int c, int d, int e, int f, int g,
                  int h, int i);
void MIX_play(int voice);
void MIX_stop(int voice);
void MIX_setpitch(int voice, int pitch);
int MIX_getframe(int voice);
void MIX_settimemult(int voice, int mult);
void MIX_sethighpass(int voice, int freq);
void MIX_setlowpass(int voice, f32 frac);
void MIXI_reverbblock(SNDFBSTATE* fb, int frames, f32* in, f32* out);
void SNDMIXI_fxinit(MIXFXHEADER* hdr);
void SNDMIXI_fxadd(int frames);
void SNDMIXI_modlapifxadd(int frames);
f32* MIX_getwetbuffer(void);
void MIX_initreverb(int rate, MIXFXHEADER* hdr);
void MIX_restorereverb(void);
void SNDMIX_setdrygain(int voice, int output, f32 gain);
void SNDMIX_setwetgain(int voice, int bus, f32 gain);
void SNDSYS_linkmaincpumixer(void);
}

SIZE_ASSERT(SFILTER, 0x1C);
SIZE_ASSERT(SNDMIXVOICE, 0x50);
SIZE_ASSERT(SNDMIX, 0x200);
SIZE_ASSERT(SNDFXSEND, 0x784);
SIZE_ASSERT(UNPACKFSTATE, 0x2C);
SIZE_ASSERT(UNPACKLFSTATE, 0x30);
SIZE_ASSERT(UNPACKPFSTATE, 0x34);
SIZE_ASSERT(UNPACKXALFSTATE, 0x44);
SIZE_ASSERT(UNPACKXAPFSTATE, 0x38);
SIZE_ASSERT(UNPACKMTFSTATE, 0xD68);
SIZE_ASSERT(UNPACKMTPFSTATE, 0xD74);
SIZE_ASSERT(HPFSTATE, 0x58);
SIZE_ASSERT(RSFSTATE, 0x3C);
SIZE_ASSERT(TIMESTRETCHSTATE, 0x1828);
OFFSET_ASSERT(SNDMIX, voices, 0x1F4);


#endif
