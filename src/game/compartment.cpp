// Compartments: the level's static geometry (.cpt) and collision (.cdb).
// Each frame the compartment BSP is frustum-culled, nodes pick up to three
// nearby lights, and their chunks queue on the material bins, which draw
// them as display lists through the "simple" ucode bins. Also character
// shadows on the geometry, animated texture swaps and the shared
// compartment whose materials others reuse.
#include "game/compartment.h"

#include "game/memory.h"
#include "game/cdbobject.h"
#include "game/cdbgeom.h"
#include "game/box.h"
#include "game/collision.h"
#include "game/camera.h"
#include "game/CFog.h"
#include "game/light.h"
#include "game/shadow.h"
#include "game/explosion.h"
#include "game/draw_context.h"
#include "game/renderlist.h"
#include "game/matrix.h"
#include "game/shape.h"
#include "game/gxstore.h"
#include "game/texture.h"
#include "game/surfacetype.h"
#include "game/gameconfig.h"
#include "game/toilet.h"
#include "game/debug_prof.h"
#include "game/offsetptr.h"
#include "game/CSPRStack.h"
#include "dolphin/gx.h"
#include "dolphin/os.h"

// The free list of light links handed to nodes each frame.
struct CLightPool {
    CLightPool() : m_data(NULL), m_used(NULL), m_free(NULL), m_capacity(0), m_count(0) {}
    void Reset() {
        m_used = NULL;
        m_free = m_data;
        m_count = 0;
        m_data[m_capacity - 1].m_poolNext = NULL;
        for (int i = 0; i < m_capacity - 1; i++) {
            m_free[i].m_poolNext = &m_free[i + 1];
        }
    }
    void Init() {
        m_capacity = 0x100;
        m_data = m_links;
        Reset();
    }
    CLightLink* Alloc() {
        CLightLink* link = m_free;
        if (link == NULL) {
            return NULL;
        }
        m_free = link->m_poolNext;
        link->m_poolNext = m_used;
        m_used = link;
        m_count++;
        return link;
    }

    /* 0x000 */ CLightLink m_links[0x100];
    /* 0xC00 */ CLightLink* m_data;
    /* 0xC04 */ CLightLink* m_used;
    /* 0xC08 */ CLightLink* m_free;
    /* 0xC0C */ int m_capacity;
    /* 0xC10 */ int m_count;
};

// Constructed but otherwise unused in the retail game.
struct STextureCache {
    STextureCache() : m_head(NULL), m_tail(NULL), m_count(0) {}
    void* m_head;
    void* m_tail;
    int m_count;
};

static int g_GCFogType[4] = {0, 2, 4, 5};  // GX_FOG_NONE, LIN, EXP, EXP2 by CFog type

static CMatrix g_ModelToScreenMatrix;
static CMatrix g_ModelToClipMatrix;
static CMatrix g_ClipToScreenMatrix;
static f32 g_WorldToCameraGCMatrix[3][4];
static CVector3 g_CamPos;
static CCompartment g_GlobalCpt;  // the shared compartment
static CCptSimpleBin g_CptSimpleBin(2);
static CCptSimpleBin g_CptSimpleAlphaBin(4);
static CCptShadowCleanUpBin g_CptShadowCleanUpBin;
static CCptShadowBin g_CptShadowBin(&g_CptShadowCleanUpBin);
static CTextureSwapCache g_TextureSwapCache;
static STextureCache g_TextureCache;
static CLightPool g_LightPool;
static CSPRStack g_SPRStack;

static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static f32 g_NearClip;
static CFrustum* g_pFrustum;
static CFog* g_pFog;
static f32 g_MipMapBias;
static bool g_bMipMapScale;
static bool g_bShadowSet;
static CCharacterShadow* g_pCurrentShadow;
static int g_SharedCompartmentSize;
bool g_bCPTFogDisabled;
bool g_bCPTForceClipped;
bool g_bCPTDisableLighting;
bool g_bDisableMipMapBias;
int g_TexturesCached;
static int g_CurrState;  // 1 opaque, 2 alpha: emitted before the next chunk
static CCPTMatBin* g_CurrMaterial;
static f32 g_NearPlane;
static f32 g_FarPlane;

static u32 s_endOfChunks = 0;

void CCPTMatBin::Link(CDmaTag* tag, CDmaPacket&) {
    for (; tag != NULL; tag = tag->m_next) {
        PTR32(CPTChunk)* data = (PTR32(CPTChunk)*)tag->m_data;
        int i = 0;
        CPTChunk* chunk = data[i++];
        while (chunk != NULL) {
            if ((u32)PTR_TO_ADDR(chunk) == 1 || (u32)PTR_TO_ADDR(chunk) == 2) {
                // Opaque (1) or alpha (2) state for the chunks that follow.
                GXLoadPosMtxImm(g_WorldToCameraGCMatrix, 0);
                GXClearVtxDesc();
                GXSetVtxDesc(9, 2);   // GX_VA_POS, GX_INDEX8
                GXSetVtxDesc(10, 2);  // GX_VA_NRM
                GXSetVtxDesc(11, 2);  // GX_VA_CLR0
                GXSetVtxDesc(13, 2);  // GX_VA_TEX0
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
                if ((u32)PTR_TO_ADDR(chunk) == 1) {
                    GXSetBlendMode(1, 1, 0, 5);
                    GXSetZCompLoc(0);
                    GXSetAlphaCompare(4, 0xA0, 0, 7, 0);
                    GXSetZMode(1, 3, 1);
                } else {
                    GXSetBlendMode(1, 4, 5, 5);
                    GXSetZCompLoc(1);
                    GXSetAlphaCompare(7, 0, 0, 7, 0);
                    GXSetZMode(1, 3, 0);
                }
                g_CurrMaterial = NULL;
                chunk = data[i++];
            }

            // The node's lights (in camera space).
            int numLights = 0;
            u32 lightMask = 0;
            int nrmId = 0;
            for (CLightLink* link = chunk->m_node->m_lights; link != NULL; link = link->m_next) {
                CLight* light = link->m_light;
                GXColor color;
                color.r = (u8)(int)light->m_color[0];
                color.g = (u8)(int)light->m_color[1];
                color.b = (u8)(int)light->m_color[2];
                color.a = (u8)(int)light->m_color[3];
                GXInitLightColor(&light->m_lightObj, color);
                CVector3 pos;
                light->GetPosition(pos);
                f32 camPos[3];
                PSMTXMultVec(g_WorldToCameraGCMatrix, &pos.x, camPos);
                GXLoadNrmMtxImm(g_WorldToCameraGCMatrix, nrmId);
                GXInitLightPos(&light->m_lightObj, camPos[0], camPos[1], camPos[2]);
                GXInitLightDistAttn(&light->m_lightObj, 0.5f * light->m_radius, 0.4f, 3);  // GX_DA_MEDIUM
                u32 id = 1 << numLights;
                GXLoadLightObjImm(&light->m_lightObj, id);
                lightMask |= id;
                nrmId += 3;
                numLights++;
            }
            GXColor amb = {0x80, 0x80, 0x80, 0xFF};
            GXSetChanAmbColor(4, amb);
            GXColor mat = {0xFF, 0xFF, 0xFF, 0xFF};
            GXSetChanMatColor(4, mat);
            if (lightMask != 0) {
                GXSetChanCtrl(4, 1, 1, 0, lightMask, 2, 1);
            } else {
                GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
            }

            CPTDisplayList* list = chunk->m_list;
            CCPTMatBin* material = chunk->m_material;
            if (list != NULL) {
                if (material != g_CurrMaterial) {
                    g_CurrMaterial = material;
                    if (material->m_shape != NULL) {
                        GXSetNumTexGens(1);
                        GXSetTevOp(0, 0);
                        GXSetTevOrder(0, 0, 0, 4);
                        GXSetTevColorOp(0, 0, 0, 0, 1, 0);
                        GXSetCullMode((material->m_flags & CPTMAT_TWO_SIDED) ^ 1);
                        if (material->m_hasTlut != 0) {
                            GXLoadTlut(&material->m_tlut, 0);
                        }
                        GXLoadTexObj(&material->m_texObj, 0);
                    } else {
                        GXSetNumTexGens(0);
                        GXSetTevOp(0, 4);
                        GXSetTevOrder(0, 0xFF, 0xFF, 4);
                        GXSetTevColorOp(0, 0, 0, 3, 1, 0);
                    }
                }
                static int prevVertFormat = 0;
                int format = list->m_flags & 1;
                if (format != prevVertFormat) {
                    if (format != 1) {
                        GXSetVtxDesc(9, 2);
                        GXSetVtxDesc(10, 2);
                        GXSetVtxDesc(11, 2);
                        GXSetVtxDesc(13, 2);
                    } else {
                        GXSetVtxDesc(9, 3);  // GX_INDEX16
                        GXSetVtxDesc(10, 3);
                        GXSetVtxDesc(11, 3);
                        GXSetVtxDesc(13, 3);
                    }
                    prevVertFormat = format;
                }
                GXCallDisplayList(list->m_list, list->m_size);
            }

            f32 identity[3][4];
            PSMTXIdentity(identity);
            for (int id = 0; id < nrmId; id += 3) {
                GXLoadNrmMtxImm(identity, id);
            }
            chunk = data[i++];
        }
    }
}

// Queues this frame's chunks into the packet (the state marker first).
int CCPTMatBin::Render(CDmaPacket& packet, void*) {
    while (m_chunks != NULL) {
        if (g_CurrState != 0) {
            *(u32*)packet.m_ptr = g_CurrState;
            packet.m_ptr += 4;
            g_CurrState = 0;
        }
        *(PTR32(CPTChunk)*)packet.m_ptr = m_chunks;
        packet.m_ptr += 4;
        CPTChunk* chunk = m_chunks;
        m_chunks = chunk->m_next;
        chunk->m_next = NULL;
    }
    *(u32*)packet.m_ptr = s_endOfChunks;
    packet.m_ptr += 4;
    return 0;
}

CTextureSwapCache::~CTextureSwapCache() {}

bool CCptShadowCleanUpBin::IsUsed() {
    return g_CptShadowBin.m_count != 0;
}

void CCptShadowCleanUpBin::Link(CDmaTag*, CDmaPacket&) {}

void CCptShadowBin::Link(CDmaTag*, CDmaPacket&) {}

int CCptShadowBin::Render(CDmaPacket&, void*) {
    return 0;
}

void CCptSimpleBin::Init() {
    if (m_type == 2) {
        g_LightPool.Reset();
    }
    m_used = false;
    CUcodeRenderBin::Init();
}

bool CCptSimpleBin::IsUsed() {
    return m_used;
}

void CCptSimpleBin::Link(CDmaTag*, CDmaPacket&) {}

static inline void SetNodeBox(CVolBox& box, const CPTNode& node) {
    box.SetBasis(node.m_axisX, node.m_axisY, node.m_axisZ);
    box.SetCenter(node.m_center);
    box.SetWidth(2.0f * node.m_axisX.w);
    box.SetDepth(2.0f * node.m_axisY.w);
    box.SetHeight(2.0f * node.m_axisZ.w);
}

static bool IntersectNode(CPTNode& node, CCharacterShadow* shadow) {
    CVolBox box;
    SetNodeBox(box, node);
    CCollision collision;
    if (box.TestCollision(shadow->GetBox(), collision, false) == CLSN_NONE) {
        return false;
    }
    return true;
}

void CCompartment::DrawNodeShadows(CPTNode& node, CCharacterShadow& shadow) {
    if (node.m_visible && IntersectNode(node, &shadow)) {
        if (node.m_chunks != NULL) {
            g_pRenderList->Render(g_CptShadowBin, &node);
        }
        if (node.m_children[0] != NULL) {
            DrawNodeShadows(*node.m_children[0], shadow);
        }
        if (node.m_children[1] != NULL) {
            DrawNodeShadows(*node.m_children[1], shadow);
        }
    }
}

int CCompartment::IsVisible(CDrawContext&) const {
    return 1;
}

int CCompartment::IsDrawEnabled() const {
    return m_drawEnabled;
}

void CCompartment::SetShadowTexture(CTexture& texture) {
    g_CptShadowBin.m_texture = &texture;
}

void CCompartment::DrawShadows(CDrawContext& ctx) {
    if (!g_config.noCharacterShadows) {
        CShadowDrawList* shadows = ctx.m_pShadows;
        for (CCharacterShadow** it = shadows->items; it != shadows->items + shadows->count; it++) {
            g_bShadowSet = false;
            g_pCurrentShadow = *it;
            DrawNodeShadows(m_header->m_nodes[0], **it);
        }
    }
}

// Queues the materials that collected chunks (opaque then alpha).
static inline void RenderMaterials(CCPTMatBin* materials, int count, CCptSimpleBin& parent) {
    for (int i = 0; i < count; i++) {
        if (materials[i].m_chunks != NULL) {
            parent.m_used = true;
            g_pRenderList->Render(materials[i], NULL);
        }
    }
}

void CCompartment::Draw(CDrawContext& ctx) {
    CCamera* camera = (CCamera*)ctx.m_pCamera;
    camera->GetPosition(g_CamPos);
    g_NearClip = ((CCamera*)ctx.m_pCamera)->m_near;
    ctx.m_pView->GetGAMECUBEMatrix34(g_WorldToCameraGCMatrix);
    g_ModelToClipMatrix.Multiply(*ctx.m_pView, *ctx.m_pProj);
    g_ClipToScreenMatrix.Multiply(*ctx.m_pClip, *ctx.m_pScreen);
    g_ModelToScreenMatrix.Multiply(g_ModelToClipMatrix, g_ClipToScreenMatrix);
    g_pFrustum = &((CCamera*)ctx.m_pCamera)->m_frustum;
    g_pFog = (CFog*)ctx.field_0xD8;
    g_NearPlane = ((CCamera*)ctx.m_pCamera)->m_near;
    g_FarPlane = ((CCamera*)ctx.m_pCamera)->m_far;
    static f32 g_DefaultHW = ((CCamera*)ctx.m_pCamera)->m_xScale;
    // Zoomed views (a changed field of view) scale the mipmap bias.
    g_bMipMapScale = ((CCamera*)ctx.m_pCamera)->m_xScale != g_DefaultHW;
    g_MipMapBias = 1.0f;
    if (g_bDisableMipMapBias) {
        g_bMipMapScale = false;
        g_MipMapBias = 0.0f;
    }
    g_TexturesCached = 0;
    if (!g_config.field_0x1) {
        PROFILE_BEGIN(PROFBIN_CPT_DRAW);
        PROFILE_BEGIN(PROFBIN_CPT_NODES);
        DrawNode(m_header->m_nodes[0], 0x3FF, *(dwi::fast_vec<CLight*>*)ctx.m_pLights);
        PROFILE_END(PROFBIN_CPT_NODES);
        PROFILE_BEGIN(PROFBIN_CPT_BINS);
        g_CurrState = 1;
        RenderMaterials(m_header->m_opaque, m_header->m_numOpaque, g_CptSimpleBin);
        g_CurrState = 2;
        RenderMaterials(m_header->m_alpha, m_header->m_numAlpha, g_CptSimpleAlphaBin);
        g_CurrState = 1;
        RenderMaterials(g_GlobalCpt.m_header->m_opaque, g_GlobalCpt.m_header->m_numOpaque, g_CptSimpleBin);
        g_CurrState = 2;
        RenderMaterials(g_GlobalCpt.m_header->m_alpha, g_GlobalCpt.m_header->m_numAlpha, g_CptSimpleAlphaBin);
        PROFILE_END(PROFBIN_CPT_BINS);
        PROFILE_END(PROFBIN_CPT_DRAW);
    }
}

// Hands the node up to three of the lights from the pool.
static inline void AssignLights(CPTNode& node, dwi::fast_vec<CLight*>& lights) {
    if (lights.size() == 0) {
        node.m_lights = NULL;
        node.m_numLights = 0;
    } else {
        node.m_numLights = lights.size();
        if (node.m_numLights > 3) {
            node.m_numLights = 3;
        }
        CLightLink* link = g_LightPool.Alloc();
        node.m_lights = link;
        CLight** it = lights.begin();
        link->m_light = *it++;
        int n = 1;
        while (it != lights.end() && n != 3) {
            CLightLink* next = g_LightPool.Alloc();
            link->m_next = next;
            link = next;
            n++;
            next->m_light = *it++;
        }
        link->m_next = NULL;
    }
}

// Culls the node against the frustum planes still straddled by its parent
// (`planes` bits), keeps the parent's lights that reach it, and queues its
// chunks on their materials.
void CCompartment::DrawNode(CPTNode& node, unsigned long planes, dwi::fast_vec<CLight*>& parentLights) {
    int culled = 0;
    unsigned long clipped = 0;
    node.m_drawFlags = 0;
    node.m_lights = NULL;
    node.m_numLights = 0;
    if (planes != 0) {
        for (int i = 0; i < FRUSTUM_NUM_PLANES; i++) {
            unsigned long bit = 1 << i;
            if (planes & bit) {
                const CPlane& plane = g_pFrustum->m_planes[i];
                f32 nx = plane.m_normal[0];
                f32 ny = plane.m_normal[1];
                f32 nz = plane.m_normal[2];
                CVector3 center = node.m_center;
                f32 radius =
                    (f32)fabs(node.m_axisX.x * nx + node.m_axisX.y * ny + node.m_axisX.z * nz) * node.m_axisX.w +
                    (f32)fabs(node.m_axisY.x * nx + node.m_axisY.y * ny + node.m_axisY.z * nz) * node.m_axisY.w +
                    (f32)fabs(node.m_axisZ.x * nx + node.m_axisZ.y * ny + node.m_axisZ.z * nz) * node.m_axisZ.w;
                f32 dist = (nx * center.x + ny * center.y + nz * center.z) - plane.m_d;
                if (dist > radius) {
                    culled = 1;
                    break;
                }
                if (dist >= -radius) {
                    clipped |= bit;
                }
            }
        }
    }
    node.m_visible = culled == 0;
    if (!node.m_visible) {
        return;
    }

    dwi::fast_vec<CLight*> lights(2);
    if (parentLights.size() != 0) {
        CLight** buffer = (CLight**)g_SPRStack.Alloc(parentLights.size() * 4);
        lights.reserve(buffer, parentLights.size());
        for (CLight** it = parentLights.begin(); it != parentLights.end(); it++) {
            CLight* light = *it;
            CVector3 pos;
            light->GetPosition(pos);
            pos.x = pos.x - node.m_center.x;
            pos.y = pos.y - node.m_center.y;
            pos.z = pos.z - node.m_center.z;
            f32 sqrDist = 0.0f;
            f32 hx = node.m_axisX.w;
            f32 hy = node.m_axisY.w;
            f32 hz = node.m_axisZ.w;
            f32 radius = light->m_radius;
            f32 px = (f32)fabs(node.m_axisX.x * pos.x + node.m_axisX.y * pos.y + node.m_axisX.z * pos.z);
            f32 pz = (f32)fabs(node.m_axisZ.x * pos.x + node.m_axisZ.y * pos.y + node.m_axisZ.z * pos.z);
            f32 py = (f32)fabs(node.m_axisY.x * pos.x + node.m_axisY.y * pos.y + node.m_axisY.z * pos.z);
            if (px > hx) {
                f32 d = px - hx;
                sqrDist = d * d + sqrDist;
            }
            if (py > hy) {
                f32 d = py - hy;
                sqrDist += d * d;
            }
            if (pz > hz) {
                f32 d = pz - hz;
                sqrDist += d * d;
            }
            if (sqrDist <= radius * radius) {
                lights.push_back(*it);
            }
        }
    }
    if (lights.size() != 0) {
        node.m_drawFlags |= CPTNODE_LIT;
    }
    if (clipped & 0x3C1) {
        node.m_drawFlags |= CPTNODE_CLIPPED;
    }
    node.m_drawFlags |= CPTNODE_FOGGED;
    if (g_bCPTForceClipped) {
        node.m_drawFlags |= CPTNODE_CLIPPED;
    }
    if (g_bCPTFogDisabled) {
        node.m_drawFlags &= ~CPTNODE_FOGGED;
    }
    if (g_bCPTDisableLighting) {
        node.m_drawFlags &= ~CPTNODE_LIT;
    }
    if (node.m_swaps != NULL) {
        if (node.m_lights == NULL) {
            AssignLights(node, lights);
        }
        CPTTexSwap* swap = node.m_swaps;
        for (int i = 0; i < node.m_numSwaps; i++) {
            CCPTMatBin* material = swap->m_material;
            swap->m_next = material->m_chunks;
            material->m_chunks = swap;
            swap++;
        }
    }
    if (node.m_chunks != NULL) {
        if (node.m_lights == NULL) {
            AssignLights(node, lights);
        }
        CPTChunk* chunk = node.m_chunks;
        for (int i = 0; i < node.m_numChunks; i++) {
            CCPTMatBin* material = chunk->m_material;
            chunk->m_next = material->m_chunks;
            material->m_chunks = chunk;
            chunk++;
        }
    }
    if (node.m_children[0] != NULL) {
        DrawNode(*node.m_children[0], clipped, lights);
    }
    if (node.m_children[1] != NULL) {
        DrawNode(*node.m_children[1], clipped, lights);
    }
    g_SPRStack.Free(parentLights.size() * 4);
}

// Explosions advance the texture swaps of the triangles they reach (unless
// a play-once swap already finished).
void CCompartment::HandleExplosion(const CExplosion& explosion) {
    const CDBTri* triBuffer[0x200];
    dwi::fast_vec<const CDBTri*> tris(-1);
    tris.reserve(triBuffer, 0x200);
    m_cdbObject.GetIntersectingTris(tris, explosion.m_sphere);
    if (tris.size() > 0) {
        int swapBuffer[0x200];
        dwi::fast_vec<int> swaps(1);
        swaps.reserve(swapBuffer, 0x200);
        for (int i = 0; i < tris.size(); i++) {
            int swap = tris.m_data[i]->m_texSwap;
            if (swap >= 0) {
                int* it = swaps.begin();
                int* end = swaps.end();
                while (it != end && *it != swap) {
                    it++;
                }
                if (it == end) {
                    swaps.push_back(swap);
                }
            }
        }
        for (int* it = swaps.begin(); it != swaps.end(); it++) {
            if (!TextureSwapCollisionDisabled(*it)) {
                TextureSwap(*it);
            }
        }
    }
}

// A play-once swap at its last frame no longer collides.
int CCompartment::TextureSwapCollisionDisabled(int index) {
    CPTTexSwap* swap = m_header->m_texSwaps[index];
    if (swap->m_mode == CPTSWAP_ONCE) {
        return swap->m_numFrames - 1 == g_TextureSwapCache.m_states[m_index][index].m_frame;
    }
    return 0;
}

void CCompartment::TextureSwap(int index) {
    CPTTexSwap* swap = m_header->m_texSwaps[index];
    if (swap->m_counter-- == 0) {
        swap->m_counter = swap->m_delay;
        int numFrames = swap->m_numFrames;
        int frame = g_TextureSwapCache.m_states[m_index][index].m_frame;
        switch (swap->m_mode) {
        case CPTSWAP_ONCE:
            frame++;
            if (frame >= numFrames) {
                frame = numFrames - 1;
            }
            break;
        case CPTSWAP_LOOP:
            frame++;
            if (frame >= numFrames) {
                frame = 0;
            }
            break;
        case CPTSWAP_PINGPONG:
            if (swap->m_reverse) {
                frame--;
                if (frame <= 0) {
                    frame = 0;
                    swap->m_reverse = 0;
                }
            } else {
                frame++;
                if (frame >= numFrames) {
                    frame = numFrames - 2;
                    swap->m_reverse = 1;
                }
            }
            break;
        }
        swap->m_material = swap->m_frames[frame];
        g_TextureSwapCache.m_states[m_index][index].m_frame = frame;
    }
}

int CCompartment::GetSharedSize() {
    return g_SharedCompartmentSize;
}

void CCompartment::LoadShared() {
    g_GlobalCpt.m_header = NULL;
    LevelFileContentsStruct_* cpt = TLT_FindNextResourceByType((TLTResourceID)3, NULL);
    if (cpt != NULL) {
        LevelFileContentsStruct_* cdb = TLT_FindNextResourceByType((TLTResourceID)2, NULL);
        if (cdb != NULL) {
            const char* cdbName = ADDR_TO_PTR(const char, cdb->fields[0].value);
            g_GlobalCpt.m_header =
                (CPTHeader*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(const char, cpt->fields[0].value), &g_SharedCompartmentSize);
            g_GlobalCpt.m_cdb = (CDB*)TLT_LoadFileFromLevelBigFile(cdbName, NULL);
            g_GlobalCpt.Init();
            g_GlobalCpt.m_cdbObject.Init(g_GlobalCpt.m_cdb);
            CSurfaceType::InitClass(g_GlobalCpt.m_cdb);
            g_TextureSwapCache.Reset();
        }
    }
}

void CCompartment::InitClass(CRenderList* renderList, CScreen* screen) {
    static bool bReady = false;
    if (!bReady) {
        g_pScreen = screen;
        g_pRenderList = renderList;
        renderList->Register(g_CptSimpleBin);
        renderList->Register(g_CptSimpleAlphaBin);
        renderList->Register(g_CptShadowBin);
        g_LightPool.Init();
    }
}

// Takes a compartment's materials (a contiguous run of siblings) out of the
// ucode bin's children.
static inline void UnlinkMaterials(CCPTMatBin* first, int count, CCptSimpleBin& parent) {
    if (first != NULL) {
        CCPTMatBin* last = first + (count - 1);
        if (parent.m_pChildren == first) {
            parent.m_pChildren = last->m_pNext;
        }
        if (first->m_pPrev != NULL) {
            first->m_pPrev->m_pNext = last->m_pNext;
        }
        if (last->m_pNext != NULL) {
            last->m_pNext->m_pPrev = first->m_pPrev;
        }
        last->m_pNext = NULL;
        first->m_pPrev = NULL;
    }
}

CCompartment::~CCompartment() {
    if (m_header != NULL) {
        UnlinkMaterials(m_header->m_opaque, m_header->m_numOpaque, g_CptSimpleBin);
        UnlinkMaterials(m_header->m_alpha, m_header->m_numAlpha, g_CptSimpleAlphaBin);
        g_TextureSwapCache.m_saved[m_index] = 1;
    }
}

CCompartment::CCompartment(const CCompartment& other)
    : m_cdbObject(other.m_cdbObject), m_index(other.m_index), m_name(other.m_name), m_header(other.m_header),
      m_cdb(other.m_cdb), m_drawEnabled(other.m_drawEnabled), m_evict(0) {}

CCompartment::CCompartment()
    : m_cdbObject(NULL), m_index(-1), m_name(NULL), m_header(NULL), m_cdb(NULL), m_drawEnabled(true),
      m_evict(0) {}

// The original unit also held the out-of-line copy of CVector3's (header
// inline) copy constructor that other objects call; MWCC inlines every use
// here, so it is defined under its mangled name.
extern "C" CVector3* __ct__8CVector3FRC8CVector3(CVector3* self, const CVector3* other) {
    f64 a = ((const f64*)other)[0];
    f64 b = ((const f64*)other)[1];
    ((f64*)self)[0] = a;
    ((f64*)self)[1] = b;
    return self;
}


// Builds a material bin in place and sets up its texture.
static inline void InitMaterial(CCPTMatBin* material, int base) {
#ifdef FRONTLINE_NATIVE
    // Built and relocated by InitMaterials.
    ShapeFile* file = ShapeFileFromBigEndian(material->m_shape);
#else
    new (material) CCPTMatBin;
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
                             (material->m_flags & CPTMAT_WRAP) != 0, (material->m_flags >> 3) & 1, 0);
                break;
            }
            case 0x18:
            case 0x19: {
                u32 format = 9;  // GX_TF_C8
                if (shape->format == 0x18) {
                    format = 8;  // GX_TF_C4
                }
                GXInitTexObjCI(&material->m_texObj, shape->data, (u16)shape->width, (u16)shape->height, format,
                               (material->m_flags & CPTMAT_WRAP) != 0, (material->m_flags >> 3) & 1, 0, 0);
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
// Natively a CCPTMatBin doesn't fit in the file's 0x70-byte record, so the
// materials are separate objects built from the records; references to a
// record (chunks, texture swaps) are pointed at its object after relocation.
struct NativeMaterials {
    u32 records;  // address of the first record
    int count;
    CCPTMatBin* objects;
};
static NativeMaterials s_nativeOpaque, s_nativeAlpha;

static CCPTMatBin* NativeMaterialFor(PTR32(CCPTMatBin) record) {
    const NativeMaterials* sets[2] = {&s_nativeOpaque, &s_nativeAlpha};
    for (int s = 0; s < 2; s++) {
        u32 offset = record.addr() - sets[s]->records;
        if (offset < (u32)sets[s]->count * sizeof(CPTMaterialRecord)) {
            return &sets[s]->objects[offset / sizeof(CPTMaterialRecord)];
        }
    }
    return record;  // NULL, or already an object
}

static CCPTMatBin* InitMaterials(PTR32(CCPTMatBin) records, int count, bool alpha, int base) {
    NativeMaterials& set = alpha ? s_nativeAlpha : s_nativeOpaque;
    set.records = records.addr();
    set.count = count;
    set.objects = count != 0 ? (CCPTMatBin*)operator new(count * sizeof(CCPTMatBin)) : NULL;
    CCPTMatBin* prev = NULL;
    for (int i = 0; i < count; i++) {
        const CPTMaterialRecord* record = (const CPTMaterialRecord*)records.get() + i;
        CCPTMatBin* material = new (&set.objects[i]) CCPTMatBin;
        material->m_flags = record->m_flags;
        material->m_shape =
            record->m_shape.addr() != 0 ? ADDR_TO_PTR(ShapeFile, record->m_shape.addr() + base) : NULL;
        InitMaterial(material, base);
        if (prev != NULL) {
            prev->m_pNext = material;
        }
        material->m_pPrev = prev;
        prev = material;
        if (material->m_flags & CPTMAT_SHARED) {
            CPTHeader* shared = g_GlobalCpt.m_header;
            material->m_shared = (alpha ? shared->m_alpha : shared->m_opaque) + record->m_shared;
        }
    }
    return set.objects;
}
#else
// Links the materials as siblings under the ucode bin and resolves shared
// ones to the shared compartment's.
static inline void InitMaterials(CCPTMatBin* materials, int count, bool alpha, int base) {
    CCPTMatBin* prev = NULL;
    CCPTMatBin* material = materials;
    for (int i = 0; i < count; i++) {
        if (material != NULL) {
            InitMaterial(material, base);
        }
        if (prev != NULL) {
            prev->m_pNext = material;
        }
        material->m_pPrev = prev;
        prev = material;
        if (material->m_flags & CPTMAT_SHARED) {
            CPTHeader* shared = g_GlobalCpt.m_header;
            material->m_shared = (alpha ? shared->m_alpha : shared->m_opaque) + (int)PTR_TO_ADDR(material->m_shared);
        }
        material++;
    }
}
#endif

// Relocates a display list and writes its array setup commands.
static inline void InitDisplayList(CPTDisplayList* list, int base) {
    offsetPtr(list->m_positions, base);
    offsetPtr(list->m_normals, base);
    offsetPtr(list->m_texCoords, base);
    offsetPtr(list->m_colors, base);
    offsetPtr(list->m_list, base);
    int indexSize;
    if ((list->m_flags & 1) != 1) {
        indexSize = 4;
    } else {
        indexSize = 8;
    }
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
    // Then a triangle strip of m_size vertices.
    cmd[0x30] = 0;
    cmd[0x31] = 0x98;
    GX_STORE16(cmd + 0x32, list->m_size);
    u16 size = list->m_size * indexSize + 0x34;
    if (size & 0x1F) {
        size += 0x20 - (size & 0x1F);
    }
    list->m_size = size;
    DCFlushRange(list->m_list, 0x34);
}

static inline void InitChunk(CPTChunk* chunk, int base) {
    offsetPtr(chunk->m_node, base);
    offsetPtr(chunk->m_material, base);
#ifdef FRONTLINE_NATIVE
    chunk->m_material = NativeMaterialFor(chunk->m_material);
#endif
    offsetPtr(chunk->m_list, base);
    if (chunk->m_list != NULL) {
        InitDisplayList(chunk->m_list, base);
    }
    if (chunk->m_material->m_flags & CPTMAT_SHARED) {
        chunk->m_material = chunk->m_material->m_shared;
    }
}

// Relocates the compartment file, builds its materials and links them
// into the ucode bins.
void CCompartment::Init() {
#ifdef FRONTLINE_NATIVE
    CPTFromBigEndian(m_header);
#endif
    int base = (int)PTR_TO_ADDR(m_header);
    offsetPtr(m_header->m_opaque, base);
    offsetPtr(m_header->m_alpha, base);
    offsetPtr(m_header->m_chunks, base);
    offsetPtr(m_header->m_nodes, base);
    offsetPtr(m_header->m_texSwaps, base);
#ifdef FRONTLINE_NATIVE
    m_header->m_opaque = InitMaterials(m_header->m_opaque, m_header->m_numOpaque, false, base);
    m_header->m_alpha = InitMaterials(m_header->m_alpha, m_header->m_numAlpha, true, base);
#else
    InitMaterials(m_header->m_opaque, m_header->m_numOpaque, false, base);
    InitMaterials(m_header->m_alpha, m_header->m_numAlpha, true, base);
#endif

    CPTChunk* chunk = m_header->m_chunks;
    for (int i = 0; i < m_header->m_numChunks; i++) {
        offsetPtr(chunk->m_node, base);
        offsetPtr(chunk->m_material, base);
#ifdef FRONTLINE_NATIVE
        chunk->m_material = NativeMaterialFor(chunk->m_material);
#endif
        offsetPtr(chunk->m_list, base);
        if (chunk->m_list != NULL) {
            InitDisplayList(chunk->m_list, base);
        }
        if (chunk->m_material->m_flags & CPTMAT_SHARED) {
            chunk->m_material = chunk->m_material->m_shared;
        }
        chunk++;
    }

    CPTNode* node = m_header->m_nodes;
    for (int i = 0; i < m_header->m_numNodes; i++) {
        offsetPtr(node->m_children[0], base);
        offsetPtr(node->m_children[1], base);
        offsetPtr(node->m_chunks, base);
        offsetPtr(node->m_swaps, base);
        CPTTexSwap* swap = node->m_swaps;
        for (int j = 0; j < node->m_numSwaps; j++) {
            InitChunk(swap, base);
            for (u32 k = 0; k < swap->m_numFrames; k++) {
                offsetPtr(swap->m_frames[k], base);
#ifdef FRONTLINE_NATIVE
                swap->m_frames[k] = NativeMaterialFor(swap->m_frames[k]);
#endif
                if (swap->m_frames[k]->m_flags & CPTMAT_SHARED) {
                    swap->m_frames[k] = swap->m_frames[k]->m_shared;
                }
            }
            swap++;
        }
        node++;
    }

    // The materials join the front of the ucode bins' children.
    if (m_header->m_opaque != NULL) {
        CRenderBin* next = g_CptSimpleBin.m_pChildren;
        g_CptSimpleBin.m_pChildren = m_header->m_opaque;
        CCPTMatBin* last = m_header->m_opaque + (m_header->m_numOpaque - 1);
        last->m_pNext = next;
        if (next != NULL) {
            next->m_pPrev = last;
        }
    }
    if (m_header->m_alpha != NULL) {
        CRenderBin* next = g_CptSimpleAlphaBin.m_pChildren;
        g_CptSimpleAlphaBin.m_pChildren = m_header->m_alpha;
        CCPTMatBin* last = m_header->m_alpha + (m_header->m_numAlpha - 1);
        last->m_pNext = next;
        if (next != NULL) {
            next->m_pPrev = last;
        }
    }

    for (int i = 0; i < m_header->m_numTexSwaps; i++) {
        offsetPtr(m_header->m_texSwaps[i], base);
    }

    // A reloaded compartment takes its saved swap state back.
    if (m_index >= 0 && g_TextureSwapCache.m_saved[m_index] != 0) {
        for (int i = 0; i < m_header->m_numTexSwaps; i++) {
            CPTTexSwap* swap = m_header->m_texSwaps[i];
            swap->m_counter = g_TextureSwapCache.m_states[m_index][i].m_flag;
            swap->m_reverse = g_TextureSwapCache.m_states[m_index][i].m_flag != 0;
        }
    }
}

void CCompartment::Reset() {
    for (int i = 0; i < m_header->m_numTexSwaps; i++) {
        CPTTexSwap* swap = m_header->m_texSwaps[i];
        swap->m_counter = 0;
        swap->m_reverse = 0;
        swap->m_material = swap->m_frames[0];
    }
}

void CCompartment::GlobalReset() {
    g_TextureSwapCache.Reset();
}

void CCompartment::Init(int index, char* name, CPTHeader* header, CDB* cdb) {
    m_index = index;
    m_name = name;
    m_header = header;
    m_cdb = cdb;
    m_cdbObject.Init(m_cdb);
    Init();
}
