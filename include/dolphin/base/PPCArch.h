// PowerPC 750CL special-purpose register access (dolphin/base/PPCArch.c).
#ifndef _DOLPHIN_BASE_PPCARCH_H_
#define _DOLPHIN_BASE_PPCARCH_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// SPR numbers. MWCC's assembler maps the HID2 mnemonic to the wrong register
// (IABR), so asm should use these instead.
#define SPR_HID0 1008
#define SPR_HID2 920
#define SPR_WPAR 921
#define SPR_L2CR 1017

#define HID0_ICE 0x00008000u
#define HID0_DCE 0x00004000u
#define HID2_LCE 0x10000000u
#define HID2_DMAQL_MASK 0x0F000000u
#define HID2_DCHERR 0x00800000u
#define HID2_DNCERR 0x00400000u
#define HID2_DCMERR 0x00200000u
#define HID2_DQOERR 0x00100000u
#define L2CR_L2E 0x80000000u
#define L2CR_L2I 0x00200000u
#define L2CR_L2IP 0x00000001u
#define SRR1_DMA_BIT 0x00200000u
#define MSR_FP 0x00002000u
#define MSR_IR 0x00000020u
#define MSR_DR 0x00000010u

u32 PPCMfmsr(void);
void PPCMtmsr(u32 newMSR);
u32 PPCMfhid0(void);
void PPCMthid0(u32 newHID0);
u32 PPCMfl2cr(void);
void PPCMtl2cr(u32 newL2cr);
void PPCMtdec(u32 newDec);
void PPCSync(void);
void PPCHalt(void);
u32 PPCMffpscr(void);
void PPCMtfpscr(u32 newFPSCR);
u32 PPCMfhid2(void);
u32 PPCMfwpar(void);
void PPCMthid2(u32 newhid2);
void PPCMtwpar(u32 newwpar);
void PPCDisableSpeculation(void);
void PPCSetFpNonIEEEMode(void);

#ifdef __cplusplus
}
#endif

#endif
