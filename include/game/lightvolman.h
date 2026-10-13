// CLightVolumeManager: picks the active level light volume (highest
// priority) and cross-fades to it over 10 seconds when it changes.
// Hand-maintained.
#ifndef _GAME_LIGHTVOLMAN_H_
#define _GAME_LIGHTVOLMAN_H_

#include "types.h"
#include "game/bpdlight.h"

#define LIGHT_BLEND_TIME 10.0f

// size 0x18
class CLightVolumeManager {
public:
    /* 0x00 */ BPDLightVolume* m_volumes[4];  // by priority, highest first
    /* 0x10 */ BPDLightVolume* m_blendFrom;   // previous volume while blending
    /* 0x14 */ f32 m_blendTime;               // time left in the blend

    CLightVolumeManager();  // 0x80079128
    ~CLightVolumeManager();  // 0x800790CC
    void Reset();  // 0x800790A8
    void Update(float dt);  // 0x80079070
    void AddVolume(BPDLightVolume* volume);  // 0x80078EE4
    void RemoveVolume(BPDLightVolume* volume);  // 0x80078DF8
    // The volume to light with (a blend of two during a transition).
    BPDLightVolume* GetVolume();  // 0x80078A3C

private:
    void StartBlend(BPDLightVolume* from);
};

#endif
