// dmcluster: skinning. The skeleton is traversed into joint matrices
// (kept in _scratchMats too), and each mesh cluster's matrix is built from
// a joint matrix with the cluster's blended rotation.
#include "game/dmcluster.h"
#include "game/dmesh.h"
#include "game/skeleton.h"
#include "game/matstack.h"
#include "game/matrix.h"
#include "game/trig.h"

static CMatrixStack* _dmeshStack;
// [0] the root, then the joint matrices of the last traversal.
static CMatrix _scratchMats[0x80];

static inline float AngleToRadians(long angle) {
    return (6.2831855f * (float)angle) / 65536.0f;
}

// Rotates the matrix's basis by y (`ry`, right/up), then z (`rz`,
// front/right), then x (`rx`, up/front); angles in 16-bit units.
static void _matrix_rot_yzx(CMatrix& m, long rx, long ry, long rz) {
    float ax = AngleToRadians(rx);
    float sx = MathSinf(ax);
    float ay = AngleToRadians(ry);
    float sy = MathSinf(ay);
    float az = AngleToRadians(rz);
    float sz = MathSinf(az);
    float cx = MathCosf(ax);
    float cy = MathCosf(ay);
    float cz = MathCosf(az);
    CVector3 right = *(CVector3*)m.m[0];
    CVector3 front = *(CVector3*)m.m[1];
    CVector3 up = *(CVector3*)m.m[2];
    CVector3 r, f, u;
    r.x = cy * right.x - sy * up.x;
    r.y = cy * right.y - sy * up.y;
    r.z = cy * right.z - sy * up.z;
    u.x = cy * up.x + sy * right.x;
    u.y = cy * up.y + sy * right.y;
    u.z = cy * up.z + sy * right.z;
    f.x = cz * front.x - sz * r.x;
    f.y = cz * front.y - sz * r.y;
    f.z = cz * front.z - sz * r.z;
    r.x = cz * r.x + sz * front.x;
    r.y = cz * r.y + sz * front.y;
    r.z = cz * r.z + sz * front.z;
    CVector3 f2, u2;
    f2.x = cx * f.x + sx * u.x;
    f2.y = cx * f.y + sx * u.y;
    f2.z = cx * f.z + sx * u.z;
    u2.x = cx * u.x - sx * f.x;
    u2.y = cx * u.y - sx * f.y;
    u2.z = cx * u.z - sx * f.z;
    m.SetRight(r);
    m.SetFront(f2);
    m.SetUp(u2);
}

void DMClusterBindSkeleton(DMClusterObject_T* cluster, CharSkel_t* skel) {
    cluster->m_skel = skel;
}

void DMClusterInitObject(DMClusterObject_T* cluster, float (*matrices0)[3][4], float (*matrices1)[3][4],
                         unsigned long count) {
    cluster->m_numMatrices = count;
    cluster->m_matrices[0] = matrices0;
    cluster->m_matrices[1] = matrices1;
}

// The frame's root transform, then the joints.
void DMClusterSkelTraverse(CharFrame_t* frame, CharSkel_t* skel, CMatrix* matrices, CVector3* min, CVector3* max) {
    _dmeshStack->SetCurrent();
    _dmeshStack->Reset();
    long yaw = frame->rot >> 8;
    CMatrix& top = _dmeshStack->m_stack[_dmeshStack->m_top];
    top.PreTranslate(*(CVector3*)frame->pos);
    _dmeshStack->m_stack[_dmeshStack->m_top].RotateY(0.0f);
    _dmeshStack->m_stack[_dmeshStack->m_top].RotateZ(AngleToRadians(yaw));
    _dmeshStack->m_stack[_dmeshStack->m_top].RotateX(0.0f);
    DMClusterSkelTraverseHeirarchy((CharOrient3_t*)frame->jointData, skel, matrices, min, max);
    g_matStack->SetCurrent();
}

// Builds each joint's matrix (into `matrices` and _scratchMats) and the
// bounds of the joint positions (a joint that lowers a minimum isn't
// checked against the maximum, as the original).
void DMClusterSkelTraverseHeirarchy(CharOrient3_t* orients, CharSkel_t* skel, CMatrix* matrices, CVector3* min,
                                    CVector3* max) {
    CharSkelBone_t* bone = skel->bones;
    u16 count = skel->numJoints;
    skel->bones[0].offset[2] = 0.0f;
    _dmeshStack->Store(&_scratchMats[0]);
    CMatrix* scratch = &_scratchMats[1];
    int depth = -1;
    float lo[4];
    float hi[4];
    lo[3] = lo[2] = lo[1] = lo[0] = 3.4028235e38f;
    hi[3] = hi[2] = hi[1] = hi[0] = -3.4028235e38f;
    while (count-- != 0) {
        s16 rx = orients->x;
        s16 ry = orients->y;
        s16 rz = orients->z;
        orients++;
        while (depth > bone->parent) {
            _dmeshStack->Pop();
            depth--;
        }
        CVector3 pos;
        pos.x = bone->offset[0];
        pos.y = bone->offset[1];
        pos.z = bone->offset[2];
        _dmeshStack->Push();
        depth++;
        _dmeshStack->m_stack[_dmeshStack->m_top].TransformPoint(pos, pos);
        CMatrix m;
        _dmeshStack->Store(&m);
        _matrix_rot_yzx(m, rx, ry, rz);
        m.SetPos(pos);
        _dmeshStack->Load(&m);
        if (lo[0] > pos.x) {
            lo[0] = pos.x;
        } else if (hi[0] < pos.x) {
            hi[0] = pos.x;
        }
        if (lo[1] > pos.y) {
            lo[1] = pos.y;
        } else if (hi[1] < pos.y) {
            hi[1] = pos.y;
        }
        if (lo[2] > pos.z) {
            lo[2] = pos.z;
        } else if (hi[2] < pos.z) {
            hi[2] = pos.z;
        }
        _dmeshStack->Store(matrices);
        *scratch = *matrices;
        bone++;
        matrices++;
        scratch++;
    }
    if (min != NULL) {
        min->x = lo[0];
        min->y = lo[1];
        min->z = lo[2];
    }
    if (max != NULL) {
        max->x = hi[0];
        max->y = hi[1];
        max->z = hi[2];
    }
}

// Writes buffer `buffer`'s cluster matrices (times the current matrix):
// each cluster's joint rotation is blended between the base pose and
// `orients` by its weight.
void DMClusterSynthesizeMatrices(unsigned long buffer, DMClusterObject_T* cluster, CDMesh* mesh,
                                 CharOrient3_t* orients) {
    float (*out)[3][4] = cluster->m_matrices[buffer];
    CharOrient3_t* base;
    if (mesh->field_0x5 == 1) {
        base = mesh->m_basePose;
    } else {
        base = (CharOrient3_t*)cluster->m_skel->jointOffsets;
    }
    CDMeshCluster* c = mesh->m_clusters;
    for (int n = mesh->m_numClusters; n > 0;) {
        u8 joint = c->m_joint;
        n--;
        u16 weight = c->m_weight;
        u8 matrix = c->m_matrix;
        c++;
        CharOrient3_t& b = base[joint];
        CharOrient3_t& o = orients[joint];
        long bx = b.x << 4;
        long by = b.y << 4;
        long bz = b.z << 4;
        s16 dx = o.x - bx;
        s16 dy = o.y - by;
        s16 dz = o.z - bz;
        CMatrix m;
        m = _scratchMats[1 + matrix];
        // (The products are shifted unsigned, as the original.)
        _matrix_rot_yzx(m, bx + ((u32)(dx * weight) >> 12), by + ((u32)(dy * weight) >> 12),
                        bz + ((u32)(dz * weight) >> 12));
        m.SetPos(*(CVector3*)_scratchMats[1 + joint].m[3]);
        CMatrix current;
        g_matStack->Store(&current);
        CMatrix result;
        result.Multiply(m, current);
        result.GetGAMECUBEMatrix34(*out);
        out++;
    }
}

void DMClusterShutdown() {
    delete _dmeshStack;
}

void DMClusterInit() {
    _dmeshStack = new CMatrixStack;
    _dmeshStack->Allocate(0x10);
}
