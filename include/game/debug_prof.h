// debug_prof: profiling counters (PROFILE_BEGIN/END bins with a 300-frame
// history), named debug timers dumped at exit, and the (empty in retail)
// debug stats render bins. Hand-maintained.
#ifndef _GAME_DEBUG_PROF_H_
#define _GAME_DEBUG_PROF_H_

#include "types.h"
#include "game_types.h"
#include "game/compartment.h"

struct CDmaPacket;
class CRenderList;
class CScreen;

// One profiling counter (size 0x18): ticks spent and times entered.
struct SProfileBin {
    SProfileBin() : m_parent(-1), m_name(NULL), m_ticks(0), m_0xC(0), m_0x10(0), m_count(0) {}
    /* 0x00 */ int m_parent;  // -1: a root
    /* 0x04 */ const char* m_name;
    /* 0x08 */ s32 m_ticks;
    /* 0x0C */ s32 m_0xC;
    /* 0x10 */ s32 m_0x10;
    /* 0x14 */ s32 m_count;
};
typedef SProfileBin CProfileBin;

// A bin's last 300 frames, with the frames of the min/max of three fields.
class SProfHistBin {
public:
    SProfHistBin();  // 0x800F34F0
    /* 0x0 */ s16 m_minTicks;
    /* 0x2 */ s16 m_maxTicks;
    /* 0x4 */ s16 m_min0xC;
    /* 0x6 */ s16 m_max0xC;
    /* 0x8 */ s16 m_min0x10;
    /* 0xA */ s16 m_max0x10;
    /* 0xC */ SProfileBin m_frames[300];
};

// Draws the profile history graph (nothing in retail). vtable 0x8018B530
class CProfHistRenderBin : public CRenderBin {
public:
    CProfHistRenderBin() { m_type = 9; }
    virtual ~CProfHistRenderBin() {}  // 0x800F3494 (weak)
    virtual int Render(CDmaPacket&, void*);  // 0x800F451C
};

// Draws the frame stats (nothing in retail). vtable 0x8018B560
class CDbgStatsRenderBin : public CRenderBin {
public:
    CDbgStatsRenderBin() { m_type = 9; }
    virtual int Render(CDmaPacket&, void*);  // 0x800F4564
    virtual ~CDbgStatsRenderBin() {}  // 0x800F45AC (weak)
};

// A named debug timer (size 0x34).
struct SDebugProfTimer {
    /* 0x00 */ int m_parent;  // -1: a root
    /* 0x04 */ char m_name[0x20];  // empty: unused
    /* 0x24 */ u32 m_count;
    /* 0x28 */ u32 m_ticks;  // below 1e9
    /* 0x2C */ u32 m_billions;  // of ticks
    /* 0x30 */ int m_running;
};

#define DEBUG_PROF_TIMER_MAX 16

void PROFILE_UPDATE();  // 0x800F3578
void DebugProfTimerVerifyNew(int, char*, int);  // 0x800F394C
void DebugProfTimerRegister(int slot, char* name, int parent);  // 0x800F3B74
void DebugProfTimerInitAll();  // 0x800F3BEC
void DebugProfTimerDump(int, int);  // 0x800F3CD0
void DebugProfTimerDumpRecurse(int, int);  // 0x800F3DA0
void DebugProfTimerDumpAll();  // 0x800F407C
void DebugProfTimerStop(int);  // 0x800F4120
void DebugProfTimerResume(int);  // 0x800F41A0
void DebugProfTimerPause(int);  // 0x800F41F0
void DebugProfTimerStart(int);  // 0x800F4240
void DebugStats_Draw(int, int, int, int, int);  // 0x800F429C
void DebugStats_Init(CScreen*, CRenderList*);  // 0x800F43C4

extern SDebugProfTimer g_pDebugProfTimers[DEBUG_PROF_TIMER_MAX];  // 0x803118DC
extern bool g_bDebugStatsDisabled;  // 0x8034E368
extern bool g_bProfileUpdateHistory;  // 0x8034E369
extern bool g_bDebugStatsDrawProfData;  // 0x8034ED5C
extern int g_HistoryCursor;  // 0x8034ED60
extern int g_GraphCursor;  // 0x8034ED64
extern float g_fGraphCursor;  // 0x8034ED68
extern int g_ProfileBinTOP;  // 0x8034ED6C
extern int g_ProfileBinSelect;  // 0x8034ED70
extern int g_ProfileBinGraph;  // 0x8034ED74

// Bins used by decompiled code.
#define PROFBIN_FRAME 0
#define PROFBIN_MAIN 1
#define PROFBIN_SCRIPT 11
#define PROFBIN_DRAW 12
#define PROFBIN_NUM 31
#define PROFBIN_CPT_DRAW 13      // CCompartment::Draw
#define PROFBIN_CPT_NODES 14     // compartment BSP walk
#define PROFBIN_CPT_BINS 15      // queueing compartment material bins
#define PROFBIN_LIST_DRAW 18
#define PROFBIN_LIST_KICK 19
#define PROFBIN_GPU_WAIT 20
#define PROFBIN_LIST_WAIT 21
#define PROFBIN_FLIP_WAIT 22
#define PROFBIN_CDB_LINE 24      // CCDBObject line tests
#define PROFBIN_CDB_LEAF 25      // line test leaf boxes
#define PROFBIN_CDB_TRIGROUP 26  // line test triangle groups
#define PROFBIN_CDB_TRI 27       // line test triangles
#define PROFBIN_CDB_FIBER 28
#define PROFBIN_CDB_SPHERE 29
#define PROFBIN_CDB_CAPSULE 30

// Profiling counters (size 0x2E8).
struct CProfileBins {
    /* 0x000 */ CProfileBin m_bins[PROFBIN_NUM];
};

#define PROFILE_BEGIN(bin) (g_ProfileBins.m_bins[bin].m_ticks -= OSGetTick())
#define PROFILE_END(bin) (g_ProfileBins.m_bins[bin].m_ticks += OSGetTick(), g_ProfileBins.m_bins[bin].m_count++)

extern CProfileBins g_ProfileBins;  // 0x802DACA0
#endif
