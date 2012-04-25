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
.globl __vbe_call_func_
__vbe_call_func_:
	pushl	%ebp		/* 0(%ebp) */
	movl	%esp,	%ebp

	pushl	%ebx		/* -4(%ebp) */
	pushl	%edx		/* -8(%ebp) */
	pushl	%edi		/* -12(%ebp) */
	pushl	%esi		/* -16(%ebp) */
	pushl	%es			/* -20(%ebp) */
	pushl	%ss			/* -24(%ebp) */

	/* push pointer segment:offset */
	movl	8(%ebp),	%eax
	pushl	%eax		/* -28(%ebp) */
	xorl	%eax,	%eax
	movw	$GDT_VBE_BIOS_CODE,	%ax
	pushl	%eax			/* -32(%ebp) */

	leal	.fmt_str,	%eax
	pushl	%eax
	call	c_printf
	popl	%eax

	/* change the segment from long to word */
	popl	%eax
	pushw	%ax

	movl	16(%ebp),	%eax
	movl	%eax,		%es
	movzwl	10(%ebp),	%eax
	movzwl	12(%ebp),	%ebx
	movzwl	14(%ebp),	%ecx

	/* disable interrupts since we're mucking with segment registers */
	cli

	/* first set up the new stack pointer, new %ss, %esp <- 0*/
	movl	%esp,	%edx
	xorl	%esp,	%esp
	movl	$GDT_VBE_STACK,	%esi
	movl	%esi,	%ss

	/* setup stack to look like interrupt */
	/* EFLAGS, CS, EIP */
	pushfl
	xorl	%edi,	%edi
	movl	%cs,	%edi
	pushl	%edi
	movl	__restore,	%edi
	pushl	%edi

	/* return data segment should point to start of data */
	xorl	%edi,	%edi

	/* call the funtion */
	;cmpl	$0,		%eax
	;je		__init_call
	
	ljmp	*(%edx)
	;lcall 	*(%edx)
	jmp		__restore
	
__init_call:
	xorl	%esp,	%esp
	movl	2(%edx),	%edi
	jmp		*PMBios(%edi)
	;call	*PMBios(%edi)

__restore:

	/* restore the stack */
	movl	%edx,	%esp
	movl	$GDT_STACK, %edx
	;movl	6(%edx),	%edx
	movl	%edx,	%ss

	leal	.ret_fmt_str,	%eax
	pushl	%eax
	call	c_printf
	popl	%eax


	popw	%ax	/* pop the args */
	popl	%eax
	popl	%es		/* and then the saved regs */
	popl	%ss
	popl	%esi
	popl	%edi
	popl	%edx
	popl	%ebx

	/* turn back on interrupts */
	sti

	/* and we're done */
	popl	%ebp
	ret
/*
** _vbe_getGDT
** Return a pointer to the location of the GDT in ARG1
*/
.globl _vbe_getGDT
_vbe_getGDT:

	/* get address in ARG1 */
	movl	4(%esp),	%eax

	/* get the GDT segment:offset address */
	sgdt	(%eax)

	ret


.section .rodata
.align 4
.fmt_str:
	.string "FAR CALL: 0x%04x:0x%08x\n"
.ret_fmt_str:
	.string "FAR CALL RET: 0x%04x:0x%08x\n"
