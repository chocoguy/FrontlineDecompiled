// RCMP::AUDIO_PLAYER: a movie's audio, played as a tap sound stream the
// movie player feeds. Hand-maintained.
#ifndef _GAME_AUDIOPLAYER_H_
#define _GAME_AUDIOPLAYER_H_

#include "types.h"
#include "game_types.h"
#include "movie/hwvideodisplaylist.h"
#include "movie/rcmp2real.h"

// size >= 0x14
class RCMP::AUDIO_PLAYER {
public:
    /* 0x0 */ f32 m_speed;
    /* 0x4 */ void* m_buffer;
    /* 0x8 */ u8 unk_0x8[0x4];
    /* 0xC */ int m_stream;   // -1 none
    /* 0x10 */ int m_request; // -1 none; queued held until StartSound

    RCMP_ALLOCATOR  // operator delete 0x800FBBEC (weak)
    int IsAudioFinished();  // 0x8010879C
    int StartSound();  // 0x801087F0
    int SetSpeed(unsigned int);  // 0x80108848
    ~AUDIO_PLAYER();  // 0x801088AC
    AUDIO_PLAYER(int, int);  // 0x8010892C
};

#endif
