// CAnimated: the animation state of an animated object (channels, state
// machine, skinned mesh render data). Hand-maintained; regions not yet
// understood are kept as byte arrays at their offsets.
#ifndef _GAME_ANIMATED_H_
#define _GAME_ANIMATED_H_

#include "types.h"
#include "game_types.h"
#include "game/animchan.h"
#include "game/CVector4.h"
#ifdef FRONTLINE_NATIVE
#include <stddef.h>

#include "game/dmcluster.h"
#include "game/dmrender.h"
#endif

class CAnimMatrixCache;
class CAnimObject;
class CDMesh;
class CDrawContext;
class CTexture;
class CVector3;
struct AnimMotFrameList_t;
struct CharFrame_t;
struct CharSkel_t;

#define ANIMATED_NUM_CHANNELS 16

// size 0xCC4; vtable 0x801838A8
class CAnimated {
public:
    virtual ~CAnimated() {}  // 0x800609A0
    virtual void UpdateAnimation(float);  // 0x80061314
    virtual void Draw(CDrawContext&);  // 0x80060E0C
    virtual int ExtractRotation(long*) { return 0; }  // 0x800609E8
    virtual int ExtractTranslation(CVector3*) { return 0; }  // 0x800609F0
    virtual void SetRotation(long) {}  // 0x800609F8
    virtual void BindSkeleton(CharSkel_t*);  // 0x80060C50
    virtual CAnimMorph* AsAnimMorph() { return NULL; }  // 0x800609FC
    virtual CAnimSkinned* AsAnimSkinned();  // 0x80060780
    /* 0x004 */ AnimObjHdr_t m_hdr;  // field_0x8 doubles as the rotation
    /* 0x010 */ AnimChannel_t m_channels[ANIMATED_NUM_CHANNELS];
    /* 0x890 */ u8 m_stateInfo[0x1A4];  // AnimSt state machine
    /* 0xA34 */ AnimMotFrameList_t* m_pFrame;  // this frame's weighted key frames
    /* 0xA38 */ u32 m_animFlags;
    /* 0xA3C */ u8 field_0xA3C;
    /* 0xA3D */ u8 unk_0xA3D[0x3];
#ifdef FRONTLINE_NATIVE
    // The render and cluster objects hold pointers, so natively they are
    // bigger than the console's regions; these unions size them by the real
    // structs and keep the names that alias their fields.
    union {
        u8 m_render[sizeof(DMRenderObject_T)];  // DMRenderObject_T
        struct {
            u8 pad_geom_[offsetof(DMRenderObject_T, m_geom)];
            u8 m_geomState[sizeof(DMGeometryState_T)];
        };
        struct {
            u8 pad_parts_[offsetof(DMRenderObject_T, m_numParts)];
            u32 field_0xB90;  // m_numParts
        };
    };
    union {
        u8 m_clusterObject[sizeof(DMClusterObject_T)];  // DMClusterObject_T
        struct {
            u32 m_numClusterMatrices;
            u8 pad_matrices_[offsetof(DMClusterObject_T, m_matrices) - 4];
            void* m_clusterMatrices;  // m_matrices[0]
        };
    };
#else
    /* 0xA40 */ u8 m_render[0x14];       // DMRenderObject_T
    /* 0xA54 */ u8 m_geomState[0xC4];    // DMGeometryState_T (part of the render object)
    /* 0xB18 */ CVector4 field_0xB18[3];
    /* 0xB48 */ CVector4 field_0xB48[4];
    /* 0xB88 */ u8 unk_0xB88[0x8];
    /* 0xB90 */ u32 field_0xB90;
    /* 0xB94 */ u8 unk_0xB94[0x10C];
    /* 0xCA0 */ u32 m_numClusterMatrices;  // DMClusterObject_T starts here
    /* 0xCA4 */ void* m_clusterMatrices;
    /* 0xCA8 */ u8 unk_0xCA8[0x8];
#endif
    /* 0xCB0 */ CDMesh* m_pMesh;
    /* 0xCB4 */ CharSkel_t* m_pSkel;
    /* 0xCB8 */ CAnimObject* m_pOwner;
    /* 0xCBC */ CharFrame_t* m_pBaseFrame;
    /* 0xCC0 */ bool field_0xCC0;
    /* 0xCC1 */ bool field_0xCC1;
    /* 0xCC2 */ u8 unk_0xCC2[0x2];
    void SetAnimSpeed(unsigned char, float);  // 0x80060C90
    void AdjustRootTrans(CVector3);  // 0x80060CC4
    void SetGeomNodeState(long, long);  // 0x80060CFC
    void BindSleeveTexture(CTexture*);  // 0x80060D20
    void BindTexture(CTexture*, char);  // part < 0: all parts  // 0x80060D6C
    void BindMesh(CDMesh*);  // 0x80060DCC
    int FinishAnimationState(unsigned short);  // returns 0  // 0x80061384
    int SetAnimationState(unsigned short, float, unsigned short&, long, void*);  // 0x800613DC
    bool ValidState(unsigned short);  // 0x800614C8
    void Init(CAnimObject*, unsigned short);  // 0x80061530
    static void InitClass();  // 0x800615A0
};

// Returned through a hidden pointer like the original's (it has a
// constructor); a plain 8-byte struct would come back in r3/r4 instead.
struct CAnimMatrixCacheEntry {
    void* vectors;   // m_vectorsPerSlot x 0x10 bytes
    void* matrices;  // m_matricesPerSlot 3x4 matrices
    CAnimMatrixCacheEntry() {}
};

// size 0x24
class CAnimMatrixCache {
public:
    // A cache slot: its owner and how many updates it has gone unused.
    struct Slot {
        void* key;
        s32 age;
    };

    /* 0x0 */ s32 m_numSlots;
    /* 0x4 */ s32 m_vectorsPerSlot;
    /* 0x8 */ s32 m_matricesPerSlot;
    /* 0xC */ s32 m_buffer;  // always 0; only one buffer set is allocated
    /* 0x10 */ Slot* m_slots;
    /* 0x14 */ u8* m_vectorBuffers[2];
    /* 0x1C */ u8* m_matrixBuffers[2];
    // Matrices for `key`, taking the free or least recently used slot for a
    // new key (isNew = true).
    CAnimMatrixCacheEntry GetEntry(void* key, bool& isNew);  // 0x80060A04
    void Update();  // 0x80060B38
    CAnimMatrixCache(int, int, int);  // 0x80060B80
};

#endif
