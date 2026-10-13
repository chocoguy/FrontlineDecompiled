// Character meshes (.dmesh): skinned models drawn as display lists through
// their parts' render bins. Hand-maintained.
#ifndef _GAME_DMESH_H_
#define _GAME_DMESH_H_

#include "types.h"
#include "game_types.h"
#include "game/skeleton.h"
#include "game/compartment.h"
#include "game/renderbin.h"

class CDMRenderBin;
class CDMesh;
struct CDmaPacket;
struct CDmaTag;
class CDrawContext;
class CPart;
class CPartBin;
class CRemapBin;
class CRenderList;
class CTexture;
class CTexturePack;
struct CharSkel_t;
struct DMRenderObject_T;

// The two top-level bins character meshes draw under: m_type 3 for the
// world, 8 for the first-person weapon (drawn after a z clear).
// size 0x24; vtable 0x8018B5CC
class CDMRenderBin : public CUcodeRenderBin {
public:
    CDMRenderBin(int type) {
        m_type = type;
        m_used = false;
    }
    virtual ~CDMRenderBin() {}  // 0x800F5860 (weak)
    virtual bool IsUsed();  // 0x800F5F04
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800F5F0C
    /* 0x20 */ bool m_used;  // a mesh was drawn this frame
};
SIZE_ASSERT(CDMRenderBin, 0x24);

// The last child of each CDMRenderBin; queued once per frame.
// size 0x20; vtable 0x8018B69C
class CFinishPartBin : public CRenderBin {
public:
    virtual ~CFinishPartBin() {}  // 0x800F6D5C (weak)
};

class CPart : public CRenderBin {
public:
    virtual int Render(CDmaPacket&, void*);  // 0x800F5FB0
    virtual ~CPart() {}  // 0x800F6988 (weak)
    virtual const char* GetName() { return "DMESH Part"; }  // 0x800F6A48 (weak)
};

// A part's display list (size 0x12, packed in the file): commands (op byte,
// u16 vertex count, then per vertex: u8 matrix slot, u16 position index,
// u16 normal index, u16 texcoord index) in 32-byte units, and the joint
// matrices it loads into the slots (slot i is matrix i * 3).
// Morph targets use only `data`, as (position, normal) index pairs.
#pragma pack(push, 2)
struct CDMeshDisplayList {
    /* 0x0 */ u8* data;
    /* 0x4 */ u16 size;  // 32-byte units
    /* 0x6 */ u16 numMatrices;
    /* 0x8 */ u8 matrices[10];
};
#pragma pack(pop)
SIZE_ASSERT(CDMeshDisplayList, 0x12);

// A drawable part of a CDMesh, loaded in place (Init constructs the bin
// part over the file data). size 0x38; vtable 0x8018B668
class CPartBin : public CPart {
public:
    static void* operator new(unsigned long, void* where) { return where; }
    virtual int Render(CDmaPacket&, void*);  // 0x800F58CC
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800F5930
    virtual void Init();  // 0x800F6168
    virtual ~CPartBin() {}  // 0x800F6AB0 (weak)
    /* 0x20 */ u32 unk_0x20;
    /* 0x24 */ u32 m_flags;  // DMESHPART_*
    /* 0x28 */ CTexture* m_texture;  // a texture name index until BindTextures
    /* 0x2C */ u32 m_numLists;
    /* 0x30 */ CDMeshDisplayList* m_lists;
    /* 0x34 */ void* field_0x34;
};
SIZE_ASSERT(CPartBin, 0x38);
typedef CPartBin CDMeshPart;

// Draws a sleeve part with one texture: each distinct texture a sleeve part
// is drawn with gets one of these, queued under the part.
// size 0x28; vtable 0x8018B630
class CRemapBin : public CRenderBin {
public:
    CRemapBin() {}  // 0x800F6D20 (weak)
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800F5FB8
    virtual int Render(CDmaPacket&, void*);  // 0x800F6134
    virtual ~CRemapBin() {}  // 0x800F6A54 (weak)
    /* 0x20 */ CPartBin* m_part;
    /* 0x24 */ CTexture* m_texture;
};
SIZE_ASSERT(CRemapBin, 0x28);

#define DMESHPART_WEAPON 0x10    // no fog (the mesh was Init for the weapon bin)
#define DMESHPART_MORPH 0x100    // on parts[1]: parts after 0 are morph targets
#define DMESHPART_SLEEVE 0x200  // takes the player's sleeve texture

// A skin cluster: a joint's rotation, partly blended from the base pose.
// size 0x4
struct CDMeshCluster {
    /* 0x0 */ u8 m_joint;  // pose and position
    /* 0x1 */ u8 m_matrix;  // the parent joint's matrix, which the rotation starts from
    /* 0x2 */ u16 m_weight;  // 0x1000 = all of the animation
};

// A switchable node: its variants (one is chosen per object). size 0x14
struct CDMeshNode {
    /* 0x00 */ u8 unk_0x0[0x10];
    /* 0x10 */ u16 m_firstVariant;
    /* 0x12 */ u8 m_numVariants;
    /* 0x13 */ u8 unk_0x13;
};

// A node variant: the parts it draws. size 0x14
struct CDMeshVariant {
    /* 0x00 */ u8 unk_0x0[0x10];
    /* 0x10 */ u16 m_firstPart;  // in m_partIndices
    /* 0x12 */ u8 m_numParts;
    /* 0x13 */ u8 unk_0x13;
};

enum {
    DMESH_INIT = 0x1,
    DMESH_SKELETON_BOUND = 0x2,
    DMESH_TEXTURES_BOUND = 0x4,
    DMESH_MORPH = 0x8,  // only parts[0] is drawn; the rest are morph targets
    DMESH_WEAPON = 0x10,
};

// A loaded .dmesh model; its pointers are file offsets until Init.
class CDMesh {
public:
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ u8 m_version;     // 6 for this executable
    /* 0x5 */ u8 field_0x5;     // 1: m_basePose is 12.4 and in file order until BindSkeleton
    /* 0x6 */ u16 m_format;     // 3
    /* 0x8 */ u32 m_flags;      // DMESH_*
    /* 0xC */ char m_name[0x14];
    /* 0x20 */ int m_numClusters;
    /* 0x24 */ CDMeshCluster* m_clusters;
    /* 0x28 */ u32 m_numParts;
    /* 0x2C */ CDMeshPart* m_parts;
    /* 0x30 */ u32 m_numTextureNames;
    /* 0x34 */ char (*m_textureNames)[0x10];
    /* 0x38 */ u32 m_numNodes;  // switchable nodes (DMGeometryState_T)
    /* 0x3C */ CDMeshNode* m_nodes;
    /* 0x40 */ CDMeshVariant* m_variants;
    /* 0x44 */ u16* m_partIndices;  // 0xFFFF: none
    /* 0x48 */ u32 m_numJoints;
    /* 0x4C */ char (*m_jointNames)[0x10];
    /* 0x50 */ CharOrient3_t* m_basePose;
    /* 0x54 */ u8 unk_0x54[0x8];
    /* 0x5C */ u32 m_numPositions;
    /* 0x60 */ s16 (*m_positions)[3];
    /* 0x64 */ u32 m_numNormals;
    /* 0x68 */ s16 (*m_normals)[3];
    /* 0x6C */ u32 m_numTexCoords;
    /* 0x70 */ s16 (*m_texCoords)[2];
    void Draw(CDrawContext&, DMRenderObject_T*);  // 0x800F6180
    void BindTextures(CTexturePack*, char*);  // 0x800F6328
    void BindSkeleton(CharSkel_t*);  // 0x800F647C
    void Init(bool);  // 0x800F6634
    static void InitClass(CRenderList*);  // 0x800F69E4
};


#endif
