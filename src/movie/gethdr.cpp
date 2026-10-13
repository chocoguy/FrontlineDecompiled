// The decoder's header parsing (MSSG gethdr.c, without its trace and
// verify output): sequence, GOP and picture headers, slice headers and the
// extensions.
#include "movie/mpeg2dec.h"

static void sequence_header();
static void group_of_pictures_header();
static void picture_header();
static void extension_and_user_data();
static void sequence_extension();
static void sequence_display_extension();
static void quant_matrix_extension();
static void sequence_scalable_extension();
static void picture_display_extension();
static void picture_coding_extension();
static void picture_spatial_scalable_extension();
static void picture_temporal_scalable_extension();
static int extra_bit_information();
static void copyright_extension();
static void user_data();
static void Update_Temporal_Reference_Tacking_Data();

// frame rates by frame_rate_code
static double frame_rate_Table[16] = {
    0.0, ((23.0 * 1000.0) / 1001.0), 24.0, 25.0, ((30.0 * 1000.0) / 1001.0), 30.0, 50.0, ((60.0 * 1000.0) / 1001.0),
    60.0, -1, -1, -1, -1, -1, -1, -1,
};

// spatial scalability: the base layer's running frame numbers
static int Temporal_Reference_Base = 0;
static int True_Framenum_max = -1;
static int Temporal_Reference_GOP_Reset = 0;

// Reads headers up to the next picture: 1 at a picture, 0 at the end of
// the sequence.
int Get_Hdr() {
    unsigned int code;
    for (;;) {
        next_start_code();
        switch (code = Get_Bits32()) {
        case SEQUENCE_HEADER_CODE:
            sequence_header();
            break;
        case GROUP_START_CODE:
            group_of_pictures_header();
            break;
        case PICTURE_START_CODE:
            picture_header();
            return 1;
        case SEQUENCE_END_CODE:
            return 0;
        default:
            break;
        }
    }
}

// Aligns to a byte and skips to the next start code.
void next_start_code() {
    Flush_Buffer(MPDld->Incnt & 7);
    while (Show_Bits(24) != 0x01L) {
        Flush_Buffer(8);
    }
}

static void sequence_header() {
    int i;
    MPDhorizontal_size = Get_Bits(12);
    MPDvertical_size = Get_Bits(12);
    MPDaspect_ratio_information = Get_Bits(4);
    MPDframe_rate_code = Get_Bits(4);
    MPDbit_rate_value = Get_Bits(18);
    marker_bit("sequence_header()");
    MPDvbv_buffer_size = Get_Bits(10);
    MPDconstrained_parameters_flag = Get_Bits(1);

    if ((MPDld->load_intra_quantizer_matrix = Get_Bits(1))) {
        for (i = 0; i < 64; i++) {
            MPDld->intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] = Get_Bits(8);
        }
    } else {
        for (i = 0; i < 64; i++) {
            MPDld->intra_quantizer_matrix[i] = MPDdefault_intra_quantizer_matrix[i];
        }
    }

    if ((MPDld->load_non_intra_quantizer_matrix = Get_Bits(1))) {
        for (i = 0; i < 64; i++) {
            MPDld->non_intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] = Get_Bits(8);
        }
    } else {
        for (i = 0; i < 64; i++) {
            MPDld->non_intra_quantizer_matrix[i] = 16;
        }
    }

    // copy luminance to chrominance matrices
    for (i = 0; i < 64; i++) {
        MPDld->chroma_intra_quantizer_matrix[i] = MPDld->intra_quantizer_matrix[i];
        MPDld->chroma_non_intra_quantizer_matrix[i] = MPDld->non_intra_quantizer_matrix[i];
    }

    extension_and_user_data();
}

static void group_of_pictures_header() {
    if (MPDld == &MPDbase) {
        Temporal_Reference_Base = True_Framenum_max + 1;  // for next GOP
        Temporal_Reference_GOP_Reset = 1;
    }
    MPDdrop_flag = Get_Bits(1);
    MPDhour = Get_Bits(5);
    MPDminute = Get_Bits(6);
    marker_bit("group_of_pictures_header()");
    MPDsec = Get_Bits(6);
    MPDframe = Get_Bits(6);
    MPDclosed_gop = Get_Bits(1);
    MPDbroken_link = Get_Bits(1);

    extension_and_user_data();
}

static void picture_header() {
    // unless later overwritten by picture_spatial_scalable_extension()
    MPDld->pict_scal = 0;

    MPDtemporal_reference = Get_Bits(10);
    MPDpicture_coding_type = Get_Bits(3);
    MPDvbv_delay = Get_Bits(16);

    if (MPDpicture_coding_type == P_TYPE || MPDpicture_coding_type == B_TYPE) {
        MPDfull_pel_forward_vector = Get_Bits(1);
        MPDforward_f_code = Get_Bits(3);
    }
    if (MPDpicture_coding_type == B_TYPE) {
        MPDfull_pel_backward_vector = Get_Bits(1);
        MPDbackward_f_code = Get_Bits(3);
    }

    extra_bit_information();
    extension_and_user_data();

    // update tracking information used to assist spatial scalability
    Update_Temporal_Reference_Tacking_Data();
}

// Returns slice_vertical_position_extension.
int slice_header() {
    int slice_vertical_position_extension;
    int quantizer_scale_code;

    slice_vertical_position_extension = (MPDld->MPEG2_Flag && MPDvertical_size > 2800) ? Get_Bits(3) : 0;

    if (MPDld->scalable_mode == SC_DP) {
        MPDld->priority_breakpoint = Get_Bits(7);
    }

    quantizer_scale_code = Get_Bits(5);
    MPDld->quantizer_scale =
        MPDld->MPEG2_Flag ? (MPDld->q_scale_type ? MPDnon_linear_quantizer_scale[quantizer_scale_code]
                                                 : quantizer_scale_code << 1)
                          : quantizer_scale_code;

    // slice_id introduced in March 1995 as part of the video corridendum
    if (Get_Bits(1)) {
        MPDld->intra_slice = Get_Bits(1);
        Get_Bits(1);  // slice_picture_id_enable
        Get_Bits(6);  // slice_picture_id
        extra_bit_information();
    } else {
        MPDld->intra_slice = 0;
    }

    return slice_vertical_position_extension;
}

static void extension_and_user_data() {
    int code, ext_ID;

    next_start_code();

    while ((code = Show_Bits(32)) == EXTENSION_START_CODE || code == USER_DATA_START_CODE) {
        if (code == EXTENSION_START_CODE) {
            Flush_Buffer32();
            ext_ID = Get_Bits(4);
            switch (ext_ID) {
            case SEQUENCE_EXTENSION_ID:
                sequence_extension();
                break;
            case SEQUENCE_DISPLAY_EXTENSION_ID:
                sequence_display_extension();
                break;
            case QUANT_MATRIX_EXTENSION_ID:
                quant_matrix_extension();
                break;
            case SEQUENCE_SCALABLE_EXTENSION_ID:
                sequence_scalable_extension();
                break;
            case PICTURE_DISPLAY_EXTENSION_ID:
                picture_display_extension();
                break;
            case PICTURE_CODING_EXTENSION_ID:
                picture_coding_extension();
                break;
            case PICTURE_SPATIAL_SCALABLE_EXTENSION_ID:
                picture_spatial_scalable_extension();
                break;
            case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
                picture_temporal_scalable_extension();
                break;
            case COPYRIGHT_EXTENSION_ID:
                copyright_extension();
                break;
            default:
                break;
            }
            next_start_code();
        } else {
            Flush_Buffer32();
            user_data();
        }
    }
}

static void sequence_extension() {
    int horizontal_size_extension;
    int vertical_size_extension;
    int bit_rate_extension;
    int vbv_buffer_size_extension;

    MPDld->MPEG2_Flag = 1;
    MPDld->scalable_mode = SC_NONE;  // unless overwritten by sequence_scalable_extension()
    MPDlayer_id = 0;  // unless overwritten by sequence_scalable_extension()

    MPDprofile_and_level_indication = Get_Bits(8);
    MPDprogressive_sequence = Get_Bits(1);
    MPDchroma_format = Get_Bits(2);
    horizontal_size_extension = Get_Bits(2);
    vertical_size_extension = Get_Bits(2);
    bit_rate_extension = Get_Bits(12);
    marker_bit("sequence_extension");
    vbv_buffer_size_extension = Get_Bits(8);
    MPDlow_delay = Get_Bits(1);
    MPDframe_rate_extension_n = Get_Bits(2);
    MPDframe_rate_extension_d = Get_Bits(5);

    MPDframe_rate = frame_rate_Table[MPDframe_rate_code] *
                    ((MPDframe_rate_extension_n + 1) / (MPDframe_rate_extension_d + 1));

    // special case for 422 profile & level must be made
    if ((MPDprofile_and_level_indication >> 7) & 1) {
        // 4:2:2 Profile @ Main Level
        if ((MPDprofile_and_level_indication & 15) == 5) {
            MPDprofile = PROFILE_422;
            MPDlevel = MAIN_LEVEL;
        }
    } else {
        MPDprofile = MPDprofile_and_level_indication >> 4;
        MPDlevel = MPDprofile_and_level_indication & 0xF;
    }

    MPDhorizontal_size = (horizontal_size_extension << 12) | (MPDhorizontal_size & 0x0fff);
    MPDvertical_size = (vertical_size_extension << 12) | (MPDvertical_size & 0x0fff);

    MPDbit_rate_value += (bit_rate_extension << 18);
    MPDbit_rate = ((double)MPDbit_rate_value) * 400.0;
    MPDvbv_buffer_size += (vbv_buffer_size_extension << 10);
}

static void sequence_display_extension() {
    MPDvideo_format = Get_Bits(3);
    MPDcolor_description = Get_Bits(1);

    if (MPDcolor_description) {
        MPDcolor_primaries = Get_Bits(8);
        MPDtransfer_characteristics = Get_Bits(8);
        MPDmatrix_coefficients = Get_Bits(8);
    }

    MPDdisplay_horizontal_size = Get_Bits(14);
    marker_bit("sequence_display_extension");
    MPDdisplay_vertical_size = Get_Bits(14);
}

static void quant_matrix_extension() {
    int i;

    if ((MPDld->load_intra_quantizer_matrix = Get_Bits(1))) {
        for (i = 0; i < 64; i++) {
            MPDld->chroma_intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] =
                MPDld->intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] = Get_Bits(8);
        }
    }

    if ((MPDld->load_non_intra_quantizer_matrix = Get_Bits(1))) {
        for (i = 0; i < 64; i++) {
            MPDld->chroma_non_intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] =
                MPDld->non_intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] = Get_Bits(8);
        }
    }

    if ((MPDld->load_chroma_intra_quantizer_matrix = Get_Bits(1))) {
        for (i = 0; i < 64; i++) {
            MPDld->chroma_intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] = Get_Bits(8);
        }
    }

    if ((MPDld->load_chroma_non_intra_quantizer_matrix = Get_Bits(1))) {
        for (i = 0; i < 64; i++) {
            MPDld->chroma_non_intra_quantizer_matrix[MPDscan[ZIG_ZAG][i]] = Get_Bits(8);
        }
    }
}

static void sequence_scalable_extension() {
    // add 1 to make SC_DP != SC_NONE
    MPDld->scalable_mode = Get_Bits(2) + 1;
    MPDlayer_id = Get_Bits(4);

    if (MPDld->scalable_mode == SC_SPAT) {
        MPDlower_layer_prediction_horizontal_size = Get_Bits(14);
        marker_bit("sequence_scalable_extension()");
        MPDlower_layer_prediction_vertical_size = Get_Bits(14);
        MPDhorizontal_subsampling_factor_m = Get_Bits(5);
        MPDhorizontal_subsampling_factor_n = Get_Bits(5);
        MPDvertical_subsampling_factor_m = Get_Bits(5);
        MPDvertical_subsampling_factor_n = Get_Bits(5);
    }

    if (MPDld->scalable_mode == SC_TEMP) {
        Error("temporal scalability not implemented\n");
    }
}

static void picture_display_extension() {
    int i;
    int number_of_frame_center_offsets;

    // derive number_of_frame_center_offsets
    if (MPDprogressive_sequence) {
        if (MPDrepeat_first_field) {
            if (MPDtop_field_first) {
                number_of_frame_center_offsets = 3;
            } else {
                number_of_frame_center_offsets = 2;
            }
        } else {
            number_of_frame_center_offsets = 1;
        }
    } else {
        if (MPDpicture_structure != FRAME_PICTURE) {
            number_of_frame_center_offsets = 1;
        } else {
            if (MPDrepeat_first_field) {
                number_of_frame_center_offsets = 3;
            } else {
                number_of_frame_center_offsets = 2;
            }
        }
    }

    for (i = 0; i < number_of_frame_center_offsets; i++) {
        MPDframe_center_horizontal_offset[i] = Get_Bits(16);
        marker_bit("picture_display_extension, first marker bit");
        MPDframe_center_vertical_offset[i] = Get_Bits(16);
        marker_bit("picture_display_extension, second marker bit");
    }
}

static void picture_coding_extension() {
    MPDf_code[0][0] = Get_Bits(4);
    MPDf_code[0][1] = Get_Bits(4);
    MPDf_code[1][0] = Get_Bits(4);
    MPDf_code[1][1] = Get_Bits(4);

    MPDintra_dc_precision = Get_Bits(2);
    MPDpicture_structure = Get_Bits(2);
    MPDtop_field_first = Get_Bits(1);
    MPDframe_pred_frame_dct = Get_Bits(1);
    MPDconcealment_motion_vectors = Get_Bits(1);
    MPDld->q_scale_type = Get_Bits(1);
    MPDintra_vlc_format = Get_Bits(1);
    MPDld->alternate_scan = Get_Bits(1);
    MPDrepeat_first_field = Get_Bits(1);
    MPDchroma_420_type = Get_Bits(1);
    MPDprogressive_frame = Get_Bits(1);
    MPDcomposite_display_flag = Get_Bits(1);

    if (MPDcomposite_display_flag) {
        MPDv_axis = Get_Bits(1);
        MPDfield_sequence = Get_Bits(3);
        MPDsub_carrier = Get_Bits(1);
        MPDburst_amplitude = Get_Bits(7);
        MPDsub_carrier_phase = Get_Bits(8);
    }
}

static void picture_spatial_scalable_extension() {
    MPDld->pict_scal = 1;  // use spatial scalability in this picture

    MPDlower_layer_temporal_reference = Get_Bits(10);
    marker_bit("picture_spatial_scalable_extension(), first marker bit");
    MPDlower_layer_horizontal_offset = Get_Bits(15);
    if (MPDlower_layer_horizontal_offset >= 16384) {
        MPDlower_layer_horizontal_offset -= 32768;
    }
    marker_bit("picture_spatial_scalable_extension(), second marker bit");
    MPDlower_layer_vertical_offset = Get_Bits(15);
    if (MPDlower_layer_vertical_offset >= 16384) {
        MPDlower_layer_vertical_offset -= 32768;
    }
    MPDspatial_temporal_weight_code_table_index = Get_Bits(2);
    MPDlower_layer_progressive_frame = Get_Bits(1);
    MPDlower_layer_deinterlaced_field_select = Get_Bits(1);
}

static void picture_temporal_scalable_extension() {
    Error("temporal scalability not supported\n");
}

static void copyright_extension() {
    MPDcopyright_flag = Get_Bits(1);
    MPDcopyright_identifier = Get_Bits(8);
    MPDoriginal_or_copy = Get_Bits(1);

    Get_Bits(7);  // reserved

    marker_bit("copyright_extension(), first marker bit");
    MPDcopyright_number_1 = Get_Bits(20);
    marker_bit("copyright_extension(), second marker bit");
    MPDcopyright_number_2 = Get_Bits(22);
    marker_bit("copyright_extension(), third marker bit");
    MPDcopyright_number_3 = Get_Bits(22);
}

// introduced in September 1995 to assist Spatial Scalability
static void Update_Temporal_Reference_Tacking_Data() {
    static int temporal_reference_wrap = 0;
    static int temporal_reference_old = 0;

    if (MPDld == &MPDbase) {  // *CH*
        if (MPDpicture_coding_type != B_TYPE && MPDtemporal_reference != temporal_reference_old) {
            // check first field of
            if (temporal_reference_wrap) {
                Temporal_Reference_Base += 1024;
                temporal_reference_wrap = 0;
            }

            // distinguish from a reset
            if ((MPDtemporal_reference < temporal_reference_old) && !Temporal_Reference_GOP_Reset) {
                temporal_reference_wrap = 1;
            }

            temporal_reference_old = MPDtemporal_reference;
            Temporal_Reference_GOP_Reset = 0;
        }

        MPDtrue_framenum = Temporal_Reference_Base + MPDtemporal_reference;

        // temporary wrap of TR at 1024 for M frames
        if (temporal_reference_wrap && MPDtemporal_reference <= temporal_reference_old) {
            MPDtrue_framenum += 1024;
        }

        True_Framenum_max = (MPDtrue_framenum > True_Framenum_max) ? MPDtrue_framenum : True_Framenum_max;
    }
}

// Skips extra_information_picture bytes; returns how many.
static int extra_bit_information() {
    int Byte_Count = 0;

    while (Get_Bits1()) {
        Flush_Buffer(8);
        Byte_Count++;
    }

    return Byte_Count;
}

void marker_bit(char* text) {
    Get_Bits(1);
}

static void user_data() {
    // skip ahead to the next start code
    next_start_code();
}
