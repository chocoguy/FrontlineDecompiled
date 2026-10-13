// Animation sequences: the interpreter for an animation's frame program.
#include "game/animseq.h"
#include "game/anim.h"
#include "game/animfile.h"
#include "game/animmot.h"

// The sequence whose user opcode is being handled.
AnimSeqAnimInfo_t* _AnimSeq_CurrentInfo;

static inline u16 FetchWord(AnimSeqAnimInfo_t* info) {
    return info->anim->program[info->pc++];
}

static inline int FetchParameter(AnimSeqAnimInfo_t* info) {
    u16 word = FetchWord(info);
    int value = word & 0xFFF;
    if (word & 0x800) {
        value |= 0xF000;
    }
    if ((word & ANIMSEQ_PARAM_LONG) == ANIMSEQ_PARAM_LONG) {
        value = (value << 16) + FetchWord(info);
    }
    return value;
}

// Runs the program up to the next frame index and returns it (0xFFFF at the
// end). `carry` is time already elapsed toward that frame; loops rewind
// position and drop it. With `skip`, user opcodes go to the skip callback.
static u16 _ParseForFrame(AnimSeqAnimInfo_t* info, float carry, u8 skip) {
    u16 frame = 0xFFFF;
    u8 done = 0;

    while (!done) {
        u16 op = FetchWord(info);
        if ((op & ANIMSEQ_OP_MASK) != ANIMSEQ_OP_MASK) {
            frame = op;
            done = 1;
            info->framesParsed++;
            continue;
        }

        if (op & ANIMSEQ_OP_USER) {
            u8 used;
            _AnimSeq_CurrentInfo = info;
            if (!skip) {
                used = _AnimDoUserOpcodeCallback(op);
            } else {
                used = _AnimDoUserOpcodeSkipCallback(op);
            }
            if (!used && (op & ANIMSEQ_PARAM_LONG) == ANIMSEQ_PARAM_LONG) {
                info->pc++;
            }
            continue;
        }

        switch (op) {
        case ANIMSEQ_OP_END:
        case ANIMSEQ_OP_STOP:
            if (info->frameCount == 0xFFFF) {
                info->frameCount = info->framesParsed;
            }
            done = 1;
            break;
        case ANIMSEQ_OP_LOOP_START:
            info->loopDepth++;
            info->loops[info->loopDepth - 1].pc = info->pc;
            info->loops[info->loopDepth - 1].count = 0;
            info->loops[info->loopDepth - 1].position = info->position + carry;
            break;
        case ANIMSEQ_OP_LOOP_END: {
            u16 count = FetchParameter(info);
            if (info->loops[info->loopDepth - 1].count == 0) {
                info->loops[info->loopDepth - 1].count = count;
            } else {
                info->loops[info->loopDepth - 1].count--;
            }
            if (info->loops[info->loopDepth - 1].count != 0) {
                carry = 0.0f;
                info->pc = info->loops[info->loopDepth - 1].pc;
                info->position = info->loops[info->loopDepth - 1].position;
            } else {
                info->loopDepth--;
            }
            break;
        }
        case ANIMSEQ_OP_DELAY:
            info->timeLeft = 0.0625f * FetchParameter(info) + info->timeLeft;
            break;
        case ANIMSEQ_OP_RESET_MOTION:
            info->motionFlags |= ANIMMOT_POS_RESET;
            info->motionFlags |= ANIMMOT_ROT_RESET;
            break;
        case ANIMSEQ_OP_CLEAR_MOTION:
            info->motionFlags |= ANIMMOT_POS_CLEAR;
            info->motionFlags |= ANIMMOT_ROT_CLEAR;
            break;
        default:
            if ((op & 0xE000) == 0xE000 && (op & ANIMSEQ_PARAM_LONG) == ANIMSEQ_PARAM_LONG) {
                info->pc++;
            }
            break;
        }
    }

    info->position += carry;
    return frame;
}

// Resets `info` to the start of `anim`.
static inline void _AnimSeqReset(AnimSeqAnimInfo_t* info, AnimSeqFrameList_t* anim) {
    info->anim = anim;
    info->rate = anim->frameTime;
    info->rate *= 0.0625f;
    if (info->rate <= 0.0f) {
        info->rate = 1.0f;
    }
    info->frameCount = 0xFFFF;
    info->framesParsed = 0;
    info->position = 0.0f;
    info->timeLeft = info->rate;
    info->pc = 0;
    info->loopDepth = 0;
    info->motionFlags = 0;
}

// Runs `anim` to its end (or to opcode `stop`) without loops or delays, to
// measure it.
static void _AnimSeqParseToEnd(AnimSeqAnimInfo_t* info, AnimSeqFrameList_t* anim, u16 stop) {
    u8 done = 0;

    _AnimSeqReset(info, anim);
    info->frame0 = _ParseForFrame(info, 0.0f, 1);
    info->frame1 = _ParseForFrame(info, 0.0f, 1);
    info->frameFrac = 0.0f;
    if (info->frame1 == 0xFFFF) {
        info->frame1 = info->frame0;
    }
    info->position += 2.0f * info->timeLeft;

    while (!done) {
        u16 op = FetchWord(info);
        if ((op & ANIMSEQ_OP_MASK) != ANIMSEQ_OP_MASK) {
            info->position += info->timeLeft;
            info->framesParsed++;
            continue;
        }
        if (op == stop) {
            done = 1;
        }
        if (op & ANIMSEQ_OP_USER) {
            _AnimSeq_CurrentInfo = info;
            _AnimDoUserOpcodeSkipCallback(op);
            continue;
        }
        switch (op) {
        case ANIMSEQ_OP_END:
            info->position -= info->timeLeft;
        case ANIMSEQ_OP_STOP:
            if (info->frameCount == 0xFFFF) {
                info->frameCount = info->framesParsed;
            }
            done = 1;
            break;
        case ANIMSEQ_OP_LOOP_START:
        case ANIMSEQ_OP_LOOP_END:
        case ANIMSEQ_OP_DELAY:
        case ANIMSEQ_OP_RESET_MOTION:
        case ANIMSEQ_OP_CLEAR_MOTION:
            break;
        default:
            if ((op & 0xE000) == 0xE000 && (op & ANIMSEQ_PARAM_LONG) == ANIMSEQ_PARAM_LONG) {
                info->pc++;
            }
            break;
        }
    }
}

bool AnimSeqStart(AnimSeqAnimInfo_t* info, AnimSeqFrameList_t* anim) {
    bool single = false;

    _AnimSeqReset(info, anim);
    info->frame0 = _ParseForFrame(info, 0.0f, 0);
    info->frame1 = _ParseForFrame(info, 0.0f, 0);
    info->frameFrac = 0.0f;
    if (info->frame1 == 0xFFFF) {
        single = true;
        info->frame1 = info->frame0;
    }
    return single;
}

bool AnimSeqGrow(AnimSeqAnimInfo_t* info, float ticks) {
    float carry = info->timeLeft;
    bool ended = false;

    info->timeLeft = carry - ticks;
    if (info->timeLeft < 0.0f) {
        while (info->timeLeft < 0.0f && !ended) {
            info->frame0 = info->frame1;
            info->frame1 = _ParseForFrame(info, carry, 0);
            if (info->frame1 == 0xFFFF) {
                ended = true;
                info->frame1 = info->frame0;
            }
            info->timeLeft = info->timeLeft + info->rate;
            carry = info->rate;
        }
        info->position = info->position + (info->rate - info->timeLeft);
    } else {
        info->position = info->position + ticks;
    }

    if (!ended) {
        info->frameFrac = (info->rate - info->timeLeft) / info->rate;
    } else {
        info->frameFrac = 1.0f;
    }
    return ended;
}

float AnimSeqGetLife(AnimSeqFrameList_t* anim) {
    AnimSeqAnimInfo_t info;
    _AnimSeqParseToEnd(&info, anim, 0xFFFF);
    return info.position;
}

u16 AnimSeqGetFrameCount(AnimSeqFrameList_t* anim) {
    AnimSeqAnimInfo_t info;
    _AnimSeqParseToEnd(&info, anim, 0xFFFF);
    return info.frameCount;
}

void AnimSeqSetRate(AnimSeqAnimInfo_t* info, float rate) {
    info->timeLeft = (info->timeLeft * rate) / info->rate;
    info->position = (info->position * rate) / info->rate;
    info->rate = rate;
}

void AnimSeqScaleDuration(AnimSeqAnimInfo_t* info, float scale) {
    if (info->frameCount == 0xFFFF) {
        AnimSeqAnimInfo_t tmp;
        _AnimSeqParseToEnd(&tmp, info->anim, 0xFFFF);
        info->frameCount = tmp.frameCount;
    }
    u16 count = info->frameCount;
    float rate = (0.0625f * ((float)count * (float)info->anim->frameTime) * scale) / (float)count;
    info->timeLeft = (info->timeLeft * rate) / info->rate;
    info->position = (info->position * rate) / info->rate;
    info->rate = rate;
}

s32 AnimSeqGetOpcodeParameter(void) {
    return FetchParameter(_AnimSeq_CurrentInfo);
}
