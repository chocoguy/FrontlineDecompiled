#include "game/motionblur.h"

#include "game/renderlist.h"

static CBlurBin g_BlurBin;
static CRenderList* g_pRenderList;
static float g_BlurAmount;

// The blur itself is not implemented on GameCube: the bin never links any
// drawing, it only reports whether a blur amount is set.
void CMotionBlur::Init(CRenderList* renderList) {
    g_pRenderList = renderList;
    renderList->Register(g_BlurBin);
}

void CMotionBlur::SetAmount(float amount) {
    g_BlurAmount = amount;
}

void CBlurBin::Link(CDmaTag*, CDmaPacket&) {}

bool CBlurBin::IsUsed() {
    return g_BlurAmount != 0.0f;
}
