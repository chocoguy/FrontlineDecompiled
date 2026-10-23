// Collision database (level .cdb data). Hand-written; extended as units that
// use it are decompiled.
#ifndef _GAME_CDB_H_
#define _GAME_CDB_H_

#include "types.h"

// One entry per surface type (0xC bytes).
struct CDBSurfaceType {
    /* 0x0 */ u8 flags;           // bit 0: bullets pass through
    /* 0x1 */ u8 unk_0x1;
    /* 0x2 */ s16 decalIndex;
    /* 0x4 */ s16 soundIndex;
    /* 0x6 */ u8 unk_0x6[0x2];
    /* 0x8 */ u32 particleCRC;
};
FILE_SIZE_ASSERT(CDBSurfaceType, 0xC);

// A collision-database vertex.
struct CDBVector {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
};

// A collision-database triangle. Contacts point at these through
// SClsnContact::m_pPoly (SClsnPoly is the same layout).
struct CDBTri {
    /* 0x0 */ PTR32(CDBVector) m_verts[3];
#ifdef FRONTLINE_NATIVE
    // clang allocates bitfields from the low bit, CodeWarrior from the high
    // bit: declared in reverse so the (swapped) file word's bits line up.
    /* 0xC */ u32 unk_0xF : 8;
    /* 0xC */ u32 m_lineSkip : 1;
    /* 0xC */ u32 m_passThru : 1;
    /* 0xC */ s32 m_texSwap : 11;
    /* 0xC */ s32 m_surfaceType : 11;
#else
    /* 0xC */ s32 m_surfaceType : 11;
    /* 0xC */ s32 m_texSwap : 11;  // compartment texture swap, -1 for none
    /* 0xC */ u32 m_passThru : 1;  // skipped while CCDBObject pass-through is disabled
    /* 0xC */ u32 m_lineSkip : 1;  // ignored by line tests unless the collision asks for every polygon
    /* 0xC */ u32 unk_0xF : 8;
#endif
};
FILE_SIZE_ASSERT(CDBTri, 0x10);

// An oriented box in the collision database.
struct CDBBox {
    /* 0x00 */ f32 m_width;
    /* 0x04 */ f32 m_depth;
    /* 0x08 */ f32 m_height;
    /* 0x0C */ f32 m_basis[3][3];  // x, y and z axes
    /* 0x30 */ f32 m_center[3];
};
FILE_SIZE_ASSERT(CDBBox, 0x3C);

// Triangles sharing a plane (variable size: 0x14 + 4 * count).
struct CDBTriGroup {
    /* 0x00 */ f32 m_normal[3];
    /* 0x0C */ f32 m_d;
    /* 0x10 */ s32 m_count;  // negated while a query has visited the group
    /* 0x14 */ PTR32(CDBTri) m_tris[1];
};

enum {
    CDBNODE_LEAF = 0,
    CDBNODE_BRANCH = 1,
};

struct CDBNode {
    /* 0x00 */ s32 m_type;  // CDBNODE_*
};

// BSP leaf (variable size: 0x44 + 4 * count).
struct CDBLeafNode : CDBNode {
    /* 0x04 */ CDBBox m_box;
    /* 0x40 */ s32 m_count;
    /* 0x44 */ PTR32(CDBTriGroup) m_groups[1];
};

// BSP split: children on the front (n.p >= d) and back sides.
struct CDBBranchNode : CDBNode {
    /* 0x04 */ PTR32(CDBNode) m_front;
    /* 0x08 */ PTR32(CDBNode) m_back;
    /* 0x0C */ f32 m_normal[3];
    /* 0x18 */ f32 m_d;
};
FILE_SIZE_ASSERT(CDBBranchNode, 0x1C);

// A level's collision database (.cdb); pointers are file offsets until
// CCDBObject::Init relocates them.
struct CDB {
    /* 0x00 */ u32 unk_0x0;
    /* 0x04 */ u32 m_fileSize;
    /* 0x08 */ s32 m_numVerts;
    /* 0x0C */ u32 m_verts;  // file offset of the CDBVector array (right after this header)
    /* 0x10 */ s32 m_numTris;
    /* 0x14 */ s32 m_numTriGroups;
    /* 0x18 */ s32 m_numLeaves;
    /* 0x1C */ s32 m_numBranches;
    /* 0x20 */ s32 m_numSurfaceTypes;  // presumably: 0 whenever surfaceTypes is
    /* 0x24 */ PTR32(CDBTri) m_tris;
    /* 0x28 */ PTR32(CDBTriGroup) m_triGroups;
    /* 0x2C */ PTR32(CDBLeafNode) m_leaves;
    /* 0x30 */ PTR32(CDBBranchNode) m_branches;  // the first is the root
    /* 0x34 */ PTR32(CDBSurfaceType) surfaceTypes;
    /* 0x38 */ CDBBox m_bounds;
};
FILE_SIZE_ASSERT(CDB, 0x74);

#ifdef FRONTLINE_NATIVE
#include "native/swap.h"

// The .cdb file is big-endian GameCube data that the CPU reads as-is; the
// native build swaps every field to little-endian before CCDBObject::Init
// relocates it. Offsets are still file-relative here.
inline void CDBFromBigEndian(CDB* cdb) {
    u8* base = (u8*)cdb;
    NativeSwap32s(cdb, sizeof(CDB) / 4);  // the header is all words and floats
    NativeSwap32s(base + cdb->m_verts, cdb->m_numVerts * 3);
    NativeSwap32s(base + cdb->m_tris.addr(), cdb->m_numTris * 4);  // 3 offsets + the bitfield word
    u8* p = base + cdb->m_triGroups.addr();
    for (s32 i = 0; i < cdb->m_numTriGroups; i++) {
        NativeSwap32s(p, 5);  // plane, count
        s32 count = ((CDBTriGroup*)p)->m_count;
        NativeSwap32s(p + 0x14, count);
        p += 0x14 + count * 4;
    }
    p = base + cdb->m_leaves.addr();
    for (s32 i = 0; i < cdb->m_numLeaves; i++) {
        NativeSwap32s(p, 0x44 / 4);  // type, box, count
        s32 count = ((CDBLeafNode*)p)->m_count;
        NativeSwap32s(p + 0x44, count);
        p += 0x44 + count * 4;
    }
    NativeSwap32s(base + cdb->m_branches.addr(), cdb->m_numBranches * (sizeof(CDBBranchNode) / 4));
    if (cdb->surfaceTypes.addr() != 0) {
        for (s32 i = 0; i < cdb->m_numSurfaceTypes; i++) {
            u8* s = base + cdb->surfaceTypes.addr() + i * sizeof(CDBSurfaceType);
            NativeSwap16(s + 0x2);
            NativeSwap16(s + 0x4);
            NativeSwap32(s + 0x8);
        }
    }
}
#endif

#endif
