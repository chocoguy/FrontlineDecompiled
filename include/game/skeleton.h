// Character skeleton files: loaded little-endian with offsets, fixed up in
// place by CharRelocateSkeleton. Hand-maintained.
#ifndef _GAME_SKELETON_H_
#define _GAME_SKELETON_H_

#include "types.h"
#include "game/endian.h"
#include "game/vector.h"
#include "game/orient.h"

class CMatrix;

#define CHARSKEL_RELOCATED 0x8000

// A node of the joint hierarchy (variable size).
struct CharJointHierarchy_t {
    /* 0x0 */ u8 numChildren;
    /* 0x1 */ u8 field_0x1;  // traversal stops once a subtree returns this value
    /* 0x2 */ u8 index;  // the joint
    /* 0x3 */ u8 unk_0x3;
    /* 0x4 */ CharJointHierarchy_t* parent;  // the root is its own parent
    /* 0x8 */ CharJointHierarchy_t* children[1];
};

// size 0x14
struct CharJoint_t {
    /* 0x0 */ CharJointHierarchy_t* node;
    /* 0x4 */ char name[0x10];
};

// An attach point (hand, head, vision cone...): a named offset from a
// joint (size 0x18).
struct CharSkelMatrixEntry_t {
    /* 0x00 */ char name[0x10];
    /* 0x10 */ u32 joint;
    /* 0x14 */ CMatrix* matrix;  // offset from the joint
};

// size 0x710
// A joint's bind offset and parent (in the skeleton header). size 0x10
struct CharSkelBone_t {
    /* 0x0 */ f32 offset[3];
    /* 0xC */ int parent;  // joints are in depth-first order
};

// A joint rotation in 16-bit angle units (y, z, x order of application).
struct CharOrient3_t {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
};

struct CharSkel_t {
    /* 0x000 */ u32 field_0x0;
    /* 0x004 */ u16 flags;  // CHARSKEL_RELOCATED once fixed up
    /* 0x006 */ u16 numJoints;
    /* 0x008 */ CharJointHierarchy_t* root;
    /* 0x00C */ CharJoint_t* joints;
    /* 0x010 */ u32 numMatrices;
    /* 0x014 */ CharSkelMatrixEntry_t* matrices;
    /* 0x018 */ u8 unk_0x18[0x8];
    union {
        /* 0x020 */ u32 table[0x50][4];
        CharSkelBone_t bones[0x50];
    };
    /* 0x520 */ s16 jointOffsets[0x50][3];
};

// Swaps the header and its fixed tables (not the data the offsets point at).
inline void EndianSwap(CharSkel_t& skel) {
    EndianSwap(skel.field_0x0);
    EndianSwap(skel.flags);
    EndianSwap(skel.numJoints);
    EndianSwap(*(u32*)&skel.root);
    EndianSwap(*(u32*)&skel.joints);
    EndianSwap(skel.numMatrices);
    EndianSwap(*(u32*)&skel.matrices);
    for (int i = 0; i < 0x50; i++) {
        EndianSwap(skel.table[i][0]);
        EndianSwap(skel.table[i][1]);
        EndianSwap(skel.table[i][2]);
        EndianSwap(skel.table[i][3]);
    }
    s16* offsets = &skel.jointOffsets[0][0];
    for (int i = 0; i < 0x28; i++, offsets += 6) {
        EndianSwap(offsets[0]);
        EndianSwap(offsets[1]);
        EndianSwap(offsets[2]);
        EndianSwap(offsets[3]);
        EndianSwap(offsets[4]);
        EndianSwap(offsets[5]);
    }
}

// Calls `fn` on each node, depth first. Returns the last subtree result.
int CharTraverseSkeleton(CharJointHierarchy_t* node, void (*fn)(CharJointHierarchy_t*, void*),
                         void* data);  // 0x800698E8
void CharRelocateSkeleton(CharSkel_t* skel);  // 0x80069468
// Index of the joint called `name`, or -1.
int CharSkelGetJointByName(CharSkel_t* skel, char* name);  // 0x800693DC

// A pose: per-joint data for a skeleton (header of the frame data).
struct CharFrame_t {
    /* 0x00 */ u32 numJoints;
    /* 0x04 */ u8 unk_0x4[0x4];
    /* 0x08 */ f32 pos[3];     // root translation not taken as root motion
    /* 0x14 */ u8 unk_0x14[0x4];
    /* 0x18 */ CVector3 floatPos;  // floating bone (AnimMotCalcFloatingBone)
    /* 0x28 */ long rot;       // root yaw not taken as root motion
    /* 0x2C */ Orient3_t floatRot;
    /* 0x38 */ s16* jointData;  // numJoints x 3
    /* 0x3C */ u8 unk_0x3C[0x4];
};

#endif
