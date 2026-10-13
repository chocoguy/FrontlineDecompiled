// RCMP::AV_PLAYER: plays an RCMP movie: streams (or preloads) the video and
// audio files, feeds the decoder the video stream's MPC chunks, and keeps the
// frame clock in step with the audio. Hand-maintained.
#ifndef _MOVIE_AVPLAYER_H_
#define _MOVIE_AVPLAYER_H_

#include "types.h"
#include "game_types.h"
#include "dolphin/os.h"
#include "movie/rcmp.h"

// A millisecond clock that can run at a 4.12 fixed point speed (0 paused).
// size 0x18
class RCMP::AV_MS_TIMER {
public:
    /* 0x00 */ u64 m_last;  // OSTime of the last update
    /* 0x08 */ u64 m_elapsed;  // ticks at speed
    /* 0x10 */ u32 m_speed;  // 0x1000: normal
    /* 0x14 */ u8 unk_0x14[0x4];

    RCMP_ALLOCATOR  // operator delete 0x800FBB40 (weak)
    AV_MS_TIMER() {
        m_elapsed = 0;
        m_last = OSGetTime();
        m_speed = 0x1000;
    }
    void Update() {
        u64 now = OSGetTime();
        if (m_speed == 0x1000) {
            m_elapsed += now - m_last;
        } else {
            m_elapsed += ((now - m_last) * m_speed) >> 12;
        }
        m_last = now;
    }
    u32 GetMS() {
        Update();
        return m_elapsed / (OS_TIMER_CLOCK / 1000);
    }
    void SetSpeed(u32 speed) {
        Update();
        m_speed = speed;
    }
};

// What the decoder sees as its stream: the player. size 0x4
class RCMP::STREAMER {
public:
    /* 0x0 */ AV_PLAYER* m_player;

    RCMP_ALLOCATOR
    ~STREAMER() {}  // 0x800FC14C (weak)
};

// size 0x60
class RCMP::AV_PLAYER {
public:
    enum LOAD_ENUM {
        LOAD_STREAM = 0,  // stream the files
        LOAD_MEMORY = 1,  // read them whole first
    };
    enum SOUND_ENUM {
        SOUND_ON = 0,
        SOUND_OFF = 1,
    };

    /* 0x00 */ AUDIO_PLAYER* m_audio;
    /* 0x04 */ void* m_videoBuffer;  // stream buffers
    /* 0x08 */ void* m_audioBuffer;
    /* 0x0C */ u8 m_separateAudio;  // a separate audio file (else interleaved)
    /* 0x10 */ int m_load;
    /* 0x14 */ int m_sound;
    /* 0x18 */ void* m_videoData;  // LOAD_MEMORY images
    /* 0x1C */ int m_videoLoad;  // their ASYNCFILE_load requests
    /* 0x20 */ void* m_audioData;
    /* 0x24 */ int m_audioLoad;
    /* 0x28 */ int m_videoStream;
    /* 0x2C */ int m_audioStream;
    /* 0x30 */ int m_videoRequest;
    /* 0x34 */ int m_audioRequest;
    /* 0x38 */ int m_startTime;  // ms added to the clock
    /* 0x3C */ u32 m_currentFrame;
    /* 0x40 */ f32 m_frameTime;  // the frame due now
    /* 0x44 */ u32 m_timeBase;  // the clock at the start (ms)
    /* 0x48 */ int m_audioPos;  // the latest audio position (ms)
    /* 0x4C */ int m_audioSync;  // follow the audio
    /* 0x50 */ int m_drift;  // filtered audio - clock difference
    /* 0x54 */ AV_MS_TIMER* m_timer;
    /* 0x58 */ STREAMER m_streamer;
    /* 0x5C */ DECODER* m_decoder;

    RCMP_ALLOCATOR  // operator delete 0x8010405C (weak)
    static void StaticReleaseRCMPChunk(DECODER*, STREAMER*, CHUNK*);  // 0x800FB0EC
    static void StaticGetRCMPChunk(DECODER*, STREAMER*, CHUNK**);  // 0x800FB144
    void GetRCMPChunk(DECODER* decoder, CHUNK** chunk);  // 0x800FB170
    int UnPause();  // 0x800FB324
    int Pause();  // 0x800FB3F8
    int IsAudioFinished();  // 0x800FB4CC
    bool IsTimeForDecode();  // 0x800FB500
    FRAME* GetFrame(float frame);  // 0x800FB710
    FRAME* GetFirstFrame(unsigned int numFrames, int startTime);  // 0x800FB770
    ~AV_PLAYER();  // 0x800FBC1C
    void Init(const char* video, int videoBufferSize, const char* audio, int audioBufferSize, LOAD_ENUM load,
              SOUND_ENUM sound);  // 0x800FBD78
    AV_PLAYER(const char* video, int videoBufferSize, const char* audio, int audioBufferSize, LOAD_ENUM load,
              SOUND_ENUM sound);  // 0x800FC114
};

#endif
