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
    /* 0x0 */ PTR32(u8) data;  // big-endian GX commands (read by the GPU, and by DMMorph)
    /* 0x4 */ u16 size;  // 32-byte units
    /* 0x6 */ u16 numMatrices;
    /* 0x8 */ u8 matrices[10];
};
#pragma pack(pop)
FILE_SIZE_ASSERT(CDMeshDisplayList, 0x12);

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
FILE_SIZE_ASSERT(CDMeshCluster, 0x4);

// A switchable node: its variants (one is chosen per object). size 0x14
struct CDMeshNode {
    /* 0x00 */ u8 unk_0x0[0x10];
    /* 0x10 */ u16 m_firstVariant;
    /* 0x12 */ u8 m_numVariants;
    /* 0x13 */ u8 unk_0x13;
};
FILE_SIZE_ASSERT(CDMeshNode, 0x14);

// A node variant: the parts it draws. size 0x14
struct CDMeshVariant {
    /* 0x00 */ u8 unk_0x0[0x10];
    /* 0x10 */ u16 m_firstPart;  // in m_partIndices
    /* 0x12 */ u8 m_numParts;
    /* 0x13 */ u8 unk_0x13;
};
FILE_SIZE_ASSERT(CDMeshVariant, 0x14);

typedef char DMeshName[0x10];
typedef s16 DMeshVector[3];
typedef s16 DMeshTexCoord[2];

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
    /* 0x24 */ PTR32(CDMeshCluster) m_clusters;
    /* 0x28 */ u32 m_numParts;
    /* 0x2C */ PTR32(CDMeshPart) m_parts;  // natively a separate array (see Init)
    /* 0x30 */ u32 m_numTextureNames;
    /* 0x34 */ PTR32(DMeshName) m_textureNames;
    /* 0x38 */ u32 m_numNodes;  // switchable nodes (DMGeometryState_T)
    /* 0x3C */ PTR32(CDMeshNode) m_nodes;
    /* 0x40 */ PTR32(CDMeshVariant) m_variants;
    /* 0x44 */ PTR32(u16) m_partIndices;  // 0xFFFF: none
    /* 0x48 */ u32 m_numJoints;
    /* 0x4C */ PTR32(DMeshName) m_jointNames;
    /* 0x50 */ PTR32(CharOrient3_t) m_basePose;
    /* 0x54 */ u8 unk_0x54[0x8];
    /* 0x5C */ u32 m_numPositions;
    /* 0x60 */ PTR32(DMeshVector) m_positions;  // big-endian (GPU)
    /* 0x64 */ u32 m_numNormals;
    /* 0x68 */ PTR32(DMeshVector) m_normals;  // big-endian (GPU)
    /* 0x6C */ u32 m_numTexCoords;
    /* 0x70 */ PTR32(DMeshTexCoord) m_texCoords;  // big-endian (GPU)
    void Draw(CDrawContext&, DMRenderObject_T*);  // 0x800F6180
    void BindTextures(CTexturePack*, char*);  // 0x800F6328
    void BindSkeleton(CharSkel_t*);  // 0x800F647C
    void Init(bool);  // 0x800F6634
    static void InitClass(CRenderList*);  // 0x800F69E4
};
FILE_SIZE_ASSERT(CDMesh, 0x74);

#ifdef FRONTLINE_NATIVE
#include "native/swap.h"

// A part as stored in the file. The GameCube builds its CPartBin over this
// record; natively CDMesh::Init builds a separate one from these fields.
struct DMeshPartRecord {
    /* 0x00 */ u8 unk_0x0[0x20];  // room for the render bin built over it
    /* 0x20 */ u32 unk_0x20;
    /* 0x24 */ u32 m_flags;
    /* 0x28 */ u32 m_texture;  // texture name index
    /* 0x2C */ u32 m_numLists;
    /* 0x30 */ PTR32(CDMeshDisplayList) m_lists;
    /* 0x34 */ PTR32(void) field_0x34;
};
FILE_SIZE_ASSERT(DMeshPartRecord, 0x38);

// .dmf (version 6) files are big-endian GameCube data. Swaps the fields the
// CPU reads; vertex arrays and display-list commands stay big-endian (the GPU
// reads them, and DMMorph reads them with big-endian loads). Meshes are
// shared between objects: a swapped one has the magic "DMN". Other versions
// (little-endian PS2 leftovers on the disc) are left alone; the game rejects
// them.
inline void DMeshFromBigEndian(CDMesh* mesh) {
    u8* base = (u8*)mesh;
    if (base[2] != 'F' || mesh->m_version != 6) {
        return;
    }
    base[2] = 'N';
    NativeSwap16(base + 6);
    NativeSwap32(base + 8);
    NativeSwap32s(base + 0x20, (sizeof(CDMesh) - 0x20) / 4);
    for (int i = 0; i < mesh->m_numClusters; i++) {
        NativeSwap16(base + mesh->m_clusters.addr() + i * sizeof(CDMeshCluster) + 2);
    }
    for (u32 i = 0; i < mesh->m_numParts; i++) {
        DMeshPartRecord* part = (DMeshPartRecord*)(base + mesh->m_parts.addr()) + i;
        NativeSwap32s(&part->unk_0x20, 6);
        for (u32 j = 0; j < part->m_numLists; j++) {
            u8* list = base + part->m_lists.addr() + j * sizeof(CDMeshDisplayList);
            NativeSwap32(list);
            NativeSwap16s(list + 4, 2);
        }
    }
    u32 numVariants = 0, numIndices = 0;
    for (u32 i = 0; i < mesh->m_numNodes; i++) {
        CDMeshNode* node = (CDMeshNode*)(base + mesh->m_nodes.addr()) + i;
        NativeSwap32s(node, 4);
        NativeSwap16(&node->m_firstVariant);
        if (node->m_firstVariant + node->m_numVariants > numVariants) {
            numVariants = node->m_firstVariant + node->m_numVariants;
        }
    }
    for (u32 i = 0; i < numVariants; i++) {
        CDMeshVariant* v = (CDMeshVariant*)(base + mesh->m_variants.addr()) + i;
        NativeSwap32s(v, 4);
        NativeSwap16(&v->m_firstPart);
        if (v->m_firstPart + v->m_numParts > numIndices) {
            numIndices = v->m_firstPart + v->m_numParts;
        }
    }
    NativeSwap16s(base + mesh->m_partIndices.addr(), numIndices);
    NativeSwap16s(base + mesh->m_basePose.addr(), mesh->m_numJoints * 3);
}
#endif


#endif
