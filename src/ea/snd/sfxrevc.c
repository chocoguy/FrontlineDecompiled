#include "ea/snd/smix.h"
#include <string.h>

// State size of each graph module type.
static s32 filtersizetable[10] = {0x20, 0x28, 0x58, 0x58, 0x58, 0x40, 0x20, 0x34, 0x24, 0x28};

// phead slot (1-based mixer output) of a module's channel, by [valid outputs - 1][outputs - 1]
// [channel - 1].
u8 mapchannel[6][6][6] = {
    {
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0},
        {1, 2, 0, 0, 0, 0},
        {1, 3, 0, 0, 0, 0},
        {1, 3, 0, 0, 0, 0},
        {1, 4, 0, 0, 0, 0},
        {1, 4, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 2, 3, 0, 0, 0},
        {1, 0, 3, 0, 0, 0},
        {2, 1, 4, 0, 0, 0},
        {2, 1, 4, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 2, 3, 4, 0, 0},
        {1, 3, 4, 5, 0, 0},
        {1, 3, 4, 5, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 2, 3, 4, 5, 0},
        {1, 2, 3, 4, 5, 0},
    },
    {
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 2, 3, 4, 5, 6},
    },
};

SNDFBSTATE sndfb[11];
s32 sndfxnummodules[3];
u8 filehead[0x38];
u8* pheaderinfo;
SFILTER** pfilterlist;
SFILTER** phead;
u8* plisthead;
u8* pconhead;
s32 lastoutputchannels;
s32 validstructoutputs;
u8* modlist;
u8* conlist;
s32 SNDMIXfxv2inited;

void SNDMIXI_restorefx2() {
    int n = SNDI_getb(pheaderinfo + (lastoutputchannels - 1) * 4 + 8, 4);
    if (n != 0) {
        for (int i = 0; i < n; i++) {
            if (pfilterlist[i]->destroy != NULL) {
                pfilterlist[i]->destroy(pfilterlist[i]);
            }
            if (pfilterlist[i] != NULL) {
                SNDMEMI_free(pfilterlist[i]);
                pfilterlist[i] = NULL;
            }
        }
    }
    if (pfilterlist != NULL) {
        SNDMEMI_free(pfilterlist);
        pfilterlist = NULL;
    }
    if (phead[0] != NULL) {
        phead[0] = NULL;
    }
    if (phead[1] != NULL) {
        phead[1] = NULL;
    }
    if (phead[2] != NULL) {
        phead[2] = NULL;
    }
    if (phead[3] != NULL) {
        phead[3] = NULL;
    }
    if (phead[4] != NULL) {
        phead[4] = NULL;
    }
    if (phead[5] != NULL) {
        phead[5] = NULL;
    }
    if (phead != NULL) {
        SNDMEMI_free(phead);
        phead = NULL;
    }
    modlist = NULL;
    conlist = NULL;
    pconhead = NULL;
    plisthead = NULL;
    SNDMIXfxv2inited = 0;
}

// Builds the filter graph for the current output configuration.
void SNDMIXI_initfx(int rate) {
    int params[4];
    int i;

    pheaderinfo = filehead;
    phead = (SFILTER**)SNDMEMI_allocz(GC_SIZE(0x1C, 7 * sizeof(SFILTER*)));
    memset(phead, 0, GC_SIZE(0x1C, 7 * sizeof(SFILTER*)));
    int cfg = (validstructoutputs - 1) * 4;
    plisthead = modlist;
    pconhead = conlist;
    int n = SNDI_getb(pheaderinfo + cfg + 8, 4);
    pfilterlist = (SFILTER**)SNDMEMI_allocz(n * GC_SIZE(4, sizeof(SFILTER*)));
    u8* m = plisthead;
    int r = rate << 8;
    for (i = 0; i < n; i++) {
        pfilterlist[i] = (SFILTER*)SNDMEMI_allocz(filtersizetable[m[0]]);
        memset(params, 0, sizeof(params));
        SFILTER* f;
        switch (m[0]) {
        case 0:
            SFILTER_createSOURCE((SRCSTATE*)pfilterlist[i]);
            break;
        case 1:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 4, 2) << 8;
            params[1] = r;
            params[2] = SNDI_getb(m + 6, 2);
            SFILTER_createLPFRC((LPFRCSTATE*)f);
            SFILTER_modifyLPFRC((LPFRCSTATE*)f, params);
            break;
        case 2:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 2, 2) << 8;
            params[1] = r;
            SFILTER_createLPFFIR8((LPFSTATE*)f);
            SFILTER_modifyLPFFIR8((LPFSTATE*)f, params);
            break;
        case 3:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 2, 2) << 8;
            params[1] = r;
            SFILTER_createHPFFIR8((HPFSTATE*)f);
            SFILTER_modifyHPFFIR8((HPFSTATE*)f, params);
            break;
        case 4:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 4, 2) << 8;
            params[1] = SNDI_getb(m + 6, 2) << 8;
            params[2] = r;
            SFILTER_createBPFFIR8((BPFSTATE*)f);
            SFILTER_modifyBPFFIR8((BPFSTATE*)f, params);
            break;
        case 5:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 2, 2) << 8;
            params[1] = r;
            params[2] = SNDI_getb(m + 4, 2) << 8;
            params[3] = SNDI_getb(m + 6, 2);
            SFILTER_createRESON((RESONSTATE*)f);
            SFILTER_modifyRESON((RESONSTATE*)f, params);
            break;
        case 6:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 2, 2);
            SFILTER_createAMPLF((AMPLFSTATE*)f);
            SFILTER_modifyAMPLF((AMPLFSTATE*)f, params);
            break;
        case 7:
            f = pfilterlist[i];
            params[0] = SNDI_getb(m + 2, 2);
            params[1] = r;
            params[2] = SNDI_getb(m + 4, 2) << 8;
            SFILTER_createECHO((ECHOSTATE*)f);
            SFILTER_modifyECHO((ECHOSTATE*)f, params);
            break;
        case 8:
            SFILTER_createMIX((MIXERSTATE*)pfilterlist[i]);
            break;
        case 9:
            SFILTER_createSPLIT((SPLITSTATE*)pfilterlist[i]);
            break;
        }
        if (m[1] != 0) {
            phead[mapchannel[validstructoutputs - 1][sndmix.numOutputs - 1][m[1] - 1]] = pfilterlist[i];
        }
        m += filtermodsizetable[m[0]];
    }
    u8* c = pconhead;
    for (i = 0; i < SNDI_getb(pheaderinfo + cfg + 0x20, 4); i++) {
        SFILTER* to = pfilterlist[SNDI_getb(c + 4, 4)];
        SFILTER_connect(pfilterlist[SNDI_getb(c, 4)], to, c[8], c[9]);
        c += 12;
    }
}

// Loads a filter graph file, using the largest configuration up to the output count.
void SNDMIXI_initfx2(int rate, MIXFXFILTHEADER* hdr) {
    int i;
    int j;
    SNDMIXfxv2inited = 1;
    SNDI_memcpy(filehead, hdr, 0x38);
    u8* p = (u8*)hdr + 0x38;
    int cfg = sndmix.numOutputs - 1;
    u8* h = filehead + cfg * 4;
    if (SNDI_getb(h + 8, 4) != 0) {
        validstructoutputs = sndmix.numOutputs;
    } else {
        while (SNDI_getb(h + 8, 4) == 0) {
            h -= 4;
            cfg--;
        }
        validstructoutputs = cfg + 1;
    }
    u8* q = filehead;
    lastoutputchannels = validstructoutputs;
    for (i = 0; i < cfg; i++) {
        int n = SNDI_getb(q + 8, 4);
        for (j = 0; j < n; j++) {
            p += filtermodsizetable[*p];
        }
        p += SNDI_getb(q + 0x20, 4) * 12;
        q += 4;
    }
    modlist = p;
    int n = SNDI_getb(filehead + cfg * 4 + 8, 4);
    for (j = 0; j < n; j++) {
        p += filtermodsizetable[*p];
    }
    conlist = p;
    SNDMIXI_initfx(rate);
}

// Same as SNDI_findprime.
int findprime(int ms, int rate) {
    int n = ms * rate / 1000;
    if (n < 3) {
        n = 3;
    }
    for (;;) {
        int root;
        int i = 0;
        for (;;) {
            int prev = i++;
            if (i * prev == n) {
                root = i - 1;
                break;
            }
            if (i * prev > n) {
                root = i - 2;
                break;
            }
        }
        int limit = root + 1;
        for (int d = 2; d <= limit; d++) {
            if (n % d == 0) {
                n++;
                break;
            }
            if (d == limit) {
                return n;
            }
        }
    }
}

extern "C" void MIX_initreverb(int rate, MIXFXHEADER* hdr) {
    MIX_restorereverb();
    sndmix.fxinit = SNDMIXI_fxinit;
    if (hdr->type == 0) {
        sndmix.postmix = SNDMIXI_fxadd;
    } else if (hdr->type == 10) {
        SNDMIXI_initfx2(rate, (MIXFXFILTHEADER*)hdr);
        sndmix.postmix = SNDMIXI_modlapifxadd;
        sndmix.fxMode[0] = 1;
        return;
    }
    s32* count = sndfxnummodules;
    SNDFBSTATE* fb = sndfb;
    MIXFXMODULE* mod = hdr->modules;
    for (int g = 0; g < 3; g++) {
        MIXFXMODULE* m = mod;
        SNDFBSTATE* f = fb;
        *count = hdr->numModules[g];
        for (int i = 0; i < *count; i++) {
            f->lpa = m->damping / 127.0f;
            f->lpb = 1.0f - f->lpa;
            f->lpa *= (hdr->damping + 100) / 350.0f;
            f->lpb *= (hdr->damping + 100) / 350.0f;
            f->g = m->feedback / 127.0f;
            f->negg = -f->g;
            int len = findprime(rate, SNDI_getb(m->delay, 2));
            f->buf = (f32*)SNDMEMI_allocz(len * 4);
            memset(f->buf, 0, len * 4);
            f->len = len;
            m++;
            mod++;
            f->rpos = len;
            fb++;
            f->wpos = 0;
            f->lp = 0.0f;
            f++;
        }
        count++;
    }
    SNDMIXI_fxinit(hdr);
    sndmix.fxMode[0] = 2;
}

extern "C" void MIX_restorereverb(void) {
    sndmix.fxinit = NULL;
    sndmix.postmix = NULL;
    if (SNDMIXfxv2inited) {
        SNDMIXI_restorefx2();
    }
    for (int i = 0; i < 11; i++) {
        if (sndfb[i].buf != NULL) {
            SNDMEMI_free(sndfb[i].buf);
            sndfb[i].buf = NULL;
        }
    }
    sndmix.fxMode[0] = 0;
}
