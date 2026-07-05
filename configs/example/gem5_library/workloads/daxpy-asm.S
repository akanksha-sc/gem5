	.arch armv8-a
	.file	"daxpy.cc"
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
	sub	sp, sp, #65536
	.cfi_def_cfa_offset 65536
	str	xzr, [sp, 1024]
	mov	x12, 10128
	sub	sp, sp, x12
	.cfi_def_cfa_offset 75664
	adrp	x1, :got:__stack_chk_guard
	add	x2, sp, 65536
	stp	x29, x30, [sp]
	.cfi_offset 29, -75664
	.cfi_offset 30, -75656
	mov	x29, sp
	ldr	x1, [x1, #:got_lo12:__stack_chk_guard]
	stp	x19, x20, [sp, 16]
	stp	x21, x22, [sp, 32]
	.cfi_offset 19, -75648
	.cfi_offset 20, -75640
	.cfi_offset 21, -75632
	.cfi_offset 22, -75624
	add	x21, sp, 120
	mov	x0, x21
	stp	x23, x24, [sp, 48]
	stp	x25, x26, [sp, 64]
	stp	d8, d9, [sp, 80]
	stp	d10, d11, [sp, 96]
	.cfi_offset 23, -75616
	.cfi_offset 24, -75608
	.cfi_offset 25, -75600
	.cfi_offset 26, -75592
	.cfi_offset 72, -75584
	.cfi_offset 73, -75576
	.cfi_offset 74, -75568
	.cfi_offset 75, -75560
	ldr	x3, [x1]
	str	x3, [x2, 10120]
	mov	x3, 0
.LEHB2:
	bl	_ZNSt13random_deviceC1Ev
.LEHE2:
	mov	x0, x21
.LEHB3:
	bl	_ZNSt13random_device9_M_getvalEv
	mov	x3, 5120
	add	x25, sp, x3
	add	x2, x25, 8
	mov	x3, 35173
	uxtw	x1, w0
	movk	x3, 0x6c07, lsl 16
	mov	x0, 1
	str	x1, [sp, 5120]
	.p2align 3,,7
.L25:
	eor	x1, x1, x1, lsr 30
	madd	w1, w1, w3, w0
	add	x0, x0, 1
	str	x1, [x2], 8
	cmp	x0, 624
	bne	.L25
	mov	x2, 4751297606875873280
	mov	x1, 4318952042648305664
	mov	x23, 22144
	fmov	d9, x2
	fmov	d8, x1
	mov	x2, 42888
	mov	x1, 10120
	movi	d10, #0
	add	x20, sp, x1
	add	x19, sp, x2
	mov	x24, 1
	movk	x23, 0x9d2c, lsl 16
	mov	x22, 4022730752
	str	x0, [sp, 10112]
	b	.L26
	.p2align 2,,3
.L32:
	ldr	x1, [x25, x0, lsl 3]
	add	x2, x0, 1
	str	x2, [sp, 10112]
	ubfx	x0, x1, 11, 32
	eor	x1, x1, x0
	and	x0, x23, x1, lsl 7
	eor	x1, x1, x0
	and	x0, x22, x1, lsl 15
	eor	x1, x1, x0
	eor	x1, x1, x1, lsr 18
	ucvtf	d11, x1
	fadd	d11, d11, d10
	cmp	x2, 623
	bhi	.L47
.L33:
	ldr	x1, [x25, x2, lsl 3]
	fmov	d1, 1.0e+0
	add	x2, x2, 1
	str	x2, [sp, 10112]
	ubfx	x0, x1, 11, 32
	eor	x1, x1, x0
	and	x0, x23, x1, lsl 7
	eor	x1, x1, x0
	and	x0, x22, x1, lsl 15
	eor	x1, x1, x0
	eor	x1, x1, x1, lsr 18
	ucvtf	d0, x1
	fmul	d0, d0, d9
	fadd	d0, d0, d11
	fmul	d0, d0, d8
	fcmpe	d0, d1
	bge	.L39
	fadd	d0, d0, d1
.L34:
	lsl	x26, x24, 3
	add	x0, x20, x26
	str	d0, [x0, -8]
	cmp	x2, 623
	bhi	.L48
.L27:
	ldr	x1, [x25, x2, lsl 3]
	add	x0, x2, 1
	str	x0, [sp, 10112]
	ubfx	x2, x1, 11, 32
	eor	x1, x1, x2
	and	x2, x23, x1, lsl 7
	eor	x1, x1, x2
	and	x2, x22, x1, lsl 15
	eor	x1, x1, x2
	eor	x1, x1, x1, lsr 18
	ucvtf	d11, x1
	fadd	d11, d11, d10
	cmp	x0, 623
	bhi	.L49
.L28:
	ldr	x1, [x25, x0, lsl 3]
	fmov	d1, 1.0e+0
	add	x0, x0, 1
	str	x0, [sp, 10112]
	ubfx	x2, x1, 11, 32
	eor	x1, x1, x2
	and	x2, x23, x1, lsl 7
	eor	x1, x1, x2
	and	x2, x22, x1, lsl 15
	eor	x1, x1, x2
	eor	x1, x1, x1, lsr 18
	ucvtf	d0, x1
	fmul	d0, d0, d9
	fadd	d0, d0, d11
	fmul	d0, d0, d8
	fcmpe	d0, d1
	bge	.L29
	fadd	d0, d0, d1
	add	x26, x26, x19
	add	x24, x24, 1
	mov	x1, 4097
	str	d0, [x26, -8]
	cmp	x24, x1
	beq	.L31
.L26:
	cmp	x0, 623
	bls	.L32
	mov	x0, x25
	bl	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	ldr	x0, [sp, 10112]
	b	.L32
	.p2align 2,,3
.L49:
	mov	x0, x25
	bl	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	ldr	x0, [sp, 10112]
	b	.L28
	.p2align 2,,3
.L48:
	mov	x0, x25
	bl	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	ldr	x2, [sp, 10112]
	b	.L27
	.p2align 2,,3
.L47:
	mov	x0, x25
	bl	_ZNSt23mersenne_twister_engineImLm32ELm624ELm397ELm31ELm2567483615ELm11ELm4294967295ELm7ELm2636928640ELm15ELm4022730752ELm18ELm1812433253EE11_M_gen_randEv
	ldr	x2, [sp, 10112]
	b	.L33
	.p2align 2,,3
.L39:
	fmov	d0, 2.0e+0
	b	.L34
	.p2align 2,,3
.L29:
	add	x26, x26, x19
	fmov	d0, 2.0e+0
	add	x24, x24, 1
	mov	x1, 4097
	str	d0, [x26, -8]
	cmp	x24, x1
	bne	.L26
.L31:
	mov	x0, 1999
	bl	m5_hypercall
	mov	x1, 1
	fmov	d2, 5.0e-1
	mov	x3, 4097
	.p2align 3,,7
.L35:
	lsl	x0, x1, 3
	add	x1, x1, 1
	add	x2, x20, x0
	add	x0, x0, x19
	ldr	d0, [x2, -8]
	ldr	d1, [x0, -8]
	fmul	d0, d0, d2
	fadd	d0, d0, d1
	str	d0, [x0, -8]
	cmp	x1, x3
	bne	.L35
	mov	x0, 2000
	bl	m5_hypercall
	add	x0, sp, 73728
	movi	d0, #0
	add	x0, x0, 1928
	.p2align 3,,7
.L36:
	ldr	d1, [x19], 8
	fadd	d0, d0, d1
	cmp	x0, x19
	bne	.L36
	adrp	x1, .LC1
	mov	w0, 1
	add	x1, x1, :lo12:.LC1
	bl	__printf_chk
.LEHE3:
	mov	x0, x21
	bl	_ZNSt13random_device7_M_finiEv
	adrp	x0, :got:__stack_chk_guard
	add	x2, sp, 65536
	ldr	x0, [x0, #:got_lo12:__stack_chk_guard]
	ldr	x3, [x2, 10120]
	ldr	x1, [x0]
	subs	x3, x3, x1
	mov	x1, 0
	bne	.L50
	ldp	x29, x30, [sp]
	mov	w0, 0
	ldp	x19, x20, [sp, 16]
	ldp	x21, x22, [sp, 32]
	ldp	x23, x24, [sp, 48]
	ldp	x25, x26, [sp, 64]
	ldp	d8, d9, [sp, 80]
	ldp	d10, d11, [sp, 96]
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
	.cfi_restore 75
	.cfi_restore 72
	.cfi_restore 73
	add	sp, sp, 1936
	.cfi_def_cfa_offset 73728
	add	sp, sp, 73728
	.cfi_def_cfa_offset 0
	ret
.L50:
	.cfi_restore_state
	bl	__stack_chk_fail
.L40:
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
	.uleb128 .L40-.LFB2912
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
