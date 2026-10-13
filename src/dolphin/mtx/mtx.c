#include <dolphin/gx.h>

typedef f32 Mtx[3][4];

void PSMTXIdentity(register Mtx m) {
    register f32 one_c = 1.0f;
    register f32 zero_c = 0.0f;
    register f32 c_01;
    register f32 c_10;

    asm {
        psq_st zero_c, 8(m), 0, 0
        ps_merge01 c_01, zero_c, one_c
        psq_st zero_c, 24(m), 0, 0
        ps_merge10 c_10, one_c, zero_c
        psq_st zero_c, 32(m), 0, 0
        psq_st c_01, 16(m), 0, 0
        psq_st c_10, 0(m), 0, 0
        psq_st c_10, 40(m), 0, 0
    }
}

// Inverse-transpose of the upper 3x3 (for normal matrices). Returns 0 if singular.
asm u32 PSMTXInvXpose(const register Mtx src, register Mtx invX) {
    nofralloc
    psq_l f0, 0(src), 1, 0
    psq_l f1, 4(src), 0, 0
    psq_l f2, 16(src), 1, 0
    ps_merge10 f6, f1, f0
    psq_l f3, 20(src), 0, 0
    psq_l f4, 32(src), 1, 0
    ps_merge10 f7, f3, f2
    psq_l f5, 36(src), 0, 0
    ps_mul f11, f3, f6
    ps_merge10 f8, f5, f4
    ps_mul f13, f5, f7
    ps_msub f11, f1, f7, f11
    ps_mul f12, f1, f8
    ps_msub f13, f3, f8, f13
    ps_msub f12, f5, f6, f12
    ps_mul f10, f3, f4
    ps_mul f9, f0, f5
    ps_mul f8, f1, f2
    ps_msub f10, f2, f5, f10
    ps_msub f9, f1, f4, f9
    ps_msub f8, f0, f3, f8
    ps_mul f7, f0, f13
    ps_sub f1, f1, f1
    ps_madd f7, f2, f12, f7
    ps_madd f7, f4, f11, f7
    ps_cmpo0 cr0, f7, f1
    bne _regular
    li r3, 0
    blr

_regular:
    fres f0, f7
    psq_st f1, 12(invX), 1, 0
    ps_add f6, f0, f0
    ps_mul f5, f0, f0
    psq_st f1, 28(invX), 1, 0
    ps_nmsub f0, f7, f5, f6
    psq_st f1, 44(invX), 1, 0
    ps_muls0 f13, f13, f0
    ps_muls0 f12, f12, f0
    ps_muls0 f11, f11, f0
    psq_st f13, 0(invX), 0, 0
    psq_st f12, 16(invX), 0, 0
    ps_muls0 f10, f10, f0
    ps_muls0 f9, f9, f0
    psq_st f11, 32(invX), 0, 0
    psq_st f10, 8(invX), 1, 0
    ps_muls0 f8, f8, f0
    li r3, 1
    psq_st f9, 24(invX), 1, 0
    psq_st f8, 40(invX), 1, 0
    blr
}

void PSMTXScale(register Mtx m, register f32 xS, register f32 yS, register f32 zS) {
    register f32 c_zero = 0.0f;

    asm {
        stfs xS, 0(m)
        psq_st c_zero, 4(m), 0, 0
        psq_st c_zero, 12(m), 0, 0
        stfs yS, 20(m)
        psq_st c_zero, 24(m), 0, 0
        psq_st c_zero, 32(m), 0, 0
        stfs zS, 40(m)
        stfs c_zero, 44(m)
    }
}
