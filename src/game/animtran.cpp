// Animation state transitions.
#include "game/animtran.h"
#include "game/anim.h"
#include "game/animchan.h"
#include "game/animdb.h"
#include "game/animfile.h"
#include "game/animseq.h"
#include "game/animst.h"
#include "game/animwgt.h"

AnimTranTransListArray_t AnimTran_TransListArray;

// The transition into the listed state from animation (file, anim), or from
// `state` when anim is 0xFFFF, that applies while animation `cond` is at
// program word `pc`. The last match wins.
static inline AnimTransition_t* _AnimTranFind(AnimTranTransListArray_t* lists, u16 file,
                                              u16 anim, u16 state, u16 cond, u16 pc) {
    AnimTransition_t* found = NULL;
    u16 sub = anim;
    u16 key;
    if (anim != 0xFFFF) {
        key = file | 0x8000;
    } else {
        sub = state;
        key = 0xFFFF;
    }
    for (int i = 0; i < lists->count; i++) {
        AnimTranList_t* list = lists->lists[i].list;
        AnimTranEntry_t* e = list->entries;
        for (int j = 0; j < list->count; j++, e++) {
            if ((e->key == key || (key == 0xFFFF && !(e->key & 0x8000))) && e->sub == sub &&
                e->cond == cond) {
                if (cond != 0xFFFF) {
                    if (pc >= e->lo && pc <= e->hi) {
                        found = e->transition;
                    }
                } else {
                    found = e->transition;
                }
            }
        }
    }
    return found;
}

// For each slot waiting on `status`: find the strongest channel of the slot's
// state (A) and of the new state (B), look up the transition (most specific
// first) and start it. Without one the slot fades out over 10 frames.
void _AnimTranDoTransition(AnimStInfo_t* stateInfo, AnimObjHdr_t* hdr, AnimChannel_t* chans,
                           u16 state, u16 listState, unsigned long status, void* userData) {
    u8 fade = 1;
    AnimTranTransListArray_t* lists = &AnimTran_TransListArray;
    AnimStSlot_t* slot = stateInfo->slots;

    for (int i = 0; i < ANIMST_NUM_SLOTS; i++, slot++) {
        if (slot->status != status || slot->state == 0xFFFF) {
            continue;
        }
        slot->status = 1;
        g_AnimDB.GetTransList(hdr->objectType, listState, lists);
        if (lists->count != 0) {
            u16 oldState = slot->state;
            float best = 0.0f;
            u16 a = 0xFFFF;
            u16 n = AnimChanGetNumRunningOrDoneByState(hdr, chans, oldState);
            for (u16 k = 0; k < n; k++) {
                u16 ch = AnimChanGetStateChannelByIndex(hdr, chans, oldState, k);
                if (ch != 0xFFFF && best < chans[ch].weight.weight) {
                    best = chans[ch].weight.weight;
                    a = ch;
                }
            }
            best = 0.0f;
            u16 b = 0xFFFF;
            n = AnimChanGetNumRunningOrDoneByState(hdr, chans, state);
            for (u16 k = 0; k < n; k++) {
                u16 ch = AnimChanGetStateChannelByIndex(hdr, chans, state, k);
                if (ch != 0xFFFF && best < chans[ch].weight.weight) {
                    best = chans[ch].weight.weight;
                    b = ch;
                }
            }

            if (a != 0xFFFF && b != 0xFFFF) {
                AnimChannel_t* chanA = &chans[a];
                AnimChannel_t* chanB = &chans[b];
                u16 bAnim = chanB->animIndex;
                u16 bFile = chanB->fileNum;
                u16 aAnim = chanA->animIndex;
                u16 aPc = chanA->seq.pc;

                AnimTransition_t* t = _AnimTranFind(lists, bFile, bAnim, state, aAnim, aPc);
                if (t == NULL) {
                    t = _AnimTranFind(lists, 0, 0xFFFF, state, aAnim, aPc);
                }
                if (t == NULL) {
                    t = _AnimTranFind(lists, bFile, bAnim, state, 0xFFFF, 0);
                }
                if (t == NULL) {
                    t = _AnimTranFind(lists, 0, 0xFFFF, state, 0xFFFF, 0);
                }

                if (t != NULL) {
                    switch (t->type) {
                    case ANIMTRAN_CUT:
                        AnimWgtSet(&slot->weight, 0.0f, 0.0f, 0, NULL);
                        fade = 0;
                        break;
                    case ANIMTRAN_BLEND: {
                        u32 frames = t->duration * chanA->seq.rate;
                        AnimWgtSetLinear(&slot->weight, -1.0f, 0.0f, frames);
                        AnimWgtSetLinear(&stateInfo->slots[i + 2].weight, 0.0f,
                                         slot[2].weight.target, frames);
                        fade = 0;
                        break;
                    }
                    case ANIMTRAN_CUT2:
                        AnimWgtSet(&slot->weight, 0.0f, 0.0f, 0, NULL);
                        fade = 0;
                        break;
                    case ANIMTRAN_PLAY: {
                        u8 failed;
                        AnimFileFormat_t* file = g_AnimDB.GetFileFromFileNum(hdr->objectType,
                                                                             t->fileNum);
                        if (file != NULL) {
                            int ch = AnimChanStartAnim(hdr, chans, file, t->animIndex, 0xFFFF,
                                                       userData, 1.0f);
                            if (ch != -1) {
                                // Cross-fade in and out over the clip's first and
                                // last `ramp` frames.
                                float life = AnimSeqGetLife(chans[ch].seq.anim);
                                u32 ramp = life * (1.0f - t->overlap) * 0.5f;
                                u32 hold = (u32)life - ramp * 2;
                                AnimWgtSetLinear(&slot->weight, -1.0f, 0.0f, ramp);
                                AnimWgtSetLinearInAndOut(&stateInfo->slots[i + 1].weight, -1.0f,
                                                         1.0f, ramp, hold);
                                AnimWgtSetLinear(&stateInfo->slots[i + 2].weight, 0.0f,
                                                 slot[2].weight.target, ramp);
                                float delay = (float)hold + (float)ramp;
                                AnimStSetStateDelay(stateInfo, state, delay);
                                AnimChanSetDelayByState(hdr, chans, state, delay);
                                failed = 0;
                            } else {
                                failed = 1;
                            }
                        } else {
                            failed = 1;
                        }
                        fade = failed;
                        break;
                    }
                    }
                }
            }
        }
        if (fade == 1) {
            AnimWgtSetLinear(&slot->weight, -1.0f, 0.0f, 10);
        }
    }
}

#define ANIMTRAN_VIA_FIRST 0x37AA
#define ANIMTRAN_VIA_LAST 0x37DB

int _AnimTranQuickScan(AnimObjHdr_t* hdr, u16 from, u16 anim, u16 state, u16 cond) {
    AnimTranTransListArray_t* lists = &AnimTran_TransListArray;
    u16 via[64];

    g_AnimDB.GetTransList(hdr->objectType, state, lists);
    if (lists->count == 0) {
        return -1;
    }
    if (_AnimTranFind(lists, 0, anim, from, cond, 0) != NULL) {
        return -1;
    }
    if (_AnimTranFind(lists, 0, 0xFFFF, from, cond, 0) != NULL) {
        return -1;
    }
    if (_AnimTranFind(lists, 0, anim, from, 0xFFFF, 0) != NULL) {
        return -1;
    }
    if (_AnimTranFind(lists, 0, 0xFFFF, from, 0xFFFF, 0) != NULL) {
        return -1;
    }

    // No direct way in: try the intermediate states that lead to `state`.
    u16 numVia = 0;
    for (int i = 0; i < lists->count; i++) {
        AnimTranList_t* list = lists->lists[i].list;
        AnimTranEntry_t* e = list->entries;
        for (int j = 0; j < list->count; j++, e++) {
            if (e->key == 0xFFFF || !(e->key & 0x8000)) {
                u16 s = e->sub;
                if (s >= ANIMTRAN_VIA_FIRST && s <= ANIMTRAN_VIA_LAST) {
                    via[numVia++] = s;
                }
            }
        }
    }
    for (int i = 0; i < numVia; i++) {
        g_AnimDB.GetTransList(hdr->objectType, via[i], lists);
        if (lists->count != 0 && _AnimTranFind(lists, 0, 0xFFFF, from, 0xFFFF, 0) != NULL) {
            return via[i];
        }
    }
    return -1;
}
