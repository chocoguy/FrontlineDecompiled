# GC_IDCT: the movie decoder's inverse DCT of an 8x8 block of shorts, in
# place, in paired singles. Hand-written assembly (the original was too).
# GQR7 (0x04070007) loads the block as s16 scaled by 1/16 and stores it as
# plain s16, so coefficients come in 16 times their value (see getblk.cpp)
# and pixels come out unscaled.
# Each pass is the AAN-style 8-point transform on two rows/columns at once;
# constdata holds the cosine factors (and a 0 the row pass uses).

.include "macros.inc"

.text
.balign 32

# One pass over 2 rows (r3: the block's next 2 rows, r4: the float
# scratch), transposing as it stores.
.fn PS_IDCT_row, global
	psq_l f0, 0x0(r3), 0, qr7
	psq_l f2, 0x4(r3), 0, qr7
	psq_l f4, 0x8(r3), 0, qr7
	psq_l f6, 0xc(r3), 0, qr7
	psq_l f10, 0x10(r3), 0, qr7
	psq_l f12, 0x14(r3), 0, qr7
	psq_l f14, 0x18(r3), 0, qr7
	psq_l f16, 0x1c(r3), 0, qr7
	ps_merge11 f1, f0, f10
	ps_merge00 f0, f0, f10
	ps_merge11 f3, f2, f12
	ps_merge00 f2, f2, f12
	ps_merge11 f5, f4, f14
	ps_merge00 f4, f4, f14
	ps_merge11 f7, f6, f16
	ps_merge00 f6, f6, f16
	ps_mul f18, f22, f0
	ps_madd f10, f23, f2, f18
	ps_madd f11, f24, f2, f18
	ps_madd f12, f23, f6, f18
	ps_madd f13, f22, f4, f18
	ps_madd f10, f22, f4, f10
	ps_madd f10, f24, f6, f10
	ps_mul f18, f23, f6
	ps_mul f19, f22, f4
	ps_mul f20, f24, f6
	ps_nmadd f18, f22, f4, f18
	ps_nmadd f19, f24, f2, f19
	ps_nmadd f20, f23, f2, f20
	ps_add f11, f11, f18
	ps_add f12, f12, f19
	ps_add f13, f13, f20
	ps_mul f18, f25, f1
	ps_mul f19, f26, f1
	ps_mul f20, f27, f1
	ps_mul f21, f28, f1
	ps_madd f18, f26, f3, f18
	ps_madd f20, f28, f5, f20
	ps_madd f21, f26, f5, f21
	ps_madd f18, f27, f5, f18
	ps_madd f20, f26, f7, f20
	ps_madd f18, f28, f7, f18
	ps_mul f14, f27, f7
	ps_nmadd f15, f25, f3, f29
	ps_mul f16, f25, f7
	ps_madd f14, f25, f5, f14
	ps_add f20, f20, f15
	ps_nmadd f16, f27, f3, f16
	ps_nmadd f14, f28, f3, f14
	ps_add f21, f21, f16
	ps_add f19, f19, f14
	ps_sub f17, f10, f18
	ps_sub f16, f11, f19
	ps_sub f15, f12, f20
	ps_sub f14, f13, f21
	ps_add f10, f10, f18
	ps_add f11, f11, f19
	ps_add f12, f12, f20
	ps_add f13, f13, f21
	ps_merge00 f0, f10, f11
	ps_merge11 f10, f10, f11
	ps_merge00 f2, f12, f13
	ps_merge11 f12, f12, f13
	ps_merge00 f4, f14, f15
	ps_merge11 f14, f14, f15
	ps_merge00 f6, f16, f17
	ps_merge11 f16, f16, f17
	psq_st f0, 0x0(r4), 0, qr0
	psq_st f2, 0x8(r4), 0, qr0
	psq_st f4, 0x10(r4), 0, qr0
	psq_st f6, 0x18(r4), 0, qr0
	psq_st f10, 0x20(r4), 0, qr0
	psq_st f12, 0x28(r4), 0, qr0
	psq_st f14, 0x30(r4), 0, qr0
	psq_st f16, 0x38(r4), 0, qr0
	blr
.endfn PS_IDCT_row

# The column pass over 2 columns of the scratch (r4), storing to the
# block (r3).
.fn PS_IDCT_col, local
	psq_l f0, 0x0(r4), 0, qr0
	psq_l f1, 0x20(r4), 0, qr0
	psq_l f2, 0x40(r4), 0, qr0
	psq_l f3, 0x60(r4), 0, qr0
	psq_l f4, 0x80(r4), 0, qr0
	psq_l f5, 0xa0(r4), 0, qr0
	psq_l f6, 0xc0(r4), 0, qr0
	psq_l f7, 0xe0(r4), 0, qr0
	ps_mul f18, f22, f0
	ps_madd f10, f23, f2, f18
	ps_madd f11, f24, f2, f18
	ps_madd f12, f23, f6, f18
	ps_madd f13, f22, f4, f18
	ps_madd f10, f22, f4, f10
	ps_madd f10, f24, f6, f10
	ps_mul f18, f23, f6
	ps_mul f19, f22, f4
	ps_mul f20, f24, f6
	ps_nmadd f18, f22, f4, f18
	ps_nmadd f19, f24, f2, f19
	ps_nmadd f20, f23, f2, f20
	ps_add f11, f11, f18
	ps_add f12, f12, f19
	ps_add f13, f13, f20
	ps_mul f18, f25, f1
	ps_mul f19, f26, f1
	ps_mul f20, f27, f1
	ps_mul f21, f28, f1
	ps_madd f18, f26, f3, f18
	ps_madd f20, f28, f5, f20
	ps_madd f21, f26, f5, f21
	ps_madd f18, f27, f5, f18
	ps_madd f20, f26, f7, f20
	ps_madd f18, f28, f7, f18
	ps_mul f14, f27, f7
	ps_nmadd f15, f25, f3, f29
	ps_mul f16, f25, f7
	ps_madd f14, f25, f5, f14
	ps_add f20, f20, f15
	ps_nmadd f16, f27, f3, f16
	ps_nmadd f14, f28, f3, f14
	ps_add f21, f21, f16
	ps_add f19, f19, f14
	ps_sub f17, f10, f18
	ps_sub f16, f11, f19
	ps_sub f15, f12, f20
	ps_sub f14, f13, f21
	ps_add f10, f10, f18
	ps_add f11, f11, f19
	ps_add f12, f12, f20
	ps_add f13, f13, f21
	psq_st f10, 0x0(r3), 0, qr7
	psq_st f11, 0x10(r3), 0, qr7
	psq_st f12, 0x20(r3), 0, qr7
	psq_st f13, 0x30(r3), 0, qr7
	psq_st f14, 0x40(r3), 0, qr7
	psq_st f15, 0x50(r3), 0, qr7
	psq_st f16, 0x60(r3), 0, qr7
	psq_st f17, 0x70(r3), 0, qr7
	blr
.endfn PS_IDCT_col

# void GC_IDCT(short* block)
.fn GC_IDCT, global
	mflr r0
	stw r0, 0x4(r1)
	stwu r1, -0x188(r1)
	stfd f14, 0x8(r1)
	stfd f15, 0x10(r1)
	stfd f16, 0x18(r1)
	stfd f17, 0x20(r1)
	stfd f18, 0x28(r1)
	stfd f19, 0x30(r1)
	stfd f20, 0x38(r1)
	stfd f21, 0x40(r1)
	stfd f22, 0x48(r1)
	stfd f23, 0x50(r1)
	stfd f24, 0x58(r1)
	stfd f25, 0x60(r1)
	stfd f26, 0x68(r1)
	stfd f27, 0x70(r1)
	stfd f28, 0x78(r1)
	stfd f29, 0x80(r1)
	lis r5, constdata@ha
	addi r5, r5, constdata@l
	lis r6, 0x407
	addi r6, r6, 0x7
	mtspr GQR7, r6
	lfs f22, 0x0(r5)
	lfs f23, 0x4(r5)
	lfs f24, 0x8(r5)
	lfs f25, 0xc(r5)
	lfs f26, 0x10(r5)
	lfs f27, 0x14(r5)
	lfs f28, 0x18(r5)
	lfs f29, 0x1c(r5)
	lis r7, 0x0
	addi r8, r3, 0x0
	addi r4, r1, 0x88
.Lrows:
	addi r7, r7, 0x1
	bl PS_IDCT_row
	cmpwi r7, 0x4
	addi r3, r3, 0x20
	addi r4, r4, 0x40
	blt .Lrows
	lis r7, 0x0
	addi r3, r8, 0x0
	addi r4, r1, 0x88
.Lcols:
	addi r7, r7, 0x1
	bl PS_IDCT_col
	cmpwi r7, 0x4
	addi r3, r3, 0x4
	addi r4, r4, 0x8
	blt .Lcols
	lwz r0, 0x18c(r1)
	mtlr r0
	lfd f14, 0x8(r1)
	lfd f15, 0x10(r1)
	lfd f16, 0x18(r1)
	lfd f17, 0x20(r1)
	lfd f18, 0x28(r1)
	lfd f19, 0x30(r1)
	lfd f20, 0x38(r1)
	lfd f21, 0x40(r1)
	lfd f22, 0x48(r1)
	lfd f23, 0x50(r1)
	lfd f24, 0x58(r1)
	lfd f25, 0x60(r1)
	lfd f26, 0x68(r1)
	lfd f27, 0x70(r1)
	lfd f28, 0x78(r1)
	lfd f29, 0x80(r1)
	addi r1, r1, 0x188
	blr
.endfn GC_IDCT

.data
.balign 32

# cos(k*pi/16)/2 factors
.obj constdata, local
	.float 0.35355338
	.float 0.46193975
	.float 0.19134171
	.float 0.49039263
	.float 0.4157348
	.float 0.27778512
	.float 0.09754516
	.float 0
.endobj constdata
