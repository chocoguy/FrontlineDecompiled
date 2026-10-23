// sound: the game's layer over the EA sound library: AEMS events (sound
// effects positioned around the listener, the ambient track, one-shot
// banks), the PATH music track, sound streams, volumes, and the subtitles
// and face animations carried in sound user data.
#include "game/sound.h"
#include "game/path.h"
#include "game/soundtable.h"
#include "game/soundschedule.h"
#include "game/scene.h"
#include "game/player.h"
#include "game/anim_obj.h"
#include "game/Soldier_object.h"
#include "game/bullet.h"
#include "game/thrownbullet.h"
#include "game/ShellMenu.h"
#include "game/Screen.h"
#include "game/pumhandler.h"
#include "game/toilet.h"
#include "game/memory.h"
#include "game/matrix.h"
#include "game/vector.h"
#include "game/color.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/real_bridge.h"
#include "ea/real.h"
#include "ea/snd/sserver.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

extern u8 g_pGenericSoundMemory[0x50];  // bsbifunc.cpp

enum {
    AEMS_MAXBANKS = 8,
    AEMS_EVENT_AMBIENT = 0xC,
    AEMS_EVENT_PAUSE = 0xD,
};

// Sound library memory.
u8 SoundBankHeapStart[0x32000] __attribute__((aligned(32)));
int SoundBankHeapSize = 0x32000;
bool SoundHasBeenInitialized;

static float g_sfxVolume = 1.0f;
static float g_musicVolume = 1.0f;

// AEMS: the event system, its module banks and the stream buffer.
static void* gAEMSsystem;
static bool gAEMSStreamUsed;
static int gAEMSbHandle[AEMS_MAXBANKS];
static void* gAEMSBank[AEMS_MAXBANKS];
static int gAEMSBankCount;
static u8 gAEMSStreamBuf[0x32000] __attribute__((aligned(32)));

// The soldier whose face the sound user data animates.
static CSoldierObject* g_objSound;

// The ambient track: event 0xC (azimuth, track, volume, distance).
static int gAEMSAmbientTrackEvent[5] = {AEMS_EVENT_AMBIENT, 0, 0, 0, 0};
static int g_hAEMSAmbientTrack = -1;
static bool g_ambientSourceOnPlayer = true;
static CVector3 g_ambientSourcePos(0.0f, 0.0f, 0.0f);

// The one-shot bank, loaded asynchronously into its own memory.
static int g_AEMSOneShotBankID = -1;
static int g_AEMSOneShotBankHandle = -1;
int g_AEMSOneShotMemOffset;
int g_AEMSOneShotLoadState;
static u8 g_AEMSOneShotStreamBuf[0xC800] __attribute__((aligned(32)));
static u8 g_AEMSOneShotMem[0x70800] __attribute__((aligned(32)));

// Music (a PATH track).
SNDPLAYOPTS gMusicOpts;
int gMusicHandle = -1;
int gMusicProjectID = -1;
void* gMusicBuf;
int gMusicBufSize;
void* gMusicMap;

// Subtitle colors: white, yellow, green.
static CColor _subtitle_rgb[3] = {
    CColor(0xFF, 0xFF, 0xFF, 0x80),
    CColor(0xFF, 0xFF, 0x00, 0x80),
    CColor(0x00, 0xFF, 0x00, 0x80),
};

// Debug output (compiled out).
static void msg(char*, ...) {}
static void err(char*, ...) {}
static void fail(char*, ...) {}

// Sound user data is little-endian.
static inline u32 Swap32(u32 v) {
#ifdef FRONTLINE_NATIVE
    return v;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

static inline u16 Swap16(u16 v) {
#ifdef FRONTLINE_NATIVE
    return v;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    return (u16)((v >> 8) | (v << 8));
}

// Starts an AEMS event (the event number is its first word). Returns its
// handle, or -1 on failure.
static inline int SendEvent(int* event) {
    // Category 3 sounds of event 7 follow the effects volume.
    if (event[0] == 7 && event[1] == 3 && event[2] < 3) {
        event[9] = (int)((float)event[9] * g_sfxVolume);
    }
    SNDSYS_entercritical();
    int handle = SNDAEMS_beginevent(event);
    SNDSYS_leavecritical();
    if (handle < 0 && handle != -8 && handle != -9) {
        err("SNDAEMS_beginevent number %d failed %d", event[0], handle);
        handle = -1;
    } else {
        msg("SNDAEMS_beginevent number %d handle=%d\n", event[0], handle);
    }
    return handle;
}

static inline void SetEffectsVolume() {
    SNDSYS_entercritical();
    int result = SNDAEMS_setglobal(0, (int)(32767.0f * g_sfxVolume));
    SNDSYS_leavecritical();
    if (result < 0) {
        err("SNDAEMS_setglobal failed %d", result);
    }
}

// Distance in AEMS units (1/256).
static inline float Distance(const CVector3& a, const CVector3& b) {
    CVector3 d;
    d.Sub(a, b);
    return d.Length();
}

// Azimuth of `d` (world space, relative to the player) around the player:
// 0x10000 is a full turn.
static inline u16 RelativeAzimuth(CVector3 d, const CPlayerObject* player) {
    s16 azimuth = 0;
    CMatrix tm;
    player->GetTMLocalToWorld(tm);
    CMatrix inv;
    inv.FastInverse(tm);
    inv.TransformVector(d, d);
    d.z = 0.0f;
    if (d.x * d.x + d.y * d.y + d.z * d.z > 0.0001f) {
        azimuth = (s16)(65536.0f * (0.15915494f * (float)atan2(d.x, d.y)));
    } else {
        azimuth = 0;
    }
    return azimuth;
}

int PlayScriptedSound(ISceneNode* node, CVector3 pos, const CSoundTable::SSoundParms& parms, int scriptEvent,
                      BSObject* script) {
    int distance = 0;
    u16 azimuth = 0;
    float rangeScale = (float)parms.m_rangeScale * 0.00390625f;
    if (parms.m_positional) {
        CPlayerObject* player;
        if (g_bInMultiplayerMode) {
            player = g_scene.FindClosestPlayer(pos);
        } else {
            player = g_scene.GetPlayer(0);
        }
        CVector3 playerPos;
        if (player != NULL) {
            player->GetPosition(playerPos);
        } else {
            playerPos.x = playerPos.y = playerPos.z = 0.0f;
        }
        distance = (int)(rangeScale * (256.0f * Distance(pos, playerPos)));
        if (distance > 0x7FFF) {
            distance = 0x7FFF;
        }
        azimuth = GetAzimuth(pos, player);
    }

    CSoldierObject* soldier = node != NULL ? node->AsSoldierObject() : NULL;
    if (soldier != NULL) {
        g_objSound = soldier;
    }

    int* event = (int*)g_pGenericSoundMemory;
    int number = 7;
    if (parms.m_altEvent) {
        number = 0xE;
    }
    int random = (int)MathFunRandomI64(parms.m_randomMin, parms.m_randomMax);
    event[0] = number;
    event[1] = parms.m_category;
    event[2] = (int)MathFunRandomI64(parms.m_variantMin, parms.m_variantMax);
    event[3] = parms.m_looped == 0 ? random : 0;
    event[4] = azimuth;
    event[5] = distance;
    event[6] = 0;
    event[7] = 0x61A7;
    event[8] = 0;
    event[9] = parms.m_volume;
    int handle = SendEvent(event);

    if (parms.m_scheduled) {
        event[3] = random;
        SoundSchedule::ESoundScheduleObjectType type;
        BSObject* owner;
        if (node != NULL) {
            owner = node->GetScriptObject();
            type = SoundSchedule::SOUND_ON_NODE;
        } else {
            owner = script;
            type = SoundSchedule::SOUND_AT_POSITION;
        }
        g_SoundSchedule.AddSound(type, node, &pos, event, 0x28, handle, 4, 5, -1, owner, (EBSEventEnum)scriptEvent,
                                 rangeScale, event[1] == 1, parms.m_looped);
    }
    return handle;
}

// Sound user data: face animations and subtitles, in chunks.
static void _userdata_client(SNDUSERDATACBINFO* info) {
    u8* data = (u8*)info->m_data;
    if (data == NULL || info->m_type != 1) {
        return;
    }
    u32 id = Swap32(*(u32*)data);
    if (id == 'SPIL') {
        // A single lip-sync frame.
        s16 face = (s16)Swap16(*(u16*)(data + 4));
        g_objSound->SetFaceAnimationState((u16)face, info->m_param);
        return;
    }
    u8* chunk = data;
    while ((uptr)((chunk - info->m_size)) < (uptr)(data)) {
        id = Swap32(((u32*)chunk)[0]);
        u32 size = Swap32(((u32*)chunk)[1]);
        switch (id) {
        case 'NYSL':
            g_objSound->SetFaceAnimationState((u16)Swap32(((u32*)chunk)[2]), info->m_param);
            break;
        case 'TBUS': {
            // A subtitle.
            u32 optional = Swap32(((u32*)chunk)[2]);
            u32 color = Swap32(((u32*)chunk)[3]);
            const char* text = (const char*)(chunk + 0x10);
            if (strlen(text) != 0 && (optional == 0 || (g_Shell.field_0x218 & SHELL_SUBTITLES))) {
                DisplaySubtitle(text, 5.0f, _subtitle_rgb[color]);
            }
            break;
        }
        case 'LUMS': {
            // A subtitle per language; ends the data.
            SOUND_MULTISUBTITLEDATA multi = *(SOUND_MULTISUBTITLEDATA*)chunk;
            EndianSwap(multi);
            const char* strings = (const char*)(chunk + sizeof(SOUND_MULTISUBTITLEDATA));
            u32 language = g_Shell.Get_LanguageSubtitleIndex();
            if (language < multi.m_count) {
                const char* text = strings + multi.m_offsets[language];
                if (strlen(text) != 0 && (multi.m_optional == 0 || (g_Shell.field_0x218 & SHELL_SUBTITLES))) {
                    DisplaySubtitle(text, 5.0f, _subtitle_rgb[multi.m_color]);
                }
            }
            return;
        }
        default:
            return;
        }
        chunk += size;
    }
}

void MUSIC_Fade(float volume, int ms) {
    PATH_fade(gMusicHandle, (int)(100.0f * (volume * g_musicVolume)), ms);
}

int MUSIC_SendEvent(int event) {
    PATH_event(gMusicHandle, (short)((gMusicProjectID << 8) | event), 1);
    return 1;
}

static inline int ClampLatency(int ms) {
    if (ms < 50) {
        return 50;
    }
    if (ms > 5000) {
        return 5000;
    }
    return ms;
}

static inline int ClampLevel(int level) {
    if (level < 0) {
        return 0;
    }
    if (level > 127) {
        return 127;
    }
    return level;
}

int MUSIC_SetLatency(int ms) {
    PATH_latency(gMusicHandle, ClampLatency(ms));
    return 1;
}

int MUSIC_SetLevel(int level) {
    PATH_control(gMusicHandle, ClampLevel(level));
    return 1;
}

bool MUSIC_Pause(bool pause) {
    if (gMusicHandle < 0) {
        return false;
    }
    PATH_pause(-1, pause != 0);
    return pause;
}

void MUSIC_Stop() {
    if (gMusicHandle >= 0) {
        PATH_stop(gMusicHandle);
    }
}

void MUSIC_Start(int level, int latency, int node) {
    if (gMusicHandle < 0) {
        return;
    }
    PATH_control(gMusicHandle, ClampLevel(level));
    PATH_latency(gMusicHandle, ClampLatency(latency));
    PATH_volume(gMusicHandle, (char)(100.0f * (g_musicVolume * g_musicVolume)));
    int result = PATH_jump(-1, node, 1);
    if (result < 0) {
        err("PATH_jump Failed (%d)", result);
    }
}

// A bullet passing the player: event 5 (azimuth where it was fired from, azimuth
// of its closest point to the player, distance).
int PlayBulletWhiz(CVector3 from, CVector3 dir, CPlayerObject* player) {
    u16 fromAzimuth = GetAzimuth(from, player);
    CVector3 playerPos;
    player->GetPosition(playerPos);
    CVector3 toPlayer;
    toPlayer.Sub(playerPos, from);
    float distance = toPlayer.Length();
    toPlayer.Normalize();
    dir.Normalize();
    float along = fabs(distance * (toPlayer.x * dir.x + toPlayer.y * dir.y + toPlayer.z * dir.z)) * 2.0f;
    CVector3 passing;
    passing.x = from.x + along * dir.x;
    passing.y = from.y + along * dir.y;
    passing.z = from.z + along * dir.z;
    int event[10];
    event[0] = 5;
    event[1] = fromAzimuth;
    event[2] = GetAzimuth(passing, player);
    event[3] = (int)distance;
    return SendEvent(event);
}

static inline CPlayerObject* ListenerFor(const CVector3& pos) {
    if (g_bInMultiplayerMode) {
        return g_scene.FindClosestPlayer(pos);
    }
    return g_scene.GetPlayer(0);
}

// A bullet hitting the scenery: event 3.
void PlayBackgroundImpactSound(CBullet* bullet, int sound) {
    CVector3 velocity;
    bullet->GetVelocity(velocity);
    if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z < 0.01f) {
        return;
    }
    CProjectileBullet* projectile = bullet->AsProjectile();
    CThrownBullet* thrown = bullet->AsThrown();
    // Thrown bullets sound for their first three bounces only.
    if (thrown != NULL && ++thrown->field_0x1FC > 3) {
        return;
    }
    CVector3 pos;
    bullet->GetPosition(pos);
    CPlayerObject* player = ListenerFor(pos);
    CVector3 playerPos;
    player->GetPosition(playerPos);
    u16 azimuth = GetAzimuth(pos, player);
    float distance = Distance(pos, playerPos);
    int event[10];
    event[0] = 3;
    event[4] = sound;
    if (projectile != NULL) {
        event[1] = 1;
    } else {
        event[1] = 2;
        if (thrown != NULL) {
            event[1] = thrown->m_type == 5 ? 5 : 2;
        }
    }
    event[3] = azimuth;
    event[2] = (int)(256.0f * distance);
    SendEvent(event);
}

// Event 3; `local` sounds play at the listener.
void PlayImpactSound(unsigned short sound, unsigned short category, CVector3 pos, bool local) {
    int event[10];
    event[0] = 3;
    event[1] = category;
    event[4] = sound;
    if (local) {
        event[3] = 0;
        event[2] = 0;
    } else {
        CPlayerObject* player = g_scene.GetPlayer(0);
        CVector3 playerPos;
        player->GetPosition(playerPos);
        u16 azimuth = GetAzimuth(pos, player);
        float distance = Distance(pos, playerPos);
        event[3] = azimuth;
        event[2] = (int)(0.5f + 256.0f * distance);
    }
    SendEvent(event);
}

// Event 6: a weapon reload sound (silent position for the player's own).
void PlayReloadSound(long sound, CAnimObject* obj) {
    u16 azimuth;
    u16 distance;
    // Objects without a script (the player's own weapons) play unplaced.
    if (obj->GetScriptObject() == NULL) {
        azimuth = 0;
        distance = 0;
    } else {
        CPlayerObject* player = g_scene.GetPlayer(0);
        CVector3 playerPos;
        player->GetPosition(playerPos);
        CVector3 pos;
        obj->GetPosition(pos);
        azimuth = GetAzimuth(pos, player);
        distance = (u16)(int)(3.0f * (256.0f * Distance(pos, playerPos)));
    }
    int event[10];
    event[0] = 6;
    event[1] = sound + 0x65;
    event[2] = azimuth;
    event[3] = distance;
    SendEvent(event);
}

void PlayAnimTriggeredSound(long sound, CAnimObject* obj) {
    CVector3 pos;
    obj->GetPosition(pos);
    CSoundTable::SSoundParms parms;
    if (g_soundMapSingleton.GetSoundEvent(sound, &parms)) {
        PlayScriptedSound(obj, pos, parms, -1, NULL);
    }
}

// Event 4: an enemy's footsteps and gear (single player only).
void PlayEnemyMovement(unsigned short surface, long move, CAnimObject* obj) {
    if (g_bInMultiplayerMode) {
        return;
    }
    CVector3 pos;
    obj->GetPosition(pos);
    CPlayerObject* player = ListenerFor(pos);
    CVector3 playerPos;
    player->GetPosition(playerPos);
    u16 azimuth = GetAzimuth(pos, player);
    float distance = Distance(pos, playerPos);
    int sound = move + surface - 0xCA01;
    if (sound >= 100) {
        switch (sound) {
        case 100:
            sound = 0x17;
            break;
        case 101:
            sound = 0x1B;
            break;
        default:
            return;
        }
    }
    int event[10];
    event[0] = 4;
    event[1] = sound;
    event[2] = obj->m_footstepSound;
    event[3] = 2;
    event[4] = azimuth;
    event[5] = (int)(3.0f * (256.0f * distance));
    event[6] = 1;
    SendEvent(event);
}

u16 GetAzimuth(CVector3 pos, const CPlayerObject* player) {
    CVector3 playerPos;
    player->GetPosition(playerPos);
    CVector3 d;
    d.Sub(pos, playerPos);
    return RelativeAzimuth(d, player);
}

void AmbientTrack_SetLocation(const CVector3* pos) {
    if (g_hAEMSAmbientTrack == -1) {
        return;
    }
    // NULL: the ambience is centered on the player.
    g_ambientSourceOnPlayer = pos == NULL;
    if (!g_ambientSourceOnPlayer) {
        g_ambientSourcePos.Copy(*pos);
    }
}

void AmbientTrack_Volume(float volume) {
    if (g_hAEMSAmbientTrack == -1) {
        return;
    }
    gAEMSAmbientTrackEvent[0] = AEMS_EVENT_AMBIENT;
    gAEMSAmbientTrackEvent[3] = (u16)(int)(32767.0f * volume);
    SNDSYS_entercritical();
    int result = SNDAEMS_updateevent(g_hAEMSAmbientTrack, gAEMSAmbientTrackEvent);
    SNDSYS_leavecritical();
    if (result < 0) {
        err("AmbientTrack_Volume(%d):  SNDAEMS_updateevent() FAILED (error == %d)\n", result);
    }
}

void AmbientTrack_Select(int track) {
    if (g_hAEMSAmbientTrack == -1) {
        return;
    }
    gAEMSAmbientTrackEvent[0] = AEMS_EVENT_AMBIENT;
    gAEMSAmbientTrackEvent[2] = track;
    SNDSYS_entercritical();
    int result = SNDAEMS_updateevent(g_hAEMSAmbientTrack, gAEMSAmbientTrackEvent);
    SNDSYS_leavecritical();
    if (result < 0) {
        err("AmbientTrack_Select(%d):  SNDAEMS_updateevent() FAILED (error == %d)\n", track, result);
    }
}

void AmbientTrack_Start() {
    gAEMSAmbientTrackEvent[0] = AEMS_EVENT_AMBIENT;
    gAEMSAmbientTrackEvent[1] = 0;
    gAEMSAmbientTrackEvent[2] = 0;
    gAEMSAmbientTrackEvent[3] = 0x7FFF;
    gAEMSAmbientTrackEvent[4] = 0;
    SNDSYS_entercritical();
    g_hAEMSAmbientTrack = SNDAEMS_beginevent(gAEMSAmbientTrackEvent);
    SNDSYS_leavecritical();
    if (g_hAEMSAmbientTrack < 0) {
        err("AmbientTrack_Start():  SNDAEMS_beginevent() FAILED!!! (error == %d)\n", g_hAEMSAmbientTrack);
        g_hAEMSAmbientTrack = -1;
    }
}

void AEMS_SetObject(CSoldierObject* obj) {
    if (obj != NULL) {
        g_objSound = obj;
    }
}

int AEMS_EndEvent(void* event, int handle) {
    if (handle == -1) {
        return -1;
    }
    SNDSYS_entercritical();
    int result = SNDAEMS_endevent(handle, event);
    SNDSYS_leavecritical();
    if (result < 0) {
        err("SNDAEMS_endeevent number %d failed %d", *(int*)event, handle);
        return -1;
    }
    msg("SNDAEMS_endeevent number %d handle=%d\n", *(int*)event, handle);
    return 0;
}

int AEMS_UpdateEvent(void* event, int handle) {
    if (handle == -1) {
        return -1;
    }
    SNDSYS_entercritical();
    int result = SNDAEMS_updateevent(handle, event);
    SNDSYS_leavecritical();
    if (result < 0 && result != -8 && result != -9) {
        err("SNDAEMS_updateevent number %d failed %d", *(int*)event, handle);
    } else {
        msg("SNDAEMS_updateevent number %d handle=%d\n", *(int*)event, handle);
    }
    return result;
}

int AEMS_BeginEvent(void* event) {
    return SendEvent((int*)event);
}

int AEMS_SendEvent(void* event) {
    return SendEvent((int*)event);
}

void AEMS_SetVolume(float volume) {
    g_sfxVolume = volume;
    SetEffectsVolume();
}

// Pausing starts the pause event (which mutes the effects); unpausing ends it.
int AEMS_Pause(bool pause) {
    static int handle = -1;
    int event[10];
    event[0] = AEMS_EVENT_PAUSE;
    SNDSYS_entercritical();
    if (pause) {
        handle = SNDAEMS_beginevent(event);
    } else if (handle != -1) {
        handle = SNDAEMS_endevent(handle, event);
    }
    SNDSYS_leavecritical();
    return handle >= 0;
}

// Loads the event system `system` and/or the module bank `bank` (with the
// stream file `streams`) from `dir`.
static void* aems_BankAlloc(void*, int size, int) {
    return gAEMSBank[gAEMSBankCount] = DWI_allocalign("AEMS Bank", size, 0x10, 0x400);
}

static void aems_Init(char* dir, char* system, char* bank, char* streams, bool shell) {
    char path[0x100];
    if (system != NULL) {
        if (gAEMSsystem != NULL) {
            fail("Can't load any more event systems");
        }
        sprintf(path, "%s%s", dir, system);
        if (shell) {
            gAEMSsystem = TLT_LoadFileFromShellBigFile(path, NULL);
        } else {
            gAEMSsystem = TLT_LoadFileFromLevelBigFile(path, NULL);
        }
        if (gAEMSsystem == NULL) {
            fail("Failure loading AEMS system '%s'", path);
        }
        int result = SNDAEMS_addsystem(gAEMSsystem);
        if (result < 0) {
            fail("SNDAEMS_addsystem(%s) failure, ret = %ld", path, result);
        }
        SNDAEMS_streaminit(gAEMSStreamBuf, sizeof(gAEMSStreamBuf), 2);
        gAEMSStreamUsed = true;
    }
    if (bank != NULL) {
        if (gAEMSBankCount >= AEMS_MAXBANKS) {
            fail("Can't load any more module banks");
        }
        sprintf(path, "%s%s", dir, bank);
        void* data = TLT_LoadFileNormal(path, NULL, 0);
        if (data == NULL) {
            data = TLT_LoadFileNormal(bank, NULL, 0);
            if (data == NULL) {
                fail("Failure loading AEMS bank '%s'", path);
            }
        }
        // Relative stream file names are in `dir`.
        if (streams != NULL && *streams != 'd') {
            sprintf(path, "%s%s", dir, streams);
            streams = path;
        }
        int handle = SNDAEMS_addmodulebank(data, streams, 0, aems_BankAlloc);
        if (handle < 0) {
            fail("SNDAEMS_addmodulebank(%s) failure, ret = %d", bank, handle);
        }
        TLT_CloseFile(data);
        gAEMSbHandle[gAEMSBankCount++] = handle;
    }
}

void* AEMSOneShotMalloc(int size) {
    int offset = g_AEMSOneShotMemOffset;
    g_AEMSOneShotMemOffset = offset + size;
    return g_AEMSOneShotMem + offset;
}

static inline void RemoveOneShotBank() {
    SNDAEMS_removemodulebank(g_AEMSOneShotBankHandle);
    g_AEMSOneShotBankID = -1;
    g_AEMSOneShotBankHandle = -1;
    g_AEMSOneShotLoadState = 0;
    g_AEMSOneShotMemOffset = 0;
}

// Starts loading one-shot bank `id` of the level, replacing the loaded one.
void LoadOneShotSound(int id, int mission, int stage) {
    if (g_AEMSOneShotBankID == id) {
        return;
    }
    char path[0x100];
    sprintf(path, "data\\%d\\%d_%d\\%d%dload%d.abk", mission, mission, stage, mission, stage, id);
    if (g_AEMSOneShotLoadState == 2) {
        RemoveOneShotBank();
    }
    g_AEMSOneShotBankHandle = SNDAEMS_asyncloadmodulebank(path, 0, 0, 0, g_AEMSOneShotStreamBuf,
                                                          sizeof(g_AEMSOneShotStreamBuf), AEMSOneShotMalloc);
    g_AEMSOneShotLoadState = 1;
    g_AEMSOneShotBankID = id;
    g_AEMSOneShotMemOffset = 0;
}

void ResetOneShotBank() {
    while (g_AEMSOneShotLoadState == 1) {
        DebugMsg("Waiting for one shot sound bank to finish loading...\n");
        int start = CScreen::GetVSyncs();
        while (CScreen::GetVSyncs() - start < 2) {
            SPIN_WAIT();
        }
        UpdateREAL();
        if (g_AEMSOneShotLoadState == 1 && SNDAEMS_asyncloadmodulebankdone() > 0) {
            g_AEMSOneShotLoadState = 2;
        }
    }
    if (g_AEMSOneShotLoadState == 2) {
        RemoveOneShotBank();
    }
}

void AEMS_Update() {
    if (g_AEMSOneShotLoadState == 1 && SNDAEMS_asyncloadmodulebankdone() > 0) {
        g_AEMSOneShotLoadState = 2;
    }
    if (g_hAEMSAmbientTrack == -1) {
        return;
    }
    // Place the ambience around the player.
    u16 azimuth = 0;
    u16 distance = 0;
    if (!g_ambientSourceOnPlayer) {
        CPlayerObject* player = g_scene.GetPlayer(0);
        CVector3 source = g_ambientSourcePos;
        CVector3 playerPos;
        player->GetPosition(playerPos);
        CVector3 d;
        d.Sub(source, playerPos);
        distance = (s16)(256.0f * d.Length());
        azimuth = RelativeAzimuth(d, player);
    }
    gAEMSAmbientTrackEvent[1] = azimuth;
    gAEMSAmbientTrackEvent[0] = AEMS_EVENT_AMBIENT;
    gAEMSAmbientTrackEvent[4] = distance;
    SNDSYS_entercritical();
    int result = SNDAEMS_updateevent(g_hAEMSAmbientTrack, gAEMSAmbientTrackEvent);
    SNDSYS_leavecritical();
    if (result < 0) {
        err("AmbientTrack_Update(%d, %d):  SNDAEMS_updateevent() FAILED (error == %d)\n", azimuth, distance, result);
    }
}

CSoundSysLock::~CSoundSysLock() {
    SNDSYS_leavecritical();
}

CSoundSysLock::CSoundSysLock() {
    SNDSYS_entercritical();
}

void CSoundStream::Unpause() {
    SNDSTRM_pitchmult(m_handle, 0x1000);
}

void CSoundStream::Pause() {
    SNDSTRM_pitchmult(m_handle, 0);
}

void CSoundStream::FadeVolume(int time, int volume) {
    SNDSTRM_autovol(m_handle, time, volume);
}

int CSoundStream::GetVolume() const {
    return SNDSTRM_getprogvol(m_handle);
}

void CSoundStream::GetRequestStatus(int request, SSndStrmRequestStatus& status) const {
    SNDSTRM_requeststatus(request, (SNDREQUESTSTATUS*)&status);
}

void CSoundStream::GetStatus(SSndStreamStatus& status) const {
    SNDSTRM_status(m_handle, (SNDSTREAMSTATUS*)&status);
}

void CSoundStream::SetVolume(int volume) {
    SNDSTRM_vol(m_handle, volume);
}

void CSoundStream::PurgeQueue() {
    SNDSTRM_purge(m_handle);
}

int CSoundStream::QueueFile(const char* name, int file, int offset) {
    return SNDSTRM_queuefile(m_handle, file, (char*)name, offset);
}

CSoundStream::~CSoundStream() {
    if (m_handle >= 0) {
        SNDSTRM_destroy(m_handle);
        MEM_free(m_buffer);
        m_buffer = NULL;
    }
}

CSoundStream::CSoundStream(int numRequests, int size) {
    m_handle = -1;
    m_buffer = NULL;
    SNDplaysetdef(&m_opts);
    int bufferSize = SNDSTRM_overhead(numRequests, size) + 0x19000;
    m_buffer = DWI_alloc("SoundStream overhead", bufferSize, 0x400);
    m_handle = SNDSTRM_create(&m_opts, numRequests, size, m_buffer, bufferSize);
    if (m_handle < 0) {
        MEM_free(m_buffer);
        m_buffer = NULL;
    }
}

static inline void ApplySoundMode(SNDSYSOPTS& opts, ESoundMode mode) {
    if (mode == SOUNDMODE_SURROUND) {
        opts.caps.speakers = 3;
    } else if (mode == SOUNDMODE_STEREO) {
        opts.caps.speakers = 2;
    } else {
        opts.caps.speakers = 1;
    }
}

void SoundSetMode(ESoundMode mode) {
    SNDSYS_entercritical();
    SNDSYSOPTS opts;
    SNDSYS_getopts(&opts);
    ApplySoundMode(opts, mode);
    opts.caps.renderModes[1] = 0x20 | 4;
    SNDSYS_setopts(&opts);
    SNDSYS_leavecritical();
}

void SoundSync() {
    AEMS_Update();
}

void SoundGetVolumes(float& sfx, float& music) {
    sfx = g_sfxVolume;
    music = g_musicVolume;
}

void SoundSetVolumes(float sfx, float music) {
    g_sfxVolume = sfx;
    g_musicVolume = music;
    SetEffectsVolume();
    PATH_volume(gMusicHandle, (char)(100.0f * (music * g_musicVolume)));
}

void SoundSetMasterVolume(int volume) {
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 127) {
        volume = 127;
    }
    SNDmastervol(volume);
}

static inline void RemoveAEMS() {
    for (int i = 0; i < AEMS_MAXBANKS; i++) {
        if (gAEMSbHandle[i] != -1) {
            int result = SNDAEMS_removemodulebank(gAEMSbHandle[i]);
            if (result < 0) {
                fail("SNDAEMS_removemodulebank failure, ret = %d\n", result);
            }
            if (gAEMSBank[i] != NULL) {
                MEM_free(gAEMSBank[i]);
            }
            gAEMSBank[i] = NULL;
            gAEMSbHandle[i] = -1;
            gAEMSBankCount--;
        }
    }
    int result = SNDAEMS_removesystem();
    if (result < 0) {
        fail("SNDAEMS_removesystem failure, ret = %d\n", result);
    }
    if (gAEMSsystem != NULL) {
        TLT_CloseFile(gAEMSsystem);
    }
    gAEMSsystem = NULL;
    if (gAEMSStreamUsed) {
        SNDAEMS_streamrestore();
    }
    gAEMSStreamUsed = false;
}

static inline void RemoveMusic() {
    if (gMusicHandle >= 0) {
        PATH_stop(gMusicHandle);
    }
    void* buffer;
    while ((buffer = PATH_destroy(-1)) != NULL) {
        MEM_free(buffer);
    }
    if (gMusicMap != NULL) {
        TLT_CloseFile(gMusicMap);
    }
    gMusicMap = NULL;
    gMusicBuf = NULL;
    gMusicHandle = -1;
}

void UnloadShellSoundModules() {
    RemoveAEMS();
}

int UnloadLevelSoundModules() {
    RemoveAEMS();
    RemoveMusic();
    return 0;
}

void LoadShellSoundModules() {
    aems_Init("data\\sound\\", "EventSystem.aem", NULL, NULL, true);
    aems_Init("data\\shell\\", NULL, "ShellBnk.abk", NULL, true);
    SetEffectsVolume();
}

// Creates the music track for map `mapFile` playing `musicFile`.
static inline void CreateMusicTrack(const char* mapFile, const char* musicFile) {
    char path[0x100];
    sprintf(path, "%s", mapFile);
    if (g_bInMultiplayerMode) {
        gMusicMap = TLT_LoadFileNormal(mapFile, NULL, 0);
    } else {
        gMusicMap = TLT_LoadFileFromLevelBigFile(path, NULL);
    }
    gMusicProjectID = PATH_addmapfile(gMusicMap);
    if (gMusicProjectID < 0) {
        err("PATH_addmapfile(%s) failed %d", path, gMusicProjectID);
    }
    gMusicBufSize = PATH_overheadtrack(2, 100);
    gMusicBufSize += 0x20000;
    gMusicBuf = DWI_alloc("Music buffer", gMusicBufSize, 0x400);
    if (gMusicBuf == NULL) {
        err("Can't alloc music streaming buffer of size %d", gMusicBufSize);
    }
    SNDplaysetdef(&gMusicOpts);
    gMusicOpts.azimuth = 0;
    gMusicOpts.vol = (u8)(int)(127.0f * g_musicVolume);
    sprintf(path, "%s", musicFile);
    gMusicHandle = PATH_createtrack((0x01000000 << gMusicProjectID) | 0x10000000, 0, path, 750, &gMusicOpts, 2,
                                    100, gMusicBuf, gMusicBufSize);
    if (gMusicHandle < 0) {
        err("Failed creating music track (%d)", gMusicHandle);
    } else {
        msg("track created 0x%x\n", gMusicHandle);
    }
    if (gMusicHandle >= 0) {
        PATH_stop(gMusicHandle);
    }
}

void LoadLevelSoundModules(int mission, int stage) {
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xC, NULL);
    TLTResourceField* fields = level->fields;
    aems_Init("data\\", ADDR_TO_PTR(char, fields[0].value), NULL, NULL, false);
    char dir[0x40];
    sprintf(dir, "data\\%d\\%d_%d\\", mission, mission, stage);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[1].value), NULL, false);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[2].value), NULL, false);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[3].value), NULL, false);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[4].value), NULL, false);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[5].value), NULL, false);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[6].value), ADDR_TO_PTR(char, level->fields[7].value), false);
    aems_Init(dir, NULL, ADDR_TO_PTR(char, level->fields[10].value), NULL, false);
    SetEffectsVolume();

    // No music in the D-Day intro.
    if (mission != 6 || stage != 1) {
        if (g_bInMultiplayerMode) {
            int track = g_Shell.m_mpMusicTrack;
            printf("Music track = %d \n", track);
            if (track == 0) {
                track = rand() % 8 + 1;
            }
            char mapFile[0x100] = {0};
            char musicFile[0x100] = {0};
            if (track == 1) {
                sprintf(mapFile, "data\\1\\1_1\\1_1M.mpf");
                sprintf(musicFile, "data\\1\\1_1\\main.mus");
            } else if (track == 4) {
                sprintf(mapFile, "data\\3\\3_3\\3_3M.mpf");
                sprintf(musicFile, "data\\3\\3_3\\main.mus");
            } else {
                sprintf(mapFile, "data\\7\\7_%d\\7_%dM.mpf", track, track);
                sprintf(musicFile, "data\\7\\7_%d\\main.mus", track);
            }
            DebugMsg("MapFile: %s \n", mapFile);
            DebugMsg("MusicFile: %s \n", musicFile);
            CreateMusicTrack(mapFile, musicFile);
        } else {
            CreateMusicTrack(ADDR_TO_PTR(const char, level->fields[8].value), ADDR_TO_PTR(const char, level->fields[9].value));
        }
    }
    g_soundMapSingleton.Init(mission, stage);
    g_AEMSOneShotLoadState = 0;
    g_AEMSOneShotMemOffset = 0;
}

int SoundClose() {
    if (!SoundHasBeenInitialized) {
        return -8;
    }
    RemoveAEMS();
    RemoveMusic();
    SNDSYS_removeuserdataclient(_userdata_client, 0);
    SNDSYS_restore();
    SoundHasBeenInitialized = false;
    return 0;
}

int SoundInit(ESoundMode mode) {
    if (SoundHasBeenInitialized) {
        return -7;
    }
    SNDSYS_vectortoreal();
    SNDSYSOPTS opts;
    SNDSYS_getopts(&opts);
    opts.caps.maxBanks = 8;
    opts.caps.unk_0x0 = 0x200;
    opts.caps.speakers = 2;
    opts.caps.unk_0x1F = 0;
    opts.caps.numRenderModes = 2;
    ApplySoundMode(opts, mode);
    opts.caps.renderModes[0] = 0x20 | 0x200;
    opts.caps.renderModes[1] = 0x20 | 4;
    SNDSYS_setopts(&opts);
    if (SNDSYSI_init(SoundBankHeapStart, SoundBankHeapSize, 0x72B00) != 0) {
        return -1;
    }
    SNDSYS_adduserdataclient(_userdata_client);
    for (int i = 0; i < AEMS_MAXBANKS; i++) {
        gAEMSbHandle[i] = -1;
    }
    SoundHasBeenInitialized = true;
    return 0;
}

void EndianSwap(SOUND_MULTISUBTITLEDATA& data) {
    u32* words = (u32*)&data;
    for (int i = 0; i < 10; i++) {
        words[i] = Swap32(words[i]);
    }
}
