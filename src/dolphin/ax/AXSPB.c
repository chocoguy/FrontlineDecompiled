#include <dolphin/ax.h>
#include <dolphin/os.h>

// Depop: the last sample of each stopped voice, summed per bus, is faded out by the DSP.
static s32 __AXSpbAL;
static s32 __AXSpbAR;
static s32 __AXSpbAS;
static s32 __AXSpbAAL;
static s32 __AXSpbAAR;
static s32 __AXSpbAAS;
static s32 __AXSpbABL;
static s32 __AXSpbABR;
static s32 __AXSpbABS;

static AXSPB __AXStudio __attribute__((aligned(32)));

AXSPB* __AXGetStudio(void) {
    return &__AXStudio;
}

// Hands the DSP up to 20 * 160 of the level this frame.
static inline void __AXDepopFade(s32* value, u16* hi, s16* delta) {
    s32 n = *value / 160;
    if (n != 0) {
        if (n > 20) {
            n = 20;
        }
        if (n < -20) {
            n = -20;
        }
        *(s32*)hi = *value;
        *value -= n * 160;
        *delta = -n;
    } else {
        *value = 0;
        *(s32*)hi = 0;
        *delta = 0;
    }
}

void __AXPrintStudio(void) {
    __AXDepopFade(&__AXSpbAL, &__AXStudio.dpopLHi, &__AXStudio.dpopLDelta);
    __AXDepopFade(&__AXSpbAR, &__AXStudio.dpopRHi, &__AXStudio.dpopRDelta);
    __AXDepopFade(&__AXSpbAS, &__AXStudio.dpopSHi, &__AXStudio.dpopSDelta);
    __AXDepopFade(&__AXSpbAAL, &__AXStudio.dpopALHi, &__AXStudio.dpopALDelta);
    __AXDepopFade(&__AXSpbAAR, &__AXStudio.dpopARHi, &__AXStudio.dpopARDelta);
    __AXDepopFade(&__AXSpbAAS, &__AXStudio.dpopASHi, &__AXStudio.dpopASDelta);
    __AXDepopFade(&__AXSpbABL, &__AXStudio.dpopBLHi, &__AXStudio.dpopBLDelta);
    __AXDepopFade(&__AXSpbABR, &__AXStudio.dpopBRHi, &__AXStudio.dpopBRDelta);
    __AXDepopFade(&__AXSpbABS, &__AXStudio.dpopBSHi, &__AXStudio.dpopBSDelta);
    DCFlushRange(&__AXStudio, sizeof(__AXStudio));
}

void __AXSPBInit(void) {
    __AXSpbABS = 0;
    __AXSpbABR = 0;
    __AXSpbABL = 0;
    __AXSpbAAS = 0;
    __AXSpbAAR = 0;
    __AXSpbAAL = 0;
    __AXSpbAS = 0;
    __AXSpbAR = 0;
    __AXSpbAL = 0;
}

void __AXSPBQuit(void) {
}

void __AXDepopVoice(AXPB* p) {
    __AXSpbAL += p->dpop.aL;
    __AXSpbAAL += p->dpop.aAuxAL;
    __AXSpbABL += p->dpop.aAuxBL;
    __AXSpbAR += p->dpop.aR;
    __AXSpbAAR += p->dpop.aAuxAR;
    __AXSpbABR += p->dpop.aAuxBR;
    __AXSpbAS += p->dpop.aS;
    __AXSpbAAS += p->dpop.aAuxAS;
    __AXSpbABS += p->dpop.aAuxBS;
}
