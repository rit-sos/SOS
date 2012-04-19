#define __KERNEL__20113__
#define __ASM__20113__

#include "headers.h"
#include "mman.h"

.text
.globl _mman_set_cr3
_mman_set_cr3:
	movl	4(%esp), %eax
	movl	%eax, %cr3
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

