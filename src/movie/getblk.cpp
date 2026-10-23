// The decoder's block decoding (MSSG getblk.c), with the bit reader inlined
// on the base layer. Coefficients come out 16 times MSSG's (the GameCube
// IDCT's fixed point scale), without MPEG-1 mismatch control; MPEG-2
// coefficients are stored through MPDtranspose_table.
#include "movie/mpeg2dec.h"

// getbits.cpp's reader, inlined on MPDbase.
static inline void Flush_Base(int n) {
    int Incnt;
    MPDbase.Bfr <<= n;
    Incnt = MPDbase.Incnt - n;
    while (Incnt <= 24) {
        MPDbase.Bfr |= *MPDbase.Rdptr++ << (24 - Incnt);
        Incnt += 8;
    }
    MPDbase.Incnt = Incnt;
}

static inline unsigned int Show_Base(int n) {
    return TOP_BITS(MPDbase.Bfr, n);
}

static inline unsigned int Get_Base(int n) {
    unsigned int val = TOP_BITS(MPDbase.Bfr, n);
    Flush_Base(n);
    return val;
}

// The DCT coefficient table entry for the next code (NULL: a bad code).
// `first`: the first coefficient of a non-intra block; `intra_vlc`: table
// B-15 (intra_vlc_format).
static inline DCTtab* DCT_Lookup(unsigned int code, int first, int intra, int intra_vlc) {
    if (code >= 16384 && !intra_vlc) {
        if (first) {
            return &DCTtabfirst[(code >> 12) - 4];
        }
        return &DCTtabnext[(code >> 12) - 4];
    }
    if (code >= 1024) {
        return intra_vlc ? &DCTtab0a[(code >> 8) - 4] : &DCTtab0[(code >> 8) - 4];
    }
    if (code >= 512) {
        return intra_vlc ? &DCTtab1a[(code >> 6) - 8] : &DCTtab1[(code >> 6) - 8];
    }
    if (code >= 256) {
        return &DCTtab2[(code >> 4) - 16];
    }
    if (code >= 128) {
        return &DCTtab3[(code >> 3) - 16];
    }
    if (code >= 64) {
        return &DCTtab4[(code >> 2) - 16];
    }
    if (code >= 32) {
        return &DCTtab5[(code >> 1) - 16];
    }
    if (code >= 16) {
        return &DCTtab6[code - 16];
    }
    return NULL;
}

// decode one intra coded MPEG-1 block
void Decode_MPEG1_Intra_Block(int comp, int dc_dct_pred[]) {
    int val, i, j, sign;
    unsigned int code;
    DCTtab* tab;
    short* bp;

    bp = MPDld->block[comp];

    // ISO/IEC 11172-2 section 2.4.3.7: Block layer.
    // decode DC coefficients
    if (comp < 4) {
        bp[0] = (dc_dct_pred[0] += Get_Luma_DC_dct_diff()) << 7;
    } else if (comp == 4) {
        bp[0] = (dc_dct_pred[1] += Get_Chroma_DC_dct_diff()) << 7;
    } else {
        bp[0] = (dc_dct_pred[2] += Get_Chroma_DC_dct_diff()) << 7;
    }

    if (MPDfault_flag) {
        return;
    }

    // D-pictures do not contain AC coefficients
    if (MPDpicture_coding_type == D_TYPE) {
        return;
    }

    // decode AC coefficients
    for (i = 1;; i++) {
        code = Show_Base(16);
        tab = DCT_Lookup(code, 0, 1, 0);
        if (tab == NULL) {
            MPDfault_flag = 1;
            return;
        }

        Flush_Base(tab->len);

        if (tab->run == 64) {  // end_of_block
            return;
        }

        if (tab->run == 65) {  // escape
            i += Get_Base(6);

            val = Get_Base(8);
            if (val == 0) {
                val = Get_Base(8);
            } else if (val == 128) {
                val = Get_Base(8) - 256;
            } else if (val > 128) {
                val -= 256;
            }

            if ((sign = (val < 0))) {
                val = -val;
            }
        } else {
            i += tab->run;
            val = tab->level;
            sign = Get_Base(1);
        }

        if (i >= 64) {
            MPDfault_flag = 1;
            return;
        }

        j = MPDscan[ZIG_ZAG][i];
        val = (MPDld->intra_quantizer_matrix[j] * (val * MPDld->quantizer_scale)) << 1;

        // saturation
        if (!sign) {
            bp[j] = (val > 2047 * 16) ? 2047 * 16 : val;  // positive
        } else {
            bp[j] = (val > 2048 * 16) ? -2048 * 16 : -val;  // negative
        }
    }
}

// decode one non-intra coded MPEG-1 block
void Decode_MPEG1_Non_Intra_Block(int comp) {
    int val, i, j, sign;
    unsigned int code;
    DCTtab* tab;
    short* bp;

    bp = MPDld->block[comp];

    // decode AC coefficients
    for (i = 0;; i++) {
        code = Show_Base(16);
        tab = DCT_Lookup(code, i == 0, 0, 0);
        if (tab == NULL) {
            MPDfault_flag = 1;
            return;
        }

        Flush_Base(tab->len);

        if (tab->run == 64) {  // end_of_block
            return;
        }

        if (tab->run == 65) {  // escape
            i += Get_Base(6);

            val = Get_Base(8);
            if (val == 0) {
                val = Get_Base(8);
            } else if (val == 128) {
                val = Get_Base(8) - 256;
            } else if (val > 128) {
                val -= 256;
            }

            if ((sign = (val < 0))) {
                val = -val;
            }
        } else {
            i += tab->run;
            val = tab->level;
            sign = Get_Base(1);
        }

        if (i >= 64) {
            MPDfault_flag = 1;
            return;
        }

        j = MPDscan[ZIG_ZAG][i];
        val = MPDld->non_intra_quantizer_matrix[j] * (((val << 1) + 1) * MPDld->quantizer_scale);

        // saturation
        if (!sign) {
            bp[j] = (val > 2047 * 16) ? 2047 * 16 : val;  // positive
        } else {
            bp[j] = (val > 2048 * 16) ? -2048 * 16 : -val;  // negative
        }
    }
}

// decode one intra coded MPEG-2 block
void Decode_MPEG2_Intra_Block(int comp, int dc_dct_pred[]) {
    int val, i, j, sign, cc;
    unsigned int code;
    DCTtab* tab;
    short* bp;
    int* qmat;
    layer_data* ld1;

    ld1 = MPDld;
    bp = ld1->block[comp];

    cc = (comp < 4) ? 0 : (comp & 1) + 1;

    qmat = ld1->intra_quantizer_matrix;  // 4:2:0

    // ISO/IEC 13818-2 section 7.2.1: decode DC coefficients
    if (cc == 0) {
        val = (dc_dct_pred[0] += Get_Luma_DC_dct_diff());
    } else if (cc == 1) {
        val = (dc_dct_pred[1] += Get_Chroma_DC_dct_diff());
    } else {
        val = (dc_dct_pred[2] += Get_Chroma_DC_dct_diff());
    }

    if (MPDfault_flag) {
        return;
    }

    bp[MPDtranspose_table[0]] = val << (7 - MPDintra_dc_precision);

    // decode AC coefficients
    for (i = 1;; i++) {
        code = Show_Base(16);
        tab = DCT_Lookup(code, 0, 1, MPDintra_vlc_format);
        if (tab == NULL) {
            MPDfault_flag = 1;
            return;
        }

        Flush_Base(tab->len);

        if (tab->run == 64) {  // end_of_block
            return;
        }

        if (tab->run == 65) {  // escape
            i += Get_Base(6);

            val = Get_Base(12);
            if ((val & 2047) == 0) {
                MPDfault_flag = 1;
                return;
            }
            if ((sign = (val >= 2048))) {
                val = 4096 - val;
            }
        } else {
            i += tab->run;
            val = tab->level;
            sign = Get_Base(1);
        }

        if (i >= 64) {
            MPDfault_flag = 1;
            return;
        }

        j = MPDscan[ld1->alternate_scan][i];
        val = qmat[j] * (val * ld1->quantizer_scale);
        bp[MPDtranspose_table[j]] = sign ? -val : val;
    }
}

// decode one non-intra coded MPEG-2 block
void Decode_MPEG2_Non_Intra_Block(int comp) {
    int val, i, j, sign;
    unsigned int code;
    DCTtab* tab;
    short* bp;
    int* qmat;

    qmat = MPDld->non_intra_quantizer_matrix;  // 4:2:0

    // decode AC coefficients
    for (i = 0;; i++) {
        code = Show_Base(16);
        tab = DCT_Lookup(code, i == 0, 0, 0);
        if (tab == NULL) {
            MPDfault_flag = 1;
            return;
        }

        Flush_Base(tab->len);

        if (tab->run == 64) {  // end_of_block
            return;
        }

        if (tab->run == 65) {  // escape
            i += Get_Base(6);

            val = Get_Base(12);
            if ((val & 2047) == 0) {
                MPDfault_flag = 1;
                return;
            }
            if ((sign = (val >= 2048))) {
                val = 4096 - val;
            }
        } else {
            i += tab->run;
            val = tab->level;
            sign = Get_Base(1);
        }

        if (i >= 64) {
            MPDfault_flag = 1;
            return;
        }

        j = MPDscan[MPDld->alternate_scan][i];
        val = (qmat[j] * (((val << 1) + 1) * MPDld->quantizer_scale)) >> 1;
        bp = MPDld->block[comp];
        bp[MPDtranspose_table[j]] = sign ? -val : val;
    }
}
