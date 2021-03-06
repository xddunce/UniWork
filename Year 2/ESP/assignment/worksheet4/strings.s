@first example assembler program
	@ ------------------------------------------------------------------
	@ Constant values used in this program
	.set 	UART_BASE, 0x80000000 @ base address of UARTS in memory
	.set 	UART_OFF,  0x10000 @ offset between UARTs in memory
	.set	UTDR,      0x14	@ Offset to the Serial Data Register
	.set	UTSR1,     0x20	@ Offset to the Serial Status port

	.set	UTSR1_TNF, 0b00000100	@ Mask to check Tx FIFO Not Full
	@ data
	.data
	@ -----------------------------------------------------------------------
	@ Assembly-language preamble
	.text				@ Executable code follows
	.global	_start			@ "_start" is required by the linker
	.extern	main			@ "main" is our main program
	.global putstring
	.global putnws
	.extern stripwhitespace

@_start:
@	bl main
@	b halt

putnws:
	@ r0 - char *string
	@ r1 - int uart
	mov ip, sp
	stmfd sp!, {fp, ip, lr, pc}
	bl stripwhitespace
	mov r2, r1 		@ switch parameters ready for putstring function
	mov r1, r0 		@ r1 - char *string
	mov r0, r2 		@ r0 - int uart
	bl putstring
	ldmea fp, {fp, sp, pc}
	
putstring:
	@r0 is uart number to use
	@r1 is address of string to print
	ldr r2, =UART_BASE
	ldr r3, =UART_OFF
	mul r4, r3, r0
	add r2, r2, r4				@ r2 is now the base of UART to use
	mov r5, #0 				@ offset into string to load, test and print if non NULL
string_loop:
	ldrb r4, [r1, r5]			@ load character from string in r1, offset by r5
	add r5, r5, #1				@ increment offset
	cmp r4, #0				@ is character loaded a null character?
	beq exit_loop

char_write:	
	ldrb r6, [r2, #UTSR1]
	tst r6, #UTSR1_TNF
	beq char_write
	
	strb r4, [r2, #UTDR] 			@ write byte to data register
	
	b string_loop
exit_loop:
	mov pc, lr

	@ -----------------------------------------------------------------------
	@ Send the character to the internal serial port

wb:		ldrb	r2, [r1, #UTSR1]	@ Check the Serial Status port again
	tst	r2, #UTSR1_TNF		@ Can a character be sent out?
	beq	wb			@ No: wait until port is ready

	strb	r0, [r1, #UTDR]	@ Send the actual character out
	mov	pc, lr			@ go back


ws:	mov r4, lr
	mov r6, #0
ws_loop:
	ldrb r0, [r5, r6]
	cmp r0, #0
	beq halt
	add r6, r6, #1
	bl wb
	b ws_loop
	
halt:		b	halt			@ Do this forever (or until stopped)

	.rept	100				@padding to stop bsp problems
	nop				@better to place at end of boot.s
	.endr					@so we don’t need to remember for each prog!
	@ -----------------------------------------------------------------------
	.end