// The MPC codec: decodes an RCMP MPEG stream with the MSSG decoder into a
// pool of frame buffers (the decoder's m_numFrames + 2), handing frames out
// reference counted. Also the MSSG decoder's globals.
#include "movie/rcmp_mpc_codec.h"
#include "movie/mpeg2dec.h"
#include "ea/real.h"

// MSSG global.h
unsigned char MPDscan[2][64] = {
    // zig-zag scan
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
    // alternate scan
    0, 8, 16, 24, 1, 9, 2, 10, 17, 25, 32, 40, 48, 56, 57, 49,
    41, 33, 26, 18, 3, 11, 4, 12, 19, 27, 34, 42, 50, 58, 35, 43,
    51, 59, 20, 28, 5, 13, 6, 14, 21, 29, 36, 44, 52, 60, 37, 45,
    53, 61, 22, 30, 7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63,
};

unsigned char MPDdefault_intra_quantizer_matrix[64] = {
    8, 16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83,
};

unsigned char MPDnon_linear_quantizer_scale[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 22,
    24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96, 104, 112,
};

unsigned char MPDtranspose_table[64];
unsigned char* MPDbackward_reference_frame[3];
unsigned char* MPDforward_reference_frame[3];
unsigned char* MPDcurrent_frame[3];
int MPDf_code[2][2];
int MPDframe_center_horizontal_offset[3];
int MPDframe_center_vertical_offset[3];
layer_data MPDbase;
int MPDquiet_flag;
int MPDtrace_flag;
int MPDfault_flag;
int MPDverbose_flag;
unsigned char* MPDclip;
int MPDcoded_picture_width;
int MPDcoded_picture_height;
int MPDchroma_width;
int MPDchroma_height;
int MPDsecond_field;
int MPDprofile;
int MPDlevel;
int MPDhorizontal_size;
int MPDvertical_size;
int MPDmb_width;
int MPDmb_height;
double MPDbit_rate;
double MPDframe_rate;
int MPDaspect_ratio_information;
int MPDframe_rate_code;
int MPDbit_rate_value;
int MPDvbv_buffer_size;
int MPDconstrained_parameters_flag;
int MPDprofile_and_level_indication;
int MPDprogressive_sequence;
int MPDchroma_format;
int MPDlow_delay;
int MPDframe_rate_extension_n;
int MPDframe_rate_extension_d;
int MPDvideo_format;
int MPDcolor_description;
int MPDcolor_primaries;
int MPDtransfer_characteristics;
int MPDmatrix_coefficients;
int MPDdisplay_horizontal_size;
int MPDdisplay_vertical_size;
int MPDtemporal_reference;
int MPDpicture_coding_type;
int MPDvbv_delay;
int MPDfull_pel_forward_vector;
int MPDforward_f_code;
int MPDfull_pel_backward_vector;
int MPDbackward_f_code;
int MPDintra_dc_precision;
int MPDpicture_structure;
int MPDtop_field_first;
int MPDframe_pred_frame_dct;
int MPDconcealment_motion_vectors;
int MPDintra_vlc_format;
int MPDrepeat_first_field;
int MPDchroma_420_type;
int MPDprogressive_frame;
int MPDcomposite_display_flag;
int MPDv_axis;
int MPDfield_sequence;
int MPDsub_carrier;
int MPDburst_amplitude;
int MPDsub_carrier_phase;
int MPDlayer_id;
int MPDlower_layer_prediction_horizontal_size;
int MPDlower_layer_prediction_vertical_size;
int MPDhorizontal_subsampling_factor_m;
int MPDhorizontal_subsampling_factor_n;
int MPDvertical_subsampling_factor_m;
int MPDvertical_subsampling_factor_n;
int MPDlower_layer_temporal_reference;
int MPDlower_layer_horizontal_offset;
int MPDlower_layer_vertical_offset;
int MPDspatial_temporal_weight_code_table_index;
int MPDlower_layer_progressive_frame;
int MPDlower_layer_deinterlaced_field_select;
int MPDcopyright_flag;
int MPDcopyright_identifier;
int MPDoriginal_or_copy;
int MPDcopyright_number_1;
int MPDcopyright_number_2;
int MPDcopyright_number_3;
int MPDdrop_flag;
int MPDhour;
int MPDminute;
int MPDsec;
int MPDframe;
int MPDclosed_gop;
int MPDbroken_link;
layer_data* MPDld;
int MPDtrue_framenum;

// The picture coding type a chunk starts (a GOP counts as an I picture, and
// so does the end of the stream).
static inline int ChunkPictureType(RCMP::CHUNK* chunk) {
    if (chunk == NULL) {
        return I_TYPE;
    }
    if (chunk->m_data[3] == 0xB8) {  // GROUP_START_CODE
        return I_TYPE;
    }
    return (chunk->m_data[5] >> 3) & 3;
}

MPC_CODEC_INTERNAL::~MPC_CODEC_INTERNAL() {
    m_used.DeleteAll();
    m_free.DeleteAll();
    MPDclip -= 384;
    if (MPDclip != NULL) {
        RCMP::rcmp_sys.m_free(MPDclip);
        MPDclip = NULL;
    }
    MPDbackward_reference_frame[0] = NULL;
    MPDbase.MPEG2_Flag = 0;
}

// Sets up the decoder for a stream starting with `chunk`: 2 when it starts
// with a sequence header, 0 otherwise.
int MPC_CODEC_INTERNAL::Init(RCMP::DECODER* decoder, RCMP::CHUNK* chunk) {
    int i;
    u8* data = chunk->m_data;
    const u8 header[4] = {0x00, 0x00, 0x01, 0xB3};

    // Clip[i] = i clamped to 0..255, for i in -384..639
    MPDclip = (unsigned char*)RCMP::rcmp_sys.m_alloc("MPDclip", 1024, 0, 0, RCMP::rcmp_sys.m_allocParam);
    MPDclip += 384;
    for (i = -384; i < 640; i++) {
        MPDclip[i] = (i < 0) ? 0 : ((i > 255) ? 255 : i);
    }
    for (i = 0; i < 64; i++) {
        MPDtranspose_table[i] = i;
    }

    for (i = 0; i < 4; i++) {
        if (header[i] != data[i]) {
            return 0;
        }
    }

    m_state = 0;
    m_decoder = decoder;
    MPDverbose_flag = 0;
    MPDtrace_flag = 0;
    MPDquiet_flag = 0;
    MPDld = &MPDbase;
    Initialize_Fast_IDCT();
    m_forward = NULL;
    m_backward = NULL;
    m_current = NULL;
    m_display = NULL;
    m_frameNumber = 0;
    return 2;
}

int MPC_CODEC_INTERNAL::GetCurrentFrameNumber() {
    return m_frameNumber;
}

float MPC_CODEC_INTERNAL::GetFrameRate() {
    return MPDframe_rate;
}

// Decodes the picture in `chunk`. The first one also sets up the sequence
// (MSSG Initialize_Sequence) and allocates the frame buffers.
void MPC_CODEC_INTERNAL::LowLevelDecodeChunk(RCMP::CHUNK* chunk) {
    unsigned int i;

    MPDld = &MPDbase;
    Initialize_Buffer(chunk->m_data);
    Get_Hdr();

    if (m_first) {
        // force MPEG-1 parameters for proper decoder behavior
        if (!MPDbase.MPEG2_Flag) {
            MPDprogressive_sequence = 1;
            MPDprogressive_frame = 1;
            MPDpicture_structure = FRAME_PICTURE;
            MPDframe_pred_frame_dct = 1;
            MPDmatrix_coefficients = 5;
        }

        // round to nearest multiple of coded macroblocks
        MPDmb_width = (MPDhorizontal_size + 15) / 16;
        MPDmb_height = (MPDbase.MPEG2_Flag && !MPDprogressive_sequence) ? 2 * ((MPDvertical_size + 31) / 32)
                                                                       : (MPDvertical_size + 15) / 16;
        MPDcoded_picture_width = 16 * MPDmb_width;
        MPDcoded_picture_height = 16 * MPDmb_height;
        MPDchroma_width = MPDcoded_picture_width >> 1;
        MPDchroma_height = MPDcoded_picture_height >> 1;

        for (i = 0; i < m_decoder->m_numFrames + 2; i++) {
            m_free.AddHead(new MPC_FRAME(MPDcoded_picture_width, MPDcoded_picture_height));
        }
    }

    Decode_Picture(this, m_frameNumber);

    if (m_first) {
        if (MPDframe_rate == 0.0) {
            MPDframe_rate = 30.0;
        }
        if (!MPDprogressive_sequence && MPDprogressive_frame == 1) {
            MPDframe_rate *= 0.8f;
        }
    }
}

// Decodes the next picture and returns the frame to show (held for the
// caller). At the end of the stream it returns the last reference frame.
MPC_FRAME* MPC_CODEC_INTERNAL::DecodeChunk(RCMP::CHUNK** chunk) {
    if (m_state == 1 || m_state == 2) {
        if (m_forward != NULL && m_forward->m_refs > 0) {
            ReleaseFrame(m_forward);
        }
        m_forward = m_backward;
        m_display = m_forward;
        m_backward = NULL;
        m_current = NULL;
        if (m_display != NULL) {
            Use(m_display);
        }
        return m_display;
    }

    if (m_first) {
        LowLevelDecodeChunk(*chunk);
        NextChunk(chunk);
        m_first = 0;
    }

    if (*chunk == NULL) {
        return Use(m_current);
    }
    LowLevelDecodeChunk(*chunk);
    return Use(m_display);
}

// Skips ahead towards picture `frame`: from an I picture when it is more
// than 2 pictures away, otherwise just over the B pictures before it.
void MPC_CODEC_INTERNAL::SkipChunks(RCMP::CHUNK** chunk, unsigned int frame) {
    int i;
    int skip;

    if (m_first || m_state != 0) {
        return;
    }
    skip = frame - m_frameNumber;
    if (skip > 2) {
        do {
            NextChunk(chunk);
        } while (ChunkPictureType(*chunk) != I_TYPE);
        MPC_FRAME* decoded = DecodeChunk(chunk);
        if (decoded != NULL) {
            ReleaseFrame(decoded);
        }
        while (ChunkPictureType(*chunk) == B_TYPE) {
            NextChunk(chunk);
        }
        return;
    }
    if (skip > 0) {
        for (i = 0; i < skip; i++) {
            if (ChunkPictureType(*chunk) == B_TYPE) {
                NextChunk(chunk);
            }
        }
    }
}

// Decodes picture `frame` (or the next one when it is behind); the caller
// releases the frame.
RCMP::FRAME* MPC_CODEC_INTERNAL::GetFrame(unsigned int frame) {
    RCMP::CHUNK* chunk = NULL;
    NextChunk(&chunk);
    SkipChunks(&chunk, frame);
    MPC_FRAME* decoded = DecodeChunk(&chunk);
    if (chunk != NULL) {
        m_decoder->ReleaseChunk(chunk);
    }
    return decoded;
}

// A frame for an I or P picture, held as a reference.
MPC_FRAME* MPC_CODEC_INTERNAL::CreateIorP() {
    if (!m_free.IsEmpty()) {
        MPC_FRAME* frame = static_cast<MPC_FRAME*>(m_free.First());
        frame->Remove();
        m_used.AddTail(frame);
        frame->m_refs = 1;
        return frame;
    }
    return NULL;
}

// A frame for a B picture.
MPC_FRAME* MPC_CODEC_INTERNAL::CreateB() {
    if (!m_free.IsEmpty()) {
        MPC_FRAME* frame = static_cast<MPC_FRAME*>(m_free.First());
        frame->Remove();
        m_used.AddTail(frame);
        frame->m_refs = 0;
        return frame;
    }
    return NULL;
}

// Drops a holder; the last one puts the frame back on the free list.
void MPC_CODEC_INTERNAL::ReleaseFrame(RCMP::FRAME* frame) {
    MPC_FRAME* f = static_cast<MPC_FRAME*>(frame);
    if (--f->m_refs <= 0) {
        f->Remove();
        m_free.AddTail(f);
    }
}

// The decoder's error report: ignored.
void Error(char* text) {}

RCMP::CODEC* RCMP::MPC_CODEC_create() {
    return new MPC_CODEC_INTERNAL;
}
