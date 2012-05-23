/*
** shm.c
**
** Implementation of named shared memory regions
**
** Corey Bloodstein (cmb4247)
*/

#define __KERNEL__20113__

#include "headers.h"
#include "mman.h"
#include "shm.h"
#include "c_io.h"

static Shm *shm_first;

/*
** Initialization
*/
void _shm_init() {
	shm_first = NULL;
	c_puts(" shm");
}

/*
** Create a new shared memory object and map it into the calling user
** process's address space. *ptr is the beginning of the virtual mapping.
*/
Status _shm_create(Pcb *pcb, const char *name, Uint32 size, Uint32 flags, void **ptr) {
	Shm *prev, *curr, *shm;
	Shm_mapping *mapping;
	Shminfo *info;
	Uint32 name_len, i, num_pages, vpg, ppg;
	Int32 diff;
	Status status;
	
	c_printf("_shm_create\n");

	if ((flags & SHM_FLAGS) != flags) {
		return BAD_PARAM;
	}

	/* first make sure there's no mapping that has that name already */
	prev = NULL;
	curr = shm_first;

	while (curr) {
		diff = _kstrcmp(name, curr->name);

		if (diff > 0) {
			/* name sorts after curr->name, keep advancing */
			prev = curr;
			curr = curr->next;
		} else if (diff < 0) {
			/* name sorts before curr->name, we can insert here */
			break;
		} else {
			/* name already exists */
			return ALREADY_EXISTS;
		}
	}

	/*
	** Looks like we can make our new shared region.
	** Allocate space for the region descriptor, plus the page list,
	** plus the name. The actual structure we get is something like:
	**
	** -12          0               28           ??
	** { heap tag } { Shm members } { ppg list } { name }
	**
	** This saves us 24 bytes of management overhead because of the way
	** the heap manager works, and we can free the lot later with one
	** call.
	*/
	name_len = _kstrlen(name);

	num_pages = (size >> 12);
	if (size & 0x0fff) {
		num_pages++;
	}

	shm = _kmalloc(sizeof(Shm) + (num_pages * sizeof(Uint32)) + name_len + 1);

	if (!shm) {
		return ALLOC_FAILED;
	}

	shm->refcount = 1;
	shm->ppgs = (Uint32*)(shm+1);
	shm->num_pages = num_pages;
	shm->flags = flags;

	shm->name = (char*)((Uint32*)(shm+1) + shm->num_pages);

	/*
	** Now we need to find physical pages to back the region.
	** We'll do this by making a new pre-backed region in the target process
	** and translating the virtual pages.
	*/
	if ((status = _mman_alloc(pcb, ptr, size, flags | MAP_USER | MAP_SHARED)) != SUCCESS) {
		_kfree(shm);
		return status;
	}

	for (i = 0, vpg = ((Uint32)*ptr) >> 12; i < num_pages; i++, vpg++) {
		if ((status = _mman_translate_page(pcb->pgdir, vpg, &ppg, 0)) != SUCCESS) {
			_kfree(shm);
			_mman_free(pcb, *ptr, size);
			return status;
		}
		shm->ppgs[i] = ppg;
		c_printf("%08x\n", shm->ppgs[i]);
	}

	/*
	** Create a mapping object to give to the pcb.
	*/
	info = &pcb->shminfo;

	for (i = 0; i < info->num_mappings; i++) {
		if (!info->mappings[i].shm) {
			mapping = &info->mappings[i];
			break;
		}
	}

	if (i == info->num_mappings) {
		if (info->num_mappings < SHM_MAX_MAPPINGS) {
			mapping = _krealloc(info->mappings, (info->num_mappings + 4) * sizeof(Shm_mapping));
	
			if (!mapping) {
				_kfree(shm);
				_mman_free(pcb, *ptr, size);
				return ALLOC_FAILED;
			}
	
			info->mappings = mapping;
		} else {
			_mman_free(pcb, *ptr, size);
			return ALLOC_FAILED;
		}
	}

	mapping = &info->mappings[info->num_mappings++];
	mapping->ptr = *ptr;
	mapping->shm = shm;

	_kmemcpy(shm->name, (char*)name, name_len + 1);

	/*
	** now shove our new thing into the list of other things
	** and get out of here
	*/
	shm->prev = prev;
	shm->next = curr;

	if (curr) {
		curr->prev = shm;
	}

	if (prev) {
		prev->next = shm;
	} else {
		shm_first = shm;
	}

	return SUCCESS;
}

/*
** Map an existing named shared memory region into the caller's address
** space. *ptr is the beginning of the mapping.
*/
Status _shm_open(Pcb *pcb, const char *name, void **ptr) {
	Shm *shm;
	Shm_mapping *mapping;
	Shminfo *info;
	Uint32 i, vpg;
	Status status;

	c_printf("_shm_open\n");

	info = &pcb->shminfo;

	/* find the mapping, if it exists */
	if ((status = _shm_find(name, &shm)) != SUCCESS) {
		return status;
	}

	/* map the region into the user's address space */
	if ((status = _mman_alloc(pcb, ptr, shm->num_pages << 12, shm->flags | MAP_USER | MAP_SHARED | MAP_VIRT_ONLY)) != SUCCESS) {
		return status;
	}

	for (i = 0, vpg = ((Uint32)*ptr) >> 12; i < shm->num_pages; i++, vpg++) {
		if ((status = _mman_map_page(pcb->pgdir, vpg, shm->ppgs[i], shm->flags | MAP_USER | MAP_SHARED)) != SUCCESS) {
			return status;
		}
		SET_PAGE_BIT(pcb->virt_map, vpg);
		PAGE_ADDREF(shm->ppgs[i]);
	}

	for (i = 0; i < info->num_mappings; i++) {
		if (!info->mappings[i].shm) {
			mapping = &info->mappings[i];
			break;
		}
	}

	/* make room for the mapping object in the pcb */
	if (i == info->num_mappings) {
		if (info->num_mappings < SHM_MAX_MAPPINGS) {
			mapping = _krealloc(info->mappings, (info->num_mappings + 4) * sizeof(Shm_mapping));
	
			if (!mapping) {
				_mman_free(pcb, *ptr, shm->num_pages << 12);
				return ALLOC_FAILED;
			}
	
			info->mappings = mapping;
		} else {
			_mman_free(pcb, *ptr, shm->num_pages << 12);
			return ALLOC_FAILED;
		}
	}

	mapping = &info->mappings[info->num_mappings++];
	mapping->ptr = *ptr;
	mapping->shm = shm;

	++shm->refcount;
	
	return SUCCESS;
}

/*
** Release a shared memory region and unmap it from the address space.
*/
Status _shm_close(Pcb *pcb, const char *name) {
	Shminfo *info;
	Shm_mapping *mapping;
	Shm *shm;
	Status status;

	c_printf("_shm_close\n");

	info = &pcb->shminfo;

	if ((status = _shm_find_mapping(pcb, name, &mapping)) != SUCCESS) {
		return status;
	}

	shm = mapping->shm;

	if ((status = _mman_free(pcb, mapping->ptr, shm->num_pages << 12)) != SUCCESS) {
		return status;
	}

	_kmemclr(mapping, sizeof(Shm_mapping));

	if (--shm->refcount == 0) {
		return _shm_delete(shm);
	}

	return SUCCESS;
}

/*
** Find a shared memory object in the system list by name.
*/
Status _shm_find(const char *name, Shm **out) {
	Shm *shm;
	Int32 diff;

	for (shm = shm_first; shm; shm = shm->next) {
		c_printf("_kstrcmp(%s, %s)\n", name, shm->name);
		diff = _kstrcmp(name, shm->name);

		if (diff < 0) {
			/* name sorts before curr->name */
			c_printf("_shm_find: not found\n");
			return NOT_FOUND;
		} else if (diff == 0) {
			/* name exists */
			*out = shm;
			return SUCCESS;
		}
	}

	c_printf("_shm_find: not found\n");

	return NOT_FOUND;
}

/*
** Find a mapping object in a user process's list by name.
*/
Status _shm_find_mapping(Pcb *pcb, const char *name, Shm_mapping **out) {
	Shm_mapping *mappings;
	Uint32 i, max;

	mappings = pcb->shminfo.mappings;
	max = pcb->shminfo.num_mappings;

	for (i = 0; i < max; i++) {
		if (mappings[i].shm) {
			if (!_kstrcmp(name, mappings[i].shm->name)) {
				*out = &mappings[i];
				return SUCCESS;
			}
		}
	}

	c_printf("_shm_find_mapping: not found\n");

	return NOT_FOUND;
}

/*
** Delete an unused shared memory object and free the physical memory.
*/
Status _shm_delete(Shm *shm) {
	c_printf("_shm_delete\n");

	if (shm->refcount != 0) {
		c_printf("_shm_delete: region is still referenced: \"%s\":%d\n",
			shm->name, shm->refcount);
		return FAILURE;
	}

	if (shm->next) {
		shm->next->prev = shm->prev;
	}

	if (shm->prev) {
		shm->prev->next = shm->next;
	} else {
		shm_first = shm->next;
	}

	_kfree(shm);

	return SUCCESS;
}

/*
** Release mappings on process shutdown
*/
Status _shm_cleanup(Pcb *pcb) {
	Shm_mapping *mappings;
	Uint32 i, max;
	Status status;

	c_printf("_shm_cleanup\n");

	mappings = pcb->shminfo.mappings;
	max = pcb->shminfo.num_mappings;

	for (i = 0; i < max; i++) {
		if (mappings[i].shm) {
			if (--mappings[i].shm->refcount == 0) {
				if ((status = _shm_delete(mappings[i].shm)) != SUCCESS) {
					return status;
				}
			}
		}
	}

	_kfree(mappings);

	return SUCCESS;
}

/*
** Copy a process's shared memory information for fork()
*/
Status _shm_copy(struct pcb *new, struct pcb *pcb) {
	Shm_mapping *mappings;
	Uint32 i;

	mappings = _kmalloc(pcb->shminfo.num_mappings * sizeof(Shm_mapping));
	if (!mappings) {
		return ALLOC_FAILED;
	}

	_kmemcpy(mappings, pcb->shminfo.mappings, pcb->shminfo.num_mappings * sizeof(Shm_mapping));

	for (i = 0; i < pcb->shminfo.num_mappings; i++) {
		if (mappings[i].shm) {
			++mappings[i].shm->refcount;
		}
	}

	new->shminfo.num_mappings = pcb->shminfo.num_mappings;

	return SUCCESS;
}

