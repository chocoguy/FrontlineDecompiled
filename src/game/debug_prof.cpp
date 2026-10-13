// debug_prof: profiling counters with a 300-frame history (browsed with the
// controller while profiling), named debug timers dumped at exit, and the
// debug stats render bins (which draw nothing in the retail game).
#include "game/debug_prof.h"
#include "game/renderlist.h"
#include "game/Screen.h"
#include "game/input.h"
#include "game/Moh2.h"
#include "dolphin/os.h"
#include "stdio.h"
#include "string.h"

#define PROFILE_HISTORY 300

// The PS2 performance counter modes (unused on GameCube).
class CPerformanceCount {
public:
    static int m_ModeCounter0;
    static int m_ModeCounter1;
};
int CPerformanceCount::m_ModeCounter0;
int CPerformanceCount::m_ModeCounter1;

// Per-frame stats handed to DebugStats_Draw.
struct SDebugStats {
    int m_listDraw;
    int m_other;
    int m_update;
    int m_draw;
    int m_extra;
};

static CScreen* g_pScreen;
static CRenderList* g_pRenderList;
static int g_screenWidth;
static int g_screenHeight;
static int currentIndex;
static SDebugStats debugStats[60];

CProfileBins g_ProfileBins;
static SProfHistBin g_ProfileBinsHistory[PROFBIN_NUM];
SDebugProfTimer g_pDebugProfTimers[DEBUG_PROF_TIMER_MAX];

bool g_bDebugStatsDisabled = true;
bool g_bProfileUpdateHistory = true;
bool g_bDebugStatsDrawProfData;
int g_HistoryCursor;
int g_GraphCursor;
float g_fGraphCursor = g_GraphCursor;
int g_ProfileBinTOP;
int g_ProfileBinSelect = g_ProfileBinTOP;
int g_ProfileBinGraph = g_ProfileBinTOP;

static CDbgStatsRenderBin g_DbgStatsRenderBin;
static CProfHistRenderBin g_ProfHistRenderBin;

// Names the profile bins and their tree.
class CProfileInitClass {
public:
    CProfileInitClass() {
        Set(0, -1, "Main");
        Set(1, 0, "Update");
        Set(2, 1, "Scene");
        Set(3, 2, "Begin");
        Set(4, 2, "Constrain");
        Set(5, 2, "AI");
        Set(6, 5, "Update");
        Set(7, 5, "Commit");
        Set(8, 2, "Collision");
        Set(9, 8, "Slow");
        Set(10, 8, "Fast");
        Set(11, 1, "Script");
        Set(12, 0, "Draw");
        Set(13, 12, "cpt");
        Set(14, 13, "cull");
        Set(15, 13, "pkt");
        Set(16, 12, "msh");
        Set(17, 12, "anim");
        Set(20, 0, "Idle");
        Set(21, 20, "GSync");
        Set(22, 20, "VSync");
        Set(18, 0, "DrawFinish");
        Set(19, 0, "DrawKick");
        Set(23, -1, "Collision");
        Set(24, 23, "Line");
        Set(25, 24, "Leaf");
        Set(26, 25, "Group");
        Set(27, 26, "Tri");
        Set(28, 23, "Fiber");
        Set(29, 23, "Sphere");
        Set(30, 23, "Capsule");
        CPerformanceCount::m_ModeCounter0 = 6;
        CPerformanceCount::m_ModeCounter1 = 6;
    }

private:
    static void Set(int bin, int parent, const char* name) {
        SProfileBin& b = g_ProfileBins.m_bins[bin];
        b.m_name = name;
        b.m_parent = parent;
        b.m_0x10 = 0;
        b.m_0xC = 0;
        b.m_ticks = 0;
    }
};

static CProfileInitClass g_ProfileDataInit;

SProfHistBin::SProfHistBin() {
    m_minTicks = -1;
    m_maxTicks = -1;
    m_min0xC = -1;
    m_max0xC = -1;
    m_min0x10 = -1;
    m_max0x10 = -1;
}

// Records this frame's bins into the history, finds each bin's extremes
// over the history, clears the bins, then handles the profiler controls.
void PROFILE_UPDATE() {
    if (g_bProfileUpdateHistory) {
        g_HistoryCursor = (g_HistoryCursor + 1) % PROFILE_HISTORY;
    }
    for (int b = 0; b < PROFBIN_NUM; b++) {
        SProfileBin& bin = g_ProfileBins.m_bins[b];
        SProfHistBin& hist = g_ProfileBinsHistory[b];
        hist.m_frames[g_HistoryCursor] = bin;
        s16 frame = (g_HistoryCursor + 1) % PROFILE_HISTORY;
        hist.m_minTicks = g_HistoryCursor;
        hist.m_maxTicks = g_HistoryCursor;
        hist.m_min0xC = g_HistoryCursor;
        hist.m_max0xC = g_HistoryCursor;
        hist.m_min0x10 = g_HistoryCursor;
        hist.m_max0x10 = g_HistoryCursor;
        for (int i = 0; i < PROFILE_HISTORY; i++) {
            SProfileBin& f = hist.m_frames[frame];
            if (f.m_ticks < hist.m_frames[hist.m_minTicks].m_ticks) {
                hist.m_minTicks = frame;
            }
            if (f.m_ticks > hist.m_frames[hist.m_maxTicks].m_ticks) {
                hist.m_maxTicks = frame;
            }
            if (f.m_0xC < hist.m_frames[hist.m_min0xC].m_0xC) {
                hist.m_min0xC = frame;
            }
            if (f.m_0xC > hist.m_frames[hist.m_max0xC].m_0xC) {
                hist.m_max0xC = frame;
            }
            if (f.m_0x10 < hist.m_frames[hist.m_min0x10].m_0x10) {
                hist.m_min0x10 = frame;
            }
            if (f.m_0x10 > hist.m_frames[hist.m_max0x10].m_0x10) {
                hist.m_max0x10 = frame;
            }
            frame = (frame + 1) % PROFILE_HISTORY;
        }
        bin.m_count = 0;
        bin.m_0x10 = 0;
        bin.m_0xC = 0;
        bin.m_ticks = 0;
    }

    if (g_inputMgr.GetValue(0, 0x7B)) {
        if (--g_ProfileBinSelect < -1) {
            g_ProfileBinSelect = -1;
        }
    }
    if (g_inputMgr.GetValue(0, 0x7C)) {
        if (++g_ProfileBinSelect >= PROFBIN_NUM) {
            g_ProfileBinSelect = PROFBIN_NUM - 1;
        }
    }
    if (g_inputMgr.GetValue(0, 0x7D)) {
        // Up to the parent (reads before the array at the top, as the original).
        int parent = g_ProfileBins.m_bins[g_ProfileBinTOP].m_parent;
        if (parent != -1) {
            g_ProfileBinTOP = parent;
        } else {
            g_ProfileBinTOP = -1;
        }
    }
    if (g_inputMgr.GetValue(0, 0x7E)) {
        g_ProfileBinTOP = g_ProfileBinSelect;
    }
    if (g_inputMgr.GetValue(0, 0x7A)) {
        g_bProfileUpdateHistory = !g_bProfileUpdateHistory;
    }
    if (!g_bProfileUpdateHistory) {
        // Scrub the frozen history with the stick.
        float v = (float)g_inputMgr.GetValue(0, 0x89) * 0.0078125f;
        g_fGraphCursor += v * (v * v);
        if (g_fGraphCursor < 0.0f) {
            g_fGraphCursor = 299.0f + g_fGraphCursor;
        } else if (g_fGraphCursor >= 300.0f) {
            g_fGraphCursor = g_fGraphCursor - 300.0f;
        }
        g_GraphCursor = (int)g_fGraphCursor;
    }
}

// Reports misuse of a timer slot (the checks are as the original's).
void DebugProfTimerVerifyNew(int slot, char* name, int parent) {
    if (g_pDebugProfTimers[slot].m_name[0] != '\0') {
        // (Counts the empty slots before the first used one.)
        int free = 0;
        while (free < DEBUG_PROF_TIMER_MAX && g_pDebugProfTimers[free].m_name[0] == '\0') {
            free++;
        }
        if (free == DEBUG_PROF_TIMER_MAX) {
            DebugMsg("Debug Prof Timer: All slots used.  Increase DEBUG_PROF_TIMER_MAX.\n");
        } else {
            DebugMsg("Debug Prof Timer: Slot %d is already used.  The first available slot is %d.\n", slot, free);
        }
    }
    if (name == NULL || *name == '\0') {
        DebugMsg("Debug Prof Timer: Slot name is NULL for slot %d.\n", slot);
    }
    strlen(name);
    for (int i = 0; i < DEBUG_PROF_TIMER_MAX; i++) {
        strcmp(name, g_pDebugProfTimers[i].m_name);
    }
    for (int p = parent; p != -1; p = g_pDebugProfTimers[p].m_parent) {
        if (p == slot) {
            DebugMsg("Debug Prof Timer: Timer %d (%s) is trying to use %d as a parent, but that leads to a circular "
                     "dependency.\n",
                     slot, name, parent);
        }
    }
    if (parent != -1 && (parent < 0 || parent >= DEBUG_PROF_TIMER_MAX)) {
        DebugMsg("Debug Prof Timer: Timer %d (%s) is trying to use %d as a parent, but %d is not a valid timer.\n",
                 slot, name, parent, parent);
    }
    if (parent != -1 && g_pDebugProfTimers[parent].m_name[0] == '\0') {
        DebugMsg("Debug Prof Timer: Timer %d (%s) is trying to use %d as a parent, but %d is not initialized.\n",
                 slot, name, parent, parent);
    }
}

void DebugProfTimerRegister(int slot, char* name, int parent) {
    DebugProfTimerVerifyNew(slot, name, parent);
    SDebugProfTimer& t = g_pDebugProfTimers[slot];
    strcpy(t.m_name, name);
    t.m_parent = parent;
    t.m_count = 0;
    t.m_ticks = 0;
    t.m_billions = 0;
    t.m_running = 0;
}

void DebugProfTimerInitAll() {
    for (int i = 0; i < DEBUG_PROF_TIMER_MAX; i++) {
        SDebugProfTimer& t = g_pDebugProfTimers[i];
        t.m_parent = -1;
        t.m_name[0] = '\0';
        t.m_count = 0;
        t.m_ticks = 0;
        t.m_billions = 0;
        t.m_running = 0;
    }
}

// One line: indent, depth and name padded to column 40, time, count.
void DebugProfTimerDump(int slot, int depth) {
    SDebugProfTimer& t = g_pDebugProfTimers[slot];
    for (int i = 0; i < depth; i++) {
        printf(" ");
    }
    printf("%2d, %s", depth, t.m_name);
    for (int i = depth + strlen(t.m_name) + 4; i < 40; i++) {
        printf(" ");
    }
    printf(", %u%09u, %u", t.m_billions, t.m_ticks, t.m_count);
    printf("\n");
}

// The timer and its children (`slot` -1: the roots).
void DebugProfTimerDumpRecurse(int slot, int depth) {
    if (slot != -1) {
        DebugProfTimerDump(slot, depth);
    }
    for (int i = 0; i < DEBUG_PROF_TIMER_MAX; i++) {
        if (slot == g_pDebugProfTimers[i].m_parent && g_pDebugProfTimers[i].m_name[0] != '\0') {
            DebugProfTimerDumpRecurse(i, depth + 1);
        }
    }
}

void DebugProfTimerDumpAll() {
    printf("------------------------------\n");
    printf("--   Begin Profile Dump     --\n");
    printf("------------------------------\n");
    printf("\n");
    printf("Rank, Name, Time, Count\n");
    DebugProfTimerDumpRecurse(-1, -1);
    printf("\n");
    printf("------------------------------\n");
    printf("--    End Profile Dump      --\n");
    printf("------------------------------\n");
}

void DebugProfTimerStop(int slot) {
    SDebugProfTimer& t = g_pDebugProfTimers[slot];
    t.m_running--;
    t.m_ticks += OSGetTick();
    if (t.m_ticks >= 1000000000) {
        t.m_billions++;
        t.m_ticks -= 1000000000;
    }
}

void DebugProfTimerResume(int slot) {
    SDebugProfTimer& t = g_pDebugProfTimers[slot];
    t.m_running++;
    t.m_ticks -= OSGetTick();
}

void DebugProfTimerPause(int slot) {
    SDebugProfTimer& t = g_pDebugProfTimers[slot];
    t.m_running--;
    t.m_ticks += OSGetTick();
}

void DebugProfTimerStart(int slot) {
    SDebugProfTimer& t = g_pDebugProfTimers[slot];
    t.m_count++;
    t.m_running++;
    t.m_ticks -= OSGetTick();
}

// Records the frame's stats and queues the stats or graph bin when enabled.
void DebugStats_Draw(int listDraw, int other, int update, int draw, int extra) {
    int index = (currentIndex + 1) % 60;
    SDebugStats& stats = debugStats[index];
    stats.m_listDraw = listDraw;
    stats.m_other = other;
    stats.m_update = update;
    stats.m_draw = draw;
    currentIndex = index;
    stats.m_extra = extra;
    if (!g_bDebugStatsDisabled) {
        if (g_bDebugStatsDrawProfData) {
            g_ProfileBinGraph = g_ProfileBinSelect;
            if (g_ProfileBinSelect != -1) {
                g_pRenderList->Render(g_ProfHistRenderBin, NULL);
            }
        } else {
            g_pRenderList->Render(g_DbgStatsRenderBin, NULL);
        }
    }
}

void DebugStats_Init(CScreen* screen, CRenderList* renderList) {
    g_pScreen = screen;
    g_screenWidth = screen->GetWidth();
    g_screenHeight = g_pScreen->GetHeight();
    g_pRenderList = renderList;
    for (int i = 0; i < 60; i++) {
        debugStats[i].m_listDraw = 0;
        debugStats[i].m_other = 0;
        debugStats[i].m_draw = 0;
        debugStats[i].m_extra = 0;
    }
    renderList->Register(g_DbgStatsRenderBin);
    g_pRenderList->Register(g_ProfHistRenderBin);
}

int CProfHistRenderBin::Render(CDmaPacket&, void*) {
    g_pScreen->GetWidth();
    g_pScreen->GetHeight();
    return 0;
}

int CDbgStatsRenderBin::Render(CDmaPacket&, void*) {
    g_pScreen->GetWidth();
    g_pScreen->GetHeight();
    return 0;
}
