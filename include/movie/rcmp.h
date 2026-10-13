// RCMP: EA's movie player framework. A DECODER pulls chunks from a stream
// (through the CODEC_IDATA callbacks) and hands them to the CODEC chosen
// for the stream, which decodes FRAMEs; a VIDEO_DRIVER draws them.
// Allocation goes through the rcmp_sys hooks. Hand-maintained.
#ifndef _MOVIE_RCMP_H_
#define _MOVIE_RCMP_H_

#include "types.h"
#include "game_types.h"

struct _GXRenderModeObj;

// Intrusive circular doubly linked lists.
namespace RCON {

template <class T>
class TDLNODE {
public:
    /* 0x0 */ TDLNODE<T>* m_next;
    /* 0x4 */ TDLNODE<T>* m_prev;

    ~TDLNODE() {}
    void Remove() {
        m_prev->m_next = m_next;
        m_next->m_prev = m_prev;
    }
};

// The list head is a sentinel node.
template <class T>
class TDLLIST {
public:
    /* 0x0 */ TDLNODE<T> m_head;

    TDLLIST() {
        m_head.m_next = &m_head;
        m_head.m_prev = &m_head;
    }
    ~TDLLIST() { DeleteAll(); }
    // Unlinks and deletes every node (as a T).
    void DeleteAll() {
        while (m_head.m_next != &m_head) {
            TDLNODE<T>* node = m_head.m_next;
            node->Remove();
            delete static_cast<T*>(node);
        }
    }
    bool IsEmpty() { return m_head.m_next == &m_head; }
    TDLNODE<T>* First() { return m_head.m_next; }
    TDLNODE<T>* End() { return &m_head; }
    void AddHead(TDLNODE<T>* node) {
        TDLNODE<T>* first = m_head.m_next;
        m_head.m_next = node;
        first->m_prev = node;
        node->m_prev = &m_head;
        node->m_next = first;
    }
    void AddTail(TDLNODE<T>* node) {
        TDLNODE<T>* last = m_head.m_prev;
        m_head.m_prev = node;
        last->m_next = node;
        node->m_next = &m_head;
        node->m_prev = last;
    }
};

}  // namespace RCON

class RCMP {
public:
    class AUDIO_PLAYER;
    class AV_MS_TIMER;
    class AV_PLAYER;
    class CHUNK;
    class CODEC;
    class CODEC_IDATA;
    class DECODER;
    class FRAME;
    class RCMP_SYSTEM;
    class STREAMER;
    class VIDEO_DRIVER;

    // EA's SHAPE image header: a decoded frame's planes follow it (Y, then
    // U and V at half size). size 0x10
    struct SHAPE {
        /* 0x0 */ u8 type;  // 0x6C
        /* 0x1 */ u8 unk_0x1[0x3];
        /* 0x4 */ s16 width;
        /* 0x6 */ s16 height;
        /* 0x8 */ u8 unk_0x8[0x4];
        /* 0xC */ u8 flags;  // 0x10: the data offset follows the header
        /* 0xD */ u8 unk_0xD[0x3];

        u8* Data() {
            if (flags & 0x10) {
                return (u8*)this + *(u32*)(this + 1);
            }
            return (u8*)(this + 1);
        }
    };

    static VIDEO_DRIVER* MOHFL_VD_create(DECODER*, FRAME*);  // 0x80102A20
    static CODEC* MPC_CODEC_create();  // 0x801042F0
    static bool MPC_CODEC_is_chunk_for_codec(unsigned int type);  // 0x80105218
    static VIDEO_DRIVER* GCSW_VD_create(DECODER*, FRAME*, _GXRenderModeObj*, void*);  // 0x801086B4
    static RCMP_SYSTEM rcmp_sys;  // 0x803158AC
};

// The player's memory hooks (RCMP::rcmp_sys). size 0x10; vtable 0x8018C464
class RCMP::RCMP_SYSTEM {
public:
    // vtable pointer at 0x0
    RCMP_SYSTEM() : m_alloc(NULL), m_free(NULL) {}
    virtual ~RCMP_SYSTEM() {}  // 0x801055B0 (weak)
    /* 0x4 */ void* (*m_alloc)(const char* name, u32 size, s32 flags, s32 offset, s32 param);
    /* 0x8 */ void (*m_free)(void* ptr);
    /* 0xC */ int m_allocParam;
    void SetREALDefaults();  // 0x80105234
};

// Class operator new/delete through rcmp_sys.
#define RCMP_ALLOCATOR                                                                     \
    static void* operator new(unsigned long size) {                                        \
        return RCMP::rcmp_sys.m_alloc("", size, 0, 0, RCMP::rcmp_sys.m_allocParam);        \
    }                                                                                      \
    static void operator delete(void* p) { RCMP::rcmp_sys.m_free(p); }

// A stream chunk. size 0xC
class RCMP::CHUNK {
public:
    /* 0x0 */ void* m_raw;  // the stream's chunk (to release)
    /* 0x4 */ u8* m_data;
    /* 0x8 */ u32 m_size;

    RCMP_ALLOCATOR
    CHUNK();  // 0x8010526C
};

// How a decoder gets its chunks. size 0x10
class RCMP::CODEC_IDATA {
public:
    /* 0x0 */ STREAMER* m_streamer;
    /* 0x4 */ void (*m_getChunk)(DECODER*, STREAMER*, CHUNK**);
    /* 0x8 */ void (*m_releaseChunk)(DECODER*, STREAMER*, CHUNK*);
    /* 0xC */ unsigned int m_numFrames;  // frames the codec buffers beyond its 2 references

    CODEC_IDATA(STREAMER* streamer, void (*getChunk)(DECODER*, STREAMER*, CHUNK**),
                void (*releaseChunk)(DECODER*, STREAMER*, CHUNK*), unsigned int numFrames);  // 0x80105258
    RCMP_ALLOCATOR
    ~CODEC_IDATA() {}  // 0x800FBBA0 (weak)
};

// A decoded picture. size 0x8
class RCMP::FRAME {
public:
    /* 0x0 */ u32 unk_0x0;
    /* 0x4 */ SHAPE* m_shape;

    RCMP_ALLOCATOR
    ~FRAME() {}  // 0x80104CB4 (weak)
};

// A stream decoder (abstract). vtable 0x8018C434
class RCMP::CODEC {
public:
    // vtable pointer at 0x0
    RCMP_ALLOCATOR
    virtual ~CODEC() {}  // 0x80104374 (weak)
    virtual int Init(DECODER* decoder, CHUNK* chunk) = 0;
    virtual FRAME* GetFrame(unsigned int frame) = 0;
    virtual int GetCurrentFrameNumber() = 0;
    virtual float GetFrameRate() = 0;
    virtual void ReleaseFrame(FRAME* frame) = 0;
};

// size 0x1C; vtable 0x8018C458
class RCMP::DECODER {
public:
    // vtable pointer at 0x0
    virtual ~DECODER();  // 0x801054CC
    /* 0x04 */ CHUNK* m_pending;  // a chunk got but not yet consumed
    /* 0x08 */ STREAMER* m_streamer;
    /* 0x0C */ void (*m_getChunk)(DECODER*, STREAMER*, CHUNK**);
    /* 0x10 */ void (*m_releaseChunk)(DECODER*, STREAMER*, CHUNK*);
    /* 0x14 */ unsigned int m_numFrames;
    /* 0x18 */ CODEC* m_codec;

    RCMP_ALLOCATOR
    DECODER(const CODEC_IDATA* idata);  // 0x80105560
    int ChooseCodec(CODEC* codec, CHUNK* chunk);  // 0x80105490: the codec's Init result (2: ok)
    void FreeChosenCodec();  // 0x8010543C
    int GetCurrentFrameNumber();  // 0x801053FC
    float GetFrameRate();  // 0x801053BC
    FRAME* GetFrame(unsigned int frame);  // 0x80105334
    void ReleaseFrame(FRAME* frame);  // 0x80105304
    CHUNK* GetChunk();  // 0x801052B0
    void ReleaseChunk(CHUNK* chunk);  // 0x80105280
};

// Draws frames. vtable 0x8018C304
class RCMP::VIDEO_DRIVER {
public:
    // vtable pointer at 0x0
    virtual ~VIDEO_DRIVER() {}  // 0x801029D8 (weak)
    virtual void SetPosition(float, float, float) = 0;
    virtual void SetScale(float, float) = 0;
    virtual void Draw(RCMP::FRAME*) = 0;
    virtual void ReDraw() = 0;
    virtual void CompleteDraw() {}  // 0x80102D24 (weak)
};

#endif
