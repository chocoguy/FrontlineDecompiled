// staticmesh: static meshes (.msh, relocated in place) and their drawing:
// the node tree is frustum-culled, visible nodes queue their chunks on the
// material bins, which record the model-view matrix and up to three
// directional lights per chunk and draw the display lists. Mirrors the
// compartment code. Hand-maintained.
#ifndef _GAME_STATICMESH_H_
#define _GAME_STATICMESH_H_

#include "types.h"
#include "game_types.h"
#include "game/matrix.h"
#include "game/MSHMaterial.h"
#include "game/compartment.h"
#include "game/shape.h"
#include "dolphin/gx.h"

struct BPDLightVolume;
struct CDmaPacket;
struct CDmaTag;
class CDrawContext;
class CMSHMatBin;
class CMshSimpleBin;
class CRenderList;
class CScreen;
class CStaticAnimMesh;
class CStaticMesh;
class MSHNode;
struct StaticMesh;
class CVolBox;

// A hard point in a mesh (size 0x50): where attached objects sit.
struct StaticMeshHardPoint {
    /* 0x00 */ CMatrix m_tm;
    /* 0x40 */ int m_id;
    /* 0x44 */ u32 unk_0x44[3];
};

// A chunk's display list. The list starts with 0x34 bytes of array setup
// commands (written by Init), then the triangles.
struct MSHDisplayList {
    /* 0x00 */ u32 m_flags;  // bit 0: 8-bit indices, bit 2: a strip
    /* 0x04 */ u8 unk_0x4[0x8];
    /* 0x0C */ u32 m_size;  // vertex count until Init, then the list's byte size
    /* 0x10 */ f32* m_positions;
    /* 0x14 */ u32* m_colors;
    /* 0x18 */ u8* m_normals;
    /* 0x1C */ f32* m_texCoords;
    /* 0x20 */ MSHVertex_GC* m_list;
};

// A piece of a node drawn with one material. size 0x20
struct MSHChunk {
    /* 0x00 */ MSHNode* m_node;
    /* 0x04 */ CMSHMatBin* m_material;
    /* 0x08 */ MSHDisplayList* m_list;
    /* 0x0C */ u32 unk_0xC;
    /* 0x10 */ MSHChunk* m_next;  // in the material's queue
    /* 0x14 */ u8 unk_0x14[0xC];
};

// A node of the mesh's bounding box tree (leaves hold chunks). The box axes
// carry the half extents. size 0x60
class MSHNode {
public:
    /* 0x00 */ MSHChunk* m_chunks;
    /* 0x04 */ int m_numChunks;
    /* 0x08 */ MSHNode* m_children[2];
    /* 0x10 */ CVector3 m_axisX;
    /* 0x20 */ CVector3 m_axisY;
    /* 0x30 */ CVector3 m_axisZ;
    /* 0x40 */ CVector3 m_center;
    /* 0x50 */ u32 m_drawFlags;  // MSHNODE_*
    /* 0x54 */ u32 unk_0x54[3];
};

enum {
    MSHNODE_LIT = 2,
    MSHNODE_CLIPPED = 4,
    MSHNODE_FOGGED = 8,
};

// The static mesh file header.
struct StaticMesh {
    /* 0x00 */ u8 unk_0x0[0x8];
    /* 0x08 */ CMSHMatBin* m_opaque;
    /* 0x0C */ int m_numOpaque;
    /* 0x10 */ CMSHMatBin* m_alpha;
    /* 0x14 */ int m_numAlpha;
    /* 0x18 */ MSHChunk* m_chunks;
    /* 0x1C */ int m_numChunks;
    /* 0x20 */ MSHNode* m_nodes;  // the root first: the mesh bounds
    /* 0x24 */ int m_numNodes;
    /* 0x28 */ StaticMeshHardPoint* m_hardPoints;
    /* 0x2C */ int m_numHardPoints;
};

// An animated mesh file: one mesh per frame.
struct StaticAnimMesh {
    /* 0x00 */ u8 unk_0x0[0x8];
    /* 0x08 */ StaticMesh** m_frames;
    /* 0x0C */ int m_numFrames;
    /* 0x10 */ int m_ticksPerFrame;
};

// size 0xC (operator new); vtable 0x8018B4F0
class CStaticMesh {
public:
    // vtable pointer at 0x0
    virtual ~CStaticMesh();  // 0x800F2510
    virtual void Draw(CDrawContext&, int);  // 0x800F1E64
    virtual void EnableLighting(bool);  // 0x800AA1A8
    virtual int IsLightingEnabled();  // 0x800ACFAC
    virtual void EnableFogging(bool);  // 0x800B327C
    virtual int IsFoggingEnabled() { return m_fogging; }  // 0x800F14F8 (weak)
    /* 0x4 */ StaticMesh* m_pData;
    /* 0x8 */ u8 m_lighting : 1;
    u8 m_fogging : 1;
    u8 m_flags8 : 6;
    /* 0x9 */ u8 unk_0x9[0x3];
    CStaticMesh();  // 0x800D67C8 (weak)
    void DrawNode(MSHNode&, unsigned long);  // 0x800F2090
    static void SetLightVolume(BPDLightVolume*);  // 0x800F2434
    static void InitClass(CRenderList*, CScreen*);  // 0x800F2478
    CStaticMesh(StaticMesh*, bool);  // 0x800F2558
    void Init();  // 0x800F25BC
    void Init(void*, bool);  // 0x800F2CB8
};

// size >= 0x10 (from field accesses); vtable 0x8018B410
class CStaticAnimMesh : public CStaticMesh {
public:
    CStaticAnimMesh() { m_anim = NULL; }
    virtual ~CStaticAnimMesh();  // 0x800F1634
    virtual void Draw(CDrawContext&, int);  // 0x800F15E4
    /* 0xC */ StaticAnimMesh* m_anim;
    int ticksPerFrame() const;  // 0x800F1504
    int numFrames() const;  // 0x800F1510
    void Init(void*, bool);  // 0x800F151C
};

// A mesh material bin (built in place in the mesh file): collects this
// frame's chunks. size 0x70; vtable 0x8018B49C
class CMSHMatBin : public MSHMaterial {
public:
    CMSHMatBin() {}
    /* 0x20 */ u8 unk_0x20[0x8];
    /* 0x28 */ GXTexObj m_texObj;
    /* 0x48 */ GXTlutObj m_tlut;
    /* 0x54 */ int m_hasTlut;
    /* 0x58 */ u8 unk_0x58[0x8];
    /* 0x60 */ ShapeFile* m_shape;  // NULL: untextured
    /* 0x64 */ u32 m_flags;  // MSHMAT_*
    /* 0x68 */ MSHChunk* m_chunks;  // queued this frame
    /* 0x6C */ u32 unk_0x6C;
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800F1690
    virtual int Render(CDmaPacket&, void*);  // 0x800F1C58
    virtual ~CMSHMatBin() {}  // 0x800F31C8 (weak)
};

enum {
    MSHMAT_TWO_SIDED = 1,
    MSHMAT_WRAP = 8,
    MSHMAT_UNLIT = 0x10,
};

// Parent of the mesh material bins (opaque or alpha). size 0x24; vtable 0x8018B4D4
class CMshSimpleBin : public CUcodeRenderBin {
public:
    CMshSimpleBin(int type) { m_type = type; }
    /* 0x20 */ bool m_used;
    /* 0x21 */ u8 unk_0x21[0x3];
    virtual void Init();  // 0x800F1DFC
    virtual bool IsUsed();  // 0x800F1E58
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800F1E60
    virtual ~CMshSimpleBin() {}  // 0x800F3234 (weak)
};

// Free functions
void UpdateLightingParams(CDmaPacket&);  // 0x800F2CE4

// Light parameters handed to the chunks: three directions, their three
// colors, the ambient color and an unused slot (model space, 0..255).
extern f32 g_LightParam[8][3];  // 0x802DA670
extern bool g_bCacheLightParam;  // 0x8034ED20

#endif
