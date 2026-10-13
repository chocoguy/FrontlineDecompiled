// The decoder's picture decoding (MSSG getpic.c) for 4:2:0 streams,
// decoding into the codec's frame buffers. Blocks are inverse transformed
// as they are decoded and cleared as they are added to the picture.
#include "movie/mpeg2dec.h"
#include "movie/rcmp_mpc_codec.h"

static void Update_Picture_Buffers(MPC_CODEC_INTERNAL* codec);
static void picture_data();
static int slice(int MBAmax);
static int start_of_slice(int MBAmax, int* MBA, int* MBAinc, int dc_dct_pred[3], int PMV[2][2][2]);
static int decode_macroblock(int* macroblock_type, int* stwtype, int* stwclass, int* motion_type, int* dct_type,
                             int PMV[2][2][2], int dc_dct_pred[3], int motion_vertical_field_select[2][2],
                             int dmvector[2]);
static void macroblock_modes(int* pmacroblock_type, int* pstwtype, int* pstwclass, int* pmotion_type,
                             int* pmotion_vector_count, int* pmv_format, int* pdmv, int* pmvscale, int* pdct_type);
static void skipped_macroblock(int dc_dct_pred[3], int PMV[2][2][2], int* motion_type,
                               int motion_vertical_field_select[2][2], int* stwtype, int* macroblock_type);
static void motion_compensation(int MBA, int macroblock_type, int motion_type, int PMV[2][2][2],
                                int motion_vertical_field_select[2][2], int dmvector[2], int stwtype, int dct_type);
static void Add_Block(int comp, int bx, int by, int dct_type, int addflag);
static void frame_reorder(int Bitstream_Framenum);

// Decodes one picture into the codec's buffers.
void Decode_Picture(MPC_CODEC_INTERNAL* codec, int bitstream_framenum) {
    if (MPDpicture_structure == FRAME_PICTURE && MPDsecond_field) {
        // odd number of field pictures
        MPDsecond_field = 0;
    }

    Update_Picture_Buffers(codec);

    picture_data();

    frame_reorder(bitstream_framenum);

    if (MPDpicture_structure != FRAME_PICTURE) {
        MPDsecond_field = !MPDsecond_field;
    }
}

// Points `planes` at frame's Y, U and V planes (NULL without a frame).
static inline void Set_Planes(unsigned char* planes[3], MPC_FRAME* frame) {
    if (frame != NULL) {
        planes[0] = frame->m_shape->Data();
        planes[1] = frame->m_shape->Data() + frame->m_shape->width * frame->m_shape->height;
        planes[2] = frame->m_shape->Data() + (frame->m_shape->width * frame->m_shape->height +
                                              frame->m_shape->width / 2 * (frame->m_shape->height / 2));
    } else {
        planes[0] = NULL;
        planes[1] = NULL;
        planes[2] = NULL;
    }
}

// Picks the frames to decode into and predict from: a B picture decodes
// into a new frame shown now; an I or P picture into a new reference frame,
// showing the previous one (the older reference is released).
static void Update_Picture_Buffers(MPC_CODEC_INTERNAL* codec) {
    switch (MPDpicture_coding_type) {
    case B_TYPE:
        codec->m_current = codec->m_display = codec->CreateB();
        break;
    case I_TYPE:
    case P_TYPE:
        if (!MPDsecond_field) {
            if (codec->m_forward != NULL) {
                codec->ReleaseFrame(codec->m_forward);
            }
            codec->m_forward = codec->m_backward;
            codec->m_display = codec->m_forward;
            codec->m_current = codec->m_backward = codec->CreateIorP();
        }
        break;
    }

    Set_Planes(MPDbackward_reference_frame, codec->m_backward);
    Set_Planes(MPDforward_reference_frame, codec->m_forward);
    Set_Planes(MPDcurrent_frame, codec->m_current);

    if (MPDpicture_structure == BOTTOM_FIELD) {
        MPDcurrent_frame[0] += MPDcoded_picture_width;
        MPDcurrent_frame[1] += MPDchroma_width;
        MPDcurrent_frame[2] += MPDchroma_width;
    }
}

// decode all macroblocks of the current picture
static void picture_data() {
    int MBAmax;

    // number of macroblocks per picture
    MBAmax = MPDmb_width * MPDmb_height;

    if (MPDpicture_structure != FRAME_PICTURE) {
        MBAmax >>= 1;  // field picture has half as many macroblocks as frame
    }

    while (slice(MBAmax) >= 0) {
    }
}

// decode slice header and all macroblocks in the slice: -1 at the end of
// the picture, 0 to go to the next slice
static int slice(int MBAmax) {
    int MBA;
    int MBAinc, macroblock_type, motion_type, dct_type;
    int dc_dct_pred[3];
    int PMV[2][2][2], motion_vertical_field_select[2][2];
    int dmvector[2];
    int stwtype, stwclass;
    int ret;

    MBA = 0;  // macroblock address
    MBAinc = 0;

    if ((ret = start_of_slice(MBAmax, &MBA, &MBAinc, dc_dct_pred, PMV)) != 1) {
        return ret;
    }

    MPDfault_flag = 0;

    for (;;) {
        // this is how we properly exit out of picture
        if (MBA >= MBAmax) {
            return -1;  // all macroblocks decoded
        }

        MPDld = &MPDbase;

        if (MBAinc == 0) {
            if (!Show_Bits(23) || MPDfault_flag) {  // next_start_code or fault
            resync:  // if Fault_Flag: resynchronize to next next_start_code
                MPDfault_flag = 0;
                return 0;  // trigger: go to next slice
            } else {
                // decode macroblock address increment
                MBAinc = Get_macroblock_address_increment();

                if (MPDfault_flag) {
                    goto resync;
                }
            }
        }

        if (MBA >= MBAmax) {
            // MBAinc points beyond picture dimensions
            return -1;
        }

        if (MBAinc == 1) {  // not skipped
            ret = decode_macroblock(&macroblock_type, &stwtype, &stwclass, &motion_type, &dct_type, PMV, dc_dct_pred,
                                    motion_vertical_field_select, dmvector);

            if (ret == -1) {
                return -1;
            }

            if (ret == 0) {
                goto resync;
            }
        } else {  // MBAinc!=1: skipped macroblock
            // ISO/IEC 13818-2 section 7.6.6
            skipped_macroblock(dc_dct_pred, PMV, &motion_type, motion_vertical_field_select, &stwtype,
                               &macroblock_type);
        }

        // ISO/IEC 13818-2 section 7.6
        motion_compensation(MBA, macroblock_type, motion_type, PMV, motion_vertical_field_select, dmvector, stwtype,
                            dct_type);

        // advance to next macroblock
        MBA++;
        MBAinc--;

        if (MBA >= MBAmax) {
            return -1;  // all macroblocks decoded
        }
    }
}

// set scratch pad macroblock to zero
static inline void Clear_Block(int comp) {}

// ISO/IEC 13818-2 section 6.3.16: -1 at the end of the picture, 0 to go
// to the next slice, 1 to decode its macroblocks
static int start_of_slice(int MBAmax, int* MBA, int* MBAinc, int dc_dct_pred[3], int PMV[2][2][2]) {
    unsigned int code;
    int slice_vert_pos_ext;

    MPDld = &MPDbase;

    MPDfault_flag = 0;

    next_start_code();
    code = Show_Bits(32);

    if (code < SLICE_START_CODE_MIN || code > SLICE_START_CODE_MAX) {
        // only slice headers are allowed in picture_data
        return -1;  // trigger: go to next picture
    }

    Flush_Buffer32();

    // decode slice header (may change quantizer_scale)
    slice_vert_pos_ext = slice_header();

    // decode macroblock address increment
    *MBAinc = Get_macroblock_address_increment();

    if (MPDfault_flag) {
        return 0;  // trigger: go to next slice
    }

    // set current location
    *MBA = ((slice_vert_pos_ext << 7) + (code & 255) - 1) * MPDmb_width + *MBAinc - 1;
    *MBAinc = 1;  // first macroblock in slice: not skipped

    // reset all DC coefficient and motion vector predictors
    // ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    // ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
    PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
    PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;

    // successfull: trigger decode macroblocks in slice
    return 1;
}

// ISO/IEC 13818-2 sections 7.2 through 7.5
static int decode_macroblock(int* macroblock_type, int* stwtype, int* stwclass, int* motion_type, int* dct_type,
                             int PMV[2][2][2], int dc_dct_pred[3], int motion_vertical_field_select[2][2],
                             int dmvector[2]) {
    int quantizer_scale_code;
    int comp;
    int motion_vector_count;
    int mv_format;
    int dmv;
    int mvscale;
    int coded_block_pattern;

    // ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes
    macroblock_modes(macroblock_type, stwtype, stwclass, motion_type, &motion_vector_count, &mv_format, &dmv,
                     &mvscale, dct_type);

    if (MPDfault_flag) {
        return 0;  // trigger: go to next slice
    }

    if (*macroblock_type & MACROBLOCK_QUANT) {
        quantizer_scale_code = Get_Bits(5);

        // ISO/IEC 13818-2 section 7.4.2.2: Quantizer scale factor
        if (MPDld->MPEG2_Flag) {
            MPDld->quantizer_scale = MPDld->q_scale_type ? MPDnon_linear_quantizer_scale[quantizer_scale_code]
                                                         : (quantizer_scale_code << 1);
        } else {
            MPDld->quantizer_scale = quantizer_scale_code;
        }

        // SCALABILITY: Data Partitioning
        if (MPDbase.scalable_mode == SC_DP) {
            // make sure base.quantizer_scale is valid
            MPDbase.quantizer_scale = MPDld->quantizer_scale;
        }
    }

    // motion vectors
    // ISO/IEC 13818-2 section 6.3.17.2: Motion vectors

    // decode forward motion vectors
    if ((*macroblock_type & MACROBLOCK_MOTION_FORWARD) ||
        ((*macroblock_type & MACROBLOCK_INTRA) && MPDconcealment_motion_vectors)) {
        if (MPDld->MPEG2_Flag) {
            motion_vectors(PMV, dmvector, motion_vertical_field_select, 0, motion_vector_count, mv_format,
                           MPDf_code[0][0] - 1, MPDf_code[0][1] - 1, dmv, mvscale);
        } else {
            motion_vector(PMV[0][0], dmvector, MPDforward_f_code - 1, MPDforward_f_code - 1, 0, 0,
                          MPDfull_pel_forward_vector);
        }
    }

    if (MPDfault_flag) {
        return 0;  // trigger: go to next slice
    }

    // decode backward motion vectors
    if (*macroblock_type & MACROBLOCK_MOTION_BACKWARD) {
        if (MPDld->MPEG2_Flag) {
            motion_vectors(PMV, dmvector, motion_vertical_field_select, 1, motion_vector_count, mv_format,
                           MPDf_code[1][0] - 1, MPDf_code[1][1] - 1, 0, mvscale);
        } else {
            motion_vector(PMV[0][1], dmvector, MPDbackward_f_code - 1, MPDbackward_f_code - 1, 0, 0,
                          MPDfull_pel_backward_vector);
        }
    }

    if (MPDfault_flag) {
        return 0;  // trigger: go to next slice
    }

    if ((*macroblock_type & MACROBLOCK_INTRA) && MPDconcealment_motion_vectors) {
        Flush_Buffer(1);  // remove marker_bit
    }

    // macroblock_pattern
    // ISO/IEC 13818-2 section 6.3.17.4: Coded block pattern
    if (*macroblock_type & MACROBLOCK_PATTERN) {
        coded_block_pattern = Get_coded_block_pattern();
    } else {
        coded_block_pattern = (*macroblock_type & MACROBLOCK_INTRA) ? (1 << 6) - 1 : 0;
    }

    if (MPDfault_flag) {
        return 0;  // trigger: go to next slice
    }

    // decode blocks
    for (comp = 0; comp < 6; comp++) {
        Clear_Block(comp);

        if (coded_block_pattern & (1 << (6 - 1 - comp))) {
            if (*macroblock_type & MACROBLOCK_INTRA) {
                if (MPDld->MPEG2_Flag) {
                    Decode_MPEG2_Intra_Block(comp, dc_dct_pred);
                } else {
                    Decode_MPEG1_Intra_Block(comp, dc_dct_pred);
                }
            } else {
                if (MPDld->MPEG2_Flag) {
                    Decode_MPEG2_Non_Intra_Block(comp);
                } else {
                    Decode_MPEG1_Non_Intra_Block(comp);
                }
            }

            // ISO/IEC 13818-2 section Annex A: inverse DCT
            GC_IDCT(MPDld->block[comp]);

            if (MPDfault_flag) {
                return 0;  // trigger: go to next slice
            }
        }
    }

    if (MPDpicture_coding_type == D_TYPE) {
        // remove end_of_macroblock (always 1, prevents startcode emulation)
        // ISO/IEC 11172-2 section 2.4.2.7 and 2.4.3.6
        marker_bit("D picture end_of_macroblock bit");
    }

    // reset intra_dc predictors
    // ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    if (!(*macroblock_type & MACROBLOCK_INTRA)) {
        dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;
    }

    // reset motion vector predictors
    if ((*macroblock_type & MACROBLOCK_INTRA) && !MPDconcealment_motion_vectors) {
        // intra mb without concealment motion vectors
        // ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
        PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
        PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
    }

    // special "No_MC" macroblock_type case
    // ISO/IEC 13818-2 section 7.6.3.5: Prediction in P pictures
    if ((MPDpicture_coding_type == P_TYPE) && !(*macroblock_type & (MACROBLOCK_MOTION_FORWARD | MACROBLOCK_INTRA))) {
        // non-intra mb without forward mv in a P picture
        // ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
        PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;

        // derive motion_type
        // ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes, frame_motion_type
        if (MPDpicture_structure == FRAME_PICTURE) {
            *motion_type = MC_FRAME;
        } else {
            *motion_type = MC_FIELD;
            // predict from field of same parity
            motion_vertical_field_select[0][0] = (MPDpicture_structure == BOTTOM_FIELD);
        }
    }

    if (*stwclass == 4) {
        // purely spatially predicted macroblock
        // ISO/IEC 13818-2 section 7.7.5.1: Resetting motion vector predictions
        PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
        PMV[0][1][0] = PMV[0][1][1] = PMV[1][1][0] = PMV[1][1][1] = 0;
    }

    // successfully decoded macroblock
    return 1;
}

// ISO/IEC 13818-2 section 6.3.17.1: Macroblock modes
static void macroblock_modes(int* pmacroblock_type, int* pstwtype, int* pstwclass, int* pmotion_type,
                             int* pmotion_vector_count, int* pmv_format, int* pdmv, int* pmvscale, int* pdct_type) {
    int macroblock_type;
    int stwtype, stwcode, stwclass;
    int motion_type = 0;
    int motion_vector_count, mv_format, dmv, mvscale;
    int dct_type;
    static unsigned char stwc_table[3][4] = {{6, 3, 7, 4}, {2, 1, 5, 4}, {2, 5, 7, 4}};
    static unsigned char stwclass_table[9] = {0, 1, 2, 1, 1, 2, 3, 3, 4};

    // get macroblock_type
    macroblock_type = Get_macroblock_type();

    if (MPDfault_flag) {
        return;
    }

    // get spatial_temporal_weight_code
    if (macroblock_type & SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG) {
        if (MPDspatial_temporal_weight_code_table_index == 0) {
            stwtype = 4;
        } else {
            stwcode = Get_Bits(2);
            stwtype = stwc_table[MPDspatial_temporal_weight_code_table_index - 1][stwcode];
        }
    } else {
        stwtype = (macroblock_type & PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS) ? 8 : 0;
    }

    // SCALABILITY: derive spatial_temporal_weight_class (Table 7-18)
    stwclass = stwclass_table[stwtype];

    // get frame/field motion type
    if (macroblock_type & (MACROBLOCK_MOTION_FORWARD | MACROBLOCK_MOTION_BACKWARD)) {
        if (MPDpicture_structure == FRAME_PICTURE) {  // frame_motion_type
            motion_type = MPDframe_pred_frame_dct ? MC_FRAME : Get_Bits(2);
        } else {  // field_motion_type
            motion_type = Get_Bits(2);
        }
    } else if ((macroblock_type & MACROBLOCK_INTRA) && MPDconcealment_motion_vectors) {
        // concealment motion vectors
        motion_type = (MPDpicture_structure == FRAME_PICTURE) ? MC_FRAME : MC_FIELD;
    }

    // derive motion_vector_count, mv_format and dmv, (table 6-17, 6-18)
    if (MPDpicture_structure == FRAME_PICTURE) {
        motion_vector_count = (motion_type == MC_FIELD && stwclass < 2) ? 2 : 1;
        mv_format = (motion_type == MC_FRAME) ? MV_FRAME : MV_FIELD;
    } else {
        motion_vector_count = (motion_type == MC_16X8) ? 2 : 1;
        mv_format = MV_FIELD;
    }

    dmv = (motion_type == MC_DMV);  // dual prime

    // field mv predictions in frame pictures have to be scaled
    // ISO/IEC 13818-2 section 7.6.3.1 Decoding process for motion vectors
    mvscale = ((mv_format == MV_FIELD) && (MPDpicture_structure == FRAME_PICTURE));

    // get dct_type (frame DCT / field DCT)
    dct_type = (MPDpicture_structure == FRAME_PICTURE) && (!MPDframe_pred_frame_dct) &&
                       (macroblock_type & (MACROBLOCK_PATTERN | MACROBLOCK_INTRA))
                   ? Get_Bits(1)
                   : 0;

    // return values
    *pmacroblock_type = macroblock_type;
    *pstwtype = stwtype;
    *pstwclass = stwclass;
    *pmotion_type = motion_type;
    *pmotion_vector_count = motion_vector_count;
    *pmv_format = mv_format;
    *pdmv = dmv;
    *pmvscale = mvscale;
    *pdct_type = dct_type;
}

// ISO/IEC 13818-2 section 7.6.6 (the blocks are already clear)
static void skipped_macroblock(int dc_dct_pred[3], int PMV[2][2][2], int* motion_type,
                               int motion_vertical_field_select[2][2], int* stwtype, int* macroblock_type) {
    // reset intra_dc predictors
    // ISO/IEC 13818-2 section 7.2.1: DC coefficients in intra blocks
    dc_dct_pred[0] = dc_dct_pred[1] = dc_dct_pred[2] = 0;

    // reset motion vector predictors
    // ISO/IEC 13818-2 section 7.6.3.4: Resetting motion vector predictors
    if (MPDpicture_coding_type == P_TYPE) {
        PMV[0][0][0] = PMV[0][0][1] = PMV[1][0][0] = PMV[1][0][1] = 0;
    }

    // derive motion_type
    if (MPDpicture_structure == FRAME_PICTURE) {
        *motion_type = MC_FRAME;
    } else {
        *motion_type = MC_FIELD;

        // predict from field of same parity
        // ISO/IEC 13818-2 section 7.6.6.1 and 7.6.6.3: P field picture and B field picture
        motion_vertical_field_select[0][0] = motion_vertical_field_select[0][1] =
            (MPDpicture_structure == BOTTOM_FIELD);
    }

    // skipped I are spatial-only predicted,
    // skipped P and B are temporal-only predicted
    // ISO/IEC 13818-2 section 7.7.6: Skipped macroblocks
    *stwtype = (MPDpicture_coding_type == I_TYPE) ? 8 : 0;

    // clear MACROBLOCK_INTRA
    *macroblock_type &= ~MACROBLOCK_INTRA;
}

// ISO/IEC 13818-2 section 7.6
static void motion_compensation(int MBA, int macroblock_type, int motion_type, int PMV[2][2][2],
                                int motion_vertical_field_select[2][2], int dmvector[2], int stwtype, int dct_type) {
    int bx, by;
    int addflag;

    // derive current macroblock position within picture
    // ISO/IEC 13818-2 section 6.3.1.6 and 6.3.1.7
    bx = 16 * (MBA % MPDmb_width);
    by = 16 * (MBA / MPDmb_width);

    // motion compensation
    if (!(macroblock_type & MACROBLOCK_INTRA)) {
        form_predictions(bx, by, macroblock_type, motion_type, PMV, motion_vertical_field_select, dmvector, stwtype);
    }

    // add block data into picture (the IDCT already ran)
    addflag = (macroblock_type & MACROBLOCK_INTRA) == 0;
    Add_Block(0, bx, by, dct_type, addflag);
    Add_Block(1, bx, by, dct_type, addflag);
    Add_Block(2, bx, by, dct_type, addflag);
    Add_Block(3, bx, by, dct_type, addflag);
    Add_Block(4, bx >> 1, by >> 1, dct_type, addflag);
    Add_Block(5, bx >> 1, by >> 1, dct_type, addflag);
}

// move/add 8x8-Block from block[comp] to backward_reference_frame
// copy reconstructed 8x8 block from block[comp] to current_frame[]
// ISO/IEC 13818-2 section 7.6.8: Adding prediction and coefficient data
// This stage also clears the block. For chroma, bx and by are already
// scaled.
static void Add_Block(int comp, int bx, int by, int dct_type, int addflag) {
    int cc, i, j, iincr;
    unsigned char* rfp;
    short* bp;

    // derive color component index
    // equivalent to ISO/IEC 13818-2 Table 7-1
    cc = (comp < 4) ? 0 : (comp & 1) + 1;  // color component index

    if (cc == 0) {
        // luminance
        if (MPDpicture_structure == FRAME_PICTURE) {
            if (dct_type) {
                // field DCT coding
                rfp = MPDcurrent_frame[0] + MPDcoded_picture_width * (by + ((comp & 2) >> 1)) + bx + ((comp & 1) << 3);
                iincr = (MPDcoded_picture_width << 1) - 8;
            } else {
                // frame DCT coding
                rfp = MPDcurrent_frame[0] + MPDcoded_picture_width * (by + ((comp & 2) << 2)) + bx + ((comp & 1) << 3);
                iincr = MPDcoded_picture_width - 8;
            }
        } else {
            // field picture
            rfp = MPDcurrent_frame[0] + (MPDcoded_picture_width << 1) * (by + ((comp & 2) << 2)) + bx +
                  ((comp & 1) << 3);
            iincr = (MPDcoded_picture_width << 1) - 8;
        }
    } else {
        // chrominance
        if (MPDpicture_structure == FRAME_PICTURE) {
            // frame DCT coding
            rfp = MPDcurrent_frame[cc] + MPDchroma_width * (by + ((comp & 2) << 2)) + bx + (comp & 8);
            iincr = MPDchroma_width - 8;
        } else {
            // field picture
            rfp = MPDcurrent_frame[cc] + (MPDchroma_width << 1) * (by + ((comp & 2) << 2)) + bx + (comp & 8);
            iincr = (MPDchroma_width << 1) - 8;
        }
    }

    bp = MPDld->block[comp];

    if (addflag) {
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                *rfp = MPDclip[*bp + *rfp];
                *bp++ = 0;
                rfp++;
            }
            rfp += iincr;
        }
    } else {
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++) {
                *rfp++ = MPDclip[*bp + 128];
                *bp++ = 0;
            }
            rfp += iincr;
        }
    }
}

// reuse old picture buffers as soon as they are no longer needed (the
// frames are not written out here)
static void frame_reorder(int Bitstream_Framenum) {
    static int Oldref_progressive_frame, Newref_progressive_frame;

    if (Bitstream_Framenum != 0) {
        if (MPDpicture_structure == FRAME_PICTURE || MPDsecond_field) {
            if (MPDpicture_coding_type != B_TYPE) {
                Newref_progressive_frame = MPDprogressive_frame;
                MPDprogressive_frame = Oldref_progressive_frame;
                Oldref_progressive_frame = MPDprogressive_frame = Newref_progressive_frame;
            }
        }
    } else {
        Oldref_progressive_frame = MPDprogressive_frame;
    }
}
