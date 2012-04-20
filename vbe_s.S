/*
** File:	vbe_s.S
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	VBE protected mode interface 
*/
#define __ASM__20113__
#define __KERNEL__20113__

#include "vbe.h"
#include "bootstrap.h"

.text
.globl _vbe_call
_vbe_call:
	pushl	%ebp		/* 0(%ebp) */
	movl	%esp,	%ebp

	pushl	%ebx		/* -4(%ebp) */
	pushl	%ecx		/* -8(%ebp) */
	pushl	%ss			/* -12(%ebp) */

	/* push pointer segment:offset */
	movl	$GDT_VBE_BIOS_CODE,	%eax
	pushw	%ax			/* -16(%ebp) */
	movl	8(%ebp),	%eax
	pushl	%eax		/* -18(%ebp) */

	/* disable interrupts since we're mucking with segment registers */
	cli

	/* first set up the new stack pointer, new %ss, %esp <- 0*/
	movl	%esp,	%ebx
	xorl	%esp,	%esp
	movl	$GDT_VBE_STACK,	%ecx
	movl	%ecx,	%ss

	/* call the funtion */
	lcall 	*-16(%ebp)

	/* restore the stack */
	movl	%ebx,	%esp
	popl	%eax	/* pop the args */
	popl	%eax
	popl	%ss		/* and then the saved regs */
	popl	%ecx
	popl	%ebx

	/* turn back on interrupts */
	sti

	/* and we're done */
	popl	%ebp
	ret
