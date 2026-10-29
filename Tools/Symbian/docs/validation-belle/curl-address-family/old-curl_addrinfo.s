	.syntax	unified
	.eabi_attribute	67, "2.09"	@ Tag_conformance
	.cpu	arm1136jf-s
	.eabi_attribute	6, 6	@ Tag_CPU_arch
	.eabi_attribute	8, 1	@ Tag_ARM_ISA_use
	.eabi_attribute	9, 1	@ Tag_THUMB_ISA_use
	.fpu	vfpv2
	.eabi_attribute	34, 0	@ Tag_CPU_unaligned_access
	.eabi_attribute	17, 1	@ Tag_ABI_PCS_GOT_use
	.eabi_attribute	20, 1	@ Tag_ABI_FP_denormal
	.eabi_attribute	21, 0	@ Tag_ABI_FP_exceptions
	.eabi_attribute	23, 3	@ Tag_ABI_FP_number_model
	.eabi_attribute	24, 1	@ Tag_ABI_align_needed
	.eabi_attribute	25, 1	@ Tag_ABI_align_preserved
	.eabi_attribute	38, 1	@ Tag_ABI_FP_16bit_format
	.eabi_attribute	18, 2	@ Tag_ABI_PCS_wchar_t
	.eabi_attribute	26, 2	@ Tag_ABI_enum_size
	.eabi_attribute	14, 0	@ Tag_ABI_PCS_R9_use
	.file	"curl_addrinfo.c"
	.section	.text.Curl_freeaddrinfo,"ax",%progbits
	.hidden	Curl_freeaddrinfo               @ -- Begin function Curl_freeaddrinfo
	.globl	Curl_freeaddrinfo
	.p2align	2
	.type	Curl_freeaddrinfo,%function
	.code	32
Curl_freeaddrinfo:                      @ @Curl_freeaddrinfo
	.fnstart
@ %bb.0:
	cmp	r0, #0
	bxeq	lr
.LBB0_1:
	.save	{r4, r5, r11, lr}
	push	{r4, r5, r11, lr}
	ldr	r4, .LCPI0_0
.LBB0_2:                                @ =>This Inner Loop Header: Depth=1
	ldr	r1, [r4]
	ldr	r5, [r0, #28]
	blx	r1
	mov	r0, r5
	cmp	r5, #0
	bne	.LBB0_2
@ %bb.3:
	pop	{r4, r5, r11, lr}
	bx	lr
	.p2align	2
@ %bb.4:
.LCPI0_0:
	.long	Curl_cfree
.Lfunc_end0:
	.size	Curl_freeaddrinfo, .Lfunc_end0-Curl_freeaddrinfo
	.cantunwind
	.fnend
                                        @ -- End function
	.section	.text.Curl_getaddrinfo_ex,"ax",%progbits
	.hidden	Curl_getaddrinfo_ex             @ -- Begin function Curl_getaddrinfo_ex
	.globl	Curl_getaddrinfo_ex
	.p2align	2
	.type	Curl_getaddrinfo_ex,%function
	.code	32
Curl_getaddrinfo_ex:                    @ @Curl_getaddrinfo_ex
	.fnstart
@ %bb.0:
	.save	{r4, r5, r6, r7, r8, r9, r10, r11, lr}
	push	{r4, r5, r6, r7, r8, r9, r10, r11, lr}
	.pad	#4
	sub	sp, sp, #4
	mov	r10, #0
	mov	r8, r3
	str	r10, [r3]
	mov	r3, sp
	bl	getaddrinfo
	cmp	r0, #0
	beq	.LBB1_2
@ %bb.1:
	add	sp, sp, #4
	pop	{r4, r5, r6, r7, r8, r9, r10, r11, pc}
.LBB1_2:
	ldr	r4, [sp]
	cmp	r4, #0
	beq	.LBB1_16
@ %bb.3:
	mov	r9, #0
	mov	r10, #0
	mov	r11, #0
	b	.LBB1_6
.LBB1_4:                                @   in Loop: Header=BB1_6 Depth=1
	cmp	r10, #0
	moveq	r10, r7
	cmp	r11, #0
	strne	r7, [r11, #28]
	mov	r11, r7
.LBB1_5:                                @   in Loop: Header=BB1_6 Depth=1
	ldr	r4, [r4, #28]
	cmp	r4, #0
	beq	.LBB1_14
.LBB1_6:                                @ =>This Inner Loop Header: Depth=1
	ldr	r0, [r4, #20]
	mov	r6, #0
	cmp	r0, #0
	beq	.LBB1_8
@ %bb.7:                                @   in Loop: Header=BB1_6 Depth=1
	bl	strlen
	add	r6, r0, #1
.LBB1_8:                                @   in Loop: Header=BB1_6 Depth=1
	ldr	r0, [r4, #4]
	cmp	r0, #2048
	bne	.LBB1_5
@ %bb.9:                                @   in Loop: Header=BB1_6 Depth=1
	ldr	r0, [r4, #24]
	cmp	r0, #0
	beq	.LBB1_5
@ %bb.10:                               @   in Loop: Header=BB1_6 Depth=1
	ldr	r0, [r4, #16]
	cmp	r0, #28
	blo	.LBB1_5
@ %bb.11:                               @   in Loop: Header=BB1_6 Depth=1
	ldr	r0, .LCPI1_0
	ldr	r1, [r0]
	add	r0, r6, #60
	blx	r1
	cmp	r0, #0
	beq	.LBB1_17
@ %bb.12:                               @   in Loop: Header=BB1_6 Depth=1
	ldm	r4, {r2, r3, r5, r12}
	mov	r7, r0
	ldr	r1, [r4, #24]
	str	r9, [r0, #28]
	str	r9, [r0, #20]
	mov	r0, #28
	str	r0, [r7, #16]
	add	r0, r7, #32
	stm	r7, {r2, r3, r5, r12}
	mov	r2, #28
	str	r0, [r7, #24]
	bl	memcpy
	cmp	r6, #0
	beq	.LBB1_4
@ %bb.13:                               @   in Loop: Header=BB1_6 Depth=1
	ldr	r0, [r7, #24]
	ldr	r1, [r4, #20]
	mov	r2, r6
	add	r0, r0, #28
	str	r0, [r7, #20]
	bl	memcpy
	b	.LBB1_4
.LBB1_14:
	ldr	r0, [sp]
	cmp	r0, #0
	beq	.LBB1_16
@ %bb.15:
	bl	freeaddrinfo
.LBB1_16:
	mov	r0, #0
	cmp	r10, #0
	moveq	r0, #8
	str	r10, [r8]
	add	sp, sp, #4
	pop	{r4, r5, r6, r7, r8, r9, r10, r11, pc}
.LBB1_17:
	ldr	r0, [sp]
	cmp	r0, #0
	beq	.LBB1_19
@ %bb.18:
	bl	freeaddrinfo
.LBB1_19:
	mov	r0, #6
	cmp	r10, #0
	beq	.LBB1_23
@ %bb.20:
	ldr	r4, .LCPI1_1
.LBB1_21:                               @ =>This Inner Loop Header: Depth=1
	ldr	r1, [r4]
	mov	r0, r10
	ldr	r5, [r10, #28]
	blx	r1
	mov	r10, r5
	cmp	r5, #0
	bne	.LBB1_21
@ %bb.22:
	mov	r10, #0
	mov	r0, #6
	str	r10, [r8]
	add	sp, sp, #4
	pop	{r4, r5, r6, r7, r8, r9, r10, r11, pc}
.LBB1_23:
	mov	r10, #0
	str	r10, [r8]
	add	sp, sp, #4
	pop	{r4, r5, r6, r7, r8, r9, r10, r11, pc}
	.p2align	2
@ %bb.24:
.LCPI1_0:
	.long	Curl_cmalloc
.LCPI1_1:
	.long	Curl_cfree
.Lfunc_end1:
	.size	Curl_getaddrinfo_ex, .Lfunc_end1-Curl_getaddrinfo_ex
	.cantunwind
	.fnend
                                        @ -- End function
	.section	.text.Curl_he2ai,"ax",%progbits
	.hidden	Curl_he2ai                      @ -- Begin function Curl_he2ai
	.globl	Curl_he2ai
	.p2align	2
	.type	Curl_he2ai,%function
	.code	32
Curl_he2ai:                             @ @Curl_he2ai
	.fnstart
@ %bb.0:
	.save	{r4, r5, r6, r7, r8, r9, r10, r11, lr}
	push	{r4, r5, r6, r7, r8, r9, r10, r11, lr}
	.pad	#4
	sub	sp, sp, #4
	mov	r4, #0
	cmp	r0, #0
	beq	.LBB2_11
@ %bb.1:
	mov	r9, r0
	ldr	r0, [r0, #16]
	ldr	r10, [r0]
	cmp	r10, #0
	beq	.LBB2_11
@ %bb.2:
	mov	r11, #0
	mov	r7, #4
	uxth	r8, r1
	b	.LBB2_4
.LBB2_3:                                @   in Loop: Header=BB2_4 Depth=1
	ldr	r0, [r9, #16]
	mov	r11, r6
	ldr	r10, [r0, r7]
	add	r7, r7, #4
	cmp	r10, #0
	beq	.LBB2_11
.LBB2_4:                                @ =>This Inner Loop Header: Depth=1
	ldr	r0, [r9]
	bl	strlen
	mov	r5, r0
	ldr	r0, .LCPI2_0
	add	r1, r5, #61
	ldr	r2, [r0]
	mov	r0, #1
	blx	r2
	cmp	r0, #0
	beq	.LBB2_7
@ %bb.5:                                @   in Loop: Header=BB2_4 Depth=1
	mov	r6, r0
	ldr	r1, [r9]
	add	r2, r6, #32
	add	r0, r0, #60
	str	r2, [r6, #24]
	add	r2, r5, #1
	str	r0, [r6, #20]
	bl	memcpy
	mov	r0, #28
	cmp	r4, #0
	str	r0, [r6, #16]
	ldr	r0, [r9, #8]
	moveq	r4, r6
	cmp	r11, #0
	mov	r1, #1
	strne	r6, [r11, #28]
	cmp	r0, #2048
	str	r0, [r6, #4]
	str	r1, [r6, #8]
	bne	.LBB2_3
@ %bb.6:                                @   in Loop: Header=BB2_4 Depth=1
	ldr	r5, [r6, #24]
	mov	r1, r10
	mov	r2, #4
	add	r0, r5, #4
	bl	memcpy
	ldrb	r0, [r9, #8]
	strh	r0, [r5]
	mov	r0, r8
	bl	htons
	strh	r0, [r5, #2]
	b	.LBB2_3
.LBB2_7:
	cmp	r4, #0
	beq	.LBB2_10
@ %bb.8:
	ldr	r5, .LCPI2_1
.LBB2_9:                                @ =>This Inner Loop Header: Depth=1
	ldr	r1, [r5]
	mov	r0, r4
	ldr	r6, [r4, #28]
	blx	r1
	mov	r4, r6
	cmp	r6, #0
	bne	.LBB2_9
.LBB2_10:
	mov	r4, #0
.LBB2_11:
	mov	r0, r4
	add	sp, sp, #4
	pop	{r4, r5, r6, r7, r8, r9, r10, r11, pc}
	.p2align	2
@ %bb.12:
.LCPI2_0:
	.long	Curl_ccalloc
.LCPI2_1:
	.long	Curl_cfree
.Lfunc_end2:
	.size	Curl_he2ai, .Lfunc_end2-Curl_he2ai
	.cantunwind
	.fnend
                                        @ -- End function
	.section	.text.Curl_ip2addr,"ax",%progbits
	.hidden	Curl_ip2addr                    @ -- Begin function Curl_ip2addr
	.globl	Curl_ip2addr
	.p2align	2
	.type	Curl_ip2addr,%function
	.code	32
Curl_ip2addr:                           @ @Curl_ip2addr
	.fnstart
@ %bb.0:
	.save	{r4, r5, r6, r7, r8, r9, r11, lr}
	push	{r4, r5, r6, r7, r8, r9, r11, lr}
	mov	r7, r0
	ldr	r0, .LCPI3_0
	mov	r9, r1
	mov	r8, r3
	mov	r5, r2
	ldr	r1, [r0]
	mov	r0, #32
	blx	r1
	cmp	r0, #0
	beq	.LBB3_5
@ %bb.1:
	mov	r4, r0
	ldr	r0, .LCPI3_1
	ldr	r1, [r0]
	mov	r0, r5
	blx	r1
	cmp	r0, #0
	beq	.LBB3_6
@ %bb.2:
	mov	r6, r0
	mov	r5, #0
	cmp	r7, #2048
	bne	.LBB3_4
@ %bb.3:
	add	r5, r4, #20
	mov	r0, r5
	mov	r1, r9
	mov	r2, #4
	mov	r7, #4
	bl	memcpy
	mov	r0, #0
	add	r1, r4, #24
	mov	r2, #2048
	str	r1, [r4, #16]
	str	r0, [r4, #28]
	stmib	r4, {r0, r2, r7}
	mov	r0, r4
	mov	r1, r8
	str	r5, [r4, #24]
	str	r6, [r4]
	bl	Curl_he2ai
	mov	r5, r0
.LBB3_4:
	ldr	r0, .LCPI3_2
	ldr	r1, [r0]
	mov	r0, r6
	blx	r1
	b	.LBB3_7
.LBB3_5:
	mov	r0, #0
	pop	{r4, r5, r6, r7, r8, r9, r11, pc}
.LBB3_6:
	mov	r5, #0
.LBB3_7:
	ldr	r0, .LCPI3_2
	ldr	r1, [r0]
	mov	r0, r4
	blx	r1
	mov	r0, r5
	pop	{r4, r5, r6, r7, r8, r9, r11, pc}
	.p2align	2
@ %bb.8:
.LCPI3_0:
	.long	Curl_cmalloc
.LCPI3_1:
	.long	Curl_cstrdup
.LCPI3_2:
	.long	Curl_cfree
.Lfunc_end3:
	.size	Curl_ip2addr, .Lfunc_end3-Curl_ip2addr
	.cantunwind
	.fnend
                                        @ -- End function
	.section	.text.Curl_str2addr,"ax",%progbits
	.hidden	Curl_str2addr                   @ -- Begin function Curl_str2addr
	.globl	Curl_str2addr
	.p2align	2
	.type	Curl_str2addr,%function
	.code	32
Curl_str2addr:                          @ @Curl_str2addr
	.fnstart
@ %bb.0:
	.save	{r4, r5, r6, r7, r8, lr}
	push	{r4, r5, r6, r7, r8, lr}
	.pad	#8
	sub	sp, sp, #8
	mov	r7, r0
	mov	r4, r1
	add	r2, sp, #4
	mov	r0, #2048
	mov	r1, r7
	bl	__inet_pton
	mov	r6, #0
	cmp	r0, #1
	blt	.LBB4_6
@ %bb.1:
	ldr	r0, .LCPI4_0
	ldr	r1, [r0]
	mov	r0, #32
	blx	r1
	cmp	r0, #0
	beq	.LBB4_6
@ %bb.2:
	mov	r5, r0
	ldr	r0, .LCPI4_1
	ldr	r1, [r0]
	mov	r0, r7
	blx	r1
	cmp	r0, #0
	beq	.LBB4_4
@ %bb.3:
	add	r6, r5, #20
	mov	r7, r0
	add	r1, sp, #4
	mov	r0, r6
	mov	r2, #4
	mov	r8, #4
	bl	memcpy
	mov	r0, #0
	add	r1, r5, #24
	mov	r2, #2048
	str	r1, [r5, #16]
	str	r0, [r5, #28]
	stmib	r5, {r0, r2, r8}
	mov	r0, r5
	mov	r1, r4
	str	r6, [r5, #24]
	str	r7, [r5]
	bl	Curl_he2ai
	mov	r6, r0
	ldr	r0, .LCPI4_2
	ldr	r1, [r0]
	mov	r0, r7
	blx	r1
	b	.LBB4_5
.LBB4_4:
	mov	r6, #0
.LBB4_5:
	ldr	r0, .LCPI4_2
	ldr	r1, [r0]
	mov	r0, r5
	blx	r1
.LBB4_6:
	mov	r0, r6
	add	sp, sp, #8
	pop	{r4, r5, r6, r7, r8, pc}
	.p2align	2
@ %bb.7:
.LCPI4_0:
	.long	Curl_cmalloc
.LCPI4_1:
	.long	Curl_cstrdup
.LCPI4_2:
	.long	Curl_cfree
.Lfunc_end4:
	.size	Curl_str2addr, .Lfunc_end4-Curl_str2addr
	.cantunwind
	.fnend
                                        @ -- End function
	.ident	"clang version 23.1.2 (https://github.com/llvm/llvm-project 85ac560262434c9ccfc0c183ec22d4138ed647fb)"
	.section	".note.GNU-stack","",%progbits
	.addrsig
	.eabi_attribute	30, 2	@ Tag_ABI_optimization_goals
