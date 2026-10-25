// Scripting system setup: one block of script memory carved up for the
// subsystems (file, machine, event, message, object, schedule, timer,
// object creation) plus three pools for script object instances, the
// per-frame update, and the block pool allocator they use.
#include "game/bsutil.h"
#include "game/bsbifunc.h"
#include "game/bsevents.h"
#include "game/bsfile.h"
#include "game/bsmachin.h"
#include "game/bsmessage.h"
#include "game/bsobject.h"
#include "game/bsschedule.h"
#include "game/bstimer.h"
#include "game/bstypes.h"
#include "game/memory.h"
#include "game/Moh2.h"
#include "game/objcreate.h"
#include "game/propdat.h"
#include "game/proxtrig.h"
#include "game/toilet.h"
#include "ea/real.h"
#include "string.h"

// Pools for script object instances (threads and locals), smallest first.
#define BSUTIL_NUM_POOLS 3

static BSUtilObjectInstanceMemoryAllocator g_pMemAllocators[BSUTIL_NUM_POOLS];
static u8* g_pScriptMemory;
static int g_memOffset;
static int g_memSize;

// Level settings field: message registrations to allow.
#define LS_MESSAGE_REGISTRATIONS 0x4A

static inline int Align16(int size) {
    return size + 0x10 - (size & 0xF);
}

// Loads (and keeps) the script class a level resource names.
void BSUtilPreLoadScriptCallback(LevelFileContentsStruct_* res) {
    BSClass_struct* cls;
    char name[0x50];
    strcpy(name, ADDR_TO_PTR(const char, res->fields[0].value));
    name[strlen(name) - 4] = 0;  // drop the extension
    BSFileGetClass(name, &cls);
}

// Instance memory of a class: each thread and its locals.
int BSUtilComputeClassMemoryRequirements(BSClass_struct* cls) {
    int size = 0;
    for (int i = 0; i < cls->m_numThreads; i++) {
        size += sizeof(BSMachineThread_struct) + cls->m_threads[i].m_numLocals * 4;
    }
    return size;
}

void BSUtilFreeObjectInstanceMemory(void* memory) {
    for (int i = 0; i < BSUTIL_NUM_POOLS; i++) {
        if (g_pMemAllocators[i].DoWeOwnThisMemory(memory)) {
            g_pMemAllocators[i].FreeElement(memory);
            return;
        }
    }
}

// From the smallest pool whose elements fit and that has one free.
void* BSUtilGetObjectInstanceMemory(BSClass_struct* cls) {
    int size = cls->m_memoryRequired;
    for (int i = 0; i < BSUTIL_NUM_POOLS; i++) {
        if (g_pMemAllocators[i].m_elementWords * 4 >= size) {
            void* memory = g_pMemAllocators[i].GetFreeElement(true);
            if (memory != NULL) {
                return memory;
            }
        }
    }
    return NULL;
}

// The next `size` bytes of script memory, aligned to `align` (a power of 2).
void* BSUtilGetMemory(int size, int align) {
    int offset = g_memOffset;
    if (align != 0) {
        offset += align - (offset & (align - 1));
    }
    g_memOffset = offset + size;
    return g_pScriptMemory + offset;
}

void BSUtilUpdate(float dt) {
    if (dt > 6.0f) {
        dt = 6.0f;
    }
    BSEventProcessEvents(0x80);
    BSProcessMessages(0x400);
    BSUpdateBIFuncs();
    BSUpdateTimer(dt);
    UpdateProximityTriggerStatus();
    UpdateObjectCreation();
}

void BSUtilShutdownScriptingSystem() {
    ShutdownObjectCreators();
    BSCleanUpBIFuncs();
    BSEndTimer();
    BSEndSchedule();
    BSEndMessage();
    BSEndEvent();
    BSEndMachine();
    FreePropertyMemory();
    MEM_free(g_pScriptMemory);
    g_pScriptMemory = NULL;
    g_memOffset = 0;
    g_memSize = 0;
}

// The instance pools' element sizes are console bytes, where a thread takes
// 0x20. Natively threads are bigger, and since a class's threads fit in its
// element, scaling by the thread size covers them. Native elements are also
// 8-byte aligned, for the threads' pointers.
#define BS_POOL_SIZE(size) GC_SIZE(size, (size) * (int)sizeof(BSMachineThread_struct) / 0x20)
#define BS_POOL_ALIGN GC_SIZE(0, 8)

static inline void InitPool(BSUtilObjectInstanceMemoryAllocator& pool, int elementSize, int count) {
    void* memory = BSUtilGetMemory(pool.ComputeNeededSize(BS_POOL_SIZE(elementSize), count, BS_POOL_ALIGN), 0);
    pool.Init(memory, BS_POOL_SIZE(elementSize), count, BS_POOL_ALIGN);
}

#ifdef FRONTLINE_NATIVE
static inline int PoolNeededSize(int elementSize, int count) {
    return g_pMemAllocators[0].ComputeNeededSize(BS_POOL_SIZE(elementSize), count, BS_POOL_ALIGN);
}
#endif

// Sets up scripting for a level. With `reset` (restarting the level) the
// script memory and the loaded classes are kept.
void BSUtilInitScriptingSystem(bool reset) {
    g_iBSMessageRegistrationListSize =
        TLT_FindNextResourceByType((TLTResourceID)0xD, NULL)->fields[LS_MESSAGE_REGISTRATIONS].value;
    if (g_iBSMessageRegistrationListSize <= 0) {
        g_iBSMessageRegistrationListSize = 0xC80;
    }
    int size = Align16(BSEventGetMemoryRequirements());
    size += Align16(BSFileGetMemoryRequirements());
    size += Align16(BSMachineGetMemoryRequirements());
    size += Align16(BSMessageGetMemoryRequirements());
    size += Align16(BSObjectGetMemoryRequirements());
    size += Align16(BSScheduleGetMemoryRequirements());
    size += Align16(BSTimerGetMemoryRequirements());
    size += Align16(CreateObjectGetMemoryRequirements());
    g_memOffset = 0;
    // (The three instance pools below take 0x25EE0 bytes.)
#ifdef FRONTLINE_NATIVE
    g_memSize = size + PoolNeededSize(0x90, 800) + PoolNeededSize(0x114, 50) + PoolNeededSize(0x2F8, 30);
#else
    g_memSize = size + 0x25EE0;
#endif
    if (!reset) {
        g_pScriptMemory = (u8*)DWI_allocalign("Script memory", g_memSize, 0x10, 0x400);
    }
    InitPool(g_pMemAllocators[0], 0x90, 800);
    InitPool(g_pMemAllocators[1], 0x114, 50);
    InitPool(g_pMemAllocators[2], 0x2F8, 30);

    BSClass_struct* classes = g_pcClassList;
    DebugMsg("INIT:  BSFile\n");
    BSInitFile();
    DebugMsg("INIT:  BSMachine\n");
    BSInitMachine();
    DebugMsg("INIT:  BSEvent\n");
    BSInitEvent();
    DebugMsg("INIT:  BSMessage\n");
    BSInitMessage();
    DebugMsg("INIT:  BSBIFunc\n");
    if (reset) {
        BSResetBIFuncs();
    } else {
        BSInitBIFuncs();
    }
    DebugMsg("INIT:  BSObject\n");
    BSInitBSObject();
    DebugMsg("INIT:  BSTimer\n");
    BSInitTimer();
    DebugMsg("INIT:  BSSchedule\n");
    BSInitSchedule();
    DebugMsg("INIT:  BSObjCreate\n");
    InitObjectCreators();
    if (reset) {
        g_pcClassList = classes;  // BSInitMachine cleared it
    }
    if (!reset) {
        TLT_InitResourceType(BSUtilPreLoadScriptCallback, (TLTResourceID)7);
    }
    char* properties = ADDR_TO_PTR(char, TLT_FindNextResourceByType((TLTResourceID)5, NULL)->fields[0].value);
    LoadPropBSPTree(ADDR_TO_PTR(char, TLT_FindNextResourceByType((TLTResourceID)4, NULL)->fields[0].value), reset);
    LoadPropertyData(properties, reset);
}

bool BSUtilObjectInstanceMemoryAllocator::DoWeOwnThisMemory(void* memory) {
    bool owned = false;
    u8* p = (u8*)memory;
    if (p >= (u8*)m_base && p < (u8*)(m_base + m_totalWords) && (p - (u8*)m_base) % (m_strideWords * 4) == 0) {
        owned = true;
    }
    return owned;
}

void BSUtilObjectInstanceMemoryAllocator::FreeElement(void* element) {
    ((u32*)element)[m_elementWords] = (u32)PTR_TO_ADDR(m_free);  // the link word is an address
    m_free = (u32*)element;
    m_numUsed--;
}

void* BSUtilObjectInstanceMemoryAllocator::GetFreeElement(bool) {
    if (m_numUsed < m_maxElements) {
        u32* element = m_free;
        m_free = ADDR_TO_PTR(u32, ((u32*)element)[m_elementWords]);
        m_numUsed++;
        return element;
    }
    return NULL;
}

// Element size rounded up (with its link word) to `align` bytes.
static inline int ElementStride(int elementSize, int align) {
    int stride = elementSize + 4;
    if (align != 0) {
        int rem = stride & (align - 1);
        if (rem != 0) {
            stride += align - rem;
        }
    }
    return stride;
}

void BSUtilObjectInstanceMemoryAllocator::Init(void* memory, int elementSize, int count, int align) {
    m_base = (u32*)memory;
    m_elementWords = elementSize >> 2;
    m_maxElements = count;
    m_strideWords = ElementStride(elementSize, align) >> 2;
    m_numUsed = 0;
    int total = count * ElementStride(elementSize, align);
    m_totalWords = (total + 0x10 - (total & 0xF)) >> 2;
    int words = m_elementWords;
    u32* link = m_base + words;
    for (int i = 0; i < m_maxElements - 1; i++) {
        *link = (u32)PTR_TO_ADDR((link + (m_strideWords - words)));
        link += m_strideWords;
    }
    *link = 0;
    m_free = m_base;
}

int BSUtilObjectInstanceMemoryAllocator::ComputeNeededSize(int elementSize, int count, int align) {
    int total = count * ElementStride(elementSize, align);
    return total + 0x10 - (total & 0xF);
}
