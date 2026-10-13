// The sound schedule: AEMS events that track a scene node or a position.
// Every frame their azimuth (relative to the nearest player), volume (from
// distance) and, for moving objects, pitch (from speed) parameters are
// rewritten in the event data; finished events are removed and can send a
// script event.
#include "game/soundschedule.h"
#include "ea/snd/sserver.h"
#include "game/ISceneNode.h"
#include "game/Moh2.h"
#include "game/ShellMenu.h"
#include "game/Soldier_object.h"
#include "game/bsobject.h"
#include "game/player.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/static_obj.h"
#include "game/vector.h"
#include "math.h"
#include "string.h"

#define SOUND_SCHEDULE_SIZE 100

// One scheduled sound (size 0x90).
struct SoundScheduleRegistrationRecord {
    SoundScheduleRegistrationRecord() {}

    /* 0x00 */ CVector3 m_position;  // SOUND_AT_POSITION
    /* 0x10 */ SoundSchedule::ESoundScheduleObjectType m_type;
    /* 0x14 */ ISceneNode* m_node;  // SOUND_ON_NODE
    /* 0x18 */ BSObject* m_object;  // gets m_event when the sound ends
    /* 0x1C */ int m_event;  // -1: none
    /* 0x20 */ int m_data[0x14];  // the AEMS event
    /* 0x70 */ int m_handle;
    // Words of m_data that get the azimuth, volume and pitch (-1: no pitch).
    /* 0x74 */ s16 m_azimuthIndex;
    /* 0x76 */ s16 m_volumeIndex;
    /* 0x78 */ s16 m_pitchIndex;
    /* 0x7A */ s16 m_size;
    /* 0x7C */ s16 m_done;
    /* 0x7E */ s16 m_delay;  // frames before the sound is updated
    /* 0x80 */ f32 m_volumeScale;
    /* 0x84 */ CSoldierObject* m_soldier;
    /* 0x88 */ bool m_tracked;  // the soldier's m_soundHandle refers to it
    /* 0x8C */ SoundScheduleRegistrationRecord* m_next;
};

static SoundScheduleRegistrationRecord g_AllRecords[SOUND_SCHEDULE_SIZE];
static SoundScheduleRegistrationRecord* g_FreeRecordList;

static inline float Length(float x, float y, float z) {
    float len = z * z + (x * x + y * y);
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    return len;
}

// The player who hears a sound at `pos`.
static inline CPlayerObject* Listener(CVector3 pos) {
    if (g_bInMultiplayerMode) {
        return g_scene.FindClosestPlayer(pos);
    }
    return g_scene.GetPlayer(0);
}

static inline void FreeRecord(SoundSchedule* schedule, SoundScheduleRegistrationRecord* rec,
                              SoundScheduleRegistrationRecord* prev, SoundScheduleRegistrationRecord* next) {
    rec->m_next = g_FreeRecordList;
    g_FreeRecordList = rec;
    schedule->m_count--;
    if (prev != NULL) {
        prev->m_next = next;
    } else {
        schedule->m_head = next;
    }
}

static inline void SendEndEvent(SoundScheduleRegistrationRecord* rec) {
    if (rec->m_object != NULL && rec->m_event != -1) {
        BSObjectTriggerEvent(rec->m_object, rec->m_event, NULL, NULL, false);
    }
}

// Pitch parameter from the speed of a moving object.
static inline u32 SpeedPitch(CStaticObject* obj, float offset) {
    int motion = obj->m_pathState;
    bool animated = false;
    if (motion != 0 && motion != 1) {
        animated = true;
    }
    if (animated) {
        return (u32)((offset + obj->m_speed) * (220.0f / (offset + obj->m_maxSpeed)));
    }
    f32* v = &obj->m_velocity.x;
    return (u32)(Length(v[0], v[1], v[2]) * (220.0f * obj->m_soundPitchScale));
}

void SoundSchedule::Update(int frames) {
    SoundScheduleRegistrationRecord* rec;
    for (rec = m_head; rec != NULL; rec = rec->m_next) {
        if (rec->m_delay != 0) {
            continue;
        }
        if (rec->m_type == SOUND_ON_NODE) {
            CVector3 pos;
            rec->m_node->GetPosition(pos);
            int azimuth;
            if (g_bInMultiplayerMode) {
                CVector3 p;
                rec->m_node->GetPosition(p);
                azimuth = GetAzimuth(p, g_scene.FindClosestPlayer(p));
            } else {
                azimuth = GetAzimuth(pos, g_scene.GetPlayer(0));
            }
            rec->m_data[rec->m_azimuthIndex] = azimuth;

            CVector3 d;
            rec->m_node->GetPosition(d);
            CVector3 listener;
            if (g_bInMultiplayerMode) {
                CVector3 p;
                rec->m_node->GetPosition(p);
                g_scene.FindClosestPlayer(p)->GetPosition(listener);
            } else {
                g_scene.GetPlayer(0)->GetPosition(listener);
            }
            d.x -= listener.x;
            d.y -= listener.y;
            d.z -= listener.z;
            rec->m_data[rec->m_volumeIndex] = (int)(256.0f * (rec->m_volumeScale * Length(d.x, d.y, d.z)));

            CStaticObject* obj = rec->m_node->AsStaticObject();
            if (obj != NULL && rec->m_pitchIndex != -1) {
                u32 pitch;
                // (One level gets an offset in the pitch formula.)
                if (g_Shell.Get_currentMission() == 4 && g_Shell.Get_currentStage() == 2 && !g_bInMultiplayerMode) {
                    pitch = SpeedPitch(obj, 0.16f);
                } else {
                    pitch = SpeedPitch(obj, 0.0f);
                }
                rec->m_data[rec->m_pitchIndex] = pitch;
            }
        } else {
            rec->m_data[rec->m_azimuthIndex] = GetAzimuth(rec->m_position, Listener(rec->m_position));
            CVector3 listener;
            Listener(rec->m_position)->GetPosition(listener);
            float dx = listener.x - rec->m_position.x;
            float dy = listener.y - rec->m_position.y;
            float dz = listener.z - rec->m_position.z;
            rec->m_data[rec->m_volumeIndex] = (int)(256.0f * (rec->m_volumeScale * Length(dx, dy, dz)));
        }
    }

    SNDSYS_entercritical();
    for (rec = m_head; rec != NULL; rec = rec->m_next) {
        if (rec->m_delay != 0) {
            rec->m_delay -= frames;
            if (rec->m_delay < 0) {
                rec->m_delay = 0;
            }
        } else if (AEMS_UpdateEvent(rec->m_data, rec->m_handle) < 0) {
            rec->m_done = 1;
        }
    }
    SNDSYS_leavecritical();

    SoundScheduleRegistrationRecord* prev = NULL;
    rec = m_head;
    while (rec != NULL) {
        SoundScheduleRegistrationRecord* next = rec->m_next;
        if (rec->m_done != 0) {
            SendEndEvent(rec);
            if (rec->m_soldier != NULL) {
                rec->m_soldier->m_soundHandle = -1;
            }
            FreeRecord(this, rec, prev, next);
        } else {
            prev = rec;
        }
        rec = next;
    }
}

// Replaces the event data of sound `handle`.
void SoundSchedule::UpdateSound(int handle, void* data, int size) {
    SoundScheduleRegistrationRecord* rec;
    for (rec = m_head; rec != NULL; rec = rec->m_next) {
        if (rec->m_handle == handle) {
            break;
        }
    }
    if (rec != NULL) {
        memcpy(rec->m_data, data, size);
    }
}

// Removes sound `handle` (and with `end`, stops it).
void SoundSchedule::RemoveSound(int handle, bool end) {
    SoundScheduleRegistrationRecord* prev = NULL;
    SoundScheduleRegistrationRecord* rec = m_head;
    while (rec != NULL) {
        SoundScheduleRegistrationRecord* next = rec->m_next;
        if (rec->m_handle == handle) {
            SendEndEvent(rec);
            if (end) {
                AEMS_EndEvent(rec->m_data, handle);
            }
            if (rec->m_soldier != NULL && rec->m_tracked) {
                rec->m_soldier->m_soundHandle = -1;
            }
            FreeRecord(this, rec, prev, next);
            return;
        }
        prev = rec;
        rec = next;
    }
}

// Schedules a playing AEMS event. A tracked sound replaces the soldier's
// previous tracked sound (a soldier says one thing at a time).
void SoundSchedule::AddSound(ESoundScheduleObjectType type, ISceneNode* node, CVector3* pos, void* data, int size,
                             int handle, int azimuthIndex, int volumeIndex, int pitchIndex, BSObject* obj,
                             EBSEventEnum event, float volumeScale, bool tracked, int delay) {
    SoundScheduleRegistrationRecord* rec = NULL;
    if (g_FreeRecordList != NULL) {
        rec = g_FreeRecordList;
        g_FreeRecordList = g_FreeRecordList->m_next;
    }
    if (rec == NULL) {
        DebugMsg("*************************************************************\n");
        DebugMsg("The Sound Schedule queue is full!\n");
        DebugMsg("the sound you tried to play is being dropped\n");
        DebugMsg("some investigation should be done as to why the queue is full\n");
        DebugMsg("the schedular size is currently set to %d", SOUND_SCHEDULE_SIZE);
        DebugMsg("*************************************************************\n");
        if (obj != NULL && event != -1) {
            BSObjectTriggerEvent(obj, event, NULL, NULL, false);
        }
        return;
    }
    rec->m_type = type;
    rec->m_node = node;
    if (pos != NULL) {
        rec->m_position = *pos;
    } else {
        rec->m_position.x = 0.0f;
        rec->m_position.y = 0.0f;
        rec->m_position.z = 0.0f;
    }
    memcpy(rec->m_data, data, size);
    rec->m_size = size;
    CSoldierObject* soldier = NULL;
    rec->m_handle = handle;
    rec->m_azimuthIndex = azimuthIndex;
    rec->m_volumeIndex = volumeIndex;
    rec->m_pitchIndex = pitchIndex;
    rec->m_object = obj;
    rec->m_event = event;
    rec->m_done = 0;
    rec->m_volumeScale = volumeScale;
    rec->m_delay = delay;
    if (node != NULL) {
        soldier = node->AsSoldierObject();
    }
    rec->m_soldier = soldier;
    rec->m_tracked = tracked;
    rec->m_next = m_head;
    m_head = rec;
    m_count++;
    if (tracked && rec->m_soldier != NULL) {
        int previous = rec->m_soldier->m_soundHandle;
        if (previous != -1) {
            SoundScheduleRegistrationRecord* p = NULL;
            for (SoundScheduleRegistrationRecord* r = m_head; r != NULL;) {
                SoundScheduleRegistrationRecord* next = r->m_next;
                if (r->m_handle == previous) {
                    SendEndEvent(r);
                    AEMS_EndEvent(r->m_data, previous);
                    if (r->m_soldier != NULL && r->m_tracked) {
                        r->m_soldier->m_soundHandle = -1;
                    }
                    FreeRecord(this, r, p, next);
                    break;
                }
                p = r;
                r = next;
            }
        }
        rec->m_soldier->m_soundHandle = handle;
    }
}

// Ends every scheduled sound.
void SoundSchedule::Shutdown() {
    SoundScheduleRegistrationRecord* rec = m_head;
    while (rec != NULL) {
        SoundScheduleRegistrationRecord* next = rec->m_next;
        SendEndEvent(rec);
        AEMS_EndEvent(rec->m_data, rec->m_handle);
        rec->m_next = g_FreeRecordList;
        g_FreeRecordList = rec;
        rec = next;
        m_count--;
    }
    m_head = NULL;
    m_count = 0;
    g_FreeRecordList = NULL;
}

void SoundSchedule::Init() {
    m_head = NULL;
    m_count = 0;
    for (int i = 0; i < SOUND_SCHEDULE_SIZE - 1; i++) {
        g_AllRecords[i].m_next = &g_AllRecords[i + 1];
    }
    g_AllRecords[SOUND_SCHEDULE_SIZE - 1].m_next = NULL;
    g_FreeRecordList = g_AllRecords;
}

SoundSchedule::~SoundSchedule() {}

SoundSchedule::SoundSchedule() {
    m_head = NULL;
    m_count = 0;
}
