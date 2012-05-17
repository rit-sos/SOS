#define __KERNEL__20113__

#include "headers.h"
#include "x86arch.h"
#include "c_io.h"
#include "queues.h"
#include "stacks.h"
#include "bootstrap.h"
#include "mman.h"
#include "syscalls.h"
#include "startup.h"

#define CHECK_PAGE_BIT(M,X)	((M)[(X)>>5] &  (0x80000000 >> ((X) & 0x1f)) != 0)
#define CLEAR_PAGE_BIT(M,X)	((M)[(X)>>5] &= ~(0x80000000 >> ((X) & 0x1f)))
#define SET_PAGE_BIT(M,X)	((M)[(X)>>5] |= (0x80000000 >> ((X) & 0x1f)))

/*
#define CLEAR_PAGE_BIT(M,X)		{											\
	if (((M) == _phys_map || (M) == _virt_map) && (X) < 0x0c00) {			\
		_mman_panic((M), (X));												\
	}																		\
	((M)[(X)>>5] &= ~(0x80000000 >> ((X) & 0x1f))); }

static void _mman_panic(Uint32 *map, Uint32 page) {
	Uint32 *ptr;														\
	c_printf("how did we get here? map=%08x, page=%08x, stack follows:\n",map,page); \
	for (ptr = (Uint32*)_get_ebp(); ptr; ptr = (Uint32*)*ptr) {
		c_printf("> %08x <\n", ptr[1]);	
		break;
	}
	for(;;);
	_kpanic("mman", "bad bad", FAILURE);
}
*/

#define PAGE_REFCOUNT(X)	(((Uint32*)REFCOUNT_BASE)[(X)])
#define PAGE_ADDREF(X)		(SET_PAGE_BIT(_phys_map,(X)),++(PAGE_REFCOUNT((X))))
#define PAGE_RELEASE(X)		(--(PAGE_REFCOUNT((X))))

/*
#define PAGE_ADDREF(X)			(											\
	SET_PAGE_BIT(_phys_map,(X)),											\
	++(PAGE_REFCOUNT((X))),													\
	(void)(((X) < 0xc00 && (X) != 0x24 && (X) != 0x23 && (X) != 0x3d)		\
		? c_printf("page=%08x ref=%d\n",(X),PAGE_REFCOUNT((X))) : 0),		\
	PAGE_REFCOUNT((X)))

#define PAGE_RELEASE(X)			(											\
	(X) == 0x22 ? _mman_panic(_phys_map,(X)),0 : --(PAGE_REFCOUNT((X))))
*/

#define PAGE_ADDREF_V(P,X,W)	{											\
	Uint32 l_ppg;															\
	if ((status = _mman_translate_page((P), (X), &l_ppg, (W))) != SUCCESS)	\
		goto Cleanup;														\
	PAGE_ADDREF(l_ppg); }

#define PAGE_RELEASE_V(P,X,W)	{											\
	Uint32 l_ppg;															\
	if ((status = _mman_translate_page((P), (X), &l_ppg, (W))) != SUCCESS)	\
		goto Cleanup;														\
	PAGE_RELEASE(l_ppg); }

//	c_printf("PAGE_RELEASE_V(%08x) => PAGE_RELEASE(%08x)\n", (X), l_ppg);

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
** Page table for everything else, particularly refcounts (2MB)
*/
volatile Pagetbl _kextrapgtbl PAGE_ALIGNED;

/*
** Special page filled with zeroes that can be used to pre-fill
** large VM allocations.
*/
static Uint32 _zero[1024] PAGE_ALIGNED;

/*
** Queue of free pgdirs. Structure is the same for pgtbls, so it's reused for
** allocating them.
*/
Queue *_pgdir_queue;

/*
** Queue of free memmaps.
*/
Queue *_map_queue;

/* map of used physical pages */
Memmap_ptr _phys_map;

/* kernel virtual address space usage */
Memmap_ptr _virt_map;

static Pagetbl_entry get_pte(Pagedir_entry *pgdir, Uint32 vpg) {
	Pagedir_entry pde;
	Pagetbl_entry pte;

	pde.dword = pgdir[(vpg >> 10) & 0x03ff].dword;

	if (pde.present) {
		pte = ((Pagetbl_entry*)(pde.dword & PD_FRAME))[vpg & 0x03ff];
	} else {
		pte.dword = 0;
	}

	return pte;
}

#define CHK(X) if ((status = (X)) != SUCCESS) goto Cleanup

static Status handle_cow(Pcb *pcb, Uint32 vpg) {
	void *ksrc, *kdst;
	Pagedir_entry *pgdir;
	Pagetbl_entry *pte;
	Uint32 *virt_map;
	Uint32 src, dst, free_src, free_dst;
	Status status;

	//c_printf("[%04x] handle_cow( %08x, %08x )\n", pcb ? pcb->pid : 0xffff, pcb, vpg);

	if (!pcb) {
		return BAD_PARAM;
	}

	pgdir = pcb->pgdir;
	virt_map = pcb->virt_map;

	free_src = free_dst = 0;

	/* get physical address of COW page */
	CHK(_mman_translate_page(pgdir, vpg, &src, 0));

	if (PAGE_RELEASE(src) == 0) {
		/*
		** This is no longer a COW page, just mark it writable.
		*/
		//c_printf("handle_cow: un-cow vpg=%08x, ppg=%08x\n", vpg, src);

		pte = &((Pagetbl_entry*)(pgdir[(vpg >> 10) & 0x03ff].dword & PD_FRAME))[vpg & 0x03ff];
		pte->cow = 0;
		pte->write = 1;
		PAGE_ADDREF(src);
		if (pgdir == _kpgdir) {
			_invlpg((void*)(vpg << 12));
		}
		status = SUCCESS;
	} else if (pcb) {
		/*
		** We need to copy the user COW page to a fresh physical page.
		*/

		//c_printf("handle_cow: MOOve vpg=%08x, ppg=%08x\n", vpg, src);

		/* map COW page into kernel */
		CHK(_mman_alloc(NULL, &ksrc, PAGESIZE, MAP_VIRT_ONLY));
		CHK(_mman_map_page((Pagedir_entry*)_kpgdir, ((Uint32)ksrc) >> 12, src, 0));
		free_src = 1;
		SET_PAGE_BIT(_virt_map, ((Uint32)ksrc) >> 12);
		PAGE_ADDREF(src);
	
		/* unmap COW page in user, release already happened above */
		CHK(_mman_unmap_page(pgdir, vpg));
		CLEAR_PAGE_BIT(virt_map, vpg);

		/* alloc new phys page for user */
		CHK(_mman_alloc_at(pcb, (void*)(vpg << 12), PAGESIZE, MAP_USER | MAP_WRITE));
	
		/* get phys addr for new phys page */
		CHK(_mman_translate_page(pgdir, vpg, &dst, 1));

		/* map new phys page into kernel */
		CHK(_mman_alloc(NULL, &kdst, PAGESIZE, MAP_VIRT_ONLY | MAP_WRITE));
		CHK(_mman_map_page((Pagedir_entry*)_kpgdir, ((Uint32)kdst) >> 12, dst, MAP_WRITE));
		free_dst = 1;
		SET_PAGE_BIT(_virt_map, ((Uint32)kdst) >> 12);
		PAGE_ADDREF(dst);

		/* copy */
		_kmemcpy(kdst, ksrc, PAGESIZE);

		/* alloc_at() already did the addref for the user */
	}

Cleanup:
	/* unmap COW and new phys page in kernel */
	//c_printf("handle_cow: %s\n", _kstatus(status));

	if (free_src) {
		_mman_free(NULL, ksrc, PAGESIZE);

		if (free_dst) {
			_mman_free(NULL, kdst, PAGESIZE);
		}
	}

	return status;
}

#undef CHK

void _mman_pagefault_isr(int vec, int code) {
	Uint32 cr2;
	Pagetbl_entry pte;

	/*
	** There are a few things that could have happened here.
	**
	** 1. A user process tried to access memory that's not in its address
	**    space. This is a segmentation violation.
	**
	** 2. A user process tried to write to read-only memory. This is also
	**    a segmentation violation.
	**
	** 3. A user process tried to read or write kernel memory. This, too,
	**    is a segmentation violation (noticing a pattern?)
	**
	** 4. A user process tried to write to a COW page. In this case, copy
	**    the physical page somewhere else and mark it writable.
	**
	** 5. A user process tried to read or write a page that's not in
	**    physical memory. In this case, copy it into physical memory.
	**
	** 6. Kernel tried to access memory that's not in its address space.
	**    this is a really bad thing. Kernel panic.
	**
	** 7. Kernel tried to write to a COW page. Why? Panic.
	**
	** 8. Kernel tried to read or write a page that's not in physical
	**    memory. I'm pretty sure we don't want to allow kernel data
	**    or code to be paged to disk, so this should never happen.
	*/

	/* first get the faulting address */
	cr2 = _mman_get_cr2();

	//c_printf("*** Page Fault ***\nvec=%02x code=%04x cr2=%08x\n", vec, code, cr2);

	if (code & PF_USER) {
		pte = get_pte(_current->pgdir, cr2 >> 12);
		if (code & PF_PRESENT) {
			if ((code & PF_WRITE) && pte.user && pte.cow) {
				/* Case 4 */
				//c_putchar('4');
				if (handle_cow(_current, cr2 >> 12) != SUCCESS) {
					/* no choice but to crash the user process */
					_sys_exit(_current);
				}
			} else {
				/* Case 2 or 3 */
				c_putchar('2');
				c_printf("[%04x] Segmentation Fault: "
					"%06x %d %d %d %d %d %d %d %d %d %d %d %d\n",
					_current->pid, pte.frame, pte.disk, pte.shared, pte.cow,
					pte.global, pte.reserved, pte.dirty, pte.accessed,
					pte.cachewrt, pte.cachedis, pte.user, pte.write, pte.present);
				_sys_exit(_current);
			}
		} else if (pte.disk) {
			/* Case 5 */
			_kpanic("mman", "_mman_pagefault_isr: disk paging not implemented", FAILURE);
		} else {
			/* Case 1 */
			c_putchar('1');
			_sys_exit(_current);
		}
	} else {
		Pagedir_entry pde = _kpgdir[cr2 >> 22];
		pte = get_pte((Pagedir_entry*)_kpgdir, cr2 >> 12);
		c_printf("pde = "
			"%05x %x %d %d %d %d %d %d %d %d\n",
			pde.frame, pde.unused0, pde.pgsize, pde.unused1,
			pde.accessed, pde.cachedis, pde.cachewrt, pde.user,
			pde.write, pde.present);
		c_printf("pte = "
			"%05x %d %d %d %d %d %d %d %d %d %d %d %d\n",
			pte.frame, pte.disk, pte.shared, pte.cow,
			pte.global, pte.reserved, pte.dirty, pte.accessed,
			pte.cachewrt, pte.cachedis, pte.user, pte.write, pte.present);
		_kpanic("mman", "_mman_pagefault_isr", FAILURE);
	}

	__outb(PIC_MASTER_CMD_PORT, PIC_EOI);
}

static int pcnt;

Status _mman_pgdir_alloc(Pagedir_entry **pgdir) {
	if (++pcnt % 100 == 0) {
		c_printf("_mman_pgdir_alloc: %d\n", pcnt);
	}
	return _q_remove(_pgdir_queue, (void**)pgdir);
}

Status _mman_pgdir_free(Pagedir_entry *pgdir) {
	int i, start;

	if (pgdir->dword == _kpgdir->dword) {
		start = 1;
	} else {
		start = 0;
	}

	/* free any attached page tables */
	for (i = start; i < 0x400; i++) {
		if (pgdir[i].present) {
			_q_insert(_pgdir_queue, (Pagetbl*)(pgdir[i].dword & PD_FRAME), (Key)0);
		}
	}

	--pcnt;

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
	for (i = 1; i < PAGESIZE / 4; i++) {
		/* if it exists, */
		if (src[i].present) {
			/* allocate a page table for the new directory. */
			if (_mman_pgdir_alloc((Pagedir_entry**)&dst[i]) != SUCCESS) {
				c_printf("_mman_pgdir_copy: _mman_pgdir_alloc failed\n");
				return ALLOC_FAILED;
			}
			/* for the page table pointed to by that pagedir entry, */
			tbl = (Pagetbl_entry*)(src[i].dword & PD_FRAME);
			/* for each page table entry, */
			for (j = 0; j < PAGESIZE / 4; j++) {
				/* if it exists, */
				if (tbl[j].present) {
					if (tbl[j].shared) {
						/* addref the shared mapping */
						//_shm_addref(tbl[j].frame);
						_kpanic("mman", "_mman_pgdir_copy encountered PT_SHARED page", FAILURE);
					} else if (tbl[j].write) {
						/* create a new cow mapping */
						tbl[j].write = 0;
						tbl[j].cow = 1;
					}
					/* copy the pte */
					((Pagetbl_entry*)(dst[i].dword & PD_FRAME))[j].dword = tbl[j].dword;
					PAGE_ADDREF(tbl[j].frame);
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
		if ((pte.present || pte.disk) && (pgdir == _kpgdir || pte.user) && (!write || pte.write || pte.cow)) {
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
** Copy data from a user buffer to a kernel buffer. Surprisingly difficult.
*/
Status _mman_get_user_data(Pcb *pcb, /* out */ void *buf, void *virt, Uint32 size) {
	void *kvaddr;
	Uint32 ppg, kvpg, vpg, num_pages, i;
	Status status;

	if (!pcb || !buf || !virt || !size) {
		return BAD_PARAM;
	}

	num_pages = size >> 12;
	if (((Uint32)virt) & 0x0fff) {
		num_pages++;
	}
	if ((size & 0x0fff) > PAGESIZE - (((Uint32)virt) & 0x0fff)) {
		num_pages++;
	}

	if (num_pages > MAX_TRANSFER_PAGES) {
		return BAD_PARAM;
	}

	/* find available virtual address range */
	if ((status = _mman_alloc(NULL, &kvaddr, PAGESIZE * num_pages, MAP_VIRT_ONLY)) != SUCCESS) {
		return status;
	}

	kvpg = ((Uint32)kvaddr) >> 12;

	/* translate and map */
	for (i = 0, vpg = ((Uint32)virt) >> 12; i < num_pages; i++, vpg++, kvpg++) {
		if ((status = _mman_translate_page(pcb->pgdir, vpg, &ppg, 0)) != SUCCESS) {
			goto Cleanup;
		}

		if ((status = _mman_map_page((Pagedir_entry*)_kpgdir, kvpg, ppg, 0)) != SUCCESS) {
			goto Cleanup;
		}

		SET_PAGE_BIT(_virt_map, kvpg);
		PAGE_ADDREF(ppg);
	}

	/* do the copy */
	_kmemcpy(buf, kvaddr + (((Uint32)virt) & 0x0fff), size);

Cleanup:
	/* unmap temporary region */
	_mman_free(NULL, kvaddr, num_pages*PAGESIZE);

	return status;
}

/*
** Copy data from a kernel buffer to a user buffer.
*/
Status _mman_set_user_data(Pcb *pcb, /* out */ void *virt, void *buf, Uint32 size) {
	void *kvaddr;
	Uint32 vpg, kvpg, num_pages, i;
	Pagedir_entry pde;
	Pagetbl_entry *pte;
	Status status;

	if (!pcb || !buf || !virt || !size) {
		return BAD_PARAM;
	}

	num_pages = size >> 12;
	if (((Uint32)virt) & 0x0fff) {
		num_pages++;
	}
	if ((size & 0x0fff) > PAGESIZE - (((Uint32)virt) & 0x0fff)) {
		num_pages++;
	}

	if (num_pages > MAX_TRANSFER_PAGES) {
		return BAD_PARAM;
	}

	/* find available virtual address range */
	if ((status = _mman_alloc(NULL, &kvaddr, PAGESIZE * num_pages, MAP_VIRT_ONLY | MAP_WRITE)) != SUCCESS) {
		return status;
	}

	kvpg = ((Uint32)kvaddr) >> 12;

	/* translate and map */
	for (i = 0, vpg = ((Uint32)virt) >> 12; i < num_pages; i++, vpg++, kvpg++) {
		pde = pcb->pgdir[(vpg >> 10) & 0x03ff];
		if (pde.present && pde.user && pde.write) {
			pte = &((Pagetbl_entry*)(pde.dword & PD_FRAME))[vpg & 0x03ff];
			if (pte->present && pte->user) {
				if (pte->cow) {
					if ((status = handle_cow(pcb, vpg)) != SUCCESS) {
						c_printf("[%04x] _mman_set_user_data: handle_cow=%08x vpg=%08x\n", pcb->pid, status, vpg);
						goto Cleanup;
					}
				}

				if (!pte->write) {
					c_printf("[%04x] _mman_set_user_data: user page %08x not writable\n", pcb->pid, vpg);
					goto Cleanup;
				}

				status = _mman_map_page((Pagedir_entry*)_kpgdir, kvpg, pte->frame, MAP_WRITE);
				if (status != SUCCESS) {
					c_printf("[%04x] _mman_set_user_data: map error kvpg=%08x ppg=%08x\n", pcb->pid, kvpg, pte->frame);
					goto Cleanup;
				}
			}
		} else {
			c_printf("[%04x] _mman_set_user_data: pgdir flags incorrect: %08x\n", pde.dword);
			goto Cleanup;
		}

		SET_PAGE_BIT(_virt_map, kvpg);
		PAGE_ADDREF(pte->frame);
	}

	/* do the copy */
	_kmemcpy(kvaddr + (((Uint32)virt) & 0x0fff), buf, size);

Cleanup:
	/* unmap temporary region */
	_mman_free(NULL, kvaddr, num_pages*PAGESIZE);

	return status;
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
	if ((flags & (MAP_WRITE | MAP_SHARED)) && (flags & MAP_COW)) {
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
		pgdir[idx].dword = ((Uint32)tbl) | (flags & MAP_PD_MASK) | PD_WRITE | PD_PRESENT;
	}

	/* now find the pagetbl entry */
	idx = virt & 0x03ff;

	if (tbl[idx].present) {
		/* mapping already exists for this address */
		Uint32 temp;
		c_printf("mapping exists at index %d in pgdir %08x!\n", idx, pgdir);
		_mman_translate_page(pgdir, virt, &temp, 0);
		c_printf("virt: 0x%08x, phys: 0x%08x want: 0x%08x ", virt << 12, temp << 12, phys << 12);
		c_printf("caller=%08x,%08x\n", *(Uint32*)(_get_ebp()+4), *((*(Uint32**)_get_ebp())+1));
		return ALREADY_EXISTS;
	}

	/* if we get here, then we're ready to add the pagetbl entry */
	tbl[idx].dword = ((phys << 12) & PT_FRAME) | (flags & MAP_FLAGS_MASK) | PT_PRESENT;

	if (pgdir == _kpgdir) {
		_invlpg((void*)(virt << 12));
	}

	return SUCCESS;
}

Status _mman_unmap_page(Pagedir_entry *pgdir, Uint32 virt) {
	Pagedir_entry pde;
	Pagetbl_entry *tbl;
	Uint32 idx;
	Uint32 *ptr;

	if (!pgdir) {
		return BAD_PARAM;
	}

	if (pgdir == _kpgdir && virt < 0xc00) {
		c_printf("how did we get here? virt=%08x, stack follows:\n", virt);
		for (idx = 0, ptr = (Uint32*)_get_ebp(); idx < 4; ptr = (Uint32*)*ptr) {
			c_printf("> %08x <\n", ptr[1]);
		}
		_kpanic("mman", "bad bad", FAILURE);
	}

	/* first find the pagetbl */
	idx = (virt >> 10) & 0x03ff;
	pde.dword = pgdir[idx].dword;
	if (!pde.present) {
		c_printf("_mman_unmap_page: pgdir entry not found (pgdir=%08x, virt=%08x)\n", pgdir, virt);
		return NOT_FOUND;
	}

	tbl = (Pagetbl_entry*)(pde.dword & PD_FRAME);

	/* now find the pagetbl entry */
	idx = virt & 0x03ff;
	if (!tbl[idx].present) {
		c_printf("_mman_unmap_page: pgtbl entry not found (pgdir=%08x, virt=%08x)\n", pgdir, virt);
		return NOT_FOUND;
	}

	/* and clear it */
	tbl[idx].dword = 0;

	if (pgdir == _kpgdir) {
		_invlpg((void*)(virt << 12));
	}

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
	** Currently sizeof(Stack) == PAGESIZE, so this is simple.
	*/
	vpg = (USER_STACK >> 12) - 1;
	ppg = ((Uint32)(pcb->stack)) >> 12;
	if ((status = _mman_map_page(pgdir, vpg, ppg, MAP_WRITE | MAP_USER)) != SUCCESS) {
		goto Cleanup;
	}
	SET_PAGE_BIT(map, vpg);
	PAGE_ADDREF(ppg);

	/*
	** Map the target process's code to USER_ENTRY and up.
	** Since we're not breaking up the program image into code and
	** data sections, this needs to be writable. Instead of making it
	** writable directly, we mark it copy on write so that if the
	** process tries to write to it we give it its own copy of it.
	*/
	vpg = USER_ENTRY >> 12;
	ppg = ((Uint32)entry) >> 12;
	for (i = 0; i < num_pages; i++, vpg++, ppg++) {
		if ((status = _mman_map_page(pgdir, vpg, ppg, MAP_COW | MAP_USER)) != SUCCESS) {
			goto Cleanup;
		}
		SET_PAGE_BIT(map, vpg);
		PAGE_ADDREF(ppg);
	}

	/*
	** Map the kernel in, but don't make it accessible to the user process.
	** This is necessary because the ISR code and core data structures need
	** to be mapped in when an IRQ comes in.
	*/
	pgdir[0] = _kpgdir[0];
	for (i = 0; i < (0x400 / 32); i++) {
		map[i] = 0xffffffff;
	}
//	for (i = 0; i < 0x400; i++) {
//		PAGE_ADDREF(i);
//	}

	pcb->pgdir = (Pagedir_ptr)pgdir;
	pcb->virt_map = map;

	/*
	** Finally, give the process a heap.
	*/
	status = _mman_alloc_at(pcb, (void*)USER_HEAP_BASE, HEAP_CHUNK_SIZE, MAP_USER | MAP_WRITE | MAP_ZERO);
	if (status != SUCCESS) {
		goto Cleanup;
	}
	pcb->heapinfo.count = 1;

	c_printf("_mman_proc_init: pcb=0x%08x, pgdir=0x%08x\n", pcb, pgdir);

	/* hey, we made it */
	return SUCCESS;

Cleanup:
	c_printf("_mman_proc_init: %08x\n", status);
	/* _cleanup() and _mman_proc_exit() will take care of the teardown work */
//	if(map)_mman_map_free(map);
//	if(pgdir) _mman_pgdir_free(pgdir);
	return status;
}

/*
** Per-process memory management cleanup
*/
Status _mman_proc_exit(Pcb *pcb) {
	Pagedir_entry *pgdir;
	Pagetbl_entry *pgtbl;
	int i, j;

	if (!pcb) {
		return BAD_PARAM;
	}

	pgdir = pcb->pgdir;

	/* walk the entire page directory and release physical memory */
	for (i = 0; i < PAGESIZE / 4; i++) {
		if (pgdir[i].present && pgdir[i].user) {
			pgtbl = (Pagetbl_entry*)(pgdir[i].dword & PD_FRAME);
			for (j = 0; j < PAGESIZE / 4; j++) {
				if (pgtbl[j].present && pgtbl[j].user) {
					if (PAGE_RELEASE(pgtbl[j].frame) == 0) {
						CLEAR_PAGE_BIT(_phys_map, pgtbl[j].frame);
					}
				}
			}
		}
	}

	if (pcb->pgdir) _mman_pgdir_free(pcb->pgdir);
	if (pcb->virt_map) _mman_map_free(pcb->virt_map);
	return SUCCESS;
}

/*
** Free allocated virtual address space and release corresponding
** physical memory.
*/
Status _mman_free(Pcb *pcb, void *ptr, Uint32 size) {
	Uint32 i, num_pages;
	Uint32 vpg, ppg;
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
		if (_mman_translate_page(pgdir, vpg, &ppg, 0) == SUCCESS) {
			if ((PAGE_RELEASE(ppg)) == 0) {
				CLEAR_PAGE_BIT(_phys_map, ppg);
			}
		}
		_mman_unmap_page(pgdir, vpg);
		CLEAR_PAGE_BIT(virt_map, vpg);
	}

	return SUCCESS;
}

Status _mman_proc_copy(Pcb *new, Pcb *old) {
	Pagetbl_entry *tbl;
	Uint32 stack;
	Status status;

	new->pgdir = NULL;
	new->virt_map = NULL;
	stack = ((USER_STACK >> 12) - 1);

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
	PAGE_RELEASE_V(new->pgdir, stack, 1);
	status = _mman_unmap_page(new->pgdir, stack);
	if (status != SUCCESS) {
		goto Cleanup;
	}

	c_printf("_mman_proc_copy: new stack is on page %08x, old stack %08x\n", ((Uint32)new->stack) >> 12, ((Uint32)old->stack) >> 12);
	status = _mman_map_page(new->pgdir, stack, ((Uint32)new->stack) >> 12, MAP_WRITE | MAP_USER);
	if (status != SUCCESS) {
		goto Cleanup;
	}
	PAGE_ADDREF((Uint32)(new->stack) >> 12);

	// un-cow the parent stack, since pgdir_copy() cow'd it
	tbl = (Pagetbl_entry*)(old->pgdir[(stack >> 10) & 0x03ff].dword & PD_FRAME);
	tbl[stack & 0x03ff].cow = 0;
	tbl[stack & 0x03ff].write = 1;

	{
		Uint32 new_ppg = -1, old_ppg = -1;
		_mman_translate_page(new->pgdir, stack, &new_ppg, 1);
		_mman_translate_page(old->pgdir, stack, &old_ppg, 1);
		c_printf("_mman_proc_copy: stack translation: new=%08x, old=%08x\n", new_ppg, old_ppg);
	}

	return SUCCESS;

Cleanup:
	/* let _cleanup() handle this */
	return status;
}

/*
** Allocate some virtual memory and return a pointer to it.
** Optionally, also set up the physical memory corresponding to it.
** See mman.h for flags.
*/
Status _mman_alloc(Pcb *pcb, void **ptr, Uint32 size, Uint32 flags) {
	Uint32 i, j, k;
	Uint32 bit, status, vpg, num_pages;
	Pagedir_entry *pgdir;
	Uint32 *virt_map, *map;

	if (!ptr) {
		return BAD_PARAM;
	}

	/* limit 4MB per alloc (for now) */
	if (size > 0x400000) {
		*ptr = NULL;
		return BAD_PARAM;
	}

	if ((flags & (MAP_FLAGS_MASK | MAP_VIRT_ONLY)) != flags) {
		return BAD_PARAM;
	}

	status = MOO;

	/*
	** if a pcb was passed in, this is for a user process, else
	** this is for the kernel
	*/
	if (pcb) {
		pgdir = pcb->pgdir;
		virt_map = pcb->virt_map;
	} else {
		pgdir = (Pagedir_entry*)_kpgdir;
		virt_map = _virt_map;
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
	for (map = virt_map + 96, i = 96; i < 0x8000; i++, map++) {
		if (*map != 0xffffffff) {
			/* save the address of the potential mapping */
			for (vpg = i << 5, k = 0x80000000; k & (*map); k >>= 1) {
				vpg++;
			}

			/* see if the potential mapping is big enough */
			for (j = 1, bit = k; j < num_pages; j++) {
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

	if (j != num_pages) {
		/* we traversed the whole list and didn't find space */
		*ptr = 0;
		return ALLOC_FAILED;
	}

	*ptr = (void*)(vpg << 12);

	if (!(flags & MAP_VIRT_ONLY)) {
		/* now we just need physical memory to back it */
		if ((status = _mman_alloc_at(pcb, *ptr, size, flags)) != SUCCESS) {
			*ptr = 0;
		}
	}

	return status;
}

Status _mman_alloc_at(Pcb *pcb, void *ptr, Uint32 size, Uint32 flags) {
	int i, j, k;
	Pagedir_entry *pgdir;
	Uint32 *virt_map;
	Uint32 *map;
	Uint32 num_pages;
	Uint32 vpg, ppg, bit;
	Status status;

	if (!ptr) {
		return BAD_PARAM;
	}

	/* ptr must be page-aligned */
	if (((Uint32)ptr) & 0x0fff) {
		return BAD_PARAM;
	}

	if ((flags & (MAP_FLAGS_MASK | MAP_ZERO)) != flags) {
		return BAD_PARAM;
	}

	if (pcb) {
		pgdir = pcb->pgdir;
		virt_map = pcb->virt_map;
	} else {
		pgdir = (Pagedir_entry*)_kpgdir;
		virt_map = _virt_map;
	}

	num_pages = size >> 12;
	if (size & 0x0fff) {
		num_pages++;
	}

	vpg = ((Uint32)ptr) >> 12;

	if (flags & MAP_ZERO) {
		flags &= ~MAP_ZERO;

		ppg = ((Uint32)_zero) >> 12;

		/* fill in the allocation with mapped-zero COW or shared pages */
		if (flags & MAP_WRITE) {
			flags = (flags & ~MAP_WRITE) | MAP_COW;
		}
	
		for (i = 0; i < num_pages; i++, vpg++) {
			if ((status = _mman_map_page(pgdir, vpg, ppg, flags)) != SUCCESS) {
				c_printf("_mman_alloc_at: _mman_map_page(vpg => zpg) failed\n");
				goto CleanupZero;
			}
			SET_PAGE_BIT(virt_map, vpg);
			PAGE_ADDREF(ppg);
		}

		return SUCCESS;

	CleanupZero:
		/* undo partial mapping on failure */
		for ( ; i >= 0; --i, --vpg) {
			_mman_unmap_page(pgdir, vpg);
			CLEAR_PAGE_BIT(virt_map, vpg);
			PAGE_RELEASE(ppg);
		}

		return ALLOC_FAILED;

	} else {
		/*
		** Find free physical pages to map to each virtual page.
		** These don't have to be continuous, so less complicated.
		*/
		for (map = _phys_map + 96, i = 96, j = 0; i < 0x8000 && j < num_pages; i++, map++) {
			if (*map != 0xffffffff) {
				for (bit = 0x80000000, k = 0; bit && j < num_pages; bit >>= 1, k++) {
					if (!((*map) & bit)) {
						/* found a free physical page */
						ppg = (i << 5) | k;
						if ((status = _mman_map_page(pgdir, vpg + j, ppg, flags)) != SUCCESS) {
							/* alloc failed */
							goto CleanupPhys;
						}
	
						/* set the bit in the physical allocation map */
						//*map |= bit;
						SET_PAGE_BIT(virt_map, vpg + j);
						PAGE_ADDREF(ppg);

						++j;
					}
				}
			}
		}
	
		/* we traversed the whole list and didn't find space */
		if (j != num_pages) {
			goto CleanupPhys;
		}

		return SUCCESS;

	CleanupPhys:

		/*
		** Undo partial allocations before returning.
		** j is number of successfully mapped pages.
		** vpg is the first virtual page mapped.
		*/
		for (i = 0; i < j; i++, vpg++) {
			if (_mman_translate_page(pgdir, vpg, &ppg, 0) == SUCCESS) {
				if (PAGE_RELEASE(ppg) == 0) {
					CLEAR_PAGE_BIT(_phys_map, ppg);
				}
			}
			_mman_unmap_page(pgdir, vpg);
			CLEAR_PAGE_BIT(virt_map, vpg);
		}

		return ALLOC_FAILED;
	}
}

Status _mman_alloc_framebuffer(void *videoBuf, Uint size) {
	Uint32 i, pages, addr;
	Status status;

	pages = size >> 12;
	if (size & 0x0fff) {
		pages++;
	}

	addr = ((Uint32)videoBuf) >> 12;

	for (i = 0; i < pages; i++, addr++) {
		if ((status = _mman_map_page((Pagedir_entry*)_kpgdir, addr, addr, MAP_WRITE)) != SUCCESS) {
			return status;
		}
		PAGE_ADDREF(addr);
		SET_PAGE_BIT(_virt_map, addr);
	}

	return SUCCESS;
}

void _mman_init(void *videoBuf, Uint size) {
	Uint32 i, addr;
	Memmap *map;
	Status status;

	pcnt = 0;

	_kmemclr((void*)_kpgdir, sizeof(Pagedir));
	_kmemclr((void*)_kpgtbl, sizeof(Pagetbl));
	_kmemclr((void*)_zero, sizeof(_zero));

	_kpgdir[0].dword = ((Uint32)_kpgtbl)      | PD_WRITE | PD_PRESENT;
	_kpgdir[1].dword = ((Uint32)_kmappgtbl)   | PD_WRITE | PD_PRESENT;
	_kpgdir[2].dword = ((Uint32)_kextrapgtbl) | PD_WRITE | PD_PRESENT;

	/* Now set up the free page tables.
	** This is going to take a lot of room.
	** For now, let's use the space from 0x100000 through 0x3fffff.
	*/
	_pgdirs = (Pagedir*)0x100000;
	_kmemclr(_pgdirs, 0x300000);

	if ((status = _q_alloc(&_pgdir_queue, NULL)) != SUCCESS) {
		_kpanic("mman", "Couldn't alloc queue for page tables", status);
	}

	for (addr = (Uint32)_pgdirs; status == SUCCESS && addr < 0x400000; addr += PAGESIZE) {
		if ((status = _q_insert(_pgdir_queue, (Pagedir_ptr)addr, (Key)0)) != SUCCESS) {
			_kpanic("mman", "_q_insert(_pgdir_queue)", status);
		}
	}

	/*
	** Next, let's make room for some maps. These are enormous.
	** We're allocating 4MB, but this only fits 32 maps!
	** The first 2 are reserved for kernel use (physical and virtual usage),
	** leaving room for 30 process VM maps.
	**
	** This mechanism should probably be replaced later, but the odds of me
	** finding time to do so are nearly zero. As a stopgap, consider adding
	** logic to create more maps out on the kernel heap when we run out.
	*/
	if ((status = _q_alloc(&_map_queue, NULL)) != SUCCESS) {
		_kpanic("mman", "_q_alloc(&_map_queue)", status);
	}

	_maps = ((Memmap*)0x400000);
	_kmemclr(_maps, 0x400000);

	_phys_map = _maps[0];
	_virt_map = _maps[1];

	for (i = 2, map = &_maps[2]; i < (0x400000 / sizeof(Memmap)); i++, map++) {
		if ((status = _q_insert(_map_queue, map, (Key)0)) != SUCCESS) {
			_kpanic("mman", "_q_insert(map)", status);
		}
	}

	/*
	** Now the refcounts for shared and COW pages. 4 bytes per physical page,
	** for a total of 4MB.
	*/
	_kmemclr(REFCOUNT_BASE, 0x400000);

	for (i = 0; i < 0x0c00; i++) {
		PAGE_ADDREF(i);
	}

	/* identity mapping for kernel's first 12MB (this just keeps getting bigger..) */
	for (i = 0, addr = 0; addr < 0x400000; i++, addr += 4096) {
		_kpgtbl[i].dword = addr | PT_WRITE | PT_PRESENT;
	}

	for (i = 0; addr < 0x800000; i++, addr += 4096) {
		_kmappgtbl[i].dword = addr | PT_WRITE | PT_PRESENT;
	}

	for (i = 0; addr < 0xc00000; i++, addr += 4096) {
		_kextrapgtbl[i].dword = addr | PT_WRITE | PT_PRESENT;
	}

	for (i = 0; i < 96; i++) {
		_phys_map[i] = 0xffffffff;
		_virt_map[i] = 0xffffffff;
	}

	if ((_mman_alloc_framebuffer(videoBuf, size)) != SUCCESS) {
		_kpanic("_mman_init", "_mman_alloc_framebuffer", FAILURE);
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
