#define __KERNEL__20113__

#include "headers.h"
#include "x86arch.h"
#include "mman.h"

//#define _kpgdir	((Pagedir*)0x00032000)
//#define _kpgtbl ((Pagetbl*)(_kpgdr+1))

extern Pagedir _kpgdir;
extern Pagetbl _kpgtbl;

Uint32 curr_idx;

void _mman_pagefault_isr(int vec, int code) {
	Uint32 cr2, idx, addr;
	Pagedir_entry *pde;
	Pagetbl_entry *pte;

	cr2 = _mman_get_cr2();

	c_printf("*** PAGE FAULT: vec=0x%08x code=0x%08x cr2=0x%08x\n ***", vec, code, cr2);

	pde = &_kpgdir[ (cr2 >> 22) & 0x000003ff ];
	if (pde->present) {
		pte = &_kpgtbl[ (cr2 >> 12) & 0x000003ff ];
		idx = (pte - _kpgtbl);
		addr = idx * 4096;
		c_printf("Mapping addr=0x%08x at idx=0x%08x\n", addr, idx);
		pte->dword = addr | PT_WRITE | PT_PRESENT | PT_USER;
	} else {
		_kpanic("mman", "Page fault for address outside of first 4MB", FAILURE);
	}
}

void _mman_init() {
	Uint32 i, addr;
	
	_kmemclr(_kpgdir, sizeof(Pagedir));
	_kmemclr(_kpgtbl, sizeof(Pagetbl));

	__install_isr(INT_VEC_PAGE_FAULT, _mman_pagefault_isr);

	/* identity mapping for kernel reserved 512k */
	for (i = 1, addr = 4096; addr < 512*1024; i++, addr += 4096) {
		_kpgtbl[i].dword = addr | PT_WRITE | PT_PRESENT;
	}

	curr_idx = i;

	_kpgdir[0].dword = ((Uint32)_kpgtbl) | PD_WRITE | PD_PRESENT;

	_mman_set_cr3(&_kpgdir);
	_mman_enable_paging();

	c_puts (" mman");

}
