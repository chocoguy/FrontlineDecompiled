// EA sound library GameCube driver (snddrv.c): DSP voices through AX, CPU mixed voices through
// the main CPU mixer (output fed back through AX aux bus A), ARAM transfers, packet streams.
#ifndef _EA_SND_SNDDRV_H_
#define _EA_SND_SNDDRV_H_

#include "ea/snd/sndi.h"
#include <dolphin/ax.h>
#include <dolphin/ar.h>
#include <dolphin/os.h>

// Per voice. size 0x30
struct SNDDRVVOICE {
    /* 0x00 */ s16 vols[3];  // per speaker
    /* 0x06 */ u8 unk_0x6[2];
    /* 0x08 */ s32 base;      // start of the voice's data in ARAM, in samples or nibbles
    /* 0x0C */ s8 pktPlayer;  // -1: none
    /* 0x0D */ u8 unk_0xD[2];
    /* 0x0F */ u8 freeNextFrame;
    /* 0x10 */ u8 unk_0x10[4];
    /* 0x14 */ u8 active;     // started on the DSP
    /* 0x15 */ u8 noVoice;    // no AX voice held
    /* 0x16 */ u8 unk_0x16[0x16];
    /* 0x2C */ AXVPB* axvpb;
};

// A queued ARAM transfer. size 0x40
struct SNDDRVDMA {
    /* 0x00 */ s32 id;  // -1: free
    /* 0x04 */ ARQRequest req;
    /* 0x24 */ u32 priority;
    /* 0x28 */ u32 src;
    /* 0x2C */ u32 dst;
    /* 0x30 */ u32 len;
    /* 0x34 */ u32 time;  // sndgs.serverTicks when posted
    /* 0x38 */ u8 unk_0x38[8];
};

// A packet player on the DSP: 16-bit PCM through a two-half ring in ARAM per channel, refilled
// from the packet queue through bounce buffers. size 0x12FC
struct SNDDRVPKTCHAN {
    /* 0x0000 */ u8 mem[0x12C0];
    /* 0x12C0 */ u8* bounce[3];  // 0x600 each, 32-byte aligned in mem
    /* 0x12CC */ s8 voice;
    /* 0x12CD */ u8 half;        // the ring half being refilled
    /* 0x12CE */ s8 startDelay;  // service calls before starting the voice
    /* 0x12CF */ u8 unk_0x12CF;
    /* 0x12D0 */ u32 aram[3];    // 0xC00 each
    /* 0x12DC */ u32 halfMark;   // sample address between the halves
    /* 0x12E0 */ s16 consumed[2];  // samples to free per half
    /* 0x12E4 */ u8 unk_0x12E4[4];
    /* 0x12E8 */ u8* pkt[3];
    /* 0x12F4 */ s32 bytes;
    /* 0x12F8 */ s32 pos;
};

// size 0xAB20
struct SNDDRV {
    /* 0x0000 */ SNDDRVVOICE voices[61];
    /* 0x0B70 */ u8 unk_0xB70[0x300];
    /* 0x0E70 */ u32 zeroAram;  // 0x500 bytes of silence in ARAM
    /* 0x0E74 */ u32 zeroAram16;
    /* 0x0E78 */ u32 zeroAramNibble;
    /* 0x0E7C */ void (*fxcb)(int);
    /* 0x0E80 */ u8 unk_0xE80[4];
    /* 0x0E84 */ SNDDRVPKTCHAN* pkt[32];
    /* 0x0F04 */ u8 unk_0xF04[0x1C];
    /* 0x0F20 */ u8 zeroMem[0x620];
    /* 0x1540 */ u8* zeroBuf;  // 32-byte aligned in zeroMem
    /* 0x1544 */ u8 unk_0x1544[4];
    /* 0x1548 */ OSThread thread;
    /* 0x1858 */ OSThreadQueue queue;
    /* 0x1860 */ u8 stack[0x8000];
    /* 0x9860 */ u32 lastDone;  // id of the last transfer to finish
    /* 0x9864 */ u32 dmaId;
    /* 0x9868 */ u8 unk_0x9868[0x40];
    /* 0x98A8 */ SNDDRVDMA dma[64];
    /* 0xA8A8 */ OSMutex mutex;
    /* 0xA8C0 */ u8 unk_0xA8C0[0x200];
    /* 0xAAC0 */ s8 mixerInited;
    /* 0xAAC1 */ u8 numFreePending;
    /* 0xAAC2 */ u8 unk_0xAAC2[6];
    /* 0xAAC8 */ void* mixMem[4];
    /* 0xAAD8 */ f32* mixOut[4];  // CPU mixer output per speaker
    /* 0xAAE8 */ u8 unk_0xAAE8[0x38];
};

extern SNDDRV snddrv;

void SNDDRV_DSPMixerUpdate();
void SNDDRV_DSPMixerSetVol(AXVPB* p, int vol);
void SNDDRV_DSPMixerReleaseChannel(AXVPB* p);
void SNDDRV_DSPMixerInitChannel(AXVPB* p, unsigned long mode, int input, unsigned short auxA, unsigned short auxB);
void SNDDRV_DSPMixerInit(int outputs);
void SNDDRV_DSPMixerSetAuxBus(AXVPB* p, unsigned short level, int bus);
void SNDDRV_DSPMixerSetPan(AXVPB* p, unsigned short* vols);
void SNDDRV_initmixer();
void SNDDRV_setfx(int);

#endif
