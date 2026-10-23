// GCSW_VD: draws movie frames by converting them in software into the
// external frame buffer (YUYV words, through the write-gather pipe where
// aligned). Each source row is written to two buffer lines.
#include "movie/swvideodisplaylist.h"
#include "dolphin/os.h"
#include "dolphin/vi.h"

GCSW_VD::GCSW_VD(RCMP::DECODER* decoder, RCMP::FRAME* frame, GXRenderModeObj* rmode, void* xfb) {
    m_decoder = decoder;
    m_renderMode = rmode;
    m_xfb = xfb;
    m_frame = frame;
    m_x = (m_renderMode->fbWidth - frame->m_shape->width) / 2;
    m_y = ((m_renderMode->efbHeight >> 1) - frame->m_shape->height) / 2;
}

RCMP::VIDEO_DRIVER* RCMP::GCSW_VD_create(DECODER* decoder, FRAME* frame, _GXRenderModeObj* rmode, void* xfb) {
    return new GCSW_VD(decoder, frame, rmode, xfb);
}

GCSW_VD::~GCSW_VD() {}

// Writes one row: YUYV words from luma pairs and the chroma samples.
static void YUV_WriteRow(unsigned char* y, unsigned char* u, unsigned char* v, char* dst, int width) {
    int half = width >> 1;
    int i = 0;
    int yi = 0;
    u32* d = (u32*)dst;
    unsigned char* up = u;
    unsigned char* vp = v;

    // until the destination is aligned
    while (i < half && ((uptr)(dst) & 0xF) != 0) {
        *d++ = (y[yi] << 24) | (*up << 16) | (y[yi + 1] << 8) | *vp;
        yi += 2;
        up++;
        vp++;
        i++;
    }
    if (i > half) {
        return;
    }
    GXRedirectWriteGatherPipe(dst);
    for (;;) {
        if (i + 8 < half) {
            unsigned char* yp = &y[yi];
            unsigned char* u8p = &u[i];
            unsigned char* v8p = &v[i];
            i += 8;
            yi = i * 2;
            GXWGFifo.u32 = (yp[0] << 24) | (u8p[0] << 16) | (yp[1] << 8) | v8p[0];
            GXWGFifo.u32 = (yp[2] << 24) | (u8p[1] << 16) | (yp[3] << 8) | v8p[1];
            GXWGFifo.u32 = (yp[4] << 24) | (u8p[2] << 16) | (yp[5] << 8) | v8p[2];
            GXWGFifo.u32 = (yp[6] << 24) | (u8p[3] << 16) | (yp[7] << 8) | v8p[3];
            GXWGFifo.u32 = (yp[8] << 24) | (u8p[4] << 16) | (yp[9] << 8) | v8p[4];
            GXWGFifo.u32 = (yp[10] << 24) | (u8p[5] << 16) | (yp[11] << 8) | v8p[5];
            GXWGFifo.u32 = (yp[12] << 24) | (u8p[6] << 16) | (yp[13] << 8) | v8p[6];
            GXWGFifo.u32 = (yp[14] << 24) | (u8p[7] << 16) | (yp[15] << 8) | v8p[7];
        } else if (i < half) {
            ((u32*)dst)[i] = (y[yi] << 24) | (u[i] << 16) | (y[yi + 1] << 8) | v[i];
            i++;
            yi = i * 2;
        } else {
            break;
        }
    }
    GXRestoreWriteGatherPipe();
}

// Writes the Y/U/V `planes` (width x height, luma rows `stride` apart) at
// (x, y) of the frame buffer, clipped to clipW x clipH. The luma row step
// is the clipped width (as in the original).
static int YUV_convert(unsigned char** planes, int x, int y, int stride, int width, int height, void* xfb,
                       int clipW, int clipH, int fbStride) {
    int halfW = width >> 1;
    if (x < 0) {
        int skip = -x;
        x = 0;
        width -= skip;
        planes[0] += skip;
        planes[1] += skip / 2;
        planes[2] += skip / 2;
    }
    if (y < 0) {
        int skip = -y;
        height -= skip;
        planes[0] += skip * stride;
        planes[1] += skip / 2 * (stride / 2);
        planes[2] += skip / 2 * (stride / 2);
        y = 0;
    }
    if (x + width > clipW) {
        width -= x + width - clipW;
    }
    if (y + height > clipH) {
        height -= y + height - clipH;
    }

    int lineBytes = fbStride * 2;
#ifdef FRONTLINE_NATIVE
    char* base = (char*)xfb + x * 2;  // no uncached mirror natively
#else
    char* base = (char*)xfb + 0x40000000 + x * 2;  // uncached
#endif
    int line = y;
    int yOffset = 0;
    int next = 1;
    for (int row = 0; row < height; row++) {
        int c = halfW * (row >> 1);
        unsigned char* yp = planes[0] + yOffset;
        unsigned char* up = planes[1] + c;
        unsigned char* vp = planes[2] + c;
        YUV_WriteRow(yp, up, vp, base + lineBytes * line, width);
        YUV_WriteRow(yp, up, vp, base + lineBytes * (y + next), width);
        yOffset += width;
        line += 2;
        next += 2;
    }
    return 0;
}

void GCSW_VD::SetPosition(float x, float y, float z) {
    m_x = x;
    m_y = y;
}

void GCSW_VD::SetScale(float x, float y) {}

void GCSW_VD::Draw(RCMP::FRAME* frame) {
    m_frame = frame;
    ReDraw();
}

void GCSW_VD::ReDraw() {
    RCMP::SHAPE* shape = m_frame->m_shape;
    void* xfb = m_xfb;
    int fbStride = (m_renderMode->fbWidth + 15) & 0xFFF0;
    unsigned char* planes[3];
    u8* data = shape->Data();
    int width = shape->width;
    int height = shape->height;
    planes[0] = data;
    planes[1] = data + width * height;
    planes[2] = planes[1] + width / 2 * (height / 2);
    VIWaitForRetrace();
    YUV_convert(planes, m_x, m_y, width, width, height, xfb, m_renderMode->fbWidth, m_renderMode->efbHeight >> 1,
                fbStride);
}

void GCSW_VD::CompleteDraw() {
    m_decoder->ReleaseFrame(m_frame);
    m_frame = NULL;
}
