	.arch armv8-a
	.file	"saxpy.cc"
	.text
	.section	.rodata._ZNSt13random_deviceC2Ev.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"default"
	.section	.text._ZNSt13random_deviceC2Ev,"axG",@progbits,_ZNSt13random_deviceC5Ev,comdat
	.align	2
	.p2align 4,,11
	.weak	_ZNSt13random_deviceC2Ev
	.type	_ZNSt13random_deviceC2Ev, %function
_ZNSt13random_deviceC2Ev:
.LFB2282:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA2282
	stp	x29, x30, [sp, -80]!
	.cfi_def_cfa_offset 80
	.cfi_offset 29, -80
	.cfi_offset 30, -72
	adrp	x3, :got:__stack_chk_guard
	adrp	x2, .LC0
	add	x2, x2, :lo12:.LC0
	mov	x29, sp
	ldr	x3, [x3, #:got_lo12:__stack_chk_guard]
	stp	x19, x20, [sp, 16]
	.cfi_offset 19, -64
	.cfi_offset 20, -56
	ldr	w5, [x2]
	ldr	w2, [x2, 3]
	ldr	x6, [x3]
	str	x6, [sp, 72]
	mov	x6, 0
	add	x19, sp, 56
	mov	x4, 7
	str	w5, [sp, 56]
	add	x1, sp, 40
	strb	wzr, [sp, 63]
	str	w2, [x19, 3]
	stp	x19, x4, [sp, 40]
.LEHB0:
	bl	_ZNSt13random_device7_M_initERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
.LEHE0:
	ldr	x0, [sp, 40]
	cmp	x0, x19
	beq	.L1
	ldr	x1, [sp, 56]
	add	x1, x1, 1
	bl	_ZdlPvm
.L1:
	adrp	x0, :got:__stack_chk_guard
	ldr	x0, [x0, #:got_lo12:__stack_chk_guard]
	ldr	x2, [sp, 72]
	ldr	x1, [x0]
	subs	x2, x2, x1
	mov	x1, 0
	bne	.L9
	ldp	x19, x20, [sp, 16]
	ldp	x29, x30, [sp], 80
	.cfi_remember_state
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
.L9:
	.cfi_restore_state
	bl	__stack_chk_fail
.L6:
	ldr	x2, [sp, 40]
	mov	x20, x0
	cmp	x2, x19
	beq	.L4
	ldr	x1, [sp, 56]
	mov	x0, x2
	add	x1, x1, 1
	bl	_ZdlPvm
.L4:
	mov	x0, x20
.LEHB1:
	bl	_Unwind_Resume
.LEHE1:
	.cfi_endproc
.LFE2282:
	.global	__gxx_personality_v0
	.section	.gcc_except_table._ZNSt13random_deviceC2Ev,"aG",@progbits,_ZNSt13random_deviceC5Ev,comdat
.LLSDA2282:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE2282-.LLSDACSB2282
.LLSDACSB2282:
	.uleb128 .LEHB0-.LFB2282
	.uleb128 .LEHE0-.LEHB0
	.uleb128 .L6-.LFB2282
	.uleb128 0
	.uleb128 .LEHB1-.LFB2282
	.uleb128 .LEHE1-.LEHB1
	.uleb128 0
	.uleb128 0
.LLSDACSE2282:
	.section	.text._ZNSt13random_deviceC2Ev,"axG",@progbits,_ZNSt13random_deviceC5Ev,comdat
	.size	_ZNSt13random_deviceC2Ev, .-_ZNSt13random_deviceC2Ev
	.weak	_ZNSt13random_deviceC1Ev
	.set	_ZNSt13random_deviceC1Ev,_ZNSt13random_deviceC2Ev
	.section	.text._ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv,"axG",@progbits,_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv,comdat
	.align	2
	.p2align 4,,11
	.weak	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	.type	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv, %function
_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv:
.LFB3572:
	.cfi_startproc
	ldr	x5, [x0]
	mov	x8, 45279
	mov	x2, x0
	add	x7, x0, 1816
	mov	x3, x0
	movk	x8, 0x9908, lsl 16
	.p2align 3,,7
.L12:
	and	x6, x5, -2147483648
	ldr	x5, [x3, 8]
	ldr	x1, [x3, 3176]
	and	x4, x5, 2147483647
	orr	x4, x4, x6
	eor	x1, x1, x4, lsr 1
	tst	x4, 1
	eor	x4, x1, x8
	csel	x1, x4, x1, ne
	str	x1, [x3], 8
	cmp	x7, x3
	bne	.L12
	ldr	x4, [x0, 1816]
	mov	x6, 45279
	add	x7, x0, 3168
	movk	x6, 0x9908, lsl 16
	.p2align 3,,7
.L14:
	and	x5, x4, -2147483648
	add	x2, x2, 8
	ldr	x4, [x2, 1816]
	ldr	x1, [x2, -8]
	and	x3, x4, 2147483647
	orr	x3, x3, x5
	eor	x1, x1, x3, lsr 1
	tst	x3, 1
	eor	x3, x1, x6
	csel	x1, x3, x1, ne
	str	x1, [x2, 1808]
	cmp	x7, x2
	bne	.L14
	ldr	x4, [x0]
	mov	x3, 45279
	ldr	x2, [x0, 4984]
	movk	x3, 0x9908, lsl 16
	ldr	x1, [x0, 3168]
	str	xzr, [x0, 4992]
	bfi	x2, x4, 0, 31
	eor	x1, x1, x2, lsr 1
	tst	x2, 1
	eor	x2, x1, x3
	csel	x1, x2, x1, ne
	str	x1, [x0, 4984]
	ret
	.cfi_endproc
.LFE3572:
	.size	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv, .-_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC1:
	.string	"%lf\n"
	.section	.text.startup,"ax",@progbits
	.align	2
	.p2align 4,,11
	.global	main
	.type	main, %function
main:
.LFB2912:
	.cfi_startproc
	.cfi_personality 0x9b,DW.ref.__gxx_personality_v0
	.cfi_lsda 0x1b,.LLSDA2912
	mov	x12, 42896
	sub	sp, sp, x12
	.cfi_def_cfa_offset 42896
	adrp	x1, :got:__stack_chk_guard
	add	x2, sp, 32768
	stp	x29, x30, [sp]
	.cfi_offset 29, -42896
	.cfi_offset 30, -42888
	mov	x29, sp
	ldr	x1, [x1, #:got_lo12:__stack_chk_guard]
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	.cfi_offset 19, -42880
	.cfi_offset 20, -42872
	.cfi_offset 21, -42864
	.cfi_offset 22, -42856
	add	x21, sp, 120
	mov	x0, x21
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	d8, d9, [sp, 80]
	str	d10, [sp, 96]
	.cfi_offset 23, -42848
	.cfi_offset 24, -42840
	.cfi_offset 25, -42832
	.cfi_offset 26, -42824
	.cfi_offset 72, -42816
	.cfi_offset 73, -42808
	.cfi_offset 74, -42800
	ldr	x3, [x1]
	str	x3, [x2, 10120]
	mov	x3, 0
.LEHB2:
	bl	_ZNSt13random_deviceC1Ev
.LEHE2:
	mov	x0, x21
.LEHB3:
	bl	_ZNSt13random_device9_M_getvalEv
	mov	x4, 5120
	add	x24, sp, x4
	uxtw	x1, w0
	mov	x3, 35173
	add	x0, x24, 8
	mov	x2, 1
	movk	x3, 0x6c07, lsl 16
	str	x1, [sp, 5120]
	.p2align 3,,7
.L25:
	eor	x1, x1, x1, lsr 30
	madd	w1, w1, w3, w2
	add	x2, x2, 1
	str	x1, [x0], 8
	cmp	x2, 624
	bne	.L25
	mov	x26, 22144
	mov	w0, 796917760
	mov	x1, 10120
	mov	x3, 26504
	movi	v10.2s, #0
	add	x20, sp, x1
	add	x19, sp, x3
	mov	x23, 1
	fmov	s9, w0
	movk	x26, 0x9d2c, lsl 16
	mov	x25, 4022730752
	fmov	s8, 1.0e+0
	str	x2, [sp, 10112]
	b	.L26
	.p2align 2,,3
.L29:
	ldr	x1, [x24, x2, lsl 3]
	add	x3, x2, 1
	str	x3, [sp, 10112]
	ubfx	x0, x1, 11, 32
	eor	x1, x1, x0
	and	x0, x26, x1, lsl 7
	eor	x1, x1, x0
	and	x0, x25, x1, lsl 15
	eor	x1, x1, x0
	eor	x1, x1, x1, lsr 18
	ucvtf	s0, x1
	fadd	s0, s0, s10
	fmul	s0, s0, s9
	fcmpe	s0, s8
	bge	.L38
	fadd	s0, s0, s8
.L27:
	lsl	x22, x23, 2
	add	x0, x20, x22
	str	s0, [x0, -4]
	cmp	x3, 623
	bhi	.L46
.L30:
	ldr	x1, [x24, x3, lsl 3]
	add	x2, x3, 1
	str	x2, [sp, 10112]
	ubfx	x0, x1, 11, 32
	eor	x1, x1, x0
	and	x0, x26, x1, lsl 7
	eor	x1, x1, x0
	and	x0, x25, x1, lsl 15
	eor	x1, x1, x0
	eor	x1, x1, x1, lsr 18
	ucvtf	s0, x1
	fadd	s0, s0, s10
	fmul	s0, s0, s9
	fcmpe	s0, s8
	bge	.L31
	fadd	s0, s0, s8
	add	x22, x22, x19
	add	x23, x23, 1
	mov	x0, 4097
	str	s0, [x22, -4]
	cmp	x23, x0
	beq	.L47
.L26:
	cmp	x2, 623
	bls	.L29
	mov	x0, x24
	bl	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	ldr	x2, [sp, 10112]
	b	.L29
	.p2align 2,,3
.L46:
	mov	x0, x24
	bl	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	ldr	x3, [sp, 10112]
	b	.L30
	.p2align 2,,3
.L38:
	fmov	s0, 2.0e+0
	b	.L27
	.p2align 2,,3
.L31:
	add	x22, x22, x19
	fmov	s0, 2.0e+0
	add	x23, x23, 1
	mov	x0, 4097
	str	s0, [x22, -4]
	cmp	x23, x0
	bne	.L26
.L47:
	mov	x0, 1999
	bl	m5_hypercall
	mov	x1, 1
	fmov	s2, 5.0e-1
	mov	x3, 4097
	.p2align 3,,7
.L34:
	lsl	x0, x1, 2
	add	x1, x1, 1
	add	x2, x20, x0
	add	x0, x0, x19
	ldr	s0, [x2, -4]
	ldr	s1, [x0, -4]
	fmul	s0, s0, s2
	fadd	s0, s0, s1
	str	s0, [x0, -4]
	cmp	x1, x3
	bne	.L34
	mov	x0, 2000
	bl	m5_hypercall
	movi	v0.2s, #0
	mov	x0, 42888
	add	x0, sp, x0
	.p2align 3,,7
.L35:
	ldr	s1, [x19], 4
	fadd	s0, s0, s1
	cmp	x0, x19
	bne	.L35
	fcvt	d0, s0
	adrp	x1, .LC1
	mov	w0, 1
	add	x1, x1, :lo12:.LC1
	bl	__printf_chk
.LEHE3:
	mov	x0, x21
	bl	_ZNSt13random_device7_M_finiEv
	adrp	x0, :got:__stack_chk_guard
	add	x2, sp, 32768
	ldr	x0, [x0, #:got_lo12:__stack_chk_guard]
	ldr	x3, [x2, 10120]
	ldr	x1, [x0]
	subs	x3, x3, x1
	mov	x1, 0
	bne	.L48
	mov	w0, 0
	mov	x12, 42896
	ldp	x29, x30, [sp]
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	d8, d9, [sp, 80]
	ldr	d10, [sp, 96]
	add	sp, sp, x12
	.cfi_remember_state
	.cfi_restore 29
	.cfi_restore 30
	.cfi_restore 25
	.cfi_restore 26
	.cfi_restore 23
	.cfi_restore 24
	.cfi_restore 21
	.cfi_restore 22
	.cfi_restore 19
	.cfi_restore 20
	.cfi_restore 74
	.cfi_restore 72
	.cfi_restore 73
	.cfi_def_cfa_offset 0
	ret
.L48:
	.cfi_restore_state
	bl	__stack_chk_fail
.L39:
	mov	x19, x0
	mov	x0, x21
	bl	_ZNSt13random_device7_M_finiEv
	mov	x0, x19
.LEHB4:
	bl	_Unwind_Resume
.LEHE4:
	.cfi_endproc
.LFE2912:
	.section	.gcc_except_table,"a",@progbits
.LLSDA2912:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE2912-.LLSDACSB2912
.LLSDACSB2912:
	.uleb128 .LEHB2-.LFB2912
	.uleb128 .LEHE2-.LEHB2
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB3-.LFB2912
	.uleb128 .LEHE3-.LEHB3
	.uleb128 .L39-.LFB2912
	.uleb128 0
	.uleb128 .LEHB4-.LFB2912
	.uleb128 .LEHE4-.LEHB4
	.uleb128 0
	.uleb128 0
.LLSDACSE2912:
	.section	.text.startup
	.size	main, .-main
	.hidden	DW.ref.__gxx_personality_v0
	.weak	DW.ref.__gxx_personality_v0
	.section	.data.rel.local.DW.ref.__gxx_personality_v0,"awG",@progbits,DW.ref.__gxx_personality_v0,comdat
	.align	3
	.type	DW.ref.__gxx_personality_v0, %object
	.size	DW.ref.__gxx_personality_v0, 8
DW.ref.__gxx_personality_v0:
	.xword	__gxx_personality_v0
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
