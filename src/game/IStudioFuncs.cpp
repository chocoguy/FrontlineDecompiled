#include "game/IStudioFuncs.h"

#include "ea/real.h"
#include "game/isexportdefs.h"
#include "game/matrix.h"
#include "game/matstack.h"
#include "game/toilet.h"

float g_fScaleValueX;
float g_fScaleValueY;
float g_fScaleValueZ;

static const char* szShellBigFileName = "data/shell/shell.viv";

void DummyPluginProcessFnc(void*, unsigned long, unsigned long, UISParam_t*, UISParam_t*) {}

void LoadShellBigFile() {
    if (g_pLevelBigFile != NULL) {
        MEM_free(g_pLevelBigFile);
    }
    g_pLevelBigFile = TLT_LoadFileNormal(szShellBigFileName, &g_LevelBigFileSize, 0x100);
}

void UnloadShellBigFile() {
    if (g_pLevelBigFile != NULL) {
        MEM_free(g_pLevelBigFile);
        g_pLevelBigFile = NULL;
    }
}

void* ResLoadScreen(unsigned short group, unsigned short screen) {
    void* data = TLT_LoadFileFromShellBigFile(ScreenInfo[group].screenNames[screen], NULL);
    return data != NULL ? data : NULL;
}

void ResUnloadScreen(unsigned short group, unsigned short screen, void* data) {
    if (data != NULL) {
        TLT_CloseFile(data);
    }
}

// Degrees, quantized to MathSinCos units (2^24 per turn), in radians.
static inline float QuantizedDegToRad(float degrees) {
    return 6.2831855f * (float)(int)(46603.38f * degrees) / 16777216.0f;
}

void iSirenTransformfnc(UISTransformAction action, UISTransform_t* xf) {
    switch (action) {
    case UIS_TRANSFORM_BEGIN:
        g_matStack->Push();
        g_matStack->Top()->PreTranslate(CVector3(xf->pos[0], xf->pos[1], xf->pos[2]));
        g_matStack->Top()->PreTranslate(CVector3(xf->pivot[0], xf->pivot[1], xf->pivot[2]));
        g_matStack->Top()->RotateX(QuantizedDegToRad(xf->rot[0]));
        g_matStack->Top()->RotateY(QuantizedDegToRad(xf->rot[1]));
        g_matStack->Top()->RotateZ(QuantizedDegToRad(xf->rot[2]));
        g_matStack->ScaleRow(CVector3(xf->scale[0], xf->scale[1], xf->scale[2]));
        g_fScaleValueX = xf->scale[0] == 1.0f ? 1.0f : xf->scale[0];
        g_fScaleValueY = xf->scale[1] == 1.0f ? 1.0f : xf->scale[1];
        g_fScaleValueZ = xf->scale[2] == 1.0f ? 1.0f : xf->scale[2];
        g_matStack->Top()->PreTranslate(CVector3(-xf->pivot[0], -xf->pivot[1], -xf->pivot[2]));
        break;
    case UIS_TRANSFORM_END:
        g_matStack->Pop();
        break;
    }
}
