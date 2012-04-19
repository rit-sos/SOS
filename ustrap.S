/*
 * Initialization code for user processes. Linked in at the entry point
 * of each user process in the makefile. See the makefile for more details.
 */

	.text
	.globl _start
_start:
	movl	$exit, (%esp)
	jmp		main
