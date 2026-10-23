// BS script messages.
#include "game/bsmessage.h"
#include "game/bsevents.h"
#include "game/bsmachin.h"
#include "game/bstypes.h"
#include "game/bsutil.h"

#define BSMSG_QUEUE_SIZE 0x400
#define BSMSG_NUM_MESSAGES 0x2D9

// A message waiting to run a handler (size 0x24).
struct BSQueuedMessage_struct {
    /* 0x00 */ BSObject* m_sender;
    /* 0x04 */ BSObject* m_target;
    /* 0x08 */ BSObject* m_object;  // the handler's
    /* 0x0C */ BSMachineThread_struct* m_thread;
    /* 0x10 */ TriggerObject_struct* m_trigger;
    /* 0x14 */ void* m_data;
    /* 0x18 */ int m_objectID;  // m_object->m_uniqueID when queued
    /* 0x1C */ BSCode_MessageHandlerEntry_struct* m_entry;
    /* 0x20 */ u16 m_state;
};

static BSQueuedMessage_struct g_MessageQueue[BSMSG_QUEUE_SIZE];
// Per message ID, the registered handlers (newest first).
static BSMessageRegistration_struct** g_MessageRegistrationArray;
static BSMessageRegistration_struct* g_MessageRegistrationFreeList;
static BSMessageRegistration_struct* g_MessageRegistrationFreeListHead;
static int g_MessageQueueHead;
static int g_MessageQueueTail;
int g_iBSMessageRegistrationListSize;
static u8* g_pBSMessageMemory;
static int g_pBSMessageMemoryOffset;

static inline int NextQueueIndex(int i) {
    i = i + 1;
    if (i >= BSMSG_QUEUE_SIZE) {
        i = 0;
    }
    return i;
}

int BSMessageGetMemoryRequirements() {
    return g_iBSMessageRegistrationListSize * sizeof(BSMessageRegistration_struct) +
           BSMSG_NUM_MESSAGES * sizeof(BSMessageRegistration_struct*);
}

int BSInitMessage() {
    g_pBSMessageMemory = (u8*)BSUtilGetMemory(
        g_iBSMessageRegistrationListSize * sizeof(BSMessageRegistration_struct) +
            BSMSG_NUM_MESSAGES * sizeof(BSMessageRegistration_struct*),
        0);
    g_pBSMessageMemoryOffset = 0;
    g_MessageQueueHead = 0;
    g_MessageQueueTail = 0;
    g_MessageRegistrationArray = (BSMessageRegistration_struct**)g_pBSMessageMemory;
    g_pBSMessageMemoryOffset = BSMSG_NUM_MESSAGES * sizeof(BSMessageRegistration_struct*);
    for (int i = 0; i < BSMSG_NUM_MESSAGES; i++) {
        g_MessageRegistrationArray[i] = NULL;
    }

    g_MessageRegistrationFreeList =
        (BSMessageRegistration_struct*)(g_pBSMessageMemory + g_pBSMessageMemoryOffset);
    g_pBSMessageMemoryOffset += g_iBSMessageRegistrationListSize * sizeof(BSMessageRegistration_struct);
    for (int i = 0; i < g_iBSMessageRegistrationListSize - 1; i++) {
        g_MessageRegistrationFreeList[i].m_next = &g_MessageRegistrationFreeList[i + 1];
    }
    g_MessageRegistrationFreeList[g_iBSMessageRegistrationListSize - 1].m_next = NULL;
    g_MessageRegistrationFreeListHead = g_MessageRegistrationFreeList;
    return 0;
}

int BSEndMessage() {
    g_MessageRegistrationArray = NULL;
    g_MessageRegistrationFreeList = NULL;
    return 0;
}

BSMessageRegistration_struct* BSRegisterMessage(BSCode_MessageHandlerEntry_struct* entry,
                                                BSObject* obj, unsigned short state,
                                                BSMachineThread_struct* thread,
                                                BSMessageRegistration_struct** replaced) {
    *replaced = NULL;
    if (entry->m_flags & BSMSG_GLOBAL) {
        // The same handler slot registered again (e.g. a state re-entered).
        for (BSMessageRegistration_struct* r = g_MessageRegistrationArray[entry->m_message];
             r != NULL; r = r->m_next) {
            BSCode_MessageHandlerEntry_struct* old = r->m_entry;
            if (thread == r->m_thread && entry->m_filterArg == old->m_filterArg &&
                entry->m_filter == old->m_filter) {
                if (entry->m_address == old->m_address) {
                    return r;
                }
                r->m_flags |= BSMSGREG_REPLACED;
                *replaced = r;
                break;
            }
        }
    }

    // (The original reads the free list link before checking for an empty list.)
    BSMessageRegistration_struct* reg = g_MessageRegistrationFreeListHead;
    if (reg == NULL) {
        return NULL;
    }
    g_MessageRegistrationFreeListHead = reg->m_next;
    reg->m_object = obj;
    reg->m_thread = thread;
    reg->m_entry = entry;
    reg->m_state = state;
    reg->m_flags = 0;
    if (entry->m_flags & BSMSG_GLOBAL) {
        reg->m_next = g_MessageRegistrationArray[entry->m_message];
        reg->m_prev = NULL;
        g_MessageRegistrationArray[entry->m_message] = reg;
        if (reg->m_next != NULL) {
            reg->m_next->m_prev = reg;
        }
    } else {
        reg->m_next = NULL;
        reg->m_prev = NULL;
    }
    return reg;
}

int BSSendMessage(unsigned short message, BSObject* sender, TriggerObject_struct* trigger,
                  BSObject* target, void* data) {
    if (sender != NULL) {
        trigger = sender->m_trigger;
    }

    for (BSMessageRegistration_struct* r = g_MessageRegistrationArray[message]; r != NULL;
         r = r->m_next) {
        if (r->m_flags & BSMSGREG_REPLACED) {
            continue;
        }
        BSObject* obj = r->m_object;
        if (obj->m_uniqueID == -1) {
            continue;
        }

        BSCode_MessageHandlerEntry_struct* entry = r->m_entry;
        u8 match = 0;
        switch (entry->m_filter) {
        case BSMSG_FILTER_ANY:
            match = 1;
            break;
        case BSMSG_FILTER_SENDER:
            switch ((u16)entry->m_filterArg) {
            case 1:
                if (obj == sender) {
                    match = 1;
                }
                break;
            case 2:
                if (obj->m_trigger->m_object == sender) {
                    match = 1;
                }
                break;
            case 3:
                if (trigger != NULL && trigger->m_object == obj && trigger->m_object != sender) {
                    match = 1;
                }
                break;
            }
            break;
        case BSMSG_FILTER_TARGET:
            switch ((u16)entry->m_filterArg) {
            case 1:
                if (obj == target) {
                    match = 1;
                }
                break;
            case 2:
                break;
            case 3:
                if (target != NULL && target->m_trigger->m_object == obj) {
                    match = 1;
                }
                break;
            }
            break;
        case BSMSG_FILTER_GROUP: {
            u8 inGroup;
            u32 group = entry->m_filterArg >> 16;
            if (trigger == NULL) {
                inGroup = 0;
            } else {
                inGroup = 0;
                u32* groups = trigger->m_data->m_groups;
                if (groups != NULL) {
                    u32* end = groups + groups[0];
                    for (u32* g = groups + 1; g <= end; g++) {
                        if ((*g & 0xFF) == group) {
                            inGroup = 1;
                            break;
                        }
                    }
                }
            }
            match = inGroup;
            break;
        }
        case BSMSG_FILTER_CATEGORY:
            match = BSMessageIsGroupCategoryMember(trigger, obj->m_trigger, entry->m_filterArg);
            break;
        }

        if (match) {
            int next = NextQueueIndex(g_MessageQueueTail);
            if (g_MessageQueueHead != next) {
                BSQueuedMessage_struct* q = &g_MessageQueue[g_MessageQueueTail];
                q->m_sender = sender;
                q->m_target = target;
                q->m_trigger = trigger;
                q->m_object = r->m_object;
                q->m_thread = r->m_thread;
                q->m_data = data;
                q->m_entry = r->m_entry;
                q->m_objectID = r->m_object->m_uniqueID;
                q->m_state = r->m_state;
                g_MessageQueueTail = next;
            }
        }
    }
    return 0;
}

void BSMessageRemoveHandler(BSMessageRegistration_struct* reg) {
    BSCode_MessageHandlerEntry_struct* entry = reg->m_entry;
    if (entry->m_flags & BSMSG_GLOBAL) {
        if (reg->m_prev != NULL) {
            reg->m_prev->m_next = reg->m_next;
        } else {
            g_MessageRegistrationArray[entry->m_message] = reg->m_next;
        }
        if (reg->m_next != NULL) {
            reg->m_next->m_prev = reg->m_prev;
        }
    }
    reg->m_next = g_MessageRegistrationFreeListHead;
    g_MessageRegistrationFreeListHead = reg;
}

int BSProcessMessages(int max) {
    int pending = g_MessageQueueTail - g_MessageQueueHead;
    if (pending < 0) {
        pending += BSMSG_QUEUE_SIZE;
    }
    int count = max < pending ? max : pending;

    for (int i = 0; i < count; i++) {
        BSEventCurrentlyProcessingMessages(true);
        BSQueuedMessage_struct* q = &g_MessageQueue[g_MessageQueueHead];
        g_MessageQueueHead = NextQueueIndex(g_MessageQueueHead);

        // Skip messages to objects destroyed (and their slots reused) since.
        if (q->m_objectID == q->m_object->m_uniqueID) {
            // Find the thread's current handler for this message.
            BSCode_MessageHandlerEntry_struct* sent = q->m_entry;
            BSMessageListEntry_struct* e = q->m_thread->m_messages;
            while (e != NULL) {
                BSCode_MessageHandlerEntry_struct* h = e->m_registration->m_entry;
                if (h->m_flags & BSMSG_BARRIER) {
                    e = NULL;
                    break;
                }
                if (h->m_message == sent->m_message && h->m_filterArg == sent->m_filterArg &&
                    h->m_filter == sent->m_filter) {
                    break;
                }
                e = e->m_next;
            }

            u8 run = 0;
            if (e != NULL) {
                if (e->m_state == q->m_state) {
                    run = 1;
                } else {
                    run = (e->m_registration->m_entry->m_flags >> 1) & 1;
                }
            }
            if (run) {
                // Call the handler with (data, sender, target, trigger) pushed.
                BSMessageRegistration_struct* reg = e->m_registration;
                BSMachineThread_struct* t = reg->m_thread;
                t->m_sp = g_piStackTop + 2;
                t->m_frame = g_piStackTop + 2;
                t->m_sp[0] = (u32)PTR_TO_ADDR(q->m_data);
                t->m_sp[1] = (u32)PTR_TO_ADDR(q->m_sender);
                t->m_sp[2] = (u32)PTR_TO_ADDR(q->m_target);
                t->m_sp[3] = (u32)PTR_TO_ADDR(q->m_trigger);
                t->m_sp += 3;
                t->m_pc = BS_CODE_ADDRESS(t->m_code, (u32)reg->m_entry->m_address);
                BSExecuteThread(reg->m_object, reg->m_thread);
            }
        }
        BSEventCurrentlyProcessingMessages(false);
        BSEventProcessImmediateEvents();
    }
    return 0;
}

bool BSMessageIsGroupCategoryMember(TriggerObject_struct* a, TriggerObject_struct* b,
                                    unsigned int category) {
    if (a == NULL || b == NULL) {
        return false;
    }
    u32 cat = category >> 16;
    u32* ga = a->m_data->m_groups;
    if (ga == NULL) {
        return false;
    }
    u32* gb = b->m_data->m_groups;
    if (gb == NULL) {
        return false;
    }

    // Both lists are sorted: skip to the category, then merge.
    u32* pa = ga + 1;
    u32* pb = gb + 1;
    u32* endA = ga + ga[0];
    u32* endB = gb + gb[0];
    for (; pa <= endA; pa++) {
        if ((*pa >> 16) >= cat) {
            break;
        }
    }
    for (; pb <= endB; pb++) {
        if ((*pb >> 16) >= cat) {
            break;
        }
    }
    while (pa <= endA && pb <= endB) {
        if ((*pa >> 16) != cat || (*pb >> 16) != cat) {
            return false;
        }
        if (*pa < *pb) {
            pa++;
        } else if (*pa > *pb) {
            pb++;
        } else {
            return true;
        }
    }
    return false;
}
