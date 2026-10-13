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

// A collision-database vertex.
struct CDBVector {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
};

// A collision-database triangle. Contacts point at these through
// SClsnContact::m_pPoly (SClsnPoly is the same layout).
struct CDBTri {
    /* 0x0 */ CDBVector* m_verts[3];
    /* 0xC */ s32 m_surfaceType : 11;
    /* 0xC */ s32 m_texSwap : 11;  // compartment texture swap, -1 for none
    /* 0xC */ u32 m_passThru : 1;  // skipped while CCDBObject pass-through is disabled
    /* 0xC */ u32 m_lineSkip : 1;  // ignored by line tests unless the collision asks for every polygon
    /* 0xC */ u32 unk_0xF : 8;
};

// An oriented box in the collision database.
struct CDBBox {
    /* 0x00 */ f32 m_width;
    /* 0x04 */ f32 m_depth;
    /* 0x08 */ f32 m_height;
    /* 0x0C */ f32 m_basis[3][3];  // x, y and z axes
    /* 0x30 */ f32 m_center[3];
};

// Triangles sharing a plane (variable size: 0x14 + 4 * count).
struct CDBTriGroup {
    /* 0x00 */ f32 m_normal[3];
    /* 0x0C */ f32 m_d;
    /* 0x10 */ s32 m_count;  // negated while a query has visited the group
    /* 0x14 */ CDBTri* m_tris[1];
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
    /* 0x44 */ CDBTriGroup* m_groups[1];
};

// BSP split: children on the front (n.p >= d) and back sides.
struct CDBBranchNode : CDBNode {
    /* 0x04 */ CDBNode* m_front;
    /* 0x08 */ CDBNode* m_back;
    /* 0x0C */ f32 m_normal[3];
    /* 0x18 */ f32 m_d;
};

// A level's collision database (.cdb); pointers are file offsets until
// CCDBObject::Init relocates them.
struct CDB {
    /* 0x00 */ u8 unk_0x0[0x14];
    /* 0x14 */ s32 m_numTriGroups;
    /* 0x18 */ s32 m_numLeaves;
    /* 0x1C */ s32 m_numBranches;
    /* 0x20 */ u8 unk_0x20[0x4];
    /* 0x24 */ CDBTri* m_tris;
    /* 0x28 */ CDBTriGroup* m_triGroups;
    /* 0x2C */ CDBLeafNode* m_leaves;
    /* 0x30 */ CDBBranchNode* m_branches;  // the first is the root
    /* 0x34 */ CDBSurfaceType* surfaceTypes;
    /* 0x38 */ CDBBox m_bounds;
};

#endif
