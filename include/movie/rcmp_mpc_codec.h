// The MPC codec: RCMP's MPEG video codec, wrapping the MSSG decoder
// (movie/mpeg2dec.h). Hand-maintained.
#ifndef _MOVIE_RCMP_MPC_CODEC_H_
#define _MOVIE_RCMP_MPC_CODEC_H_

#include "types.h"
#include "game_types.h"
#include "movie/rcmp.h"
#include "ea/real.h"

// A decoded frame buffer, on the codec's used or free list. size 0x14
class MPC_FRAME : public RCMP::FRAME, public RCON::TDLNODE<MPC_FRAME> {
public:
    /* 0x10 */ int m_refs;  // holders: the decoder's references and the caller

    RCMP_ALLOCATOR
    MPC_FRAME(unsigned int width, unsigned int height) {
        m_refs = 0;
        unk_0x0 = 0;
        m_shape = (RCMP::SHAPE*)RCMP::rcmp_sys.m_alloc("MPC_FRAME::m_Shape",
                                                      width * height + height / 2 * (width / 2) * 2 + 0x10, 0,
                                                      0x10, RCMP::rcmp_sys.m_allocParam);
        MEM_fill(m_shape, 0, 0x10);
        m_shape->type = 0x6C;
        m_shape->width = width;
        m_shape->height = height;
    }
    ~MPC_FRAME() {
        if (m_shape != NULL) {
            RCMP::rcmp_sys.m_free(m_shape);
            m_shape = NULL;
        }
    }
};

// size 0x38; vtable 0x8018C414
class MPC_CODEC_INTERNAL : public RCMP::CODEC {
public:
    /* 0x04 */ MPC_FRAME* m_forward;  // the reference frames
    /* 0x08 */ MPC_FRAME* m_backward;
    /* 0x0C */ MPC_FRAME* m_current;  // being decoded
    /* 0x10 */ MPC_FRAME* m_display;  // to return
    /* 0x14 */ int m_state;  // 0 decoding, 1 at the last chunk, 2 flushing
    /* 0x18 */ u8 unk_0x18[0x4];
    /* 0x1C */ int m_first;  // the sequence header is still to read
    /* 0x20 */ int m_frameNumber;
    /* 0x24 */ RCON::TDLLIST<MPC_FRAME> m_used;
    /* 0x2C */ RCON::TDLLIST<MPC_FRAME> m_free;
    /* 0x34 */ RCMP::DECODER* m_decoder;

    MPC_CODEC_INTERNAL() { m_first = 1; }
    virtual ~MPC_CODEC_INTERNAL();  // 0x80104FB8
    virtual int Init(RCMP::DECODER* decoder, RCMP::CHUNK* chunk);  // 0x80104D44
    virtual RCMP::FRAME* GetFrame(unsigned int frame);  // 0x801044F8
    virtual int GetCurrentFrameNumber();  // 0x80104D3C
    virtual float GetFrameRate();  // 0x80104D30
    virtual void ReleaseFrame(RCMP::FRAME* frame);  // 0x801043D0
    MPC_FRAME* CreateB();  // 0x80104420
    MPC_FRAME* CreateIorP();  // 0x8010448C
    void SkipChunks(RCMP::CHUNK** chunk, unsigned int frame);  // 0x801045B0
    MPC_FRAME* DecodeChunk(RCMP::CHUNK** chunk);  // 0x8010481C
    void LowLevelDecodeChunk(RCMP::CHUNK* chunk);  // 0x80104A20

    // Moves on to the stream's next chunk.
    void NextChunk(RCMP::CHUNK** chunk) {
        if (*chunk != NULL) {
            m_decoder->ReleaseChunk(*chunk);
        }
        *chunk = m_decoder->GetChunk();
        m_frameNumber++;
        if (*chunk == NULL) {
            if (m_state == 0) {
                m_state = 1;
            } else {
                m_state = 2;
            }
        }
    }
    // Hands out `frame` (one more holder).
    MPC_FRAME* Use(MPC_FRAME* frame) {
        frame->Remove();
        m_used.AddTail(frame);
        frame->m_refs++;
        return frame;
    }
};

#endif
