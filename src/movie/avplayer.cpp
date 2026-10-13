// RCMP::AV_PLAYER: an RCMP movie. The video (and audio, when separate) are
// streams with filters picking the MPC video chunks and the audio track;
// the decoder pulls video chunks through GetRCMPChunk. The frame due is
// the millisecond clock times the frame rate, with the clock pulled
// towards the audio's position when they drift more than ~1/4 s.
#include "movie/avplayer.h"
#include "game/audioplayer.h"
#include "ea/real.h"
#include "ea/snd/sserver.h"

void RCMP::AV_PLAYER::StaticReleaseRCMPChunk(DECODER* decoder, STREAMER* streamer, CHUNK* chunk) {
    STREAM_release(streamer->m_player->m_videoStream, chunk->m_raw);
    if (chunk != NULL) {
        delete chunk;
    }
}

void RCMP::AV_PLAYER::StaticGetRCMPChunk(DECODER* decoder, STREAMER* streamer, CHUNK** chunk) {
    streamer->m_player->GetRCMPChunk(decoder, chunk);
}

// The next video chunk for the codec (NULL at the end of the stream). The
// first one also chooses the codec.
void RCMP::AV_PLAYER::GetRCMPChunk(DECODER* decoder, CHUNK** out) {
    int kind = 0;
    int stream = m_videoStream;
    *out = NULL;
    for (;;) {
        u8* raw = (u8*)STREAM_get(stream);
        SYNCTASK_run(0);
        if (raw != NULL) {
            u32 type = *(u32*)raw;
            if (MPC_CODEC_is_chunk_for_codec(type)) {
                kind = 1;
            }
            if (kind != 0) {
                CHUNK* chunk = new CHUNK;
                *out = chunk;
                chunk->m_raw = raw;
                // little-endian size after the type
                if (kind == 2) {
                    chunk->m_size = raw[4] | (raw[5] << 8) | (raw[6] << 16) | (raw[7] << 24);
                    chunk->m_data = raw;
                } else {
                    chunk->m_size = raw[4] | (raw[5] << 8) | (raw[6] << 16) | (raw[7] << 24);
                    chunk->m_data = raw + 8;
                }
                if (decoder->m_codec == NULL && kind == 1) {
                    if (decoder->ChooseCodec(MPC_CODEC_create(), chunk) != 2) {
                        decoder->FreeChosenCodec();
                    }
                }
                return;
            }
            char name[5];  // (debug output of the skipped chunk's type)
            *(u32*)name = type;
            name[4] = 0;
            STREAM_release(stream, raw);
        }
        if (STREAM_isendofstream(stream)) {
            return;
        }
    }
}

int RCMP::AV_PLAYER::UnPause() {
    if (m_audio != NULL) {
        m_audio->SetSpeed(0x1000);
    }
    m_timer->SetSpeed(0x1000);
    return 0;
}

int RCMP::AV_PLAYER::Pause() {
    if (m_audio != NULL) {
        m_audio->SetSpeed(0);
    }
    m_timer->SetSpeed(0);
    return 0;
}

int RCMP::AV_PLAYER::IsAudioFinished() {
    if (m_audio != NULL) {
        return m_audio->IsAudioFinished();
    }
    return 1;
}

// Advances the frame clock; true when the next frame is due.
bool RCMP::AV_PLAYER::IsTimeForDecode() {
    u32 now = m_timer->GetMS();
    int time = now - m_timeBase;
    if (m_audioSync) {
        SNDSTREAMSTATUS streamStatus;
        SNDREQUESTSTATUS requestStatus;
        SNDSYS_entercritical();
        SNDSTRM_status(m_audio->m_stream, &streamStatus);
        SNDSTRM_requeststatus(streamStatus.unk_0x4, &requestStatus);
        SNDSYS_leavecritical();
        int audio = requestStatus.played - SNDPROFILE_outputlatency();
        if (audio > m_audioPos) {
            m_audioPos = audio;
            m_drift = (audio - time) + (m_drift - m_drift / 8);
            int drift = m_drift;
            int size = -drift;
            if (drift > size) {
                size = drift;
            }
            if (size > 0x108) {
                m_timeBase -= drift / 8;
                m_drift = 0;
                time = now - m_timeBase;
            }
        }
    }
    m_frameTime = (u32)(time + m_startTime) * (0.001f * m_decoder->GetFrameRate());
    return !(m_frameTime < m_currentFrame + 1.0f);
}

// Decodes the frame for time `frame` (in frames); the caller releases it.
RCMP::FRAME* RCMP::AV_PLAYER::GetFrame(float frame) {
    m_frameTime = frame;
    FRAME* result = m_decoder->GetFrame(m_frameTime);
    m_currentFrame = m_decoder->GetCurrentFrameNumber();
    return result;
}

// Starts playback: queues preloaded files, starts the audio, creates the
// decoder (buffering `numFrames` frames) and returns the first frame.
RCMP::FRAME* RCMP::AV_PLAYER::GetFirstFrame(unsigned int numFrames, int startTime) {
    int size;
    STREAM_setpriority(m_audioStream, 0x96, 0x32);
    STREAM_setpriority(m_videoStream, 0x96, 0x32);
    if (m_videoLoad != 0) {
        ASYNCFILE_release(m_videoLoad, (int*)&m_videoData, &size);
        m_videoRequest = STREAM_queuemem(m_videoStream, m_videoData, size, 0);
    }
    if (m_audioLoad != 0) {
        ASYNCFILE_release(m_audioLoad, (int*)&m_audioData, &size);
        m_audioRequest = STREAM_queuemem(m_audioStream, m_audioData, size, 0);
    }
    if (!m_separateAudio) {
        m_audioRequest = m_videoRequest;
    }
    if (m_sound == SOUND_ON) {
        m_audio = new AUDIO_PLAYER(m_audioStream, m_audioRequest);
        m_audioSync = m_audio->m_stream >= 0;
    } else {
        m_audioSync = 0;
    }

    m_decoder = NULL;
    if (rcmp_sys.m_alloc != NULL && rcmp_sys.m_free != NULL) {
        m_streamer.m_player = this;
        CODEC_IDATA idata(&m_streamer, StaticGetRCMPChunk, StaticReleaseRCMPChunk, numFrames);
        m_decoder = new DECODER(&idata);
    }
    FRAME* first = m_decoder->GetFrame(0);

    m_timer = new AV_MS_TIMER;
    m_timeBase = m_timer->GetMS();
    m_audioPos = 0;
    m_drift = 0;
    m_startTime = startTime;
    if (m_audio != NULL) {
        m_audio->StartSound();
    }
    if (m_audio != NULL) {
        m_audio->SetSpeed(0x1000);
    }
    m_timer->SetSpeed(0x1000);
    return first;
}

RCMP::AV_PLAYER::~AV_PLAYER() {
    if (m_decoder != NULL) {
        delete m_decoder;
        m_decoder = NULL;
    }
    if (m_audio != NULL) {
        delete m_audio;
        m_audio = NULL;
    }
    if (m_timer != NULL) {
        delete m_timer;
        m_timer = NULL;
    }
    STREAM_destroy(m_videoStream);
    m_videoStream = 0;
    if (m_separateAudio) {
        STREAM_destroy(m_audioStream);
        m_audioStream = 0;
    }
    if (m_videoData != NULL) {
        MEM_free(m_videoData);
        m_videoData = NULL;
    }
    if (m_audioData != NULL) {
        MEM_free(m_audioData);
        m_audioData = NULL;
    }
    if (m_videoBuffer != NULL) {
        MEM_free(m_videoBuffer);
        m_videoBuffer = NULL;
    }
    if (m_audioBuffer != NULL) {
        MEM_free(m_audioBuffer);
        m_audioBuffer = NULL;
    }
}

// Opens the streams: the video file (MPC chunks, track 1) and its audio,
// from `audio` when given (track 1 of its own stream) or interleaved in the
// video file (track 2, on a tap of the video stream).
void RCMP::AV_PLAYER::Init(const char* video, int videoBufferSize, const char* audio, int audioBufferSize,
                           LOAD_ENUM load, SOUND_ENUM sound) {
    MEM_fill(this, 0, sizeof(AV_PLAYER));
    m_load = load;
    m_sound = sound;
    m_videoData = NULL;
    m_audioData = NULL;
    m_videoLoad = 0;
    m_audioLoad = 0;
    m_currentFrame = 0;
    m_timer = NULL;
    m_decoder = NULL;
    m_audio = NULL;
    if (!(rcmp_sys.m_alloc != NULL && rcmp_sys.m_free != NULL)) {
        DEBUG_break();
        return;
    }

    if (audio != NULL) {
        m_separateAudio = 1;
        m_videoBuffer = rcmp_sys.m_alloc("AV::VideoStreamBuffer", videoBufferSize, 0, 0, rcmp_sys.m_allocParam);
        m_audioBuffer = rcmp_sys.m_alloc("AV::AudioStreamBuffer", audioBufferSize, 0, 0, rcmp_sys.m_allocParam);
        m_videoStream = STREAM_create(2, 2, 1, m_videoBuffer, videoBufferSize);
        m_audioStream = STREAM_create(2, 2, 1, m_audioBuffer, audioBufferSize);
        STREAM_setpriority(m_audioStream, 0x97, 0x33);
        STREAM_setpriority(m_videoStream, 0x97, 0x33);
        STREAM_setfilter(m_videoStream, 1, 0xFF, 'M', 1);
        STREAM_setfilter(m_videoStream, 2, 0, 0, -2);
        if (m_sound == SOUND_ON) {
            STREAM_setfilter(m_audioStream, 1, 0xFFFF, 'CS', 1);
        } else {
            STREAM_setfilter(m_audioStream, 1, 0xFFFF, 'CS', -1);
        }
        STREAM_setfilter(m_audioStream, 2, 0, 0, -2);
        if (m_load == LOAD_MEMORY) {
            m_audioLoad = ASYNCFILE_load(audio, rcmp_sys.m_allocParam);
            m_videoLoad = ASYNCFILE_load(video, rcmp_sys.m_allocParam);
        } else {
            m_audioRequest = STREAM_queuefile(m_audioStream, audio, 0, 0);
            m_videoRequest = STREAM_queuefile(m_videoStream, video, 0, 0);
        }
    } else {
        m_separateAudio = 0;
        m_videoBuffer = rcmp_sys.m_alloc("AV::VideoStreamBuffer", videoBufferSize, 0, 0, rcmp_sys.m_allocParam);
        m_audioBuffer = NULL;
        m_videoStream = STREAM_create(2, 3, 2, m_videoBuffer, videoBufferSize);
        m_audioStream = STREAM_taphandle(m_videoStream, 2);
        STREAM_setpriority(m_audioStream, 0x97, 0x33);
        STREAM_setpriority(m_videoStream, 0x97, 0x33);
        STREAM_setfilter(m_videoStream, 1, 0xFF, 'M', 1);
        STREAM_setfilter(m_videoStream, 3, 0, 0, -2);
        if (m_sound == SOUND_ON) {
            STREAM_setfilter(m_videoStream, 2, 0xFFFF, 'CS', 2);
        } else {
            STREAM_setfilter(m_videoStream, 2, 0xFFFF, 'CS', -1);
        }
        if (m_load == LOAD_MEMORY) {
            m_videoLoad = ASYNCFILE_load(video, rcmp_sys.m_allocParam);
        } else {
            m_videoRequest = STREAM_queuefile(m_videoStream, video, 0, 0);
        }
    }
}

RCMP::AV_PLAYER::AV_PLAYER(const char* video, int videoBufferSize, const char* audio, int audioBufferSize,
                           LOAD_ENUM load, SOUND_ENUM sound) {
    m_streamer.m_player = NULL;
    Init(video, videoBufferSize, audio, audioBufferSize, load, sound);
}
