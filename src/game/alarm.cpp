// Alarm script built-ins: alarm objects register themselves, soldiers look
// for the closest free alarm within 50 units and occupy it while raising it.
#include "game/alarm.h"
#include "game/ISceneNode.h"
#include "game/bsbif.h"
#include "game/bsobject.h"
#include "game/vector.h"
#include "math.h"

#define ALARM_MAX_DISTANCE 50.0f

dwi::list<AlarmInfo> g_alarms;
bool g_alarm_activated;

static inline float NodeDistance(ISceneNode* a, ISceneNode* b) {
    CVector3 pa;
    CVector3 pb;
    a->GetPosition(pa);
    b->GetPosition(pb);
    float dx = pa.x - pb.x;
    float dy = pa.y - pb.y;
    float dz = pa.z - pb.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// The closest unoccupied alarm within 50 units of `self`, or g_alarms.end().
static inline dwi::list<AlarmInfo>::node* FindClosestFreeAlarm(ISceneNode* self) {
    float best = ALARM_MAX_DISTANCE;
    dwi::list<AlarmInfo>::node* closest = g_alarms.end();
    for (dwi::list<AlarmInfo>::node* n = g_alarms.begin(); n != g_alarms.end(); n = n->next) {
        float dist = NodeDistance(self, n->value.m_object);
        if (dist < best && n->value.m_occupant == NULL) {
            closest = n;
            best = dist;
        }
    }
    return closest;
}

// RegisterAlarm(): adds the calling object.
void BIFunc_RegisterAlarm(int** sp, void* self) {
    if (g_alarms.capacity() == 0) {
        g_alarms.reserve(15, "Alarm Pool");
    }
    AlarmInfo info;
    info.m_object = (ISceneNode*)self;
    info.m_occupant = NULL;
    g_alarms.push_back(info);
    BIFPop(sp);
}

// UnregisterAlarm(): removes the calling object, telling its occupant.
void BIFunc_UnregisterAlarm(int** sp, void* self) {
    BIFPop(sp);
    for (dwi::list<AlarmInfo>::node* n = g_alarms.begin(); n != g_alarms.end(); n = n->next) {
        if (n->value.m_object == self) {
            BSObject* occupant = n->value.m_occupant;
            if (occupant != NULL) {
                BSObjectTriggerEvent(occupant, 0xF7, NULL, occupant, false);
            }
            g_alarms.erase(n);
            return;
        }
    }
}

// SetAlarmOccupy(alarm, occupy): marks the alarm (by its script) as used by
// the calling soldier, or frees it.
void BIFunc_SetAlarmOccupy(int** sp, void* self) {
    BSObject* alarm = (BSObject*)BIFArg(sp, 0);
    bool occupy = BIFArg(sp, 1) != 0;
    ISceneNode* soldier = (ISceneNode*)((ISceneNode*)self)->AsSoldierObject();
    BSObject* script = (BSObject*)soldier->GetScriptObject();
    for (dwi::list<AlarmInfo>::node* n = g_alarms.begin(); n != g_alarms.end(); n = n->next) {
        if ((BSObject*)n->value.m_object->GetScriptObject() == alarm) {
            n->value.m_occupant = occupy ? script : NULL;
            break;
        }
    }
    BIFPop(sp);
}

// GetClosestAlarm(): script of the closest free alarm, or 0.
void BIFunc_GetClosestAlarm(int** sp, void* self) {
    dwi::list<AlarmInfo>::node* closest = FindClosestFreeAlarm((ISceneNode*)self);
    BIFPop(sp);
    if (closest != g_alarms.end()) {
        BIFReturn(sp, (int)closest->value.m_object->GetScriptObject());
    } else {
        BIFReturn(sp, 0);
    }
}

// AlarmIsAvailable(): whether a free alarm is close enough.
void BIFunc_AlarmIsAvailable(int** sp, void* self) {
    bool available = g_alarms.size() > 0;
    if (available) {
        if (FindClosestFreeAlarm((ISceneNode*)self) == g_alarms.end()) {
            available = false;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, available);
}

void BIFunc_IsAlarmActivated(int** sp, void*) {
    BIFPop(sp);
    BIFReturn(sp, g_alarm_activated);
}

void BIFunc_SetAlarmActivated(int** sp, void*) {
    g_alarm_activated = BIFArg(sp, 0) != 0;
    BIFPop(sp);
}
