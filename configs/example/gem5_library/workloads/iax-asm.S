	.arch armv8-a
	.file	"iax.cc"
	.text
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align	3
.LC0:
	.string	"%d\n"
	.section	.text.startup,"ax",@progbits
	.align	2
	.p2align 4,,11
	.global	main
	.type	main, %function
main:
.LFB2912:
	.cfi_startproc
	mov	x12, 32832
	sub	sp, sp, x12
	.cfi_def_cfa_offset 32832
	adrp	x0, :got:__stack_chk_guard
	add	x1, sp, 32768
	stp	x29, x30, [sp]
	.cfi_offset 29, -32832
	.cfi_offset 30, -32824
	mov	x29, sp
	ldr	x0, [x0, #:got_lo12:__stack_chk_guard]
	stp	x19, x20, [sp, 16]
	.cfi_offset 19, -32816
	.cfi_offset 20, -32808
	add	x20, sp, 56
	str	x21, [sp, 32]
	.cfi_offset 21, -32800
	mov	x19, x20
	add	x21, x20, 16384
	ldr	x2, [x0]
	str	x2, [x1, 56]
	mov	x2, 0
	.p2align 3,,7
.L2:
	bl	rand
	str	w0, [x19], 4
	cmp	x19, x21
	bne	.L2
	mov	x2, 16440
	mov	x0, 1999
	add	x19, sp, x2
	bl	m5_hypercall
	mov	x1, 1
	mov	x3, 4097
	.p2align 3,,7
.L3:
	lsl	x0, x1, 2
	add	x1, x1, 1
	add	x2, x20, x0
	add	x0, x19, x0
	ldr	w2, [x2, -4]
	lsl	w2, w2, 1
	str	w2, [x0, -4]
	cmp	x1, x3
	bne	.L3
	mov	x0, 2000
	bl	m5_hypercall
	mov	x1, 32824
	mov	x0, x19
	add	x3, sp, x1
	mov	w2, 0
	.p2align 3,,7
.L4:
	ldr	w1, [x0], 4
	add	w2, w2, w1
	cmp	x0, x3
	bne	.L4
	adrp	x1, .LC0
	mov	w0, 1
	add	x1, x1, :lo12:.LC0
	bl	__printf_chk
	adrp	x0, :got:__stack_chk_guard
	add	x2, sp, 32768
	ldr	x0, [x0, #:got_lo12:__stack_chk_guard]
	ldr	x3, [x2, 56]
	ldr	x1, [x0]
	subs	x3, x3, x1
	mov	x1, 0
	bne	.L11
	mov	w0, 0
	mov	x12, 32832
	ldp	x29, x30, [sp]
	ldp	x19, x20, [sp, 16]
	ldr	x21, [sp, 32]
	add	sp, sp, x12
	.cfi_remember_state
	.cfi_restore 29
	.cfi_restore 30
	.cfi_restore 21
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
.L11:
	.cfi_restore_state
	bl	__stack_chk_fail
	.cfi_endproc
.LFE2912:
	.size	main, .-main
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
