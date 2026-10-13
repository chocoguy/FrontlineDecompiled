#include "game/animwgt.h"

#include <string.h>

static void _AnimWgtConstantFunction(AnimWgtInfo_t* info) {}

static void _AnimWgtLinearFunction(AnimWgtInfo_t* info) {
    if (info->weight == info->target) {
        return;
    }
    info->weight = info->weight + (info->target - info->data.linear.start) /
                                      (float)info->data.linear.frames;
    if (info->data.linear.start < info->target) {
        if (info->weight > info->target) {
            info->weight = info->target;
        }
    } else {
        if (info->weight < info->target) {
            info->weight = info->target;
        }
    }
}

// Note: only zeroes the weight if the info pointer is (impossibly) at -8, as
// in the original; in effect it never changes the weight.
static void _AnimWgtWaitToStopFunction(AnimWgtInfo_t* info) {
    if ((void*)&info->type == NULL) {
        info->weight = 0.0f;
    }
}

static void _AnimWgtLinearInAndOutFunction(AnimWgtInfo_t* info) {
    AnimWgtLinearInAndOut_t* d = &info->data.inOut;
    bool step = false;
    switch (d->phase) {
    case 0:
        step = true;
        if (--d->counter >= 0) {
            break;
        }
        d->phase = 1;
        // fall through
    case 1:
        if (--d->holdFrames >= 0) {
            break;
        }
        d->phase = 2;
        d->counter = d->rampFrames;
        // fall through
    case 2:
        step = true;
        break;
    }
    if (step == true) {
        float from, to;
        if (d->phase != 2) {
            from = d->from;
            to = d->to;
        } else {
            from = d->to;
            to = d->from;
        }
        info->weight = info->weight + (to - from) / (float)d->rampFrames;
        if (from < to) {
            if (info->weight >= to) {
                info->weight = to;
            }
        } else {
            if (info->weight <= to) {
                info->weight = to;
            }
        }
    }
}

void (*_AnimWgt_pFunctions[4])(AnimWgtInfo_t*) = {
    _AnimWgtConstantFunction,
    _AnimWgtLinearFunction,
    _AnimWgtWaitToStopFunction,
    _AnimWgtLinearInAndOutFunction,
};
// Size of each type's data.
u8 _AnimWgt_TypeStructSize[4] = {0, sizeof(AnimWgtLinear_t), 4, sizeof(AnimWgtLinearInAndOut_t)};

extern "C" s32 AnimWgtInit(void) {
    return 0;
}

extern "C" s32 AnimWgtShutdown(void) {
    return 0;
}

extern "C" void AnimWgtSet(AnimWgtInfo_t* info, float weight, float target, s32 type, void* data) {
    if (weight != ANIMWGT_KEEP) {
        info->weight = weight;
    }
    if (target != ANIMWGT_KEEP) {
        info->target = target;
    }
    info->type = type;
    if (data != NULL && _AnimWgt_TypeStructSize[info->type] != 0) {
        memcpy(&info->data, data, _AnimWgt_TypeStructSize[info->type]);
    }
}

extern "C" void AnimWgtProcess(AnimWgtInfo_t* info, float frames) {
    for (u32 i = 0; (float)i < frames; i++) {
        _AnimWgt_pFunctions[info->type](info);
    }
}

extern "C" void AnimWgtSetLinear(AnimWgtInfo_t* info, float weight, float target, u32 frames) {
    AnimWgtLinear_t d;
    if (_AnimWgt_TypeStructSize[ANIMWGT_LINEAR] != 0) {
        memset(&d, 0, _AnimWgt_TypeStructSize[ANIMWGT_LINEAR]);
    }
    if (weight == ANIMWGT_KEEP) {
        weight = info->weight;
    }
    d.frames = frames;
    d.start = weight;
    AnimWgtSet(info, weight, target, ANIMWGT_LINEAR, &d);
}

extern "C" void AnimWgtSetLinearInAndOut(AnimWgtInfo_t* info, float from, float to, u8 rampFrames,
                                         u8 holdFrames) {
    AnimWgtLinearInAndOut_t d;
    if (_AnimWgt_TypeStructSize[ANIMWGT_LINEAR_IN_AND_OUT] != 0) {
        memset(&d, 0, _AnimWgt_TypeStructSize[ANIMWGT_LINEAR_IN_AND_OUT]);
    }
    if (from == ANIMWGT_KEEP) {
        from = info->weight;
    }
    d.from = from;
    d.rampFrames = rampFrames;
    d.counter = rampFrames;
    d.holdFrames = holdFrames;
    d.to = to;
    d.phase = 0;
    AnimWgtSet(info, from, to, ANIMWGT_LINEAR_IN_AND_OUT, &d);
}
