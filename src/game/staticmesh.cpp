// staticmesh: static meshes and their drawing. Each Draw culls the mesh's
// node tree against the camera frustum, queues the visible chunks on their
// material bins and has the render list record them (model-view matrix,
// lights, chunk); Link later replays them as display lists. Mirrors the
// compartment code.
#include "game/staticmesh.h"

#include "game/renderlist.h"
#include "game/renderbin.h"
#include "game/draw_context.h"
#include "game/camera.h"
#include "game/CFog.h"
#include "game/light.h"
#include "game/bpdlight.h"
#include "game/matrix.h"
#include "game/shape.h"
#include "game/gxstore.h"
#include "game/gameconfig.h"
#include "game/offsetptr.h"
#include "dolphin/gx.h"
#include "dolphin/os.h"
#include "string.h"
#include "math.h"

static int g_GCFogType[4] = {0, 2, 4, 5};  // GX_FOG_NONE, LIN, EXP, EXP2 by CFog type

static CMatrix g_ActualWorldToCameraMatrix;  // the view
static CMatrix g_WorldToCameraMatrix;  // model-view of the mesh being drawn
static CMatrix g_ModelToScreenMatrix;
static CMatrix g_ModelToClipMatrix;
static CMatrix g_ClipToScreenMatrix;
static CVector3 g_CamPos;  // in the mesh's space
f32 g_LightParam[8][3];
static CMshSimpleBin g_MshSimpleBin(3);
static CMshSimpleBin g_MshSimpleAlphaBin(7);

static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static BPDLightVolume* g_pLightVolume;
static bool g_bAllowLighting;
static CDrawContext* g_pContext;
static CFrustum* g_pFrustum;
static CFog* g_pFog;
static const CMatrix* g_pModelToWorld;
bool g_bCacheLightParam;
static int g_CurrState;  // 1 opaque, 2 alpha: emitted before the next chunk
static CMSHMatBin* g_CurrMaterial;
static f32 g_NearPlane;
static f32 g_FarPlane;

static u32 s_endOfChunks = 0;

enum {
    LIGHT_AMBIENT = 6,
    // A chunk's record: model-view matrix, light parameters, the chunk.
    RECORD_LIGHTS = 12,
    RECORD_CHUNK = 12 + 24,
    RECORD_SIZE = RECORD_CHUNK + 1,
};

int CStaticAnimMesh::ticksPerFrame() const {
    return m_anim->m_ticksPerFrame;
}

int CStaticAnimMesh::numFrames() const {
    return m_anim->m_numFrames;
}

void CStaticAnimMesh::Init(void* data, bool relocate) {
    m_anim = (StaticAnimMesh*)data;
    if (relocate) {
        StaticAnimMesh* anim = m_anim;
#ifdef FRONTLINE_NATIVE
        StaticAnimMeshFromBigEndian(anim);
#endif
        offsetPtr(anim->m_frames, (int)PTR_TO_ADDR(anim));
        for (int i = 0; i < m_anim->m_numFrames; i++) {
            offsetPtr(m_anim->m_frames[i], (int)PTR_TO_ADDR(anim));
            m_pData = m_anim->m_frames[i];
            CStaticMesh::Init();
        }
    }
    m_pData = m_anim->m_frames[0];
}

// Draws frame `frame`.
void CStaticAnimMesh::Draw(CDrawContext& ctx, int frame) {
    m_pData = m_anim->m_frames[frame];
    CStaticMesh::Draw(ctx, frame);
    m_pData = m_anim->m_frames[0];
}

CStaticAnimMesh::~CStaticAnimMesh() {}

// Replays the recorded chunks.
void CMSHMatBin::Link(CDmaTag* tag, CDmaPacket&) {
    static GXLightObj light[3];
    static int CurIndexType = 0;  // 1 8-bit, 2 16-bit indices
    int i = 0;
    for (; tag != NULL; tag = tag->m_next, i = 0) {
        u32* data = (u32*)tag->m_data;
        u32 word = data[0];
        while (word != 0) {
            if (word == 1 || word == 2) {
                // Opaque (1) or alpha (2) state for the chunks that follow.
                GXClearVtxDesc();
                GXSetVtxAttrFmt(0, 9, 1, 4, 0);   // xyz f32
                GXSetVtxAttrFmt(0, 10, 0, 1, 0);  // normal s8
                GXSetVtxAttrFmt(0, 11, 1, 5, 0);  // rgba8
                GXSetVtxAttrFmt(0, 13, 1, 4, 0);  // st f32
                GXSetNumChans(1);
                GXSetNumTexGens(1);
                GXSetTexCoordGen2(0, 0, 4, 60, 0, 125);
                GXSetColorUpdate(1);
                GXSetNumTevStages(1);
                GXSetTevOp(0, 0);
                GXSetTevOrder(0, 0, 0, 4);
                GXColor fogColor;
                fogColor.r = g_pFog->m_color.r;
                fogColor.g = g_pFog->m_color.g;
                fogColor.b = g_pFog->m_color.b;
                fogColor.a = 0xFF;
                GXSetFog(g_GCFogType[g_pFog->m_enabled], g_pFog->m_start, g_pFog->m_range, g_NearPlane, g_FarPlane,
                         fogColor);
                if (word == 1) {
                    GXSetBlendMode(1, 1, 0, 5);
                    GXSetZCompLoc(0);
                    GXSetAlphaCompare(4, 0xA0, 0, 7, 0);
                } else {
                    GXSetBlendMode(1, 4, 5, 5);
                    GXSetZCompLoc(1);
                    GXSetAlphaCompare(7, 0, 0, 7, 0);
                }
                GXSetZMode(1, 3, word == 1);
                i++;
                g_CurrMaterial = NULL;
                CurIndexType = 0;
            }

            GXLoadPosMtxImm((f32(*)[4])&data[i], 0);
            f32(*params)[3] = (f32(*)[3])&data[i + RECORD_LIGHTS];
            if (!(m_flags & MSHMAT_UNLIT)) {
                u32 lightMask = 0;
                for (int n = 0; n < 3; n++) {
                    f32* color = params[3 + n];
                    if (color[0] != 0.0f || color[1] != 0.0f || color[2] != 0.0f) {
                        GXColor c;
                        c.r = (u8)(int)color[0];
                        c.g = (u8)(int)color[1];
                        c.b = (u8)(int)color[2];
                        c.a = 0xFF;
                        GXInitLightColor(&light[n], c);
                        GXInitLightPos(&light[n], params[n][0], params[n][1], params[n][2]);
                        u32 id = 1 << n;
                        GXLoadLightObjImm(&light[n], id);
                        lightMask |= id;
                    }
                }
                GXColor amb;
                amb.r = (u8)(int)params[LIGHT_AMBIENT][0];
                amb.g = (u8)(int)params[LIGHT_AMBIENT][1];
                amb.b = (u8)(int)params[LIGHT_AMBIENT][2];
                amb.a = 0xFF;
                GXSetChanAmbColor(4, amb);
                GXColor mat = {0xAC, 0xAC, 0xAC, 0xFF};
                GXSetChanMatColor(4, mat);
                if (lightMask != 0) {
                    GXSetChanCtrl(4, 1, 0, 0, lightMask, 2, 2);
                } else {
                    GXSetChanMatColor(4, amb);
                    GXSetChanCtrl(4, 0, 0, 0, 0, 0, 2);
                }
            } else {
                GXColor white = {0xFF, 0xFF, 0xFF, 0xFF};
                GXSetChanAmbColor(4, white);
                GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
            }

            MSHChunk* chunk = ADDR_TO_PTR(MSHChunk, data[i + RECORD_CHUNK]);
            MSHDisplayList* list = chunk->m_list;
            CMSHMatBin* material = chunk->m_material;
            if (list != NULL) {
                if (material->m_shape != NULL && material != g_CurrMaterial) {
                    g_CurrMaterial = material;
                    GXSetCullMode((material->m_flags & MSHMAT_TWO_SIDED) ^ 1);
                    if (material->m_hasTlut != 0) {
                        GXLoadTlut(&material->m_tlut, 0);
                    }
                    GXLoadTexObj(&material->m_texObj, 0);
                }
                if (list->m_flags & 1) {
                    if (CurIndexType != 1) {
                        GXSetVtxDesc(9, 2);   // GX_INDEX8
                        GXSetVtxDesc(10, 2);
                        GXSetVtxDesc(11, 2);
                        GXSetVtxDesc(13, 2);
                        CurIndexType = 1;
                    }
                } else if (CurIndexType != 2) {
                    GXSetVtxDesc(9, 3);  // GX_INDEX16
                    GXSetVtxDesc(10, 3);
                    GXSetVtxDesc(11, 3);
                    GXSetVtxDesc(13, 3);
                    CurIndexType = 2;
                }
                GXCallDisplayList(list->m_list, list->m_size);
            }
            // (The original steps one word past the next record, as here.)
            word = data[i + RECORD_SIZE];
            i += RECORD_SIZE + 1;
        }
    }
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    f32 view[3][4];
    g_ActualWorldToCameraMatrix.GetGAMECUBEMatrix34(view);
    GXLoadPosMtxImm(view, 0);
}

// Records this frame's chunks (the state marker first).
int CMSHMatBin::Render(CDmaPacket& packet, void*) {
    while (m_chunks != NULL) {
        if (g_CurrState != 0) {
            *(u32*)packet.m_ptr = g_CurrState;
            packet.m_ptr += 4;
            g_CurrState = 0;
        }
        f32 modelView[3][4];
        g_WorldToCameraMatrix.GetGAMECUBEMatrix34(modelView);
        memcpy(packet.m_ptr, modelView, sizeof(modelView));
        packet.m_ptr += sizeof(modelView);
        UpdateLightingParams(packet);
        *(PTR32(MSHChunk)*)packet.m_ptr = m_chunks;
        packet.m_ptr += 4;
        MSHChunk* chunk = m_chunks;
        m_chunks = chunk->m_next;
        chunk->m_next = NULL;
    }
    *(u32*)packet.m_ptr = s_endOfChunks;
    packet.m_ptr += 4;
    return 0;
}

void CMshSimpleBin::Init() {
    m_used = false;
    CUcodeRenderBin::Init();
}

bool CMshSimpleBin::IsUsed() {
    return m_used;
}

void CMshSimpleBin::Link(CDmaTag*, CDmaPacket&) {}

// Queues the materials that collected chunks.
static inline void RenderMaterials(CMSHMatBin* materials, int count, CMshSimpleBin& parent) {
    for (int i = 0; i < count; i++) {
        if (materials[i].m_chunks != NULL) {
            parent.m_used = true;
            g_pRenderList->Render(materials[i], NULL);
        }
    }
}

void CStaticMesh::Draw(CDrawContext& ctx, int) {
    if (g_config.noStaticMeshes) {
        return;
    }
    g_pContext = &ctx;
    g_bCacheLightParam = false;
    g_bAllowLighting = m_lighting;
    CCamera* camera = (CCamera*)ctx.m_pCamera;
    camera->GetPosition(g_CamPos);
    g_CamPos.Copy(ctx.m_pWorld->TransformToLocal(g_CamPos));
    g_NearPlane = ((CCamera*)ctx.m_pCamera)->m_near;
    g_FarPlane = ((CCamera*)ctx.m_pCamera)->m_far;
    g_pModelToWorld = ctx.m_pWorld;
    CMatrix modelView;
    modelView.Multiply(*g_pModelToWorld, *ctx.m_pView);
    g_WorldToCameraMatrix = modelView;
    g_ActualWorldToCameraMatrix = *ctx.m_pView;
    g_ModelToClipMatrix.Multiply(modelView, *ctx.m_pProj);
    g_ClipToScreenMatrix.Multiply(*ctx.m_pClip, *ctx.m_pScreen);
    g_ModelToScreenMatrix.Multiply(g_ModelToClipMatrix, g_ClipToScreenMatrix);
    g_pFrustum = &((CCamera*)ctx.m_pCamera)->m_frustum;
    g_pFog = (CFog*)ctx.field_0xD8;
    DrawNode(*m_pData->m_nodes, 0x3FF);
    g_CurrState = 1;
    RenderMaterials(m_pData->m_opaque, m_pData->m_numOpaque, g_MshSimpleBin);
    g_CurrState = 2;
    RenderMaterials(m_pData->m_alpha, m_pData->m_numAlpha, g_MshSimpleAlphaBin);
}

// Culls the node (moved to world space) against the frustum planes still
// straddled by its parent (`planes` bits), then queues its chunks or
// visits its children.
void CStaticMesh::DrawNode(MSHNode& node, unsigned long planes) {
    MSHNode world = node;
    g_pModelToWorld->TransformVector(world.m_axisX, node.m_axisX);
    g_pModelToWorld->TransformVector(world.m_axisY, node.m_axisY);
    g_pModelToWorld->TransformVector(world.m_axisZ, node.m_axisZ);
    g_pModelToWorld->TransformPoint(world.m_center, node.m_center);
    unsigned long clipped = 0;
    if (planes != 0) {
        for (int i = 0; i < FRUSTUM_NUM_PLANES; i++) {
            unsigned long bit = 1 << i;
            if (planes & bit) {
                const CPlane& plane = g_pFrustum->m_planes[i];
                f32 nx = plane.m_normal[0];
                f32 ny = plane.m_normal[1];
                f32 nz = plane.m_normal[2];
                CVector3 center = world.m_center;
                f32 radius = (f32)fabs(world.m_axisX.x * nx + world.m_axisX.y * ny + world.m_axisX.z * nz) +
                             (f32)fabs(world.m_axisY.x * nx + world.m_axisY.y * ny + world.m_axisY.z * nz) +
                             (f32)fabs(world.m_axisZ.x * nx + world.m_axisZ.y * ny + world.m_axisZ.z * nz);
                f32 dist = (nx * center.x + ny * center.y + nz * center.z) - plane.m_d;
                if (dist > radius) {
                    return;
                }
                if (dist >= -radius) {
                    clipped |= bit;
                }
            }
        }
    }
    node.m_drawFlags = 0;
    if (clipped & 0x3C1) {
        node.m_drawFlags |= MSHNODE_CLIPPED;
    }
    if (m_fogging) {
        node.m_drawFlags |= MSHNODE_FOGGED;
    }
    if (m_lighting) {
        node.m_drawFlags |= MSHNODE_LIT;
    }
    if (!m_lighting) {
        // (Permanently: the materials are shared by every user of the mesh.)
        for (int i = 0; i < m_pData->m_numOpaque; i++) {
            m_pData->m_opaque[i].m_flags |= MSHMAT_UNLIT;
        }
        for (int i = 0; i < m_pData->m_numAlpha; i++) {
            m_pData->m_alpha[i].m_flags |= MSHMAT_UNLIT;
        }
    }
    if (node.m_chunks != NULL) {
        if (!g_config.noStaticMeshes) {
            MSHChunk* chunk = node.m_chunks;
            for (int i = 0; i < node.m_numChunks; i++) {
                CMSHMatBin* material = chunk->m_material;
                chunk->m_next = material->m_chunks;
                material->m_chunks = chunk;
                chunk++;
            }
        }
    } else {
        if (node.m_children[0] != NULL) {
            DrawNode(*node.m_children[0], clipped);
        }
        if (node.m_children[1] != NULL) {
            DrawNode(*node.m_children[1], clipped);
        }
    }
}

// NULL for no level lighting.
void CStaticMesh::SetLightVolume(BPDLightVolume* volume) {
    g_pLightVolume = volume;
    g_bCacheLightParam = false;
    if (volume != NULL) {
        memset(g_LightParam, 0, sizeof(g_LightParam));
    }
}

void CStaticMesh::InitClass(CRenderList* renderList, CScreen* screen) {
    // (Never set, as the original.)
    static bool bReady = false;
    if (!bReady) {
        g_pScreen = screen;
        g_pRenderList = renderList;
        renderList->Register(g_MshSimpleBin);
        renderList->Register(g_MshSimpleAlphaBin);
        g_pLightVolume = NULL;
        memset(g_LightParam, 0, sizeof(g_LightParam));
    }
}

CStaticMesh::~CStaticMesh() {}

CStaticMesh::CStaticMesh(StaticMesh* data, bool relocate) {
    m_pData = data;
    m_lighting = 1;
    m_fogging = 1;
    if (relocate) {
        Init();
    }
}

// Builds a material bin in place and sets up its texture.
static inline void InitMaterial(CMSHMatBin* material, int base) {
#ifdef FRONTLINE_NATIVE
    // Built and relocated by InitMaterials.
    ShapeFile* file = ShapeFileFromBigEndian(material->m_shape);
#else
    new (material) CMSHMatBin;
    offsetPtr(material->m_shape, base);
    ShapeFile* file = material->m_shape;
#endif
    if (file != NULL) {
        Shape* shape = (Shape*)((u8*)file + file->firstShape);
        // The pixels (0x10 bytes in) must be 32-byte aligned.
        if (((u32)PTR_TO_ADDR(shape) & 0x10) == 0x10) {
            u8* palette;
            if (shape->palette == 0) {
                palette = NULL;
            } else {
                palette = (u8*)shape + shape->palette;
                u8 type = SHAPE_TYPE(palette);
                if (type != 0x20 && type != 0x21 && (u8)(type - 0x30) > 1) {
                    if (type != 0x32) {
                        palette = NULL;
                    }
                }
            }
            material->m_hasTlut = palette != NULL;
            switch (shape->format) {
            case 0x16:
            case 0x1E: {
                u32 format = 6;  // GX_TF_RGBA8
                if (shape->format == 0x1E) {
                    format = 0xE;  // GX_TF_CMPR
                }
                GXInitTexObj(&material->m_texObj, shape->data, (u16)shape->width, (u16)shape->height, format,
                             (material->m_flags & MSHMAT_WRAP) != 0, (material->m_flags >> 3) & 1, 0);
                break;
            }
            case 0x18:
            case 0x19: {
                u32 format = 9;  // GX_TF_C8
                if (shape->format == 0x18) {
                    format = 8;  // GX_TF_C4
                }
                GXInitTexObjCI(&material->m_texObj, shape->data, (u16)shape->width, (u16)shape->height, format,
                               (material->m_flags & MSHMAT_WRAP) != 0, (material->m_flags >> 3) & 1, 0, 0);
                GXInitTlutObj(&material->m_tlut, palette + 0x10, 2, (u16)((Shape*)palette)->width);
                break;
            }
            default:
                OSReport("Unknown shape type\n");
                break;
            }
        }
    }
}

#ifdef FRONTLINE_NATIVE
// Natively a CMSHMatBin doesn't fit in the file's 0x70-byte record, so the
// materials are separate objects built from the records; chunks that point at
// a record are pointed at its object after relocation.
struct NativeMaterials {
    u32 records;  // address of the first record
    int count;
    CMSHMatBin* objects;
};
static NativeMaterials s_nativeOpaque, s_nativeAlpha;

static CMSHMatBin* NativeMaterialFor(PTR32(CMSHMatBin) record) {
    const NativeMaterials* sets[2] = {&s_nativeOpaque, &s_nativeAlpha};
    for (int s = 0; s < 2; s++) {
        u32 offset = record.addr() - sets[s]->records;
        if (offset < (u32)sets[s]->count * sizeof(MSHMaterialRecord)) {
            return &sets[s]->objects[offset / sizeof(MSHMaterialRecord)];
        }
    }
    return record;
}

// Builds the materials and links them as siblings; returns the array.
static CMSHMatBin* InitMaterials(PTR32(CMSHMatBin) records, int count, NativeMaterials& set, int base) {
    set.records = records.addr();
    set.count = count;
    set.objects = count != 0 ? (CMSHMatBin*)operator new(count * sizeof(CMSHMatBin)) : NULL;
    CMSHMatBin* prev = NULL;
    for (int i = 0; i < count; i++) {
        const MSHMaterialRecord* record = (const MSHMaterialRecord*)records.get() + i;
        CMSHMatBin* material = new (&set.objects[i]) CMSHMatBin;
        material->m_flags = record->m_flags;
        material->m_shape =
            record->m_shape.addr() != 0 ? ADDR_TO_PTR(ShapeFile, record->m_shape.addr() + base) : NULL;
        InitMaterial(material, base);
        if (prev != NULL) {
            prev->m_pNext = material;
        }
        material->m_pPrev = prev;
        prev = material;
    }
    return set.objects;
}
#else
// Builds the materials and links them as siblings; returns the last.
static inline CMSHMatBin* InitMaterials(CMSHMatBin* materials, int count, int base) {
    CMSHMatBin* prev = NULL;
    CMSHMatBin* material = materials;
    for (int i = 0; i < count; i++) {
        if (material != NULL) {
            InitMaterial(material, base);
        }
        if (prev != NULL) {
            prev->m_pNext = material;
        }
        material->m_pPrev = prev;
        prev = material;
        material++;
    }
    return prev;
}
#endif

// Relocates a display list and writes its array setup commands.
static inline void InitDisplayList(MSHDisplayList* list, int base) {
    offsetPtr(list->m_positions, base);
    offsetPtr(list->m_normals, base);
    offsetPtr(list->m_texCoords, base);
    offsetPtr(list->m_colors, base);
    offsetPtr(list->m_list, base);
    u8* cmd = (u8*)list->m_list;
    // CP array bases (physical) and strides: position, normal, color, tex0.
    cmd[0x00] = 8;
    cmd[0x01] = 0xA0;
    GX_STORE32(cmd + 0x02, (u32)PTR_TO_ADDR(list->m_positions) & 0x3FFFFFFF);
    cmd[0x06] = 8;
    cmd[0x07] = 0xB0;
    GX_STORE32(cmd + 0x08, 12);
    cmd[0x0C] = 8;
    cmd[0x0D] = 0xA1;
    GX_STORE32(cmd + 0x0E, (u32)PTR_TO_ADDR(list->m_normals) & 0x3FFFFFFF);
    cmd[0x12] = 8;
    cmd[0x13] = 0xB1;
    GX_STORE32(cmd + 0x14, 3);
    cmd[0x18] = 8;
    cmd[0x19] = 0xA2;
    GX_STORE32(cmd + 0x1A, (u32)PTR_TO_ADDR(list->m_colors) & 0x3FFFFFFF);
    cmd[0x1E] = 8;
    cmd[0x1F] = 0xB2;
    GX_STORE32(cmd + 0x20, 4);
    cmd[0x24] = 8;
    cmd[0x25] = 0xA4;
    GX_STORE32(cmd + 0x26, (u32)PTR_TO_ADDR(list->m_texCoords) & 0x3FFFFFFF);
    cmd[0x2A] = 8;
    cmd[0x2B] = 0xB4;
    GX_STORE32(cmd + 0x2C, 8);
    // Then the triangles (a strip or a list) of m_size vertices.
    cmd[0x30] = 0;
    cmd[0x31] = (list->m_flags & 4) ? 0x98 : 0x90;
    GX_STORE16(cmd + 0x32, list->m_size);
    int indexSize = 8;
    if (list->m_flags & 1) {
        indexSize = 4;
    }
    int size = list->m_size * indexSize + 0x34;
    if (size & 0x1F) {
        size += 0x20 - (size & 0x1F);
    }
    list->m_size = size;
    DCFlushRange(list->m_list, 0x34);
}

// Relocates the mesh file, builds its materials and links them into the
// ucode bins.
void CStaticMesh::Init() {
#ifdef FRONTLINE_NATIVE
    StaticMeshFromBigEndian(m_pData);
#endif
    int base = (int)PTR_TO_ADDR(m_pData);
    offsetPtr(m_pData->m_opaque, base);
    offsetPtr(m_pData->m_alpha, base);
    offsetPtr(m_pData->m_chunks, base);
    offsetPtr(m_pData->m_nodes, base);
    offsetPtr(m_pData->m_hardPoints, base);
#ifdef FRONTLINE_NATIVE
    m_pData->m_opaque = InitMaterials(m_pData->m_opaque, m_pData->m_numOpaque, s_nativeOpaque, base);
    m_pData->m_alpha = InitMaterials(m_pData->m_alpha, m_pData->m_numAlpha, s_nativeAlpha, base);
#else
    InitMaterials(m_pData->m_opaque, m_pData->m_numOpaque, base);
    InitMaterials(m_pData->m_alpha, m_pData->m_numAlpha, base);
#endif
    MSHChunk* chunk = m_pData->m_chunks;
    for (int i = 0; i < m_pData->m_numChunks; i++) {
        offsetPtr(chunk->m_node, base);
        offsetPtr(chunk->m_material, base);
#ifdef FRONTLINE_NATIVE
        chunk->m_material = NativeMaterialFor(chunk->m_material);
#endif
        offsetPtr(chunk->m_list, base);
        if (chunk->m_list != NULL) {
            InitDisplayList(chunk->m_list, base);
        }
        chunk++;
    }
    MSHNode* node = m_pData->m_nodes;
    for (int i = 0; i < m_pData->m_numNodes; i++) {
        offsetPtr(node->m_children[0], base);
        offsetPtr(node->m_children[1], base);
        offsetPtr(node->m_chunks, base);
        node++;
    }
    // Prepend the materials to the ucode bins' children.
    if (m_pData->m_opaque != NULL) {
        CRenderBin* rest = g_MshSimpleBin.m_pChildren;
        g_MshSimpleBin.m_pChildren = m_pData->m_opaque;
        m_pData->m_opaque[m_pData->m_numOpaque - 1].m_pNext = rest;
    }
    if (m_pData->m_alpha != NULL) {
        CRenderBin* rest = g_MshSimpleAlphaBin.m_pChildren;
        g_MshSimpleAlphaBin.m_pChildren = m_pData->m_alpha;
        m_pData->m_alpha[m_pData->m_numAlpha - 1].m_pNext = rest;
    }
}

void CStaticMesh::Init(void* data, bool relocate) {
    m_pData = (StaticMesh*)data;
    if (relocate) {
        Init();
    }
}

// Writes the chunk's light parameters (computed once per mesh draw): the
// light volume's directional lights (into the mesh's space) and ambient,
// plus the strongest nearby dynamic light in place of the third.
void UpdateLightingParams(CDmaPacket& packet) {
    if (g_pLightVolume != NULL && g_bAllowLighting) {
        if (!g_bCacheLightParam) {
            g_bCacheLightParam = true;
            CMatrix worldToModel;
            worldToModel.FastInverse(*g_pModelToWorld);
            int numLights = 0;
            BPDLight* light = g_pLightVolume->lights;
            f32(*param)[3] = g_LightParam;
            for (int i = 0; i < g_pLightVolume->numLights; i++) {
                switch (light->type) {
                case BPD_LIGHT_AMBIENT:
                    g_LightParam[LIGHT_AMBIENT][0] = 255.0f * (2.0f * light->color[0]) * light->intensity;
                    g_LightParam[LIGHT_AMBIENT][1] = 255.0f * (2.0f * light->color[1]) * light->intensity;
                    g_LightParam[LIGHT_AMBIENT][2] = 255.0f * (2.0f * light->color[2]) * light->intensity;
                    break;
                case BPD_LIGHT_DIRECTIONAL: {
                    // (No limit of three, as the original.)
                    CVector3 dir;
                    dir.x = light->dir[0];
                    dir.y = light->dir[1];
                    dir.z = light->dir[2];
                    CVector3 local;
                    worldToModel.TransformVector(local, dir);
                    numLights++;
                    param[0][0] = local.x;
                    param[0][1] = local.y;
                    param[0][2] = local.z;
                    param[3][0] = 255.0f * light->color[0] * light->intensity;
                    param[3][1] = 255.0f * light->color[1] * light->intensity;
                    param[3][2] = 255.0f * light->color[2] * light->intensity;
                    param++;
                    break;
                }
                }
                light++;
            }

            CLightDrawList* lights = g_pContext->m_pLights;
            if (lights != NULL && lights->count != 0) {
                CVector3 pos = *(const CVector3*)g_pContext->m_pWorld->m[3];
                CLight* best = NULL;
                f32 bestWeight = 0.0f;
                for (CLight** it = lights->items; it != g_pContext->m_pLights->items + g_pContext->m_pLights->count;
                     it++) {
                    CVector3 d;
                    (*it)->GetPosition(d);
                    d.x = d.x - pos.x;
                    d.y = d.y - pos.y;
                    d.z = d.z - pos.z;
                    CLight* l = *it;
                    f32 sqrDist = d.x * d.x + d.y * d.y + d.z * d.z;
                    f32 sqrRadius = l->m_radius * l->m_radius;
                    if (sqrDist < sqrRadius) {
                        f32 weight = 1.0f - sqrDist / sqrRadius;
                        if (weight > bestWeight) {
                            bestWeight = weight;
                            best = l;
                        }
                    }
                }
                if (best != NULL) {
                    if (numLights >= 3) {
                        numLights = 2;
                    }
                    CVector3 dir;
                    best->GetPosition(dir);
                    dir.x = dir.x - pos.x;
                    dir.y = dir.y - pos.y;
                    dir.z = dir.z - pos.z;
                    dir.Normalize();
                    f32* p = g_LightParam[numLights];
                    p[0] = dir.x;
                    p[1] = dir.y;
                    p[2] = dir.z;
                    p[9] = bestWeight * best->m_color[0];
                    p[10] = bestWeight * best->m_color[1];
                    p[11] = bestWeight * best->m_color[2];
                }
            }
        }
        memcpy(packet.m_ptr, g_LightParam, sizeof(g_LightParam));
    } else {
        memset(packet.m_ptr, 0, sizeof(g_LightParam));
    }
    packet.m_ptr += sizeof(g_LightParam);
}
