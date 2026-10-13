// A movie's audio: a tap stream (1 request of 30 blocks) whose data the
// movie player pushes in; the request is queued held and released by
// StartSound once the video is ready.
#include "game/audioplayer.h"
#include "ea/real.h"
#include "ea/snd/sserver.h"

int RCMP::AUDIO_PLAYER::IsAudioFinished() {
    SNDSTREAMSTATUS status;
    SNDREQUESTSTATUS request;
    SNDSYS_entercritical();
    SNDSTRM_status(m_stream, &status);
    SNDSTRM_requeststatus(status.unk_0x4, &request);
    SNDSYS_leavecritical();
    return request.remaining == 0;
}

int RCMP::AUDIO_PLAYER::StartSound() {
    if (m_request != -1) {
        SNDSYS_entercritical();
        int result = SNDSTRM_modifyhold(m_request, 0);
        SNDSYS_leavecritical();
        return result;
    }
    return 0;
}

int RCMP::AUDIO_PLAYER::SetSpeed(unsigned int speed) {
    if (m_stream != -1) {
        SNDSYS_entercritical();
        int result = SNDSTRM_pitchmult(m_stream, speed);
        SNDSYS_leavecritical();
        return result;
    }
    return 0;
}

RCMP::AUDIO_PLAYER::~AUDIO_PLAYER() {
    if (m_buffer != NULL) {
        SNDSTRM_destroy(m_stream);
        MEM_free(m_buffer);
        m_buffer = NULL;
    }
}

// `format` is the stream format, `id` the request's id.
RCMP::AUDIO_PLAYER::AUDIO_PLAYER(int format, int id) {
    SNDPLAYOPTS opts;
    if (!SNDSYS_inited()) {
        DEBUG_break();
        return;
    }
    m_request = -1;
    m_buffer = NULL;
    m_stream = -1;
    SNDplaysetdef(&opts);
    int size = SNDSTRM_overheadtap(1, 30);
    m_buffer = RCMP::rcmp_sys.m_alloc("AV::audiobuff", size, 0, 0, RCMP::rcmp_sys.m_allocParam);
    m_stream = SNDSTRM_createtap(format, &opts, 1, 30, m_buffer, size);
    m_request = SNDSTRM_queuerequestid(m_stream, -1, id);
    m_speed = 1.0f;
}
