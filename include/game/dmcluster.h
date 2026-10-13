// dmcluster: skinning: skeleton traversal into joint matrices and the
// cluster (skin) matrices of DM meshes. Hand-maintained.
#ifndef _GAME_DMCLUSTER_H_
#define _GAME_DMCLUSTER_H_

#include "types.h"
#include "game_types.h"
#include "game/skeleton.h"

class CDMesh;
class CMatrix;
class CVector3;
struct CharFrame_t;
struct CharOrient3_t;
struct CharSkel_t;

// An object's cluster matrices (double-buffered) and skeleton. size 0x10
struct DMClusterObject_T {
    /* 0x0 */ u32 m_numMatrices;
    /* 0x4 */ float (*m_matrices[2])[3][4];
    /* 0xC */ CharSkel_t* m_skel;
};

// Free functions
// static void _matrix_rot_yzx(CMatrix&, long, long, long);  // 0x800F4E98
void DMClusterBindSkeleton(DMClusterObject_T*, CharSkel_t*);  // 0x800F5290
void DMClusterInitObject(DMClusterObject_T*, float(*)[3][4], float(*)[3][4], unsigned long);  // 0x800F5298
void DMClusterSkelTraverse(CharFrame_t*, CharSkel_t*, CMatrix*, CVector3*, CVector3*);  // 0x800F52A8
void DMClusterSkelTraverseHeirarchy(CharOrient3_t*, CharSkel_t*, CMatrix*, CVector3*, CVector3*);  // 0x800F53C0
void DMClusterSynthesizeMatrices(unsigned long, DMClusterObject_T*, CDMesh*, CharOrient3_t*);  // 0x800F561C
void DMClusterShutdown();  // 0x800F57B4
void DMClusterInit();  // 0x800F57DC

#endif
