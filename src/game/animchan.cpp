#include "game/animchan.h"

#include <math.h>
#include "game/anim.h"
#include "game/animemot.h"
#include "game/animseq.h"

#define ANIMCHAN_STEALABLE 0x8000

extern "C" void AnimChanInitChannels(AnimObjHdr_t* hdr, AnimChannel_t* chans) {
    u8 type = 4;
    if (hdr->field_0x0 == 0) {
        type = 1;
    }
    for (int i = 0; i < hdr->numChannels; i++, chans++) {
        chans->type = type;
        chans->status = ANIMCHAN_FREE;
        chans->seq.anim = NULL;
        chans->speed = 1.0f;
    }
}

extern "C" void AnimChanProcess(AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                                float dt) {
    for (int i = 0; i < hdr->numChannels; i++, chans++) {
        float speed = chans->speed;
        if (chans->status == ANIMCHAN_DONE) {
            chans->status = ANIMCHAN_FREE;
        }
        if (chans->status == ANIMCHAN_FREE) {
            continue;
        }
        float frames = dt * speed;
        chans->delay = chans->delay - frames;
        if (chans->delay >= 0.0f) {
            continue;
        }
        chans->delay = 0.0f;
        s32 weightType = chans->weight.type;
        AnimWgtProcess(&chans->weight, frames);
        // A channel whose weight faded out is finished.
        if (chans->weight.weight < 0.0000001f && weightType != ANIMWGT_CONSTANT) {
            chans->status = ANIMCHAN_DONE;
        }
        if (chans->status != ANIMCHAN_FREE) {
            _AnimUserCallbackPushInfo(userData);
            _AnimUserCallbackPushState(chans->state);
            bool done = AnimSeqGrow(&chans->seq, frames);
            _AnimUserCallbackPopState();
            _AnimUserCallbackPopInfo();
            if (done == true) {
                chans->status = ANIMCHAN_DONE;
            }
        }
    }
}

// Takes a free channel, else a finished one, else the stealable channel with
// the lowest weight.
extern "C" int AnimChanStartAnim(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimFileFormat_t* file,
                                 u16 animIndex, u16 state, void* userData, float weight) {
    int index;
    for (u8 status = ANIMCHAN_FREE; status < 2; status++) {
        AnimChannel_t* c = chans;
        for (index = 0; index < hdr->numChannels; index++, c++) {
            if (c->status == status) {
                goto found;
            }
        }
    }
    {
        float lowest = 1.0f;
        index = -1;
        AnimChannel_t* c = chans;
        for (int i = 0; i < hdr->numChannels; i++, c++) {
            if ((c->state & ANIMCHAN_STEALABLE) && c->weight.weight <= lowest) {
                lowest = c->weight.weight;
                index = i;
            }
        }
        if (index != -1) {
            chans[index].status = ANIMCHAN_FREE;
        } else {
            index = -1;
        }
    }
found:
    if (index != -1) {
        AnimChannel_t* c = &chans[index];
        AnimWgtSet(&c->weight, weight, weight, ANIMWGT_CONSTANT, NULL);
        c->fileNum = AnimFileGetFileNum(file);
        c->file = file;
        c->animIndex = animIndex;
        c->state = state;
        c->motionHeading = hdr->field_0x8;
        c->motionRot = 0;
        c->motionPos[0] = c->motionPos[1] = c->motionPos[2] = 0.0f;
        c->motionPosScale = 1.0f;
        c->motionRotScale = 0x01000000;
        c->delay = 0.0f;
        c->speed = 1.0f;
        AnimSeqFrameList_t* anim = AnimFileGetAnimation(file, animIndex);
        _AnimUserCallbackPushState(c->state);
        _AnimUserCallbackPushInfo(userData);
        bool done = AnimSeqStart(&c->seq, anim);
        _AnimUserCallbackPopState();
        _AnimUserCallbackPopInfo();
        c->animFlags = anim->flags;
        if (!done) {
            c->status = ANIMCHAN_RUNNING;
        }
    }
    return index;
}

extern "C" void AnimChanStopByStateIndex(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state) {
    for (int i = 0; i < hdr->numChannels; i++) {
        if (chans[i].status != ANIMCHAN_FREE && state == chans[i].state) {
            chans[i].status = ANIMCHAN_FREE;
        }
    }
}

extern "C" void AnimChanSwitchAnimByDuration(AnimObjHdr_t* hdr, AnimChannel_t* chans, int index,
                                             AnimFileFormat_t* file, u16 animIndex, u16 state,
                                             void* userData) {
    AnimChannel_t* c = &chans[index];
    if (c->status != ANIMCHAN_RUNNING) {
        return;
    }
    c->fileNum = AnimFileGetFileNum(file);
    c->file = file;
    c->state = state;
    if (c->seq.frameCount == 0xFFFF) {
        c->seq.frameCount = AnimSeqGetFrameCount(c->seq.anim);
    }
    float duration = chans[index].seq.rate * (float)c->seq.frameCount;
    float position = fmod(chans[index].seq.position, duration);
    AnimSeqFrameList_t* anim = AnimFileGetAnimation(file, animIndex);
    _AnimUserCallbackPushState(c->state);
    _AnimUserCallbackPushInfo(userData);
    AnimSeqAnimInfo_t* seq = &chans[index].seq;
    bool done = AnimSeqStart(seq, anim);
    _AnimUserCallbackPopState();
    _AnimUserCallbackPopInfo();
    chans[index].animFlags = anim->flags;
    if (done == true) {
        c->status = ANIMCHAN_DONE;
        return;
    }
    // Same duration as before, starting at the same point in it.
    c->seq.frameCount = AnimSeqGetFrameCount(chans[index].seq.anim);
    AnimSeqSetRate(seq, duration / (float)c->seq.frameCount);
    _AnimUserCallbackPushInfo(userData);
    _AnimUserCallbackPushState(c->state);
    done = AnimSeqGrow(seq, position);
    _AnimUserCallbackPopState();
    _AnimUserCallbackPopInfo();
    if (done == true) {
        c->status = ANIMCHAN_DONE;
    }
}

extern "C" int AnimChanGetNumActiveByState(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state) {
    int count = 0;
    for (int i = 0; i < hdr->numChannels; i++, chans++) {
        if (chans->status == ANIMCHAN_RUNNING && state == chans->state) {
            count++;
        }
    }
    return count;
}

extern "C" int AnimChanGetNumRunningOrDoneByState(AnimObjHdr_t* hdr, AnimChannel_t* chans,
                                                  u16 state) {
    int count = 0;
    for (int i = 0; i < hdr->numChannels; i++, chans++) {
        if ((chans->status == ANIMCHAN_RUNNING || chans->status == ANIMCHAN_DONE) &&
            state == chans->state) {
            count++;
        }
    }
    return count;
}

extern "C" u16 AnimChanGetStateChannelByIndex(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state,
                                              u16 index) {
    int count = 0;
    for (int i = 0; i < hdr->numChannels; i++, chans++) {
        if ((chans->status == ANIMCHAN_RUNNING || chans->status == ANIMCHAN_DONE) &&
            state == chans->state) {
            if ((u16)count == index) {
                return i;
            }
            count++;
        }
    }
    return 0xFFFF;
}

extern "C" void AnimChanSetDelayByState(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state,
                                        float delay) {
    for (int i = 0; i < hdr->numChannels; i++, chans++) {
        if (chans->status == ANIMCHAN_RUNNING && state == chans->state) {
            chans->delay = delay;
        }
    }
}

extern "C" void AnimChanSetSpeed(AnimChannel_t* chan, float speed) {
    chan->speed = speed;
}
