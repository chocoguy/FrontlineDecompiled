#include "game/bsobject.h"

#include "game/bsevents.h"
#include "game/bsfile.h"
#include "game/bsmachin.h"
#include "game/bstimer.h"
#include "game/bsutil.h"

#define BS_MAX_OBJECTS 0x370
#define BS_ERR_NO_CLASS 0xD

static BSUtilObjectInstanceMemoryAllocator g_pBSObjectAllocator;
static int g_uniqueIDCount;

int BSObjectGetMemoryRequirements() {
    return g_pBSObjectAllocator.ComputeNeededSize(sizeof(BSObject), BS_MAX_OBJECTS, 0);
}

int BSInitBSObject() {
    int size = g_pBSObjectAllocator.ComputeNeededSize(sizeof(BSObject), BS_MAX_OBJECTS, 0);
    g_pBSObjectAllocator.Init(BSUtilGetMemory(size, 0), sizeof(BSObject), BS_MAX_OBJECTS, 0);
    return 0;
}

int CreateBSObject(const char* className, BSObject** out, BSGO_Basic* gameObject,
                   TriggerObject_struct* trigger, void** outAlias) {
    *out = (BSObject*)g_pBSObjectAllocator.GetFreeElement(true);
    if (outAlias != NULL) {
        *outAlias = *out;
    }
    (*out)->m_class = NULL;
    BSFileGetClass(className, &(*out)->m_class);
    if ((*out)->m_class == NULL) {
        return BS_ERR_NO_CLASS;
    }
    (*out)->m_threads = (BSMachineThread_struct*)BSUtilGetObjectInstanceMemory((*out)->m_class);
    (*out)->m_uniqueID = g_uniqueIDCount++;
    (*out)->m_trigger = trigger;
    (*out)->m_gameObject = gameObject;
    (*out)->m_started = false;

    u32* locals = (u32*)((*out)->m_threads + (*out)->m_class->m_numThreads);
    int localOffset = 0;
    for (u32 i = 0; i < (*out)->m_class->m_numThreads; i++) {
        BSCode_struct* code = &(*out)->m_class->m_threads[i];
        int err = BSCreateThread(*out, code, &(*out)->m_threads[i], locals + localOffset);
        if (err != 0) {
            return err;
        }
        localOffset += code->m_numLocals;
    }
    return 0;
}

int DestroyBSObject(BSObject* obj) {
    obj->m_uniqueID = -1;
    for (u32 i = 0; i < obj->m_class->m_numThreads; i++) {
        BSDestroyThread(&obj->m_threads[i]);
    }
    BSUnregisterTimerEvents(obj);
    BSUtilFreeObjectInstanceMemory(obj->m_threads);
    g_pBSObjectAllocator.FreeElement(obj);
    return 0;
}

bool BSObjectTriggerEvent(BSObject* obj, unsigned short event, void* data, BSObject* sender,
                          bool immediate) {
    bool handled = false;
    BSMachineThread_struct* thread = obj->m_threads;
    int numThreads = obj->m_class->m_numThreads;
    if (obj->m_uniqueID != -1 && (obj->m_started || !immediate || event == BSEVENT_START)) {
        for (int i = 0; i < numThreads; i++, thread++) {
            BSCode_EventHandlerEntry_struct* entry;
            int state = BSEventFindDeclaredState(event, thread, &entry);
            if (entry != NULL) {
                handled = true;
                if (immediate) {
                    BSEventQueueImmediateEvent(obj, thread, entry, state, data, sender);
                } else {
                    BSEventTriggerEvent(obj, thread, entry, state, data, sender);
                }
            }
        }
        if (immediate) {
            BSEventProcessImmediateEvents();
        }
    }
    // Event data blocks nobody took are released here.
    if (BSEventIsEventMemoryBlock(data) && !handled) {
        BSEventCheckMemBlockReferenceCount(data);
    }
    if (event == BSEVENT_START && !handled) {
        obj->m_started = true;
    }
    return handled;
}
