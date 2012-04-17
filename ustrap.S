/*
 * 
 */

	.text
	.globl _start
_start:
	movl	$exit, 8(%ebp)
	jmp		main
