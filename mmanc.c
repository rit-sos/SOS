#define __KERNEL__20113__

#include "headers.h"
#include "x86arch.h"
#include "queues.h"
#include "stacks.h"
#include "bootstrap.h"
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
	Uint32 cr0, cr2, cr3;
//	Uint32 idx, addr;
//	Pagedir_entry pde;
//	Pagetbl_entry pte;
//	Pagedir_entry *pgdir = (Pagedir_entry*)_current->pgdir;
//	Status status;

	cr0 = _mman_get_cr0();
	cr2 = _mman_get_cr2();
	cr3 = _mman_get_cr3();
//	addr = cr2 >> 12;

	c_printf("PAGE FAULT: vec=0x%08x code=0x%08x\n", vec, code);
	c_printf("current user: pid=%04d  pgdir=0x%08x\n", _current->pid, _current->pgdir);
	c_printf("cr0=0x%08x  cr2=0x%08x  cr3=0x%08x\n", cr0, cr2, cr3);
//	c_printf("Mapping vpg 0x%08x to ppg 0x%08x\n", addr, addr);

//	if ((status = _mman_map_page(pgdir, addr, addr, MAP_WRITE | MAP_USER)) != SUCCESS) {
//		_kpanic("_mman_pagefault_isr", "%s", status);
//	}

	_kpanic("mman", "page fault handler not fully implemented", FAILURE);
}

static int pcnt;

Status _mman_pgdir_alloc(Pagedir_entry **pgdir) {
	if (++pcnt % 100 == 0) {
		c_printf("_mman_pgdir_alloc: %d\n", pcnt);
	}
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

Status _mman_pgdir_copy(Pagedir_entry *dst, Memmap_ptr dstmap, Pagedir_entry *src, Memmap_ptr srcmap) {
	int i, j;
	Pagetbl_entry *tbl;

	_kmemclr(dst, sizeof(Pagedir));

	/* copy kernel low 4MB mapping */
	dst[0] = _kpgdir[0];

	/* for each pagedir entry, */
	for (i = 1; i < 0x1000 / 4; i++) {
		/* if it exists, */
		if (src[i].present) {
			c_printf("source pagedir %08x, pgtbl %04x (dword:%08x) is present\n", src, i, src[i].dword);
			/* allocate a page table for the new directory. */
			if (_mman_pgdir_alloc((Pagedir_entry**)&dst[i]) != SUCCESS) {
				c_printf("_mman_pgdir_copy: _mman_pgdir_alloc failed\n");
				return ALLOC_FAILED;
			}
			/* for the page table pointed to by that pagedir entry, */
			tbl = (Pagetbl_entry*)(src[i].dword & PD_FRAME);
			/* for each page table entry, */
			for (j = 0; j < 0x1000; j++) {
				/* if it exists, */
				if (tbl[j].present) {
					/* copy the mapping, but make the pages COW */
					((Pagetbl_entry*)(dst[i].dword & PD_FRAME))[j].dword = tbl[j].dword | PT_COW;
				}
			}
			/* add the flags from the original pagedir entry to the new pagedir entry */
			dst[i].dword |= (src[i].dword & ~PD_FRAME);
		}
	}

	_kmemcpy(dstmap, srcmap, sizeof(Memmap));

	return SUCCESS;
}

/*
** Note: these are page numbers, not addresses.
** To translate an address, call _mman_translate_addr.
** Also, this translation is relative to the passed-in pgdir.
*/
Status _mman_translate_page(Pagedir_entry *pgdir, Uint32 virt, Uint32 *phys, Uint32 write) {
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
	if (pde.present && (pgdir == _kpgdir || pde.user) && (!write || pde.write)) {
		tbl = (Pagetbl_entry*)(pde.dword & PD_FRAME);
		idx = virt & 0x03ff;
		pte.dword = tbl[idx].dword;
		if ((pte.present || pte.disk) && (pgdir == _kpgdir || pte.user) && (!write || pte.write)) {
			*phys = (pte.dword & PT_FRAME) >> 12;
			return SUCCESS;
		}
	}

	*phys = NULL;
	return NOT_FOUND;
}

/*
** Copy data from a user buffer to a kernel buffer. Surprisingly difficult.
*/
Status _mman_get_user_data(Pcb *pcb, /* out */ void *buf, void *virt, Uint32 size) {
	Uint32 undo[MAX_TRANSFER_PAGES];
	Uint32 ppgs[MAX_TRANSFER_PAGES];
	void *phys;
	Uint32 ppg, vpg, test, num_pages, i;
	Status status;

	//c_putchar('1');

	if (!pcb || !buf || !virt || !size) {
		return BAD_PARAM;
	}

	//c_putchar('2');

	num_pages = size >> 12;
	if (((Uint32)virt) & 0x0fff) {
		num_pages++;
	}
	if ((size & 0x0fff) > 0x1000 - (((Uint32)virt) & 0x0fff)) {
		num_pages++;
	}

	if (num_pages > MAX_TRANSFER_PAGES) {
		return BAD_PARAM;
	}

//	c_printf("[%04d] _mman_get_user_data: {vaddr=%08x, size=%d} spans %d pages\n", pcb->pid, virt, size, num_pages);

	//c_putchar('3');

	_kmemclr(undo, sizeof(undo));

	//c_putchar('4');

	/* translate and map */
	for (i = 0, vpg = ((Uint32)virt) >> 12; i < num_pages; i++, vpg++) {
		if ((status = _mman_translate_page(pcb->pgdir, vpg, &ppg, 0)) != SUCCESS) {
			goto Cleanup;
		}

		if ((status = _mman_translate_page((Pagedir_entry*)_kpgdir, ppg, &test, 0)) == NOT_FOUND) {
			if ((status = _mman_map_page((Pagedir_entry*)_kpgdir, ppg, ppg, 0)) != SUCCESS) {
				goto Cleanup;
			}
			c_printf("[%04d] _mman_get_user_data: ppg %08x (vpg %08x) not found, adding to undo\n", pcb->pid, ppg, vpg);
			undo[i] = ppg;
		} else if (status != SUCCESS) {
			goto Cleanup;
		}

		ppgs[i] = ppg;
	}

	//c_putchar('5');

	/* copy 1st (may be partial) */
	phys = (void*)((ppgs[0] << 12) | (((Uint32)virt) & 0x0fff));
	if (num_pages > 1) {
		test = 0x1000 - (((Uint32)virt) & 0x0fff);
	} else {
		test = size;
	}
//	c_printf("[%04d] _mman_get_user_data: copying phys=%08x to buf=%08x, %d bytes\n", pcb->pid, phys, buf, test);
	_kmemcpy(buf, phys, test);

	//c_putchar('6');

	/* adjust so that we can write full pages */
	buf += test;

	/* copy 2nd to (n-1)st */
	for (i = 1; i < num_pages - 1; i++) {
		_kmemcpy(buf, (void*)(ppgs[i] << 12), 0x1000);
		buf += 0x1000;
	}

	//c_putchar('7');

	/* copy last (may be partial) */
	if (num_pages > 1) {
		size = (size - test) % 0x1000;
		_kmemcpy(buf, (void*)(ppgs[num_pages-1] << 12), size);
	}

Cleanup:

	//c_putchar('8');

	/* unmap pages in the undo list */
	for (i = 0; i < num_pages; i++) {
		if (undo[i]) {
			_mman_unmap_page((Pagedir_entry*)_kpgdir, undo[i]);
		}
	}

//	c_printf("[%04d] _mman_get_user_data: returning status %08x\n", pcb->pid, status);

	return status;
}

/*
** Copy data from a kernel buffer to a user buffer.
*/
Status _mman_set_user_data(Pcb *pcb, /* out */ void *virt, void *buf, Uint32 size) {
	Uint32 undo[MAX_TRANSFER_PAGES];
	Uint32 ppgs[MAX_TRANSFER_PAGES];
	void *phys;
	Uint32 ppg, vpg, test, num_pages, i;
	Status status;

	if (!pcb || !buf || !virt || !size) {
		return BAD_PARAM;
	}

	num_pages = size >> 12;
	if (((Uint32)virt) & 0x0fff) {
		num_pages++;
	}
	if ((size & 0x0fff) > 0x1000 - (((Uint32)virt) & 0x0fff)) {
		num_pages++;
	}

	if (num_pages > MAX_TRANSFER_PAGES) {
		return BAD_PARAM;
	}

	_kmemclr(undo, sizeof(undo));

	/* translate and map */
	for (i = 0, vpg = ((Uint32)virt) >> 12; i < num_pages; i++, vpg++) {
		if ((status = _mman_translate_page(pcb->pgdir, vpg, &ppg, 1)) != SUCCESS) {
			goto Cleanup;
		}

		if ((status = _mman_translate_page((Pagedir_entry*)_kpgdir, ppg, &test, 1)) == NOT_FOUND) {
			if ((status = _mman_map_page((Pagedir_entry*)_kpgdir, ppg, ppg, MAP_WRITE)) != SUCCESS) {
				goto Cleanup;
			}
			c_printf("[%04d] _mman_set_user_data: ppg %08x (vpg %08x) not found, adding to undo\n", pcb->pid, ppg, vpg);
			undo[i] = ppg;
		} else if (status != SUCCESS) {
			goto Cleanup;
		}

		ppgs[i] = ppg;
	}

	/* copy 1st (may be partial) */
	phys = (void*)((ppgs[0] << 12) | (((Uint32)virt) & 0x0fff));
	if (num_pages > 1) {
		test = 0x1000 - (((Uint32)virt) & 0x0fff);
	} else {
		test = size;
	}
	_kmemcpy(phys, buf, test);

	/* adjust so that we can write full pages */
	buf += test;

	/* copy 2nd to (n-1)st */
	for (i = 1; i < num_pages - 1; i++) {
		_kmemcpy((void*)(ppgs[i] << 12), buf, 0x1000);
		buf += 0x1000;
	}

	/* copy last (may be partial) */
	if (num_pages > 1) {
		size = (size - test) % 0x1000;
		_kmemcpy((void*)(ppgs[num_pages-1] << 12), buf, size);
	}

Cleanup:
	/* unmap pages in the undo list */
	for (i = 0; i < num_pages; i++) {
		if (undo[i]) {
			_mman_unmap_page((Pagedir_entry*)_kpgdir, undo[i]);
		}
	}

	return status;
}


/*
** Translate a virtual address to a physical address for a given process's
** address space (pgdir).
*/
Status _mman_translate_addr(Pagedir_entry *pgdir, void *virt, void **phys, Uint32 write) {
	Uint32 status;
	Uint32 ppg;

	if (!pgdir || !phys) {
		return BAD_PARAM;
	}

	if ((status = _mman_translate_page(pgdir, ((Uint32)virt) >> 12, &ppg, write)) != SUCCESS) {
		*phys = NULL;
		return status;
	}

	*phys = (void*)((ppg << 12) + (((Uint32)virt) & 0x0fff));

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

//	int i;

	if (!pgdir) {
		return BAD_PARAM;
	}

	/* being able to write these pages would be a bad thing */
//	if ((flags & MAP_WRITE) && ((flags & MAP_COW) || (flags & MAP_ZERO))) {
//		return BAD_PARAM;
//	}

//	c_printf("_mman_map_page: pgdir %08x, %08x ==> %08x\n", pgdir, virt, phys);
//	for (i = 0x0fffffff; i; --i);

	/* first find the pagetbl */
	idx = (virt >> 10) & 0x03ff;
	pde.dword = pgdir[idx].dword;
	if (pde.present) {
		/* found a pagetbl */
		tbl = (Pagetbl_entry*)(pde.dword & PD_FRAME);
	} else {
		/* no pagetbl here, alloc a new one */
		c_printf("_mman_map_page: need a new pgtbl for pgdir %08x, %08x ==> %08x\n", pgdir, virt, phys);
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
		_mman_translate_page(pgdir, virt, &temp, 0);
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
** General per-process setup for ring3 processes
*/
Status _mman_proc_init(Pcb *pcb) {
	Status status;
	Pagedir_entry *pgdir = NULL;
	void (*entry)(void);
	Uint32 vpg, ppg, size, num_pages, i;
	Memmap_ptr map = NULL;

	if (!pcb) {
		return BAD_PARAM;
	}

	pcb->pgdir = NULL;

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
		goto Cleanup;
	}
	_kmemclr(map, sizeof(Memmap));

	/*
	** Map the target process's stack to USER_STACK and down.
	** Currently sizeof(Stack) == 0x1000, so this is simple.
	*/
	vpg = (USER_STACK >> 12) - 1;
	ppg = ((Uint32)(pcb->stack)) >> 12;
	if ((status = _mman_map_page(pgdir, vpg, ppg, MAP_WRITE | MAP_USER)) != SUCCESS) {
		goto Cleanup;
	}
	SET_PAGE_BIT(map, vpg);

	/*
	** Map the target process's code to USER_ENTRY and up.
	** Since we're not breaking up the program image into code and
	** data sections, this needs to be writable. Instead of making it
	** writable directly, we mark it copy on write so that if the
	** process tries to write to it we give it its own copy of it.
	**
	** Of course, COW isn't done, so just mark it writable anyway.
	*/
	vpg = USER_ENTRY >> 12;
	ppg = ((Uint32)entry) >> 12;
	for (i = 0; i < num_pages; i++, vpg++, ppg++) {
		if ((status = _mman_map_page(pgdir, vpg, ppg, MAP_COW | MAP_WRITE | MAP_USER)) != SUCCESS) {
			goto Cleanup;
		}
		SET_PAGE_BIT(map, vpg);
	}

	/*
	** Map the kernel in, but don't make it accessible to the user process.
	** This is necessary because for two reasons. First, when we get an IRQ
	** we automatically get the system esp from the TSS, but we're still in
	** the user's address space. At that point, we have to immediately switch
	** to the kernel pagedir and then proceed as normal.
	** Secondly, and probably more importantly, the ISR code itself is in
	** the kernel address space...
	*/
	pgdir[0] = _kpgdir[0];
	for (i = 0; i < (0x400 / 32); i++) {
		map[i] = 0xffffffff;
	}

	c_printf("_mman_proc_init: pcb=0x%08x, pgdir=0x%08x\n", pcb, pgdir);

	/* hey, we made it */
	pcb->pgdir = (Pagedir_ptr)pgdir;
	pcb->virt_map = map;
	return SUCCESS;

Cleanup:
	if(map)   _mman_map_free(map);
	if(pgdir) _mman_pgdir_free(pgdir);
	return status;
}

Status _mman_proc_exit(Pcb *pcb) {
	if (pcb->pgdir) _mman_pgdir_free(pcb->pgdir);
	if (pcb->virt_map) _mman_map_free(pcb->virt_map);
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

Status _mman_proc_copy(Pcb *new, Pcb *old) {
	Status status;

	new->pgdir = NULL;
	new->virt_map = NULL;

	// get a new pgtbl
	status = _mman_pgdir_alloc(&new->pgdir);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	// and a new map
	status = _mman_map_alloc(&new->virt_map);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	// copy parent
	status = _mman_pgdir_copy(new->pgdir, new->virt_map, old->pgdir, old->virt_map);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	// move the stack mapping
	status = _mman_unmap_page(new->pgdir, (USER_STACK >> 12) - 1);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	status = _mman_map_page(new->pgdir, (USER_STACK >> 12) - 1, ((Uint32)new->stack) >> 12, MAP_WRITE | MAP_USER);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	return SUCCESS;

Cleanup:
	if (new->pgdir) _mman_pgdir_free(new->pgdir);
	if (new->virt_map) _mman_map_free(new->virt_map);
	return status;
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
		pgdir = pcb->pgdir;
		virt_map = pcb->virt_map;
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
	for (map = virt_map, i = start_page; i < end_page; i++, map++) {
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
	for (map = _phys_map, i = start_phys, j = 0; i < end_phys && j < num_pages; i++, map++) {
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

	pcnt = 0;

	_kmemclr((void*)_kpgdir, sizeof(Pagedir));
	_kmemclr((void*)_kpgtbl, sizeof(Pagetbl));
//	_kmemclr(Zero, sizeof(Zero));

	_kpgdir[0].dword = ((Uint32)_kpgtbl) | PD_WRITE | PD_PRESENT;
	_kpgdir[1].dword = ((Uint32)_kmappgtbl) | PD_WRITE | PD_PRESENT;

	/* Now set up the free page tables.
	** This is going to take a lot of room.
	** For now, let's use the space from 0x100000 through 0x3fffff.
	*/
	_pgdirs = (Pagedir*)0x100000;
	_kmemclr(_pgdirs, 0x300000);

	if ((status = _q_alloc(&_pgdir_queue, NULL)) != SUCCESS) {
		_kpanic("mman", "Couldn't alloc queue for page tables", status);
	}

	for (addr = (Uint32)_pgdirs; status == SUCCESS && addr < 0x400000; addr += 0x1000) {
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

	/* identity mapping for kernel's first 8MB */
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

	/* let's also put the kernel pagedir in the TSS */
	//*((Pagedir_entry**)(TSS_START+BOOT_ADDRESS+28)) = (Pagedir_entry*)_kpgdir;

	/* cross your fingers... */
	_mman_kernel_mode();

	/* let the world know that we didn't crash (yet)! */
	c_puts (" mman");
}

void _mman_kernel_mode(void) {
	_mman_set_cr3((Pagedir_entry*)_kpgdir);
	_mman_enable_paging();
//	c_printf("_mman_kernel_mode\n");
}

void _mman_restore_pgdir(void) {
//	c_printf("_mman_restore_pgdir => %08x\n", _current->pgdir);
	_mman_set_cr3(_current->pgdir);
	_mman_enable_paging();
}

void _mman_info(void) {
	Uint32 cr0, cr2, cr3;

	cr0 = _mman_get_cr0();
	cr2 = _mman_get_cr2();
	cr3 = _mman_get_cr3();

	c_printf("kpgdir=%08x    current->pgdir=%08x\n"
	         "cr0=%08x cr2=%08x cr3=%08x\n",
	         _kpgdir, _current->pgdir, cr0, cr2, cr3);
}
