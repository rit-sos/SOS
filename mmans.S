/*
** mmans.S
**
** Assembly functions for memory management
**
** Corey Bloodstein (cmb4247)
*/

#define __KERNEL__20113__
#define __ASM__20113__

#include "headers.h"
#include "offsets.h"
#include "mman.h"

/*
** Set the cr3 register to a new value.
**
** The cr3 register is used to give the MMU the physical address of the
** base of the page directory. Thus, calling this function switches the
** current page directory.
*/
.text
.globl _mman_set_cr3
_mman_set_cr3:
	movl	4(%esp), %eax
	movl	%eax, %cr3
	ret

/*
** Set the paging and supervisor write fault bits in cr0.
*/
.globl _mman_enable_paging
_mman_enable_paging:
	movl	%cr0, %eax
	orl		$0x80010000, %eax
	movl	%eax, %cr0
	ret

/*
** Get the contents of cr0.
*/
.globl _mman_get_cr0
_mman_get_cr0:
	movl	%cr0, %eax
	ret

/*
** Get the contents of cr2.
**
** During a page fault, cr2 contains the faulting virtual address.
*/
.globl _mman_get_cr2
_mman_get_cr2:
	movl	%cr2, %eax
	ret

/*
** Get the contents of cr3. See _mman_set_cr3 for details.
*/
.global _mman_get_cr3
_mman_get_cr3:
	movl	%cr3, %eax
	ret

/*
** Invalidate a TLB entry for a given virtual address in the current
** virtual address space. Call to prevent a cached translation from
** being used when kernel virtual pages change their corresponding
** physical pages (present to present remapping).
*/
.global _invlpg
_invlpg:
	movl	4(%esp), %eax
	invlpg	(%eax)
	ret
