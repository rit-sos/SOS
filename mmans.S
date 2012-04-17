#define __KERNEL__20113__
#define __ASM__20113__

#include "headers.h"
#include "mman.h"

.text
.globl _mman_set_cr3
_mman_set_cr3:
	movl	8(%esp), %eax
	movl	%cr3, %eax
#	movl	8(%esp), %cr3
	ret

.globl _mman_enable_paging
_mman_enable_paging:
	movl	%cr0, %eax
	orl		$0x80000000, %eax
	movl	%eax, %cr0
	ret

.globl _mman_get_cr2
_mman_get_cr2:
	movl	%cr2, %eax
	ret

.section _mman_kernel_data
.org 0x34000
.globl _kpgdir
_kpgdir:

.org (0x34000+1024*4)
.globl _kpgtbl
_kpgtbl:


