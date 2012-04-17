/*
 * 
 */

	.text
	.globl _start
_start:
	movl	$exit, (%esp)
	jmp		main
