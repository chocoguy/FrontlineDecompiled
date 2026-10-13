// The RCMP decoder: feeds the chosen codec chunks through the stream's
// callbacks and forwards frame requests to it. Also the memory hooks.
#include "movie/rcmp.h"

RCMP::RCMP_SYSTEM RCMP::rcmp_sys;

RCMP::DECODER::DECODER(const CODEC_IDATA* idata)
    : m_streamer(NULL), m_getChunk(NULL), m_releaseChunk(NULL), m_numFrames(2) {
    m_streamer = idata->m_streamer;
    m_getChunk = idata->m_getChunk;
    m_releaseChunk = idata->m_releaseChunk;
    m_numFrames = idata->m_numFrames;
    m_codec = NULL;
    m_pending = NULL;
}

RCMP::DECODER::~DECODER() {
    FreeChosenCodec();
}

// Starts `codec` on the stream's first chunk.
int RCMP::DECODER::ChooseCodec(CODEC* codec, CHUNK* chunk) {
    m_codec = codec;
    m_pending = chunk;
    return m_codec->Init(this, chunk);
}

void RCMP::DECODER::FreeChosenCodec() {
    if (m_codec != NULL) {
        delete m_codec;
        m_codec = NULL;
    }
}

int RCMP::DECODER::GetCurrentFrameNumber() {
    if (m_codec == NULL) {
        return 0;
    }
    return m_codec->GetCurrentFrameNumber();
}

float RCMP::DECODER::GetFrameRate() {
    if (m_codec == NULL) {
        return 0.0f;
    }
    return m_codec->GetFrameRate();
}

RCMP::FRAME* RCMP::DECODER::GetFrame(unsigned int frame) {
    if (m_codec == NULL && m_pending == NULL) {
        m_getChunk(this, m_streamer, &m_pending);
    }
    if (m_codec == NULL) {
        return NULL;
    }
    return m_codec->GetFrame(frame);
}

void RCMP::DECODER::ReleaseFrame(FRAME* frame) {
    m_codec->ReleaseFrame(frame);
}

RCMP::CHUNK* RCMP::DECODER::GetChunk() {
    CHUNK* chunk;
    if (m_pending != NULL) {
        chunk = m_pending;
        m_pending = NULL;
        return chunk;
    }
    m_getChunk(this, m_streamer, &chunk);
    return chunk;
}

void RCMP::DECODER::ReleaseChunk(CHUNK* chunk) {
    m_releaseChunk(this, m_streamer, chunk);
}

RCMP::CHUNK::CHUNK() {
    m_data = NULL;
    m_size = 0;
    m_raw = NULL;
}

RCMP::CODEC_IDATA::CODEC_IDATA(STREAMER* streamer, void (*getChunk)(DECODER*, STREAMER*, CHUNK**),
                               void (*releaseChunk)(DECODER*, STREAMER*, CHUNK*), unsigned int numFrames) {
    m_numFrames = numFrames;
    m_streamer = streamer;
    m_getChunk = getChunk;
    m_releaseChunk = releaseChunk;
}
