// The movie player's MPEG video decoder: the MSSG mpeg2decode reference
// decoder (global.h) with an MPD prefix on its globals, reading from memory
// instead of a file. Its globals are defined in rcmp_mpc_codec.cpp.
// Hand-maintained.
#ifndef _MOVIE_MPEG2DEC_H_
#define _MOVIE_MPEG2DEC_H_

#include "types.h"

// The bitstream and per-layer decoder state. size 0xA48
struct layer_data {
    /* 0x000 */ unsigned char* Rdptr;  // next byte to read
    /* 0x004 */ unsigned int Bfr;  // bits not yet consumed, MSB first
    /* 0x008 */ int Incnt;  // valid bits in Bfr
    /* 0x00C */ int unk_0xC;
    // sequence header and quant_matrix_extension()
    /* 0x010 */ int intra_quantizer_matrix[64];
    /* 0x110 */ int non_intra_quantizer_matrix[64];
    /* 0x210 */ int chroma_intra_quantizer_matrix[64];
    /* 0x310 */ int chroma_non_intra_quantizer_matrix[64];
    /* 0x410 */ int load_intra_quantizer_matrix;
    /* 0x414 */ int load_non_intra_quantizer_matrix;
    /* 0x418 */ int load_chroma_intra_quantizer_matrix;
    /* 0x41C */ int load_chroma_non_intra_quantizer_matrix;
    /* 0x420 */ int MPEG2_Flag;
    /* 0x424 */ int scalable_mode;  // sequence scalable extension
    /* 0x428 */ int q_scale_type;  // picture coding extension
    /* 0x42C */ int alternate_scan;
    /* 0x430 */ int pict_scal;  // picture spatial scalable extension
    /* 0x434 */ int priority_breakpoint;  // slice/macroblock
    /* 0x438 */ int quantizer_scale;
    /* 0x43C */ int intra_slice;
    /* 0x440 */ u8 unk_0x440[0x8];
    /* 0x448 */ short block[12][64];
};

// A variable-length code: the value and the code's length in bits.
struct VLCtab {
    /* 0x0 */ signed char val;
    /* 0x1 */ signed char len;
};

// A DCT coefficient code. size 0x3
struct DCTtab {
    /* 0x0 */ signed char run;  // 64: end of block, 65: escape
    /* 0x1 */ signed char level;
    /* 0x2 */ signed char len;
};

// picture coding types
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

// start codes
#define PICTURE_START_CODE 0x100
#define SLICE_START_CODE_MIN 0x101
#define SLICE_START_CODE_MAX 0x1AF
#define USER_DATA_START_CODE 0x1B2
#define SEQUENCE_HEADER_CODE 0x1B3
#define SEQUENCE_ERROR_CODE 0x1B4
#define EXTENSION_START_CODE 0x1B5
#define SEQUENCE_END_CODE 0x1B7
#define GROUP_START_CODE 0x1B8
#define SYSTEM_START_CODE_MIN 0x1B9
#define SYSTEM_START_CODE_MAX 0x1FF

// extension start code IDs
#define SEQUENCE_EXTENSION_ID 1
#define SEQUENCE_DISPLAY_EXTENSION_ID 2
#define QUANT_MATRIX_EXTENSION_ID 3
#define COPYRIGHT_EXTENSION_ID 4
#define SEQUENCE_SCALABLE_EXTENSION_ID 5
#define PICTURE_DISPLAY_EXTENSION_ID 7
#define PICTURE_CODING_EXTENSION_ID 8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID 9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID 10

#define ZIG_ZAG 0
#define PROFILE_422 (128 + 5)
#define MAIN_LEVEL 8

// picture structure
#define TOP_FIELD 1
#define BOTTOM_FIELD 2
#define FRAME_PICTURE 3

// macroblock type
#define MACROBLOCK_INTRA 1
#define MACROBLOCK_PATTERN 2
#define MACROBLOCK_MOTION_BACKWARD 4
#define MACROBLOCK_MOTION_FORWARD 8
#define MACROBLOCK_QUANT 16
#define SPATIAL_TEMPORAL_WEIGHT_CODE_FLAG 32
#define PERMITTED_SPATIAL_TEMPORAL_WEIGHT_CLASS 64

// motion_type
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8 2
#define MC_DMV 3

// mv_format
#define MV_FIELD 0
#define MV_FRAME 1

// chroma_format
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

// scalable_mode
#define SC_NONE 0
#define SC_DP 1
#define SC_SPAT 2
#define SC_SNR 3
#define SC_TEMP 4

// Globals (MSSG global.h), defined in rcmp_mpc_codec.cpp.
extern layer_data* MPDld;  // 0x8034EF30: the layer being decoded (&MPDbase)
extern layer_data MPDbase;
extern unsigned char MPDscan[2][64];  // zig-zag and alternate scans
extern unsigned char MPDdefault_intra_quantizer_matrix[64];
extern unsigned char MPDtranspose_table[64];
extern unsigned char MPDnon_linear_quantizer_scale[32];
extern unsigned char* MPDbackward_reference_frame[3];
extern unsigned char* MPDforward_reference_frame[3];
extern unsigned char* MPDcurrent_frame[3];
extern int MPDf_code[2][2];
extern int MPDframe_center_horizontal_offset[3];
extern int MPDframe_center_vertical_offset[3];
extern unsigned char* MPDclip;  // clip[-384..639] -> 0..255
extern double MPDbit_rate;
extern double MPDframe_rate;
extern int MPDquiet_flag;
extern int MPDtrace_flag;
extern int MPDfault_flag;
extern int MPDverbose_flag;
extern int MPDcoded_picture_width;
extern int MPDcoded_picture_height;
extern int MPDchroma_width;
extern int MPDchroma_height;
extern int MPDsecond_field;
extern int MPDprofile;
extern int MPDlevel;
extern int MPDhorizontal_size;
extern int MPDvertical_size;
extern int MPDmb_width;
extern int MPDmb_height;
extern int MPDaspect_ratio_information;
extern int MPDframe_rate_code;
extern int MPDbit_rate_value;
extern int MPDvbv_buffer_size;
extern int MPDconstrained_parameters_flag;
extern int MPDprofile_and_level_indication;
extern int MPDprogressive_sequence;
extern int MPDchroma_format;
extern int MPDlow_delay;
extern int MPDframe_rate_extension_n;
extern int MPDframe_rate_extension_d;
extern int MPDvideo_format;
extern int MPDcolor_description;
extern int MPDcolor_primaries;
extern int MPDtransfer_characteristics;
extern int MPDmatrix_coefficients;
extern int MPDdisplay_horizontal_size;
extern int MPDdisplay_vertical_size;
extern int MPDtemporal_reference;
extern int MPDpicture_coding_type;
extern int MPDvbv_delay;
extern int MPDfull_pel_forward_vector;
extern int MPDforward_f_code;
extern int MPDfull_pel_backward_vector;
extern int MPDbackward_f_code;
extern int MPDintra_dc_precision;
extern int MPDpicture_structure;
extern int MPDtop_field_first;
extern int MPDframe_pred_frame_dct;
extern int MPDconcealment_motion_vectors;
extern int MPDintra_vlc_format;
extern int MPDrepeat_first_field;
extern int MPDchroma_420_type;
extern int MPDprogressive_frame;
extern int MPDcomposite_display_flag;
extern int MPDv_axis;
extern int MPDfield_sequence;
extern int MPDsub_carrier;
extern int MPDburst_amplitude;
extern int MPDsub_carrier_phase;
extern int MPDlayer_id;
extern int MPDlower_layer_prediction_horizontal_size;
extern int MPDlower_layer_prediction_vertical_size;
extern int MPDhorizontal_subsampling_factor_m;
extern int MPDhorizontal_subsampling_factor_n;
extern int MPDvertical_subsampling_factor_m;
extern int MPDvertical_subsampling_factor_n;
extern int MPDlower_layer_temporal_reference;
extern int MPDlower_layer_horizontal_offset;
extern int MPDlower_layer_vertical_offset;
extern int MPDspatial_temporal_weight_code_table_index;
extern int MPDlower_layer_progressive_frame;
extern int MPDlower_layer_deinterlaced_field_select;
extern int MPDcopyright_flag;
extern int MPDcopyright_identifier;
extern int MPDoriginal_or_copy;
extern int MPDcopyright_number_1;
extern int MPDcopyright_number_2;
extern int MPDcopyright_number_3;
extern int MPDdrop_flag;
extern int MPDhour;
extern int MPDminute;
extern int MPDsec;
extern int MPDframe;
extern int MPDclosed_gop;
extern int MPDbroken_link;
extern int MPDtrue_framenum;

void Error(char* text);

// getbits.cpp
int Get_Bits32();
void Flush_Buffer32();
int Get_Bits(int n);
void Flush_Buffer(int n);
int Get_Bits1();
int Show_Bits(int n);
void Initialize_Buffer(unsigned char* buffer);

// getvlc.cpp
extern DCTtab DCTtabfirst[12], DCTtabnext[12], DCTtab0[60], DCTtab0a[252], DCTtab1[8], DCTtab1a[8], DCTtab2[16],
    DCTtab3[16], DCTtab4[16], DCTtab5[16], DCTtab6[16];
int Get_macroblock_type();
int Get_motion_code();
int Get_dmvector();
int Get_coded_block_pattern();
int Get_macroblock_address_increment();
int Get_Luma_DC_dct_diff();
int Get_Chroma_DC_dct_diff();

// idct.cpp
void Initialize_Fast_IDCT();
void Fast_IDCT(short* block);

// gc_idct.asm: the paired-single IDCT of an 8x8 block, in place
extern "C" void GC_IDCT(short* block);

// getpic.cpp
class MPC_CODEC_INTERNAL;
void Decode_Picture(MPC_CODEC_INTERNAL* codec, int frame);

// getblk.cpp
void Decode_MPEG1_Intra_Block(int comp, int dc_dct_pred[]);
void Decode_MPEG1_Non_Intra_Block(int comp);
void Decode_MPEG2_Intra_Block(int comp, int dc_dct_pred[]);
void Decode_MPEG2_Non_Intra_Block(int comp);

// motion.cpp
void motion_vectors(int PMV[2][2][2], int dmvector[2], int motion_vertical_field_select[2][2], int s,
                    int motion_vector_count, int mv_format, int h_r_size, int v_r_size, int dmv, int mvscale);
void motion_vector(int* PMV, int* dmvector, int h_r_size, int v_r_size, int dmv, int mvscale, int full_pel_vector);
void Dual_Prime_Arithmetic(int DMV[][2], int* dmvector, int mvx, int mvy);

// recon.cpp
void form_predictions(int bx, int by, int macroblock_type, int motion_type, int PMV[2][2][2],
                      int motion_vertical_field_select[2][2], int dmvector[2], int stwtype);

// gethdr.cpp
int Get_Hdr();
void next_start_code();
int slice_header();
void marker_bit(char* text);

#endif
