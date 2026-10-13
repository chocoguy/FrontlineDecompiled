// sound: the game's layer over the EA sound library: AEMS events (sound
// effects, the ambient track, one-shot banks), the PATH music track, sound
// streams, volumes, and subtitles carried in sound user data.
// Hand-maintained.
#ifndef _GAME_SOUND_H_
#define _GAME_SOUND_H_

#include "types.h"
#include "game_types.h"
#include "game/soundtable.h"
#include "ea/real.h"

struct BSObject;
class CAnimObject;
class CBullet;
class CPlayerObject;
class CSoldierObject;
class CSoundStream;
class CSoundSysLock;
class CVector3;
class ISceneNode;
struct SNDUSERDATACBINFO;
struct SOUND_MULTISUBTITLEDATA;
struct SSndStreamStatus;
struct SSndStrmRequestStatus;

// Holds the sound system's critical section for its lifetime.
class CSoundSysLock {
public:
    ~CSoundSysLock();  // 0x800ECBA8
    CSoundSysLock();  // 0x800ECBF8
};

// A stream's state (size 0xC; SNDSTREAMSTATUS).
struct SSndStreamStatus {
    /* 0x0 */ s32 m_queued;  // requests queued
    /* 0x4 */ u8 unk_0x4[0x8];
};

// A queued file's state (size 0x10; SNDREQUESTSTATUS).
struct SSndStrmRequestStatus {
    /* 0x0 */ s32 m_state;  // 1 waiting, 0/2 playing, 3: finished
    /* 0x4 */ s32 m_played;
    /* 0x8 */ s32 m_remaining;
    /* 0xC */ s32 m_position;
};

// An SNDSTRM stream with its own buffer. size 0x20 (operator new)
class CSoundStream {
public:
    /* 0x0 */ s32 m_handle;  // -1 when creating it failed
    /* 0x4 */ void* m_buffer;
    /* 0x8 */ SNDPLAYOPTS m_opts;
    void Unpause();  // 0x800ECC28
    void Pause();  // 0x800ECC50
    void FadeVolume(int time, int volume);  // 0x800ECC78
    int GetVolume() const;  // 0x800ECC9C
    void GetRequestStatus(int, SSndStrmRequestStatus&) const;  // 0x800ECCC0
    void GetStatus(SSndStreamStatus&) const;  // 0x800ECCE8
    void SetVolume(int);  // 0x800ECD0C
    void PurgeQueue();  // 0x800ECD30
    // Returns the request id.
    int QueueFile(const char* name, int file, int offset);  // 0x800ECD54
    ~CSoundStream();  // 0x800ECD84
    CSoundStream(int numRequests, int size);  // 0x800ECDF0
};
SIZE_ASSERT(CSoundStream, 0x20);

// A multi-language subtitle chunk ('SMUL') in a sound's user data; the
// strings follow it. Little-endian in the data. size 0x28
struct SOUND_MULTISUBTITLEDATA {
    /* 0x0 */ u32 m_id;
    /* 0x4 */ u32 m_size;  // whole chunk
    /* 0x8 */ u32 m_count;  // languages
    /* 0xC */ u32 m_optional;  // shown only with subtitles on
    /* 0x10 */ u32 m_color;  // _subtitle_rgb index
    /* 0x14 */ u32 m_offsets[5];  // per language, from the end of this header
};

// Sound mode (SoundInit / SoundSetMode).
enum {
    SOUNDMODE_MONO = 0,
    SOUNDMODE_STEREO = 1,
    SOUNDMODE_SURROUND = 2,
};

// Free functions
int PlayScriptedSound(ISceneNode*, CVector3, const CSoundTable::SSoundParms&, int, BSObject*);  // 0x800E9BA8
// static void msg(char*, ...);  // 0x800EA0E0
// static void err(char*, ...);  // 0x800EA130
// static void _userdata_client(SNDUSERDATACBINFO*);  // 0x800EA180
void MUSIC_Fade(float, int);  // 0x800EA4E4
int MUSIC_SendEvent(int);  // 0x800EA528
int MUSIC_SetLatency(int);  // 0x800EA568
int MUSIC_SetLevel(int);  // 0x800EA5B4
bool MUSIC_Pause(bool);  // 0x800EA5FC
void MUSIC_Stop();  // 0x800EA658
void MUSIC_Start(int, int, int);  // 0x800EA684
int PlayBulletWhiz(CVector3, CVector3, CPlayerObject*);  // 0x800EA76C
void PlayBackgroundImpactSound(CBullet*, int);  // 0x800EAD38
void PlayImpactSound(unsigned short, unsigned short, CVector3, bool);  // 0x800EB17C
void PlayReloadSound(long, CAnimObject*);  // 0x800EB4B8
void PlayAnimTriggeredSound(long, CAnimObject*);  // 0x800EB824
void PlayEnemyMovement(unsigned short, long, CAnimObject*);  // 0x800EB8B0
u16 GetAzimuth(CVector3, const CPlayerObject*);  // 0x800EBC90
void AmbientTrack_SetLocation(const CVector3*);  // 0x800EBDF4
void AmbientTrack_Volume(float);  // 0x800EBE2C
void AmbientTrack_Select(int);  // 0x800EBEDC
void AmbientTrack_Start();  // 0x800EBF6C
void AEMS_SetObject(CSoldierObject*);  // 0x800EBFF4
int AEMS_EndEvent(void*, int);  // 0x800EC004
int AEMS_UpdateEvent(void*, int);  // 0x800EC0B4
int AEMS_BeginEvent(void*);  // 0x800EC170
int AEMS_SendEvent(void*);  // 0x800EC26C
void AEMS_SetVolume(float);  // 0x800EC368
int AEMS_Pause(bool);  // 0x800EC3E0
// static void fail(char*, ...);  // 0x800EC480
// static void aems_Init(char*, char*, char*, char*, bool);  // 0x800EC4D0
// static void aems_BankAlloc(void*, int, int);  // 0x800EC6F4
void* AEMSOneShotMalloc(int);  // 0x800EC738
void LoadOneShotSound(int, int, int);  // 0x800EC754
void ResetOneShotBank();  // 0x800EC834
void AEMS_Update();  // 0x800EC910
void SoundSetMode(ESoundMode);  // 0x800ECEA8
void SoundSync();  // 0x800ECF2C
void SoundGetVolumes(float&, float&);  // 0x800ECF4C
void SoundSetVolumes(float, float);  // 0x800ECF60
void SoundSetMasterVolume(int);  // 0x800ED018
void UnloadShellSoundModules();  // 0x800ED050
int UnloadLevelSoundModules();  // 0x800ED164
void LoadShellSoundModules();  // 0x800ED2C4
void LoadLevelSoundModules(int, int);  // 0x800ED374
int SoundClose();  // 0x800ED984
int SoundInit(ESoundMode);  // 0x800EDB14
void EndianSwap(SOUND_MULTISUBTITLEDATA&);  // 0x800EDC3C

extern u8 SoundBankHeapStart[0x32000];  // 0x801F7470
extern SNDPLAYOPTS gMusicOpts;  // 0x802D84D0
extern int SoundBankHeapSize;  // 0x8034E2E0
extern int gMusicHandle;  // 0x8034E2FC, PATH track, -1 none
extern int gMusicProjectID;  // 0x8034E300
extern bool SoundHasBeenInitialized;  // 0x8034ECB8
extern int g_AEMSOneShotMemOffset;  // 0x8034ECCC
// 0 none, 1 loading, 2 loaded
extern int g_AEMSOneShotLoadState;  // 0x8034ECD0
extern void* gMusicBuf;  // 0x8034ECDC
extern int gMusicBufSize;  // 0x8034ECE0
extern void* gMusicMap;  // 0x8034ECE4

#endif
