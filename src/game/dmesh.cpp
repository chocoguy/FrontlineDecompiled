// Character meshes. Each part of a mesh is its own render bin: a drawn
// object queues a copy of its render state (lights, skin matrices, its
// projection) on each visible part, and the part's Link draws them as
// skinned display lists. Parts hang under one of two top-level bins, the
// world's and the first-person weapon's (which clears z first). Sleeve
// parts take a texture per object, through remap bins queued under them.
#include "game/dmesh.h"
#include "game/dmrender.h"
#include "game/dmcluster.h"
#include "game/renderlist.h"
#include "game/draw_context.h"
#include "game/camera.h"
#include "game/CFog.h"
#include "game/texture.h"
#include "game/texpack.h"
#include "game/matrix.h"
#ifdef FRONTLINE_NATIVE
#include "native/hooks.h"
#endif
#include "game/offsetptr.h"
#include "game/Screen.h"
#include "game/ShellMenu.h"
#include "game/Moh2.h"
#include "dolphin/gx.h"
#include "dolphin/os.h"
#include "string.h"

static int g_GCFogType[4] = {0, 2, 4, 5};  // GX_FOG_NONE, LIN, EXP, EXP2 by CFog type

static CDMRenderBin _DMRenderBin(3);
static CDMRenderBin _DMRenderBinWeapon(8);
static CFinishPartBin _finish_bin;
static CFinishPartBin _finish_bin_weapon;
static CRemapBin _remap_pool[32];
static CMatrix g_PerspectiveMatrix;

static CRenderList* _renderList;
static int _remap_next;
static CFog* g_pFog;
static f32 g_NearPlane;
static f32 g_FarPlane;

static bool _finish_draw = true;
static bool _finish_draw_weapon = true;

// What CPartBin::Render queues for each drawn object.
struct SPartPacket {
    DMRenderObject_T m_obj;
    CMatrix m_proj;
};

int CPartBin::Render(CDmaPacket& packet, void* data) {
    memcpy(packet.m_ptr, data, sizeof(DMRenderObject_T));
    packet.m_ptr += sizeof(DMRenderObject_T);
    *(CMatrix*)packet.m_ptr = g_PerspectiveMatrix;
    packet.m_ptr += sizeof(CMatrix);
    return 0;
}

void CPartBin::Link(CDmaTag* tag, CDmaPacket& packet) {
    GXLightObj lights[3];
    f32 nrm[3][4];
    f32 identity[3][4];
    f32 proj[7];
    GXColor fogColor;
    GXColor ambColor;
    GXColor matColor;
    u32 lightMask = 0;

    // Indexed s16 positions and normals, skinned by up to ten matrices.
    GXClearVtxDesc();
    GXSetVtxDesc(0, 1);  // GX_VA_PNMTXIDX, GX_DIRECT
    GXSetVtxAttrFmt(1, 9, 1, 3, 14);   // pos xyz s16
    GXSetVtxAttrFmt(1, 10, 0, 3, 15);  // nrm s16
    GXSetVtxDesc(13, 3);  // GX_VA_TEX0, GX_INDEX16
    GXSetVtxAttrFmt(1, 13, 1, 3, 14);  // st s16
    GXSetNumChans(1);
    GXSetNumTevStages(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 60, 0, 125);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 0);
    if (m_flags & DMESHPART_WEAPON) {
        GXSetFog(0, 0.0f, 0.0f, 0.0f, 0.0f, fogColor);
    } else {
        fogColor.r = g_pFog->m_color.r;
        fogColor.g = g_pFog->m_color.g;
        fogColor.b = g_pFog->m_color.b;
        fogColor.a = 0xFF;
        GXSetFog(g_GCFogType[g_pFog->m_enabled], g_pFog->m_start, g_pFog->m_range, g_NearPlane, g_FarPlane,
                 fogColor);
    }
    GXSetBlendMode(1, 4, 5, 5);
    GXSetAlphaCompare(7, 0, 0, 7, 0);
    GXSetZCompLoc(1);
    GXSetZMode(1, 3, 1);
    GXSetCullMode(0);

    for (; tag != NULL; tag = tag->m_next) {
        GXGetProjectionv(proj);
        SPartPacket* data = (SPartPacket*)tag->m_data;
        DMRenderObject_T* obj = &data->m_obj;
        CDMesh* mesh = obj->m_model;
        DMClusterObject_T* cluster = obj->m_cluster;
        float(*matrices)[3][4] = obj->m_clusterMatrices;
        GXSetProjection(data->m_proj.m, GX_PERSPECTIVE);
        // A morphed object draws its own copy of the list, with the
        // vertices inline.
        bool morphed = obj->m_parts[0].m_list != NULL;
        DCStoreRangeNoSync(matrices, cluster->m_numMatrices * sizeof(*matrices));
        if (morphed) {
            DCStoreRangeNoSync(obj->m_parts[0].m_list, (u16)obj->m_parts[0].m_listSize);
        }
        if (morphed) {
            GXSetVtxDesc(9, 1);
            GXSetVtxDesc(10, 1);
        } else {
            GXSetVtxDesc(9, 3);
            GXSetVtxDesc(10, 3);
        }
        GXSetArray(9, mesh->m_positions, 6);
        GXSetArray(10, mesh->m_normals, 6);
        GXSetArray(13, mesh->m_texCoords, 4);
        GXSetArray(21, matrices, 0x30);  // GX_POS_MTX_ARRAY
        GXInvalidateVtxCache();

        lightMask = 0;
        for (int i = 0; i < 3; i++) {
            f32* color = obj->m_lightColor[i];
            if (color[0] != 0.0f || color[1] != 0.0f || color[2] != 0.0f) {
                GXColor lightColor;
                lightColor.r = color[0];
                lightColor.g = color[1];
                lightColor.b = color[2];
                lightColor.a = obj->m_ambient[3];
                GXInitLightColor(&lights[i], lightColor);
                GXInitLightPos(&lights[i], obj->m_lightDir[i][0], obj->m_lightDir[i][1], obj->m_lightDir[i][2]);
                u32 light = 1 << i;
                GXLoadLightObjImm(&lights[i], light);
                lightMask |= light;
            }
        }
        ambColor.r = obj->m_ambient[0];
        ambColor.g = obj->m_ambient[1];
        ambColor.b = obj->m_ambient[2];
        ambColor.a = obj->m_ambient[3];
        GXSetChanAmbColor(4, ambColor);  // GX_COLOR0A0
        matColor.r = 0xFF;
        matColor.g = 0xFF;
        matColor.b = 0xFF;
        matColor.a = obj->m_ambient[3];
        GXSetChanMatColor(4, matColor);
        if (lightMask != 0) {
            GXSetChanCtrl(4, 1, 0, 0, lightMask, 2, 2);  // lit, clamped diffuse
        } else {
            GXSetChanMatColor(4, ambColor);
            GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
        }
        if (m_texture != NULL) {
            m_texture->Load(packet, NULL);
        }
        PPCSync();

        for (u32 i = 0; i < m_numLists; i++) {
            CDMeshDisplayList* list = &m_lists[i];
            for (int j = 0; j < list->numMatrices; j++) {
                u8 matrix = list->matrices[j];
                GXLoadPosMtxIndx(matrix, j * 3);
                PSMTXInvXpose(matrices[matrix], nrm);
                GXLoadNrmMtxImm(nrm, j * 3);
            }
            if (morphed) {
                GXCallDisplayList(obj->m_parts[0].m_list, (u16)obj->m_parts[0].m_listSize);
            } else {
                GXCallDisplayList(list->data, list->size << 5);
            }
        }
        GXSetProjectionv(proj);
    }

    GXSetChanCtrl(4, 0, 0, 1, lightMask, 0, 2);
    GXSetFog(0, 0.0f, 0.0f, 0.0f, 0.0f, fogColor);
    PSMTXIdentity(identity);
    for (int i = 0; i < 30; i += 3) {
        GXLoadNrmMtxImm(identity, i);
    }
}

bool CDMRenderBin::IsUsed() {
    return m_used;
}

void CDMRenderBin::Link(CDmaTag*, CDmaPacket&) {
    m_used = false;
    if (m_type == 8) {
        _finish_draw_weapon = true;
    }
    if (m_type != 8) {
        _finish_draw = true;
    }
    if (m_type == 8) {
        // Clear z so the weapon draws over the world.
        static const GXColor clear = {0, 0, 0, 0};
        GXSetZMode(1, 3, 1);
        GXSetColorUpdate(0);
        GXSetCopyClear(clear, 0xFFFFFF);
        GXCopyDisp(g_screen.m_pDrawBuffer, 1);
        GXSetColorUpdate(1);
#ifdef FRONTLINE_NATIVE
        NativeDrawClass(NATIVE_DRAW_VIEWMODEL);
#endif
    }
}

int CPart::Render(CDmaPacket&, void*) {
    return 0;
}

// Draws the part as if it had this bin's queue and texture.
void CRemapBin::Link(CDmaTag* tag, CDmaPacket& packet) {
    CRenderBin saved;
    m_part->m_texture = m_texture;
    static_cast<CRenderBinData&>(saved) = *m_part;
    static_cast<CRenderBinData&>(*m_part) = *this;
    m_part->Link(tag, packet);
    static_cast<CRenderBinData&>(*this) = *m_part;
    static_cast<CRenderBinData&>(*m_part) = saved;
}

int CRemapBin::Render(CDmaPacket& packet, void* data) {
    m_part->Render(packet, data);
    return 0;
}

void CPartBin::Init() {
    field_0xC = 0;
    m_count = 0;
    _remap_next = 0;
    m_pChildren = NULL;
}

void CDMesh::Draw(CDrawContext& ctx, DMRenderObject_T* obj) {
    g_pFog = (CFog*)ctx.field_0xD8;
    g_NearPlane = ((CCamera*)ctx.m_pCamera)->m_near;
    g_FarPlane = ((CCamera*)ctx.m_pCamera)->m_far;
    g_PerspectiveMatrix = *ctx.m_pProj;
    // Once per frame each top-level bin queues its finish bin.
    if (!(m_flags & DMESH_WEAPON)) {
        _DMRenderBin.m_used = true;
        if (_finish_draw) {
            _finish_draw = false;
            _renderList->Render(_finish_bin, NULL);
        }
    } else {
        _DMRenderBinWeapon.m_used = true;
        if (_finish_draw_weapon) {
            _finish_draw_weapon = false;
            _renderList->Render(_finish_bin_weapon, NULL);
        }
    }

    DMGeomPartData_T* data = obj->m_parts;
    for (u32 i = 0; i < (u32)obj->m_numParts; i++, data++) {
        CDMeshPart* part = &m_parts[data->m_part];
        if (part->m_flags & DMESHPART_SLEEVE) {
            CRemapBin* remap = (CRemapBin*)part->m_pChildren;
            while (remap != NULL && remap->m_texture != part->m_texture) {
                remap = (CRemapBin*)remap->m_pNext;
            }
            if (remap == NULL) {
                // No bounds check on the pool (32 per frame).
                remap = &_remap_pool[_remap_next++];
                remap->field_0xC = 0;
                remap->m_count = 0;
                remap->m_part = part;
                remap->m_texture = part->m_texture;
                remap->m_pPrev = NULL;
                remap->m_pNext = part->m_pChildren;
                if (part->m_pChildren != NULL) {
                    part->m_pChildren->m_pPrev = remap;
                }
                part->m_pChildren = remap;
            }
            _renderList->Render(*remap, obj);
        } else {
            _renderList->Render(*part, obj);
        }
    }
}

// Looks up each part's texture by name. The player's body and head
// textures (and in multiplayer the GI's) are left for the sleeve texture.
void CDMesh::BindTextures(CTexturePack* pack, char*) {
    for (u32 i = 0; i < m_numParts; i++) {
        char* name = m_textureNames[(u32)PTR_TO_ADDR(m_parts[i].m_texture)];
        bool body = stricmp(name, "mohf_body") == 0;
        bool head = stricmp(name, "mohf_head") == 0;
        bool gi = false;
        if (g_bInMultiplayerMode && stricmp(name, "gi_256") == 0) {
            gi = true;
        }
        if (!body && !head && !gi) {
            m_parts[i].m_texture = pack != NULL ? pack->FindTexture(name) : NULL;
            m_parts[i].m_flags &= ~DMESHPART_SLEEVE;
        } else {
            m_parts[i].m_flags |= DMESHPART_SLEEVE;
            if (gi) {
                m_parts[i].m_flags |= DMESHPART_SLEEVE;  // as the original: the same flag again
            }
        }
    }
    m_flags |= DMESH_TEXTURES_BOUND;
}

// Maps the mesh's joints to the skeleton's: the clusters' joints and the
// base pose (12.4 in the file) move to skeleton order.
void CDMesh::BindSkeleton(CharSkel_t* skel) {
    s8 jointMap[0x80];
    CharOrient3_t pose[130];

    if (m_flags & DMESH_SKELETON_BOUND) {
        return;
    }
    for (u32 i = 0; i < m_numJoints; i++) {
        int joint = CharSkelGetJointByName(skel, m_jointNames[i]);
        if (joint >= 0) {
            jointMap[i] = joint;
        } else {
            jointMap[i] = -1;
        }
    }
    for (u32 i = 0; i < (u32)m_numClusters; i++) {
        u8 joint = m_clusters[i].m_joint;
        if (jointMap[joint] == -1) {
            // Not fatal: the cluster then reads past the skeleton's joints.
            DebugMsg("ERROR: Skeleton node '%s' not found in skeleton for model '%s'\n", m_jointNames[joint], m_name);
        }
        m_clusters[i].m_joint = jointMap[m_clusters[i].m_joint];
        m_clusters[i].m_matrix = skel->joints[m_clusters[i].m_joint].node->parent->index;
    }
    if (field_0x5 == 1) {
        memcpy(pose, m_basePose, m_numJoints * sizeof(CharOrient3_t));
        for (u32 i = 0; i < m_numJoints; i++) {
            if (jointMap[i] != -1) {
                CharOrient3_t* dst = &m_basePose[jointMap[i]];
                dst->x = pose[i].x >> 4;
                dst->y = pose[i].y >> 4;
                dst->z = pose[i].z >> 4;
            }
        }
    }
    m_flags |= DMESH_SKELETON_BOUND;
}

// Relocates the loaded mesh, makes its parts render bins and links them
// under the world's or the weapon's bin.
void CDMesh::Init(bool weapon) {
    CDMeshPart* prev = NULL;
#ifdef FRONTLINE_NATIVE
    DMeshFromBigEndian(this);
#endif

    m_flags |= weapon ? DMESH_WEAPON : 0;
    if (m_flags & DMESH_INIT) {
        return;
    }
    int base = (int)PTR_TO_ADDR(this);
    offsetPtr(m_clusters, base);
    offsetPtr(m_parts, base);
#ifdef FRONTLINE_NATIVE
    // A CPartBin doesn't fit in the file's 0x38-byte part record natively, so
    // the parts are separate objects built from the records.
    {
        const DMeshPartRecord* records = (const DMeshPartRecord*)m_parts.get();
        CDMeshPart* parts = (CDMeshPart*)operator new(m_numParts * sizeof(CDMeshPart));
        for (u32 i = 0; i < m_numParts; i++) {
            CDMeshPart* part = new (&parts[i]) CPartBin;
            part->unk_0x20 = records[i].unk_0x20;
            part->m_flags = records[i].m_flags;
            part->m_texture = ADDR_TO_PTR(CTexture, records[i].m_texture);  // an index until BindTextures
            part->m_numLists = records[i].m_numLists;
            part->m_lists = records[i].m_lists.addr() != 0 ? ADDR_TO_PTR(CDMeshDisplayList, records[i].m_lists.addr() + base) : NULL;
            part->field_0x34 = records[i].field_0x34.addr() != 0 ? ADDR_TO_PTR(void, records[i].field_0x34.addr() + base) : NULL;
        }
        m_parts = parts;
    }
#endif
    offsetPtr(m_textureNames, base);
    offsetPtr(m_nodes, base);
    offsetPtr(m_variants, base);
    offsetPtr(m_partIndices, base);
    offsetPtr(m_jointNames, base);
    offsetPtr(m_basePose, base);
    offsetPtr(m_positions, base);
    offsetPtr(m_normals, base);
    offsetPtr(m_texCoords, base);
    DCStoreRange(m_positions, m_numPositions * 6);
    DCStoreRange(m_normals, m_numNormals * 6);
    DCStoreRange(m_texCoords, m_numTexCoords * 4);
    if (m_numParts > 1 && (m_parts[1].m_flags & DMESHPART_MORPH)) {
        m_flags |= DMESH_MORPH;
    }

    for (u32 i = 0; i < m_numParts; i++) {
#ifdef FRONTLINE_NATIVE
        CDMeshPart* part = &m_parts[i];  // built above
#else
        CDMeshPart* part = new (&m_parts[i]) CPartBin;
        offsetPtr(part->field_0x34, base);
        offsetPtr(part->m_lists, base);
#endif
        for (u32 j = 0; j < part->m_numLists; j++) {
            offsetPtr(part->m_lists[j].data, base);
        }
        if (!(m_flags & DMESH_MORPH)) {
            if (prev != NULL) {
                prev->m_pNext = part;
            }
            part->m_pPrev = prev;
            prev = part;
        }
        if (m_flags & DMESH_WEAPON) {
            part->m_flags |= DMESHPART_WEAPON;
        }
    }

    if (m_numParts != 0) {
        CRenderBin* next;
        if (weapon) {
            next = _DMRenderBinWeapon.m_pChildren;
            _DMRenderBinWeapon.m_pChildren = m_parts;
        } else {
            next = _DMRenderBin.m_pChildren;
            _DMRenderBin.m_pChildren = m_parts;
        }
        // Morph meshes only draw part 0.
        if (m_flags & DMESH_MORPH) {
            m_parts->m_pNext = next;
            if (next != NULL) {
                next->m_pPrev = m_parts;
            }
        } else {
            m_parts[m_numParts - 1].m_pNext = next;
            if (next != NULL) {
                next->m_pPrev = &m_parts[m_numParts - 1];
            }
        }
    }
    m_flags |= DMESH_INIT;
}

void CDMesh::InitClass(CRenderList* list) {
    _renderList = list;
    list->Register(_DMRenderBin);
    _renderList->Register(_DMRenderBinWeapon);
    _DMRenderBin.m_pChildren = &_finish_bin;
    _DMRenderBinWeapon.m_pChildren = &_finish_bin_weapon;
    _remap_next = 0;
}
