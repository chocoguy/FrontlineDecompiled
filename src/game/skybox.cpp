// The sky box: six textured faces of a cube centred on the camera, drawn
// with the view rotation only and no depth test/write. Faces turned away
// from the camera are skipped.
#include "game/skybox.h"

#include "game/CSkyBoxRenderBin.h"
#include "game/renderlist.h"
#include "game/screen.h"
#include "game/draw_context.h"
#include "game/gameconfig.h"
#include "game/toilet.h"
#include "game/Moh2.h"
#ifdef FRONTLINE_NATIVE
#include "native/hooks.h"
#endif
#include "dolphin/gx.h"

class CVector4 {
public:
    f32 x, y, z, w;
};

// Shared with the render bins: the frame's projection, then the cube.
static CMatrix g_ProjectionMatrix;
static CVector3 g_skyBoxVerts[8] = {
    CVector3(-1.0f, -1.0f, -1.0f), CVector3(1.0f, -1.0f, -1.0f), CVector3(1.0f, -1.0f, 1.0f),
    CVector3(-1.0f, -1.0f, 1.0f),  CVector3(-1.0f, 1.0f, -1.0f), CVector3(1.0f, 1.0f, -1.0f),
    CVector3(1.0f, 1.0f, 1.0f),    CVector3(-1.0f, 1.0f, 1.0f),
};
static CVector3 g_skyBoxNormals[CSkyBox::NUM_FACES] = {
    CVector3(0.0f, -1.0f, 0.0f), CVector3(-1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f),
    CVector3(1.0f, 0.0f, 0.0f),  CVector3(0.0f, 0.0f, 1.0f),  CVector3(0.0f, 0.0f, -1.0f),
};
static CSkyBoxRenderBin g_skyBoxRenderBin[CSkyBox::NUM_FACES];

static const int g_skyBoxVertIndices[CSkyBox::NUM_FACES][4] = {
    {2, 3, 0, 1}, {3, 7, 4, 0}, {7, 6, 5, 4}, {6, 2, 1, 5}, {3, 2, 6, 7}, {4, 5, 1, 0},
};
static const f32 g_skyBoxUV[4][2] = {
    {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
};

CRenderList* CSkyBox::sm_pRenderList;
CScreen* CSkyBox::sm_pScreen;
CSkyBox* CSkyBox::sm_pSingleton;

bool CSkyBoxRenderBin::IsUsed() {
    bool used = false;
    if (m_pSkyBox != NULL && m_visible) {
        used = true;
    }
    return used;
}

void CSkyBoxRenderBin::Link(CDmaTag* tag, CDmaPacket& packet) {
    if (tag == NULL) {
        return;
    }
#ifdef FRONTLINE_NATIVE
    NativeDrawClass(NATIVE_DRAW_SKY);
#endif
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);   // GX_VA_POS, direct
    GXSetVtxDesc(11, 1);  // GX_VA_CLR0, direct
    GXSetVtxDesc(13, 1);  // GX_VA_TEX0, direct
    GXSetProjection(g_ProjectionMatrix.m, (GXProjectionType)0);
    GXSetCurrentMtx(3);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);   // xyz f32
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);  // rgba8
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);  // st f32
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTevStages(1);
    GXSetBlendMode(1, 4, 5, 5);
    GXSetZMode(0, 7, 0);
    GXSetCullMode(0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    for (; tag != NULL; tag = tag->m_next) {
        u32* data = (u32*)tag->m_data;
        CSkyBox* sky = ADDR_TO_PTR(CSkyBox, data[0]);
        int face = data[1];
        f32 mtx[3][4];
        sky->m_view.GetGAMECUBEMatrix34(mtx);
        GXLoadPosMtxImm(mtx, 3);
        sky->m_textures[face].Load(packet, NULL);
        GXBegin(0xA0, 0, sky->m_numVerts[face]);  // GX_TRIANGLEFAN
        for (int i = 0; i < sky->m_numVerts[face]; i++) {
            CSkyBox::SVertex& v = m_pSkyBox->m_verts[face][i];
            GXPosition3f32(v.m_pos.x, v.m_pos.y, v.m_pos.z);
            GXColor4u8(0xFF, 0xFF, 0xFF, 0xFF);
            GXTexCoord2f32(v.m_u, v.m_v);
        }
        GXEnd();
    }
    GXSetCurrentMtx(0);
#ifdef FRONTLINE_NATIVE
    NativeDrawClass(NATIVE_DRAW_WORLD);
#endif
}

int CSkyBoxRenderBin::Render(CDmaPacket& packet, void* data) {
    m_pSkyBox = (CSkyBox*)data;
    *(PTR32(CSkyBox)*)packet.m_ptr = m_pSkyBox;
    packet.m_ptr += 4;
    *(int*)packet.m_ptr = m_face;
    packet.m_ptr += 4;
    return 0;
}

CSkyBoxRenderBin::CSkyBoxRenderBin() {
    m_pSkyBox = NULL;
    m_face = 0;
    m_visible = false;
    m_type = 1;
    Init();
}

CSkyBoxRenderBin::~CSkyBoxRenderBin() {}

// Like the original, the guard is never set, so each call registers again.
void CSkyBox::InitClass(CRenderList* renderList, CScreen* screen) {
    static bool ready = false;
    if (!ready) {
        sm_pRenderList = renderList;
        sm_pScreen = screen;
        for (int i = 0; i < NUM_FACES; i++) {
            g_skyBoxRenderBin[i].m_face = i;
            sm_pRenderList->Register(g_skyBoxRenderBin[i]);
        }
    }
}

static inline void SetFacesVisible(bool visible) {
    for (int i = 0; i < CSkyBox::NUM_FACES; i++) {
        g_skyBoxRenderBin[i].m_visible = visible;
    }
}

// The sky covers the screen, so the screen stops clearing.
CSkyBox* CSkyBox::CreateInstance() {
    if (sm_pSingleton == NULL) {
        sm_pSingleton = new CSkyBox;
        sm_pScreen->SetClear(false);
        SetFacesVisible(true);
    }
    return sm_pSingleton;
}

CSkyBox* CSkyBox::GetInstance() {
    return sm_pSingleton;
}

void CSkyBox::DestroyInstance() {
    if (sm_pSingleton != NULL) {
        delete sm_pSingleton;
    }
    sm_pScreen->SetClear(true);
    sm_pSingleton = NULL;
    SetFacesVisible(false);
}

CSkyBox::~CSkyBox() {
    for (int i = 0; i < NUM_FACES; i++) {
        TLT_CloseFile(m_files[i]);
    }
}

void CSkyBox::SetTextures() {
    LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
    if (res == NULL) {
        DebugMsg("No Skybox textures found in level contents file\n");
        return;
    }
    for (int i = 0; i < NUM_FACES; i++) {
        m_files[i] = TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(const char, res->fields[1 + i].value), NULL);
    }
    for (int i = 0; i < NUM_FACES; i++) {
        if (m_files[i] != NULL) {
            m_textures[i].Set((ShapeFile*)m_files[i], false);
        }
    }
}

// The sky option off: the screen clears instead and no face draws.
void CSkyBox::Draw(CDrawContext& ctx) {
    g_ProjectionMatrix = *ctx.m_pProj;
    if (g_config.field_0x7) {
        sm_pScreen->SetClear(true);
        SetFacesVisible(false);
        return;
    }
    sm_pScreen->SetClear(false);
    m_view = *ctx.m_pView;
    CVector3 zero(0.0f, 0.0f, 0.0f);
    m_view.SetPos(zero);
    m_viewProj.Multiply(m_view, *ctx.m_pProj);
    // The transformed corners are not used (as in the original).
    CVector4 corners[8];
    for (int i = 0; i < 8; i++) {
        m_viewProj.Transform(corners[i], g_skyBoxVerts[i]);
    }
    CVector3 forward;
    ctx.m_pCamera->GetForward(forward);
    for (int face = 0; face < NUM_FACES; face++) {
        const CVector3& n = g_skyBoxNormals[face];
        bool facing = forward.z * n.z + (forward.x * n.x + forward.y * n.y) > 0.0f;
#ifdef FRONTLINE_NATIVE
        // A wider view sees faces at its sides that this test drops (past a
        // tangent of 0.707; the original's 0.700 is just inside it), and so
        // do frames drawn between the game's with the camera turned on.
        facing = facing || g_nativeWideScale > 1.0f || NativeHighFrameRate();
#endif
        if (facing) {
            g_skyBoxRenderBin[face].m_visible = true;
            for (int i = 0; i < 4; i++) {
                SVertex& v = m_verts[face][i];
                const CVector3& p = g_skyBoxVerts[g_skyBoxVertIndices[face][i]];
                v.m_pos.x = p.x;
                v.m_pos.y = p.y;
                v.m_pos.z = p.z;
                v.m_u = g_skyBoxUV[i][0];
                v.m_v = g_skyBoxUV[i][1];
                v.m_w = 0.0f;
            }
            m_numVerts[face] = 4;
            sm_pRenderList->Render(g_skyBoxRenderBin[face], this);
        } else {
            g_skyBoxRenderBin[face].m_visible = false;
        }
    }
}

int CSkyBox::IsDrawEnabled() const {
    return m_drawEnabled;
}

int CSkyBox::IsVisible(CDrawContext&) const {
    return 1;
}
