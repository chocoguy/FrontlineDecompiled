// CScreen: the display. Two external frame buffers are drawn and swapped at
// vertical retrace (VBlankHandler); Wait returns how many retraces the frame
// took, which the game uses as its time step.
#include "game/Screen.h"
#include "game/Moh2.h"
#include "game/fader.h"
#include "game/filesys.h"
#include "game/matrix.h"
#include "game/memory.h"
#include "game/vector.h"
#include "ea/real.h"
#include <dolphin/gx.h>
#include <dolphin/vi.h>
#include "stdio.h"

// SSH ("SHPS") image file header, as written by CScreenFieldMode::_Capture.
struct SSHFileHeader {
    /* 0x0 */ char magic[4];  // "SHPS"
    /* 0x4 */ s32 size;
    /* 0x8 */ s32 numImages;
    /* 0xC */ char id[4];  // "GRAB"
};

struct SSHDirEntry {
    /* 0x0 */ s32 name;
    /* 0x4 */ s32 offset;
    /* 0x8 */ s32 field_0x8;
    /* 0xC */ s32 field_0xC;
};

struct SSHImageHeader {
    /* 0x0 */ u32 format : 7;  // 3 = 16-bit, 4 = 24-bit, 5 = 32-bit
    /* 0x0 */ u32 flag : 1;
    /* 0x0 */ u32 nextOffset : 24;
    /* 0x4 */ s16 width;
    /* 0x6 */ s16 height;
    /* 0x8 */ s16 centerX;
    /* 0xA */ s16 centerY;
    /* 0xC */ u16 x : 12;
    /* 0xC */ u16 flag0 : 1;
    /* 0xC */ u16 flag1 : 1;
    /* 0xC */ u16 flag2 : 1;
    /* 0xC */ u16 flag3 : 1;
    /* 0xE */ u16 y : 12;
    /* 0xE */ u16 levels : 4;
};

static CScreen* g_prevScreen;
static CScreen* g_pScreen;
static volatile int g_FrameNum;  // retraces since start-up
static int g_WaitFrameNum;
static volatile bool g_bSwap;  // a frame is waiting for the next retrace
static bool g_bReady;
static bool g_bCapture;

// At each retrace: shows the finished frame, if one is waiting.
void CScreen::VBlankHandler(u32) {
    CScreen* screen = g_pScreen;
    g_FrameNum++;
    if (screen != NULL && g_bSwap) {
        if (screen->m_retraces++ > 1) {
            screen->m_missedFrame = true;
        }
        screen->PutDisp();
        screen->SetDraw();
        screen->m_field ^= 1;
        g_bSwap = false;
    }
}

CScreen::~CScreen() {
    if (g_pScreen == this) {
        g_pScreen = g_prevScreen;
    } else if (g_prevScreen == this) {
        g_prevScreen = NULL;
    }
}

void CScreen::Init() {
    g_bReady = true;
    VIInit();
    VISetPreRetraceCallback(VBlankHandler);
}

CScreen* CScreen::GetCurrent() {
    return g_pScreen;
}

// Sets the GX viewport to the render mode's frame buffer, jittered by the
// next field when rendering fields.
static inline void SetFieldViewport(GXRenderModeObj* rm) {
    if (rm->field_rendering) {
        GXSetViewportJitter(0.0f, 0.0f, rm->fbWidth, rm->efbHeight, 0.0f, 1.0f, VIGetNextField());
    } else {
        GXSetViewport(0.0f, 0.0f, rm->fbWidth, rm->efbHeight, 0.0f, 1.0f);
    }
}

// Makes this the current screen: sets up NTSC 480i video (with overscan),
// allocates the two frame buffers and the GX FIFO, and starts GX. Returns the
// previous screen.
CScreen* CScreen::SetCurrent() {
    m_widthScale = m_width / 640.0f;
    m_heightScale = m_height / 448.0f;
    if (g_pScreen == this) {
        return g_prevScreen;
    }
    g_prevScreen = g_pScreen;
    g_pScreen = this;

    m_pRenderMode = &GXNtsc480IntDf;
    DebugMsg("INTERLACED MODE");
    GXAdjustForOverscan(m_pRenderMode, &m_renderMode, 0, 16);
    m_pRenderMode = &m_renderMode;
    int size = ((m_pRenderMode->fbWidth + 15) & 0xFFF0) * m_pRenderMode->xfbHeight * 2;
    m_frameBuffer1 = DWI_allocalign("FrameBuffer 1", size, 0x20, 0x400);
    m_frameBuffer2 = DWI_allocalign("FrameBuffer 2", size, 0x20, 0x400);
    m_pDrawBuffer = m_frameBuffer2;
    VIConfigure(m_pRenderMode);
    m_fifo = DWI_allocalign("Fifo", 0x40000, 0x20, 0x400);
    m_pFifoObj = GXInit(m_fifo, 0x40000);

    GXSetViewport(0.0f, 0.0f, m_pRenderMode->fbWidth, m_pRenderMode->efbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, m_pRenderMode->fbWidth, m_pRenderMode->efbHeight);
    u16 xfbHeight = GXSetDispCopyYScale(GXGetYScaleFactor(m_pRenderMode->efbHeight, m_pRenderMode->xfbHeight));
    GXSetDispCopySrc(0, 0, m_pRenderMode->fbWidth, m_pRenderMode->efbHeight);
    GXSetDispCopyDst(m_pRenderMode->fbWidth, xfbHeight);
    GXSetCopyFilter(m_pRenderMode->aa, m_pRenderMode->sample_pattern, TRUE, m_pRenderMode->vfilter);
    if (m_pRenderMode->aa) {
        GXSetPixelFmt(2, 0);  // RGB565, Z16 (antialiased)
    } else {
        GXSetPixelFmt(0, 0);  // RGB8, Z24
    }
    GXCopyDisp(m_pDrawBuffer, TRUE);
    GXSetDispCopyGamma(0);

    VISetNextFrameBuffer(m_frameBuffer1);
    m_pDrawBuffer = m_frameBuffer2;
    VIFlush();
    VIWaitForRetrace();
    if (m_pRenderMode->viTVmode & 1) {
        VIWaitForRetrace();
    }
    SetFieldViewport(m_pRenderMode);
    GXInvalidateVtxCache();
    return g_prevScreen;
}

// Copies the finished frame to the draw buffer (clearing the EFB) and queues
// it to be shown at the next retrace.
int CScreen::Flip() {
    GXSetZMode(TRUE, 3, TRUE);
    GXSetColorUpdate(m_clear != 0);
    GXColor clearColor = {m_clearColor.r, m_clearColor.g, m_clearColor.b};
    GXSetCopyClear(clearColor, 0xFFFFFF);
    GXCopyDisp(m_pDrawBuffer, TRUE);
    GXDrawDone();
    VISetNextFrameBuffer(m_pDrawBuffer);
    static bool DemoFirstFrame = true;
    if (DemoFirstFrame) {
        VISetBlack(FALSE);
        DemoFirstFrame = false;
    }
    VIFlush();
    g_bSwap = true;
    return 0;
}

// Waits for the queued frame to be shown, advances the fader by the retraces
// that took, and switches to the other frame buffer. Returns the retraces.
int CScreen::Wait() {
    while (g_bSwap == true) {
        SPIN_WAIT();
    }
    u32 retraces = g_FrameNum - g_WaitFrameNum;
    g_WaitFrameNum = g_FrameNum;
    CFader::Update(retraces);
    if (m_pDrawBuffer == m_frameBuffer1) {
        m_pDrawBuffer = m_frameBuffer2;
    } else {
        m_pDrawBuffer = m_frameBuffer1;
    }
    SetFieldViewport(m_pRenderMode);
    GXInvalidateVtxCache();
    if (!m_clear) {
        GXSetColorUpdate(TRUE);
    }
    return retraces;
}

int CScreen::GetVSyncs() {
    return g_FrameNum;
}

void CScreen::SetClear(bool clear) {
    m_clear = clear;
}

void CScreen::SetClearColor(const CColor& color) {
    m_clearColor = color;
}

void CScreen::SetClearRect(const CRect& rect) {
    m_clearRect = rect;
}

int CScreen::GetHeight() {
    return m_height;
}

int CScreen::GetWidth() {
    return m_width;
}

const s32& CScreen::GetFrameFormat() {
    return m_frameFormat;
}

const s32& CScreen::GetDepthFormat() {
    return m_depthFormat;
}

int CScreen::GetDepthBufferMax() {
    switch (m_depthFormat) {
    case 0:
        return 0xFFFF;
    case 1:
        return 0xFFFFFF;
    case 2:
        return 0xFFFFFFF;
    default:
        return 0xFFFFFFF;
    }
}

// No clear while a screenshot is pending.
bool CScreen::GetClear() {
    if (g_bCapture) {
        return false;
    }
    return m_clear;
}

const CColor& CScreen::GetClearColor() {
    return m_clearColor;
}

const CRect& CScreen::GetClearRect() {
    return m_clearRect;
}

void CScreen::Clear() {
    GXCopyDisp(m_pDrawBuffer, TRUE);
}

void CScreen::PutDisp() {}

// View space to screen pixels: x and y from -1..1 to the screen (y down),
// z to the depth buffer's range.
const CMatrix& CScreen::GetTMViewToScreen() {
    if (!m_viewToScreenValid) {
        u32 depthMax = GetDepthBufferMax();
        m_viewToScreen.BuildTrans(CVector3(0.0f, 0.0f, -1.0f));
        m_viewToScreen.Scale(CVector3(m_width * 0.5f, -m_height * 0.5f, -0.5f * depthMax * 0.0625f));
        m_viewToScreenValid = true;
    }
    return m_viewToScreen;
}

// Requests a screenshot (debug).
void CScreen::Capture() {
    g_bCapture = true;
}

void CScreen::_Capture() {
    DebugMsg("CScreen::_Capture() does nothing...\n");
}

CScreenFieldMode::CScreenFieldMode(EScreenWidth width, EFrameFormat frameFormat, EDepthFormat depthFormat)
    : CScreen(width, frameFormat, depthFormat) {}

CScreen* CScreenFieldMode::SetCurrent() {
    m_clearRect.left = 0;
    m_clearRect.top = 0;
    m_clearRect.right = m_width;
    m_clearRect.bottom = m_height;
    Rebuild();
    return CScreen::SetCurrent();
}

CScreenFieldMode::~CScreenFieldMode() {}

void CScreenFieldMode::Rebuild() {}

void CScreenFieldMode::SetDraw() {}

void CScreenFieldMode::PutDisp() {}

// Writes the frame as host0:screen_NNNN.ssh (debug; the copy of the frame
// into the buffer is compiled out, so only the header and an empty image are
// written).
void CScreenFieldMode::_Capture() {
    char path[256];
    static int _captureNum = 0;
    sprintf(path, "host0:screen_%04d.ssh", _captureNum++);
    int file = File_Open(path, (EOpenMode)0x62);
    if (file < 0) {
        return;
    }

    int bytesPerPixel;
    switch (m_frameFormat) {
    case 0:
        bytesPerPixel = 2;
        break;
    case 1:
        bytesPerPixel = 3;
        break;
    default:
        bytesPerPixel = 4;
        break;
    }
    int stride = m_width * bytesPerPixel;
    int size = stride * m_height;
    int height = m_height;
    int rowsPerCopy = 0x7FFF0 / stride;
    u16* pixels = (u16*)DWI_alloc(NULL, size, 0x400);
    for (int row = 0; row < height;) {
        int end = row + rowsPerCopy;
        if (end > height) {
            end = height;
            rowsPerCopy = height - row;
        }
        row = end;
    }

    SSHFileHeader fileHeader;
    fileHeader.magic[0] = 'S';
    fileHeader.magic[1] = 'H';
    fileHeader.magic[2] = 'P';
    fileHeader.magic[3] = 'S';
    fileHeader.size = size + 0x30;
    fileHeader.numImages = 1;
    fileHeader.id[0] = 'G';
    fileHeader.id[1] = 'R';
    fileHeader.id[2] = 'A';
    fileHeader.id[3] = 'B';
    SSHDirEntry dir;
    dir.name = 0;
    dir.offset = 0x20;
    dir.field_0xC = 0;
    dir.field_0x8 = 0;
    SSHImageHeader image;
    u8 format;
    if (m_frameFormat == 0) {
        format = 3;
    } else {
        format = 5;
        if (m_frameFormat == 1) {
            format = 4;
        }
    }
    image.format = format;
    image.flag = 0;
    image.nextOffset = 0;
    image.width = m_width;
    image.height = m_height;
    image.centerX = 0;
    image.centerY = 0;
    image.x = 0;
    image.flag0 = 0;
    image.flag1 = 0;
    image.flag2 = 0;
    image.flag3 = 0;
    image.y = 0;
    image.levels = 0;

    // 16-bit pixels get their alpha bit set.
    if (m_frameFormat == 0) {
        int count = m_width * m_height;
        for (int i = 0; i < count; i++) {
            pixels[i] |= 0x8000;
        }
    }
    File_Seek(file, 0, (ESeekOrigin)0);
    File_Write(file, &fileHeader, sizeof(fileHeader));
    File_Write(file, &dir, sizeof(dir));
    File_Write(file, &image, sizeof(image));
    File_Write(file, pixels, size);
    File_Close(file);
    MEM_free(pixels);
}
