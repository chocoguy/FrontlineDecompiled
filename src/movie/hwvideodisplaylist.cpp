// GCHW_VD: draws movie frames with GX. The frame's planes are bound in place
// as C8 textures and converted to RGB by tBigYUVSwizzler's TEV setup on one
// screen-aligned quad.
#include "movie/hwvideodisplaylist.h"
#include "game/Screen.h"
#include "dolphin/os.h"
#ifdef FRONTLINE_NATIVE
#include "native/hooks.h"
#endif

static const u32 kAttrs[3] = {GX_VA_POS, GX_VA_CLR0, GX_VA_TEX0};

GCHW_VD::GCHW_VD(RCMP::DECODER* decoder, RCMP::FRAME* frame) {
    GXRenderModeObj* rmode = g_screen.m_pRenderMode;
    m_decoder = decoder;
    int width = frame->m_shape->width;
    int height = frame->m_shape->height;
    m_width = width;
    m_height = height;
    m_screenHeight = g_screen.GetHeight();
    m_screenWidth = g_screen.GetWidth();
    m_renderMode = rmode;
    GXInitTexObjCI(&m_yTex, NULL, width, height, 9, 0, 0, 0, 2);  // C8
    GXInitTexObjLOD(&m_yTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    int cw = width / 2;
    int ch = height / 2;
    GXInitTexObjCI(&m_vTex, NULL, cw, ch, 9, 0, 0, 0, 1);
    GXInitTexObjLOD(&m_vTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXInitTexObjCI(&m_uTex, NULL, cw, ch, 9, 0, 0, 0, 0);
    GXInitTexObjLOD(&m_uTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    m_swizzler = NEW_tBigYUVSwizzlerTexture(&m_yTex, &m_vTex, &m_uTex);
    m_frame = NULL;
    m_x0 = (m_renderMode->fbWidth - width) / 2;
    m_y0 = (m_renderMode->efbHeight - height) / 2;
    m_x1 = m_x0 + width;
    m_y1 = m_y0 + height;
    m_z = 0.0f;
    m_scaleX = 1.0f;
    m_scaleY = 1.0f;
}

RCMP::VIDEO_DRIVER* RCMP::MOHFL_VD_create(DECODER* decoder, FRAME* frame) {
    return new GCHW_VD(decoder, frame);
}

void GCHW_VD::SetPosition(float x, float y, float z) {
    m_x0 = x;
    m_y0 = y;
    m_x1 = m_width * m_scaleX + m_x0;
    m_y1 = m_height * m_scaleY + m_y0;
    m_z = z;
}

void GCHW_VD::SetScale(float x, float y) {
    m_scaleX = x;
    m_scaleY = y;
    m_x1 = m_width * m_scaleX + m_x0;
    m_y1 = m_height * m_scaleY + m_y0;
}

GCHW_VD::~GCHW_VD() {
    DELETE_tBigYUVSwizzler(m_swizzler);
    if (m_frame != NULL) {
        m_decoder->ReleaseFrame(m_frame);
    }
}

void GCHW_VD::iDraw() {
    RCMP::SHAPE* shape = m_frame->m_shape;
    u8* y = shape->Data();
    int width = shape->width;
    int height = shape->height;
    int cw = width / 2;
    int ch = height / 2;
    u32 size = width * height;
    u8* u = y + size;
    u8* v = u + cw * ch;
    f32 proj[4][4];
    f32 view[3][4];
    u32 attrs[3];
    u32 cnts[3];
    u32 types[3];
    u8 fracs[3];
    u32 i;

    GXSetCurrentMtx(GX_IDENTITY);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetCullMode(0);
    GXSetVtxAttrFmt(7, GX_VA_POS, 1, 4, 0);  // XYZ F32
    GXSetVtxAttrFmt(7, GX_VA_CLR0, 1, 5, 0);  // RGBA8
    GXSetVtxAttrFmt(7, GX_VA_TEX0, 1, 4, 0);  // ST F32
    C_MTXOrtho(proj, 0.0f, (f32)m_screenHeight, 0.0f, (f32)m_screenWidth, 5.0f, 2000.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    PSMTXIdentity(view);
    view[2][3] = -1200.0f;
    GXLoadPosMtxImm(view, 0);
    GXSetCurrentMtx(0);
    GXSetNumTevStages(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTexGens(1);
    GXSetNumChans(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 2, 2);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 0);

    DCFlushRangeNoSync(y, size);
    size >>= 2;
    DCFlushRangeNoSync(v, size);
    DCFlushRangeNoSync(u, size);
    GXInitTexObjCI(&m_yTex, y, width, height, 9, 0, 0, 0, 2);
    GXInitTexObjLOD(&m_yTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXInitTexObjCI(&m_vTex, v, cw, ch, 9, 0, 0, 0, 1);
    GXInitTexObjLOD(&m_vTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    GXInitTexObjCI(&m_uTex, u, cw, ch, 9, 0, 0, 0, 0);
    GXInitTexObjLOD(&m_uTex, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    tBigYUVSwizzler_DrawSetup(m_swizzler, &m_yTex, &m_vTex, &m_uTex);
#ifdef FRONTLINE_NATIVE
    if (NativeGameMode() == NATIVE_MODE_REWRITTEN) {
        // The planes read directly and filtered, without the swizzle's point
        // lookups, so a movie scaled up is smooth. The TEV's YUV to RGB stays.
        NativeLinearTexture(&m_uTex);
        NativeLinearTexture(&m_vTex);
        NativeLinearTexture(&m_yTex);
        GXLoadTexObj(&m_uTex, 0);
        GXLoadTexObj(&m_vTex, 1);
        GXLoadTexObj(&m_yTex, 2);
        GXSetNumIndStages(0);
        for (i = 0; i < 5; i++) {
            GXSetTevDirect(i);
        }
    }
#endif

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    // save vertex format 0 while we use it
    attrs[0] = kAttrs[0];
    attrs[1] = kAttrs[1];
    attrs[2] = kAttrs[2];
    for (i = 0; i < 3; i++) {
        GXGetVtxAttrFmt(0, attrs[i], &cnts[i], &types[i], &fracs[i]);
    }
    GXSetVtxAttrFmt(0, GX_VA_POS, 1, 4, 0);
    GXSetVtxAttrFmt(0, GX_VA_CLR0, 1, 5, 0);
    GXSetVtxAttrFmt(0, GX_VA_TEX0, 1, 4, 0);
    GXSetZMode(0, 7, 0);
    PPCSync();
    GXBegin(GX_QUADS, 0, 4);
    GXPosition3f32(m_x0, m_y0, m_z);
    GXColor1u32(0xFFFFFFFF);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(m_x1, m_y0, m_z);
    GXColor1u32(0xFFFFFFFF);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(m_x1, m_y1, m_z);
    GXColor1u32(0xFFFFFFFF);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(m_x0, m_y1, m_z);
    GXColor1u32(0xFFFFFFFF);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();

    // put back the state other drawing expects
    GXSetNumIndStages(0);
    GXSetTevSwapModeTable(0, 0, 1, 2, 3);
    GXSetTevSwapModeTable(1, 0, 0, 0, 3);
    GXSetTevSwapModeTable(2, 1, 1, 1, 3);
    GXSetTevSwapModeTable(3, 2, 2, 2, 3);
    GXSetTevDirect(0);
    GXSetTevDirect(1);
    GXSetTevDirect(2);
    GXSetTevDirect(3);
    GXSetTevDirect(4);
    GXSetTevSwapMode(1, 0, 0);
    GXSetTevSwapMode(2, 0, 0);
    for (i = 0; i < 3; i++) {
        GXSetVtxAttrFmt(0, attrs[i], cnts[i], types[i], fracs[i]);
    }
}

// Shows `frame`, releasing the one shown before.
void GCHW_VD::Draw(RCMP::FRAME* frame) {
    if (m_frame != NULL) {
        m_decoder->ReleaseFrame(m_frame);
    }
    m_frame = frame;
    iDraw();
}

void GCHW_VD::ReDraw() {
    iDraw();
}

void GCHW_VD::CompleteDraw() {}
