// Animation states: up to four blended state slots per animated object. The
// current state fades in while older ones fade out; per-type callbacks start,
// update and stop each state's animations.
#include "game/animst.h"
#include "game/anim.h"
#include "game/animchan.h"
#include "game/animdb.h"
#include "game/animtran.h"
#include "game/animwgt.h"
#include "string.h"

static AnimFileStateAnimList_t _AnimSt_pStatAnimList;
static AnimStFinishCallback _AnimSt_FinishCb;
static AnimStCallbackTable_t* _AnimSt_CallbackTable;

static inline void CallStateCallback(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state,
                                     void* userData, int event, int arg, float frames) {
    u16 type = hdr->objectType;
    if (!(state & 0x8000)) {
        g_AnimDB.GetStateAnimList(type, state, &_AnimSt_pStatAnimList);
        _AnimSt_CallbackTable->entries[g_AnimDB.GetStateCallbackID(state, type)].callback(
            &_AnimSt_pStatAnimList, state, hdr, chans, userData, event, arg, frames);
    }
}

// Removes free slots between used ones.
static inline void CompactSlots(AnimStInfo_t* info) {
    int last = 3;
    if (info->slots[3].status == ANIMST_FREE) {
        last = 2;
        if (info->slots[2].status == ANIMST_FREE) {
            last = 1;
            if (info->slots[1].status == ANIMST_FREE) {
                last = 0;
                if (info->slots[0].status == ANIMST_FREE) {
                    last = -1;
                }
            }
        }
    }
    if (last <= 0) {
        return;
    }
    int i = 0;
    while (last > i) {
        if (info->slots[i].status == ANIMST_FREE) {
            if (i != last) {
                for (int j = i; j < last; j++) {
                    memcpy(&info->slots[j], &info->slots[j + 1], sizeof(AnimStSlot_t));
                    info->slots[j + 1].status = ANIMST_FREE;
                    info->slots[j + 1].delay = 0.0f;
                }
            }
            last--;
        } else {
            i++;
        }
    }
}

static inline void StopSlot(AnimStInfo_t* info, int i, AnimObjHdr_t* hdr, AnimChannel_t* chans,
                            void* userData, float frames) {
    CallStateCallback(hdr, chans, info->slots[i].state, userData, ANIMST_EVENT_STOP, 0, frames);
    AnimChanStopByStateIndex(hdr, chans, info->slots[i].state);
    info->slots[i].status = ANIMST_FREE;
}

// A state ID (counting down from 0xFFFE) no slot uses, for retiring a slot's
// state without confusing it with a new start of the same state.
static inline u16 UnusedStateID(AnimStInfo_t* info) {
    u16 id = 0xFFFE;
    u8 unique;
    do {
        unique = 1;
        if (info->slots[0].state == id) {
            unique = 0;
            id--;
        } else if (info->slots[1].state == id) {
            unique = 0;
            id--;
        } else if (info->slots[2].state == id) {
            unique = 0;
            id--;
        } else if (info->slots[3].state == id) {
            unique = 0;
            id--;
        }
    } while (!unique);
    return id;
}

// A free slot, stopping a delayed state, or the oldest one, if all are used.
static u16 _AnimStGetFreeSlotIndex(AnimStInfo_t* info, AnimObjHdr_t* hdr, AnimChannel_t* chans,
                                   void* userData, unsigned short) {
    int found = -1;
    int stopped = 0;

    CompactSlots(info);
    bool full = false;
    if (info->slots[0].status == ANIMST_FREE) {
        found = 0;
    } else if (info->slots[1].status == ANIMST_FREE) {
        found = 1;
    } else if (info->slots[2].status == ANIMST_FREE) {
        found = 2;
    } else if (info->slots[3].status == ANIMST_FREE) {
        found = 3;
    } else {
        full = true;
    }

    if (full) {
        for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
            if (info->slots[i].delay != 0.0f) {
                StopSlot(info, i, hdr, chans, userData, 1.0f);
                CompactSlots(info);
                stopped = 1;
                break;
            }
        }
        if (stopped == 1) {
            if (info->slots[0].status == ANIMST_FREE) {
                found = 0;
            } else if (info->slots[1].status == ANIMST_FREE) {
                found = 1;
            } else if (info->slots[2].status == ANIMST_FREE) {
                found = 2;
            } else if (info->slots[3].status == ANIMST_FREE) {
                found = 3;
            }
        }
    }

    if (found == -1) {
        StopSlot(info, 0, hdr, chans, userData, 1.0f);
        CompactSlots(info);
        found = 3;
    }
    return found;
}

s32 AnimStInit(void) {
    return 0;
}

s32 AnimStShutdown(void) {
    if (_AnimSt_CallbackTable != NULL) {
        _AnimSt_CallbackTable = NULL;
    }
    return 0;
}

void AnimStSetCallbackTable(AnimStCallbackTable_t* table) {
    _AnimSt_CallbackTable = table;
}

void AnimStRegisterFinishCallback(AnimStFinishCallback callback) {
    _AnimSt_FinishCb = callback;
}

void AnimStInitStructure(AnimStInfo_t* info) {
    info->slots[0].state = 0;
    info->slots[0].status = ANIMST_FREE;
    info->slots[1].state = 0;
    info->slots[1].status = ANIMST_FREE;
    info->slots[2].state = 0;
    info->slots[2].status = ANIMST_FREE;
    info->slots[3].state = 0;
    info->slots[3].status = ANIMST_FREE;
}

static inline u16 CurrentState(AnimStInfo_t* info) {
    u16 state = 0xFFFF;
    if (info->slots[3].status == ANIMST_CURRENT) {
        state = info->slots[3].state;
    }
    if (info->slots[2].status == ANIMST_CURRENT) {
        state = info->slots[2].state;
    }
    if (info->slots[1].status == ANIMST_CURRENT) {
        state = info->slots[1].state;
    }
    if (info->slots[0].status == ANIMST_CURRENT) {
        state = info->slots[0].state;
    }
    return state;
}

int AnimStTransPreCheck(AnimStInfo_t* info, AnimObjHdr_t* hdr, u16 state) {
    u16 current = CurrentState(info);
    if (current == 0xFFFF || current == state) {
        return -1;
    }
    g_AnimDB.GetStateAnimList(hdr->objectType, current, &_AnimSt_pStatAnimList);
    u16 currentAnim = _AnimSt_pStatAnimList.anims[0].animIndex;
    g_AnimDB.GetStateAnimList(hdr->objectType, state, &_AnimSt_pStatAnimList);
    return _AnimTranQuickScan(hdr, state, _AnimSt_pStatAnimList.anims[0].animIndex, current,
                              currentAnim);
}

int AnimStStartState(AnimStInfo_t* info, AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state,
                     void* userData, int arg, float weight) {
    u16 previous = CurrentState(info);

    // The current state becomes pending. If it was delayed, the slot before it
    // (a placeholder) takes over its unassigned channels and fades out.
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        if (info->slots[i].status != ANIMST_CURRENT) {
            continue;
        }
        info->slots[i].status = ANIMST_PENDING;
        if (info->slots[i].delay != 0.0f && i > 0) {
            AnimStSlot_t* prev = &info->slots[i - 1];
            if (prev->state == 0xFFFF) {
                prev->state = UnusedStateID(info);
                AnimWgtSetLinear(&prev->weight, -1.0f, 0.0f, 10);
                while (AnimChanGetNumActiveByState(hdr, chans, 0xFFFF) != 0) {
                    chans[AnimChanGetStateChannelByIndex(hdr, chans, 0xFFFF, 0)].state = prev->state;
                }
            }
        }
        break;
    }

    if (info->slots[0].state == 0xFFFF) {
        info->slots[0].delay = 1.0f;
    }
    if (info->slots[1].state == 0xFFFF) {
        info->slots[1].delay = 1.0f;
    }
    if (info->slots[2].state == 0xFFFF) {
        info->slots[2].delay = 1.0f;
    }
    if (info->slots[3].state == 0xFFFF) {
        info->slots[3].delay = 1.0f;
    }

    // A placeholder slot for channels started before the state is assigned.
    u16 placeholder = _AnimStGetFreeSlotIndex(info, hdr, chans, userData, 0xFFFF);

    // Retire older slots running the same state.
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        AnimStSlot_t* slot = &info->slots[i];
        if (slot->status != ANIMST_FREE && slot->state == state) {
            slot->state = UnusedStateID(info);
            slot->flags &= ~ANIMST_UPDATE_CALLBACK;
            while (AnimChanGetNumActiveByState(hdr, chans, state) != 0) {
                chans[AnimChanGetStateChannelByIndex(hdr, chans, state, 0)].state = slot->state;
            }
        }
    }

    AnimStSlot_t* slot = &info->slots[placeholder];
    memset(slot, 0, sizeof(AnimStSlot_t));
    slot->state = 0xFFFF;
    slot->status = ANIMST_RUNNING;
    slot->delay = 0.0f;
    AnimWgtSet(&slot->weight, 0.0f, 0.0f, 0, NULL);

    slot = &info->slots[_AnimStGetFreeSlotIndex(info, hdr, chans, userData, state)];
    memset(slot, 0, sizeof(AnimStSlot_t));
    slot->state = state;
    slot->status = ANIMST_CURRENT;
    slot->delay = 0.0f;
    AnimWgtSetLinear(&slot->weight, 0.0f, weight, 10);

    CallStateCallback(hdr, chans, state, userData, ANIMST_EVENT_START, arg, 1.0f);
    _AnimTranDoTransition(info, hdr, chans, state, previous, ANIMST_PENDING, userData);
    return 0;
}

void AnimStProcess(AnimStInfo_t* info, AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                   float frames) {
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        AnimStSlot_t* slot = &info->slots[i];
        if (slot->status == ANIMST_FREE) {
            break;
        }
        float delay = slot->delay - frames;
        slot->delay = delay;
        if (delay >= 0.0f) {
            continue;
        }
        slot->delay = 0.0f;
        AnimWgtProcess(&slot->weight, frames);
        if (slot->weight.weight > 1e-7f) {
            if (slot->flags & ANIMST_UPDATE_CALLBACK) {
                CallStateCallback(hdr, chans, slot->state, userData, ANIMST_EVENT_UPDATE, 0,
                                  frames);
            }
            if (AnimChanGetNumActiveByState(hdr, chans, slot->state) == 0) {
                if (slot->status == ANIMST_CURRENT) {
                    u16 state = slot->state;
                    u16 type = hdr->objectType;
                    if (!(state & 0x8000)) {
                        g_AnimDB.GetStateAnimList(type, state, &_AnimSt_pStatAnimList);
                        _AnimSt_CallbackTable->entries[g_AnimDB.GetStateCallbackID(state, type)]
                            .callback(&_AnimSt_pStatAnimList, state, hdr, chans, userData,
                                      ANIMST_EVENT_FINISHED, 0, frames);
                        _AnimSt_FinishCb(state, userData);
                    }
                } else {
                    StopSlot(info, i, hdr, chans, userData, frames);
                }
            }
        } else {
            StopSlot(info, i, hdr, chans, userData, frames);
        }
    }
    CompactSlots(info);
}

int AnimStGetCallbackIndexFromID(u32 id) {
    for (u32 i = 0; i < _AnimSt_CallbackTable->count; i++) {
        if (id == _AnimSt_CallbackTable->entries[i].id) {
            return i;
        }
    }
    return -1;
}

float AnimStGetStateWeight(AnimStInfo_t* info, u16 state) {
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        if (info->slots[i].status != ANIMST_FREE && info->slots[i].state == state) {
            return info->slots[i].weight.weight;
        }
    }
    return 0.0f;
}

void* AnimStGetStateUserMemory(AnimStInfo_t* info, u16 state) {
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        if (info->slots[i].status != ANIMST_FREE && info->slots[i].state == state) {
            return info->slots[i].userMemory;
        }
    }
    return NULL;
}

u16 AnimStGetCurrentState(void* info) {
    return CurrentState((AnimStInfo_t*)info);
}

void AnimStSetStateFlags(AnimStInfo_t* info, u16 state, int flags) {
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        if (info->slots[i].status != ANIMST_FREE && info->slots[i].state == state) {
            info->slots[i].flags |= flags;
        }
    }
}

void AnimStSetStateDelay(AnimStInfo_t* info, u16 state, float delay) {
    for (int i = 0; i < ANIMST_NUM_SLOTS; i++) {
        if (info->slots[i].status != ANIMST_FREE && info->slots[i].state == state) {
            info->slots[i].delay = delay;
        }
    }
}
