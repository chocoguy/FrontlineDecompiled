#include "game/animintf.h"

#include <stdio.h>
#include "game/ShellMenu.h"
#include "game/Soldier_object.h"
#include "game/anim.h"
#include "game/anim_callback.h"
#include "game/anim_obj.h"
#include "game/animated.h"
#include "game/animplayer.h"
#include "game/animseq.h"
#include "game/animst.h"
#include "game/animwgt.h"
#include "game/input.h"
#include "game/player.h"
#include "game/player_weapon_object.h"
#include "game/sound.h"

// User opcodes (0xC8xx: events and effects, 0xC9xx: animation control,
// 0xCAxx: enemy movement sounds). Opcodes 0xE000-0xFFFF are parameters.
#define OP_SCRIPT_EVENT_7E_PARAM 0xC803
#define OP_SCRIPT_EVENT_7D 0xC804
#define OP_SCRIPT_EVENT_7F 0xC805
#define OP_RELOAD_SOUND 0xC806
#define OP_SCRIPT_EVENT_81 0xC807
#define OP_SCRIPT_EVENT_E2 0xC808
#define OP_SCRIPT_EVENT_E3 0xC809
#define OP_SCRIPT_EVENT_A7 0xC80A
#define OP_RUMBLE 0xC80B
#define OP_SCRIPT_EVENT_B3 0xC80C
#define OP_HEAD_TRACK_ON 0xC80D
#define OP_HEAD_TRACK_OFF 0xC80E
#define OP_TRIGGERED_SOUND 0xC80F
#define OP_SCRIPT_EVENT_FC 0xC810
#define OP_SCRIPT_EVENT_FF 0xC811
#define OP_SET_CC0 0xC812
#define OP_FINISH_STATE 0xC900

static inline void ReportUnhandledOpcode(unsigned short opcode) {
    // The messages are formatted but never printed.
    char paramMsg[0x50];
    char opcodeMsg[0x50];
    if ((opcode & 0xE000) == 0xE000) {
        sprintf(paramMsg, "Opcode Parameter Not Processed: %x\n", opcode);
    } else {
        sprintf(opcodeMsg, "Invalid User Animation Opcode: %x\n", opcode);
    }
}

// Runs a user opcode for the CAnimated in `userData`. Opcodes for a state
// other than the current one are ignored (but count as handled).
int CAnimCallbacks::_AnimIntfOpcodeProcess(void* userData, unsigned short opcode,
                                           unsigned long state) {
    CAnimated* animated = (CAnimated*)userData;
    int handled = 0;
    u32 port = 0;
    CAnimObject* owner = animated->m_pOwner;
    CSoldierObject* soldier = owner->AsSoldierObject();
    CAnimatedPlayer* animPlayer = owner->AsAnimatedPlayerObject();
    CPlayerWeaponObject* weaponObj = owner->AsPlayerWeaponObject();

    if (state != AnimStGetCurrentState((u8*)animated + 0x890) && state != 0xFFFF) {
        return 1;
    }
    switch (opcode) {
    case OP_FINISH_STATE:
        animated->m_animFlags |= 0x10;
        animated->FinishAnimationState(state);
        handled = 1;
        break;
    case 0xCA00:
    case 0xCA0A:
    case 0xCA13:
    case 0xCA16:
    case 0xCA18:
    case 0xCA19:
    case 0xCA1E:
    case 0xCA22:
    case 0xCA24:
    case 0xCA29:
    case 0xCA2F:
    case 0xCA34:
    case 0xCA38:
    case 0xCA39:
    case 0xCA3B:
    case 0xCA3D:
    case 0xCA43:
    case 0xCA53:
    case 0xCA5A:
    case 0xCA5D:
        handled = 1;
        PlayEnemyMovement(opcode, AnimSeqGetOpcodeParameter(), owner);
        break;
    case OP_SCRIPT_EVENT_7D:
        owner->TriggerScriptEvent(0x7D, NULL, false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_7E_PARAM:
        owner->TriggerScriptEvent(0x7E, (void*)AnimSeqGetOpcodeParameter(), false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_7F:
        owner->TriggerScriptEvent(0x7F, NULL, false);
        handled = 1;
        break;
    case OP_RELOAD_SOUND:
        PlayReloadSound(AnimSeqGetOpcodeParameter(), owner);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_81:
        owner->TriggerScriptEvent(0x81, NULL, false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_E2:
        owner->TriggerScriptEvent(0xE2, NULL, false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_E3:
        owner->TriggerScriptEvent(0xE3, NULL, false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_A7:
        owner->TriggerScriptEvent(0xA7, NULL, false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_FC:
        owner->TriggerScriptEvent(0xFC, NULL, false);
        handled = 1;
        break;
    case OP_SCRIPT_EVENT_FF:
        owner->TriggerScriptEvent(0xFF, (void*)(AnimSeqGetOpcodeParameter() - 1), false);
        handled = 1;
        break;
    case OP_SET_CC0: {
        long value = AnimSeqGetOpcodeParameter() - 1;
        animated->field_0xCC1 = true;
        animated->field_0xCC0 = value != 0;
        break;
    }
    case OP_RUMBLE: {
        long pattern = AnimSeqGetOpcodeParameter();
        long strength = AnimSeqGetOpcodeParameter();
        if (weaponObj != NULL) {
            port = weaponObj->m_pPlayer->m_controllerPort;
        } else if (animPlayer != NULL) {
            port = animPlayer->m_pPlayer->m_controllerPort;
        }
        g_inputMgr.SetFeedback(port, pattern - 1, 0, 1.0f, (float)strength / 10.0f);
        handled = 1;
        break;
    }
    case OP_SCRIPT_EVENT_B3:
        if (!g_bInMultiplayerMode || weaponObj != NULL) {
            owner->TriggerScriptEvent(0xB3, NULL, false);
        }
        handled = 1;
        break;
    case OP_HEAD_TRACK_ON:
        handled = 1;
        soldier->SetHeadTrackPlayer(true, AnimSeqGetOpcodeParameter());
        break;
    case OP_HEAD_TRACK_OFF:
        handled = 1;
        soldier->SetHeadTrackPlayer(false, AnimSeqGetOpcodeParameter());
        break;
    case OP_TRIGGERED_SOUND:
        handled = 1;
        PlayAnimTriggeredSound(AnimSeqGetOpcodeParameter(), owner);
        break;
    default:
        ReportUnhandledOpcode(opcode);
        break;
    }
    return handled;
}

// Called for opcodes in skipped animation; consumes their parameters.
int CAnimCallbacks::_AnimIntfOpcodeSkip(void* userData, unsigned short opcode,
                                        unsigned long state) {
    int handled = 0;
    switch (opcode) {
    case OP_SCRIPT_EVENT_7E_PARAM:
    case OP_SCRIPT_EVENT_7D:
    case OP_SCRIPT_EVENT_7F:
    case OP_SCRIPT_EVENT_E2:
    case OP_SCRIPT_EVENT_E3:
    case OP_SCRIPT_EVENT_B3:
        handled = 1;
        break;
    case OP_RELOAD_SOUND:
        handled = 0;
        break;
    case OP_RUMBLE:
        AnimSeqGetOpcodeParameter();
        AnimSeqGetOpcodeParameter();
        handled = 1;
        break;
    case OP_HEAD_TRACK_ON:
    case OP_HEAD_TRACK_OFF:
    case OP_TRIGGERED_SOUND:
    case OP_SCRIPT_EVENT_FF:
    case OP_SET_CC0:
    case 0xCA00:
    case 0xCA0A:
    case 0xCA13:
        AnimSeqGetOpcodeParameter();
        handled = 1;
        break;
    case OP_FINISH_STATE:
        handled = 1;
        break;
    default:
        ReportUnhandledOpcode(opcode);
        break;
    }
    return handled;
}

extern "C" void AnimSystem_Init(void) {
    AnimInit();
    AnimSetUserOpcodeCallbacks(CAnimCallbacks::_AnimIntfOpcodeProcess,
                               CAnimCallbacks::_AnimIntfOpcodeSkip);
    AnimWgtInit();
    AnimStInit();
    AnimCallback_Init();
}

extern "C" void AnimSystem_Shutdown(void) {
    AnimStShutdown();
    AnimWgtShutdown();
    AnimShutdown();
}
