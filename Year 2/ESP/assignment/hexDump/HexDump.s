@ -----------------------------------------------------------------------
	@ Constant values
	.set	mem_start,	0xc0000100	@start of memory block
	.set	mem_size,	0x100		@size of block to print out

	.macro	loadsp, rb
	mov	\rb, #0x80000000	@ physical base address
	add	\rb, \rb, #0x00010000	@ Ser1
	.endm

	.macro	writeb, rb
	str	\rb, [r3, #0x14]	@ UTDR
	.endm
	@ -----------------------------------------------------------------------
	@ Global pre-initialised variables and string constants

	.data				@ Data segment follows
dumpMessage:	.asciz "Dump of "
	@ -----------------------------------------------------------------------
	@ Assembly-language preamble for the main module

	.text				@ Executable code follows

_start:		.global	_start			@ "_start" is required by the linker
	.global	main			@ "main" is our main program

	b	main			@ Start running the main program

	@ -----------------------------------------------------------------------
	@ Start of the main program

main:						@ Entry to the function "main"
	ldr 	r0, =dumpMessage
	mov r1, #8
	bl	puts
	
	ldr	r0, =mem_start		@ load up the start of the memory block
	mov r1, #8
	bl 	phex

	ldr r4, =mem_start
	mov	r5, #0
showAddress:	
	mov	r0, #'\n'
	bl	putc
	
	add r0, r4, r5
	mov r1, #8
	bl phex
	mov r0, #' '
	bl putc
md:	
	ldr	r0, [r4, r5]		@ load up the word to print out
	mov	r1, #8			@ size in digits to print out
	bl	phex
	mov	r0, #' '
	bl	putc
	add	r5, r5, #4
	cmp	r5, #mem_size
	beq	halt
	and	r6, r5, #0x3F
	cmp 	r6, #0
	beq 	showAddress
	b 	md

halt:		b	halt

phexbuf:		.space	12
		.size	phexbuf, . - phexbuf

phex:		adr	r3, phexbuf
	mov	r2, #0
	strb	r2, [r3, r1]
1:	subs	r1, r1, #1
	movmi	r0, r3
	bmi	puts
	and	r2, r0, #15
	mov	r0, r0, lsr #4
	cmp	r2, #10
	addge	r2, r2, #7
	add	r2, r2, #'0'
	strb	r2, [r3, r1]
	b	1b

puts:	loadsp	r3
1:	ldrb	r2, [r0], #1
	teq	r2, #0
	moveq	pc, lr
2:	writeb	r2
	mov	r1, #0x2000
3:	subs	r1, r1, #1
	bne	3b
	teq	r2, #'\n'
	moveq	r2, #'\r'
	beq	2b
	teq	r0, #0
	bne	1b
	mov	pc, lr
putc:
	mov	r2, r0
	mov	r0, #0
	loadsp	r3
	b	2b


	mov	pc,lr			@ Return to caller (end of "main")
	@ -----------------------------------------------------------------------

	.rept	50
	nop
	.endr

	.end