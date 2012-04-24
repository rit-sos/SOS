#define __KERNEL__20113__

#include "headers.h"
#include "x86arch.h"
#include "queues.h"
#include "stacks.h"
#include "mman.h"

#define CHECK_PAGE_BIT(M,X)	((M)[(X)>>5] &  (1 << ((X) & 0x1f)) != 0)
#define CLEAR_PAGE_BIT(M,X)	((M)[(X)>>5] &= ~(1 << ((X) & 0x1f)))
#define SET_PAGE_BIT(M,X)	((M)[(X)>>5] |= (1 << ((X) & 0x1f)))

Pagedir *_pgdirs;
Pagetbl *_pgtbls;
Memmap *_maps;

/*
** kernel page directory
*/
volatile Pagedir _kpgdir PAGE_ALIGNED;

/*
** Special page table for the first 4MB of the kernel's address space.
** If the kernel needs more than 4MB for anything, _kalloc() can be
** used to map in outside pages.
*/
volatile Pagetbl _kpgtbl PAGE_ALIGNED;

/*
** Special page table for the pages that hold the allocation maps
*/
volatile Pagetbl _kmappgtbl PAGE_ALIGNED;

/*
** Special page filled with zeroes that can be used to pre-fill
** large VM allocations.
*/
//static Uint32 Zero[1024] PAGE_ALIGNED;

/*
** Queue of free pgdirs. Structure is the same for pgtbls, so it's reused for
** allocating them.
*/
Queue *_pgdir_queue;

/*
** Queue of free memmaps.
*/
Queue *_map_queue;

/* bitmap of used physical pages */
Memmap_ptr _phys_map;

/* kernel virtual address space usage */
Memmap_ptr _virt_map;

void _mman_pagefault_isr(int vec, int code) {
	Uint32 cr2, addr;
//	Uint32 idx, addr;
//	Pagedir_entry pde;
//	Pagetbl_entry pte;
	Pagedir_entry *pgdir = (Pagedir_entry*)_current->pgdir;
	Status status;

	cr2 = _mman_get_cr2();
	addr = cr2 >> 12;

	c_printf("*** PAGE FAULT: vec=0x%08x code=0x%08x cr2=0x%08x ***\n", vec, code, cr2);
	c_printf("Mapping vpg 0x%08x to ppg 0x%08x\n", addr, addr);

	if ((status = _mman_map_page(pgdir, addr, addr, MAP_WRITE | MAP_USER)) != SUCCESS) {
		_kpanic("_mman_pagefault_isr", "%s", status);
	}

	//_kpanic("mman", "page fault handler not fully implemented", FAILURE);
}

Status _mman_pgdir_alloc(Pagedir_entry **pgdir) {
	return _q_remove(_pgdir_queue, (void**)pgdir);
}

Status _mman_pgdir_free(Pagedir_entry *pgdir) {
	Uint32 i;

	/* free any attached page tables */
	for (i = 0; i < 0x400; i++) {
		if (pgdir[i].present) {
			_q_insert(_pgdir_queue, (Pagetbl*)(pgdir[i].dword & PD_FRAME), (Key)0);
		}
	}

	return _q_insert(_pgdir_queue, pgdir, (Key)0);
}

Status _mman_map_alloc(Memmap_ptr *map) {
	return _q_remove(_map_queue, (void**)map);
}

Status _mman_map_free(Memmap_ptr map) {
	return _q_insert(_map_queue, map, (Key)0);
}

/*
** Note: these are page numbers, not addresses.
** To translate an address, call _mman_translate_addr.
** Also, this translation is relative to the passed-in pgdir.
*/
Status _mman_translate_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 *phys) {
	Pagedir_entry pde;
	Pagetbl_entry pte;
	Pagetbl_entry *tbl;
	Uint32 idx;

	if (!pgdir || !phys) {
		return BAD_PARAM;
	}

	/*
	** The upper 20 bits of the virtual address are divided into two fields.
	**
	** The first 10 bits are the index of the page directory entry.
	**   (pgdir_entry & PD_FRAME) is the address of the page table.
	**
	** The next 10 bits are the index of the page table entry.
	**   (pgtbl_entry & PT_FRAME) is the physical address of the page.
	*/

	idx = (virt >> 10) & 0x03ff;
	pde.dword = pgdir[idx].dword;
	if (pde.present) {
		tbl = (Pagetbl_entry*)(pde.dword & PD_FRAME);
		idx = virt & 0x03ff;
		pte.dword = tbl[idx].dword;
		if (pte.present || pte.disk) {
			*phys = (pte.dword & PT_FRAME) >> 12;
			return SUCCESS;
		}
	}

	*phys = NULL;
	return NOT_FOUND;
}

/*
** Translate a virtual address to a physical address for a given process's
** address space (pgdir).
*/
Status _mman_translate_addr(Pagedir_entry *pgdir, void *virt, void **phys) {
	Uint32 status;
	Uint32 ppg;

	if ((status = _mman_translate_page(pgdir, ((Uint32)virt) >> 12, &ppg)) != SUCCESS) {
		*phys = NULL;
		return status;
	}

	*phys = (void*)((ppg << 12) + (((Uint32)virt) & 0x03ff));

	return SUCCESS;
}

/*
** Add a mapping to a process's page directory.
** These are page numbers, not addresses.
** See mman.h for flag definitions.
*/
Status _mman_map_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 phys, Uint32 flags) {
	Pagedir_entry pde;
	Pagetbl_entry *tbl;
	Status status;
	Uint32 idx;

	if (!pgdir) {
		return BAD_PARAM;
	}

	/* being able to write these pages would be a bad thing */
	if ((flags & MAP_WRITE) && ((flags & MAP_COW) || (flags & MAP_ZERO))) {
		return BAD_PARAM;
	}

	/* first find the pagetbl */
	idx = (virt >> 10) & 0x03ff;
	pde.dword = pgdir[idx].dword;
	if (pde.present) {
		/* found a pagetbl */
		tbl = (Pagetbl_entry*)(pde.dword & PD_FRAME);
	} else {
		/* no pagetbl here, alloc a new one */
		if ((status = _mman_pgdir_alloc((Pagedir_entry**)&tbl)) != SUCCESS) {
			return status;
		}
		_kmemclr(tbl, sizeof(Pagedir));

		/* store the new pagetbl address into the pgdir */
		pgdir[idx].dword = ((Uint32)tbl) | (flags & MAP_PD_MASK) | PT_PRESENT;
	}

	/* now find the pagetbl entry */
	idx = virt & 0x03ff;

	if (tbl[idx].present) {
		/* mapping already exists for this address */
		Uint32 temp;
		c_printf("mapping exists at index %d (0x%04x)!\n", idx, idx);
		_mman_translate_page(pgdir, virt, &temp);
		c_printf("virt: 0x%08x, phys: 0x%08x (want: 0x%08x)\n", virt << 12, temp << 12, phys << 12);
		return ALLOC_FAILED;
	}

	/* if we get here, then we're ready to add the pagetbl entry */
	tbl[idx].dword = ((phys << 12) & PT_FRAME) | (flags & MAP_FLAGS_MASK) | PT_PRESENT;

	return SUCCESS;
}

Status _mman_unmap_page(Pagedir_entry *pgdir, Uint32 virt) {
	Pagedir_entry pde;
	Pagetbl_entry *tbl;
	Uint32 idx;

	if (!pgdir) {
		return BAD_PARAM;
	}

	/* first find the pagetbl */
	idx = (virt >> 10) & 0x03ff;
	pde.dword = pgdir[idx].dword;
	if (!pde.present) {
		return NOT_FOUND;
	}

	tbl = (Pagetbl_entry*)(pde.dword & PD_FRAME);

	/* now find the pagetbl entry */
	idx = virt & 0x03ff;
	if (!tbl[idx].present) {
		return NOT_FOUND;
	}

	/* and clear it */
	tbl[idx].dword = 0;

	/* if the whole pagetbl is empty, we can free it */
	for (idx = 0; idx < 0x400 && !(tbl[idx].present); idx++) {
		;
	}

	if (idx == 0x400) {
		idx = (virt >> 10) & 0x03ff;
		pgdir[idx].dword = 0;
		return _mman_pgdir_free((Pagedir_entry*)tbl);
	}

	return SUCCESS;
}

/*
** General per-process setup
*/
Status _mman_proc_init(Pcb *pcb) {
	Status status;
	Pagedir_entry *pgdir;
	void (*entry)(void);
	Uint32 vpg, ppg, size, num_pages, i;
	Memmap_ptr map;

	pcb->pgdir = NULL;

	if (!pcb) {
		return BAD_PARAM;
	}

	/* get the physical address of the program's entry point */
	if ((status = _get_proc_address(pcb->program, &entry, &size)) != SUCCESS) {
		return status;
	}

	num_pages = size >> 12;
	if (size & 0x0fff) {
		num_pages++;
	}

	/* allocate and clear a page directory */
	if ((status = _mman_pgdir_alloc(&pgdir)) != SUCCESS) {
		return status;
	}
	_kmemclr(pgdir, sizeof(Pagedir));

	/* allocate and clear a map */
	if ((status = _mman_map_alloc(&map)) != SUCCESS) {
		_mman_pgdir_free(pgdir);
		return status;
	}
	_kmemclr(map, sizeof(Memmap));

	/*
	** Map the target process's stack to USER_STACK and down.
	** Currently sizeof(Stack) == 0x1000, so this is simple.
	*/
	vpg = (USER_STACK >> 12) - 1;
	ppg = ((Uint32)(pcb->stack - 1)) >> 12;
	if ((status = _mman_map_page(pgdir, vpg, ppg, MAP_WRITE | MAP_USER)) != SUCCESS) {
		_mman_map_free(map);
		_mman_pgdir_free(pgdir);
		return status;
	}
	SET_PAGE_BIT(map, vpg);

	/*
	** Map the target process's code to USER_ENTRY and up.
	** Since we're not breaking up the program image into code and
	** data sections, this needs to be writable. Once COW is done,
	** this should really be using that.
	*/
	vpg = USER_ENTRY >> 12;
	ppg = ((Uint32)entry) >> 12;
	for (i = 1; i < size; i++, vpg++) {
		if ((status = _mman_map_page(pgdir, vpg, ppg, MAP_WRITE | MAP_USER)) != SUCCESS) {
			_mman_map_free(map);
			_mman_pgdir_free(pgdir);
			return status;
		}
		SET_PAGE_BIT(map, vpg);
	}

	/*
	** Until ring3-ring0 transition works, we need to map the kernel in too.
	** Once that's done, this will be unnecessary.
	*/
#ifndef USE_TSS
	for (i = 0; i < 0x400; i++) {
		if ((status = _mman_map_page(pgdir, i, i, MAP_WRITE)) != SUCCESS) {
			_mman_map_free(map);
			_mman_pgdir_free(pgdir);
			return status;
		}
		SET_PAGE_BIT(map, i);
	}
#endif

	c_printf("_mman_proc_init: pcb=0x%08x, pgdir=0x%08x\n", pcb, pgdir);

	/* hey, we made it */
	pcb->pgdir = (Pagedir_ptr)pgdir;
	pcb->virt_map = map;
	return SUCCESS;
}

Status _mman_proc_exit(Pcb *pcb) {
	_mman_pgdir_free(pcb->pgdir);
	_mman_map_free(pcb->virt_map);
	return SUCCESS;
}

Status _mman_free(void *ptr, Uint32 size, Pcb *pcb) {
	Uint32 i, num_pages;
	Uint32 vpg; //, ppg;
	Uint32 *virt_map;
	Pagedir_entry *pgdir;

	if (!ptr) {
		return BAD_PARAM;
	}

	if (pcb) {
		virt_map = pcb->virt_map;
		pgdir = pcb->pgdir;
	} else {
		virt_map = _virt_map;
		pgdir = (Pagedir_ptr)_kpgdir;
	}

	num_pages = size >> 12;
	if (size & 0x0fff) {
		num_pages++;
	}

	vpg = ((Uint32)ptr) >> 12;

	for (i = 0; i < num_pages; i++, vpg++) {
		//if (_mman_translate_page(pgdir, vpg, &ppg) == SUCCESS) {
		//	CLEAR_PAGE_BIT(ppg);
		//}
		_mman_unmap_page(pgdir, vpg);
	}

	return SUCCESS;
}

/*
** Allocate some physical memory and return a pointer to its virtual mapping.
** See mman.h for flags.
*/
Status _mman_alloc(void **ptr, Uint32 size, Pcb *pcb, Uint32 flags) {
	Uint32 i, j, k, bit, status, vpg; //, ppg;
	Uint32 num_pages;
	Uint32 *map;
	Pagedir_entry *pgdir;
	Uint32 *virt_map;

	if (!ptr) {
		return BAD_PARAM;
	}

	/* limit 4MB per alloc (for now) */
	if (size > 0x400000) {
		*ptr = NULL;
		return BAD_PARAM;
	}

	/*
	** if a pcb was passed in, this is for a user process, else
	** this is for the kernel
	*/
	if (pcb) {
		virt_map = pcb->virt_map;
		pgdir = pcb->pgdir;
	} else {
		virt_map = _virt_map;
		pgdir = (Pagedir_ptr)_kpgdir;
	}

	/* see how many pages we need to map */
	num_pages = size >> 12;
	if (size & 0x0fff) {
		num_pages++;
	}

	/*
	** Find a continuous piece of the virtual address space.
	** I don't really care how long this takes.
	*/
	for (map = virt_map + 8, i = 0x100; i < 0x8000; i++, map++) {
		if (*map != 0xffffffff) {
			/* save the address of the potential mapping */
			for (vpg = i << 5, k = bit; k; k >>= 1) {
				vpg++;
			}

			/* see if the potential mapping is big enough */
			for (j = 1; j < num_pages; j++) {
				/* advance to the next bit */
				if ((bit >>= 1) == 0) {
					/* if we reached the end of the dword, advance map */
					if (i == 0x7fff) {
						/* we're at the end of the map, so alloc fails */
						*ptr = 0;
						return ALLOC_FAILED;
					}

					i++;
					map++;
					bit = 0x80000000;
				}

				/* if the region is too small, break */
				if ((*map) & bit) {
					break;
				}
			}

			/* if we succeeded, break; else try again */
			if (j == num_pages) {
				break;
			}
		}
	}

	/* we traversed the whole list and didn't find space */
	if (j != num_pages) {
		*ptr = 0;
		return ALLOC_FAILED;
	}

	/*
	** Find free physical pages to map to each virtual page.
	** These don't have to be continuous, so less complicated.
	*/
	for (map = _phys_map + 8, i = 0x100, j = 0; i < 0x8000 && j < num_pages; i++, map++) {
		if (*map != 0xffffffff) {
			for (bit = 0x80000000, k = 0; bit; bit >>= 1, k++) {
				if (!((*map) & bit)) {
					/* found a free physical page */
					if ((status = _mman_map_page(pgdir, vpg + j, (i << 5) | k, flags)) != SUCCESS) {
						/* alloc failed */
						goto Cleanup;
					}

					/* set the bit in the physical allocation map */
					*map |= bit;
				}
			}
		}
	}

	/* we traversed the whole list and didn't find space */
	if (j != num_pages) {
		goto Cleanup;
	}

	/*
	** Return the virtual address of the mapped region in ptr.
	** This will, of course, only be valid for the process corresponding
	** to the passed-in pcb.
	*/
	*ptr = (void*)(vpg << 12);
	return SUCCESS;

Cleanup:
	/*
	** Undo partial allocations before returning.
	** j is number of successfully mapped pages.
	** vpg is the first virtual page mapped.
	*/
	for (i = 0; i < j; i++, vpg++) {
		//if (_mman_translate_page(pgdir, vpg, &ppg) == SUCCESS) {
		//	CLEAR_PAGE_BIT(ppg);
		//}
		_mman_unmap_page(pgdir, vpg);
	}

	*ptr = NULL;
	return ALLOC_FAILED;
}

void _mman_init() {
	Uint32 i, addr;
	Memmap *map;
	Status status;

	_kmemclr((void*)_kpgdir, sizeof(Pagedir));
	_kmemclr((void*)_kpgtbl, sizeof(Pagetbl));
//	_kmemclr(Zero, sizeof(Zero));

	_kpgdir[0].dword = ((Uint32)_kpgtbl) | PD_WRITE | PD_PRESENT;
	_kpgdir[1].dword = ((Uint32)_kmappgtbl) | PD_WRITE | PD_PRESENT;

	/* Now set up the free page tables.
	** This is going to take a lot of room.
	** For now, let's use the space from 0x100000 through 0x1fffff.
	*/
	_pgdirs = (Pagedir*)0x100000;
	_kmemclr(_pgdirs, 0x100000);

	if ((status = _q_alloc(&_pgdir_queue, NULL)) != SUCCESS) {
		_kpanic("mman", "Couldn't alloc queue for page tables", status);
	}

	for (addr = (Uint32)_pgdirs; status == SUCCESS && addr < 0x200000; addr += 0x1000) {
		if ((status = _q_insert(_pgdir_queue, (Pagedir_ptr)addr, (Key)0)) != SUCCESS) {
			_kpanic("mman", "_q_insert(_pgdir_queue)", status);
		}
	}

	/*
	** Next, let's make room for some maps. These are enormous.
	** We're allocating 4MB, but this only fits 32 maps!
	** The first 2 are reserved for kernel use (physical and virtual usage),
	** leaving room for 30 process VM maps.
	*/
	if ((status = _q_alloc(&_map_queue, NULL)) != SUCCESS) {
		_kpanic("mman", "_q_alloc(&_map_queue)", status);
	}

	_maps = ((Memmap*)0x400000);
	_kmemclr(_maps, 0x400000);

	_phys_map = _maps[0];
	_virt_map = _maps[1];

	for (i = 2, map = _maps + 2; i < (0x400000 / sizeof(Memmap)); i++, map++) {
		if ((status = _q_insert(_map_queue, map, (Key)0)) != SUCCESS) {
			_kpanic("mman", "_q_insert(map)", status);
		}
	}

	/* identity mapping for kernel's first 4MB */
	for (i = 0, addr = 0; addr < 0x400000; i++, addr += 4096) {
		_kpgtbl[i].dword = addr | PT_WRITE | PT_PRESENT;
	}

	for (i = 0; addr < 0x800000; i++, addr += 4096) {
		_kmappgtbl[i].dword = addr | PT_WRITE | PT_PRESENT;
	}

	for (i = 0; i < 64; i++) {
		_phys_map[i] = 0xffffffff;
		_virt_map[i] = 0xffffffff;
	}

	__install_isr(INT_VEC_PAGE_FAULT, _mman_pagefault_isr);

	/* cross your fingers... */
	_mman_set_cr3((Pagedir_entry*)_kpgdir);
	_mman_enable_paging();

	/* let the world know that we didn't crash (yet)! */
	c_puts (" mman");
}

void _mman_kernel_mode(void) {
//	c_printf("stack: 0x%08x\n", _current->stack);
//	c_printf("ctxt stack: 0x%08x\n", _current->context->esp);
//	_kpanic("_mman_kernel_mode", "hello", FAILURE);
	_mman_set_cr3((Pagedir_entry*)_kpgdir);
}

void _mman_restore_pgdir(void) {
//	c_printf("stack: 0x%08x\n", _current->stack);
//	c_printf("ctxt stack: 0x%08x\n", _current->context->esp);
//	_kpanic("_mman_restore_pgdir", "hello", FAILURE);
//	_current->context->esp = (_current->context->esp - ((Uint32)_current->stack)) + (USER_STACK - sizeof(Stack));
//	c_printf("adjusted: 0x%08x\n", _current->context->esp);
	_mman_set_cr3(_current->pgdir);
}
