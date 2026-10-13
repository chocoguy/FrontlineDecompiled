#include "ea/snd/eaxadecf.h"

namespace SND {

// Predictor coefficients by filter: s1 * [0][f] + s2 * [1][f].
f32 xafilterf[2][4] = {
    {0.0f, 0.9375f, 1.796875f, 1.53125f},
    {0.0f, 0.0f, -0.8125f, -0.859375f},
};

// Nibble values by shift: (signed nibble) << (12 - shift).
f32 xatablef[16][16] = {
    {0.0f, 4096.0f, 8192.0f, 12288.0f, 16384.0f, 20480.0f, 24576.0f, 28672.0f, -32768.0f, -28672.0f, -24576.0f, -20480.0f, -16384.0f, -12288.0f, -8192.0f, -4096.0f},
    {0.0f, 2048.0f, 4096.0f, 6144.0f, 8192.0f, 10240.0f, 12288.0f, 14336.0f, -16384.0f, -14336.0f, -12288.0f, -10240.0f, -8192.0f, -6144.0f, -4096.0f, -2048.0f},
    {0.0f, 1024.0f, 2048.0f, 3072.0f, 4096.0f, 5120.0f, 6144.0f, 7168.0f, -8192.0f, -7168.0f, -6144.0f, -5120.0f, -4096.0f, -3072.0f, -2048.0f, -1024.0f},
    {0.0f, 512.0f, 1024.0f, 1536.0f, 2048.0f, 2560.0f, 3072.0f, 3584.0f, -4096.0f, -3584.0f, -3072.0f, -2560.0f, -2048.0f, -1536.0f, -1024.0f, -512.0f},
    {0.0f, 256.0f, 512.0f, 768.0f, 1024.0f, 1280.0f, 1536.0f, 1792.0f, -2048.0f, -1792.0f, -1536.0f, -1280.0f, -1024.0f, -768.0f, -512.0f, -256.0f},
    {0.0f, 128.0f, 256.0f, 384.0f, 512.0f, 640.0f, 768.0f, 896.0f, -1024.0f, -896.0f, -768.0f, -640.0f, -512.0f, -384.0f, -256.0f, -128.0f},
    {0.0f, 64.0f, 128.0f, 192.0f, 256.0f, 320.0f, 384.0f, 448.0f, -512.0f, -448.0f, -384.0f, -320.0f, -256.0f, -192.0f, -128.0f, -64.0f},
    {0.0f, 32.0f, 64.0f, 96.0f, 128.0f, 160.0f, 192.0f, 224.0f, -256.0f, -224.0f, -192.0f, -160.0f, -128.0f, -96.0f, -64.0f, -32.0f},
    {0.0f, 16.0f, 32.0f, 48.0f, 64.0f, 80.0f, 96.0f, 112.0f, -128.0f, -112.0f, -96.0f, -80.0f, -64.0f, -48.0f, -32.0f, -16.0f},
    {0.0f, 8.0f, 16.0f, 24.0f, 32.0f, 40.0f, 48.0f, 56.0f, -64.0f, -56.0f, -48.0f, -40.0f, -32.0f, -24.0f, -16.0f, -8.0f},
    {0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f, 28.0f, -32.0f, -28.0f, -24.0f, -20.0f, -16.0f, -12.0f, -8.0f, -4.0f},
    {0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f, -16.0f, -14.0f, -12.0f, -10.0f, -8.0f, -6.0f, -4.0f, -2.0f},
    {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, -8.0f, -7.0f, -6.0f, -5.0f, -4.0f, -3.0f, -2.0f, -1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
};

// Decodes p->count samples (rounded up to whole blocks) to p->dst.
void decodexac(MXAPACKETF* p) {
    u8* start = p->src;
    p->dst[-2] = p->s2;
    p->dst[-1] = p->s1;
    while (p->count > 0) {
        p->count -= 28;
        int filter = *p->src >> 4;
        f32 k1 = xafilterf[0][filter];
        f32 k2 = xafilterf[1][filter];
        f32* table = xatablef[*p->src++ & 0xF];
        int i = 0;
        for (int j = 0; j < 7; j++) {
            u8 b = p->src[i++];
            p->dst[0] = p->dst[-1] * k1 + table[b >> 4] + p->dst[-2] * k2;
            p->dst[1] = p->dst[0] * k1 + table[b & 0xF] + p->dst[-1] * k2;
            p->dst += 2;
            b = p->src[i++];
            p->dst[0] = p->dst[-1] * k1 + table[b >> 4] + p->dst[-2] * k2;
            p->dst[1] = p->dst[0] * k1 + table[b & 0xF] + p->dst[-1] * k2;
            p->dst += 2;
        }
        p->src += 14;
    }
    p->s2 = p->dst[-2];
    p->s1 = p->dst[-1];
    p->src = start;
}

void* CEAXABLKDecf::operator new(unsigned long size) {
    return CODANew(size);
}

void CEAXABLKDecf::operator delete(void* ptr) {
    CODADelete(ptr);
}

CEAXABLKDecf::CEAXABLKDecf() {
    size = 0;
    remaining = 0;
    buffered = 0;
    pkt.s1 = 0.0f;
    pkt.s2 = 0.0f;
}

// Starts a new run of blocks; fails while the previous one has samples left.
int CEAXABLKDecf::Feed(void* data, int size, int samples) {
    if (data == NULL) {
        return -1;
    }
    if (remaining != 0) {
        return -1;
    }
    remaining = samples;
    this->size = size;
    pkt.src = (u8*)data;
    return 0;
}

// Decodes up to `samples` to *out (which is not advanced). Returns the samples written.
int CEAXABLKDecf::Decode(f32** out, int samples) {
    int done = 0;
    pkt.dst = *out;
    int n = remaining;
    if (n == 0) {
        return 0;
    }
    if (samples < n) {
        n = samples;
    }
    int left = n;
    if (buffered != 0) {
        if (buffered <= n) {
            n = buffered;
        }
        for (int i = 0; i < n; i++) {
            *pkt.dst = *bufPos;
            pkt.dst++;
            bufPos++;
        }
        done = n;
        left -= n;
        remaining -= n;
        buffered -= n;
    }
    int whole = left / 28 * 28;
    pkt.count = whole;
    int rest = left - pkt.count;
    if (pkt.count > 0) {
        decodexac(&pkt);
        done += whole;
        remaining -= whole;
        pkt.src += whole / 28 * 15;
    }
    if (rest > 0) {
        // Decode the next block into buf and hand out the first `rest` samples.
        f32* dst = pkt.dst;
        pkt.dst = buf;
        pkt.count = rest;
        decodexac(&pkt);
        remaining -= rest;
        pkt.src += 15;
        bufPos = pkt.dst + pkt.count;
        buffered = -pkt.count;
        pkt.dst -= 28;
        for (int i = 0; i < rest; i++) {
            dst[i] = pkt.dst[i];
        }
        done += rest;
    }
    if (remaining <= 0) {
        buffered = 0;
    }
    return done;
}

XAFSTATE CEAXABLKDecf::GetState() {
    XAFSTATE state;
    state.s1 = pkt.s1;
    state.s2 = pkt.s2;
    return state;
}

void CEAXABLKDecf::SetState(XAFSTATE* state) {
    pkt.s1 = state->s1;
    pkt.s2 = state->s2;
}

}  // namespace SND
