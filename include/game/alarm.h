#ifndef _GAME_ALARM_H_
#define _GAME_ALARM_H_

#include "types.h"
#include "game_types.h"
#include "dwi/list.h"

class ISceneNode;
struct BSObject;

// An alarm object registered by its script.
struct AlarmInfo {
    /* 0x0 */ ISceneNode* m_object;
    /* 0x4 */ BSObject* m_occupant;  // script of the soldier using it, if any
};

extern dwi::list<AlarmInfo> g_alarms;
extern bool g_alarm_activated;

void BIFunc_SetAlarmActivated(int**, void*);  // 0x80059974
void BIFunc_IsAlarmActivated(int**, void*);  // 0x800599C4
void BIFunc_AlarmIsAvailable(int**, void*);  // 0x800599F8
void BIFunc_GetClosestAlarm(int**, void*);  // 0x80059BA0
void BIFunc_SetAlarmOccupy(int**, void*);  // 0x80059D5C
void BIFunc_UnregisterAlarm(int**, void*);  // 0x80059E6C
void BIFunc_RegisterAlarm(int**, void*);  // 0x80059F3C

#endif
