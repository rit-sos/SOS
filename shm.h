/*
** shm.h
**
** Definitions for named shared memory regions
**
** Corey Bloodstein (cmb4247)
*/

#ifndef _SHM_H
#define _SHM_H

#define SHM_WRITE			(0x00000002)
#define SHM_FLAGS			(SHM_WRITE)

#if defined(__KERNEL__20113__) && !defined(__ASM__20113__)

#define SHM_MAX_MAPPINGS	(64)

/*
** Shared memory region descriptor.
**
** Owned by the shared memory manager, connected to by any number of processes
** via mapping descriptors.
*/
typedef struct shm {
	/* reference count of the region, physical memory can be freed when this reaches 0 */
	Uint32 refcount;

	/* array of physical pages backing this shm region */
	Uint32 *ppgs;

	/* size of the mapping in pages */
	Uint32 num_pages;

	/* combination of SHM_FLAGS, constant for the lifetime of the region */
	Uint32 flags;

	/* the name of the shared mapping */
	char *name;

	/*
	** The previous and next shm region in the list of allocated regions.
	** These make up the sorted list (by name) of shared memory regions.
	*/
	struct shm *next;
	struct shm *prev;
} Shm;

/*
** Shared memory mapping descriptor.
**
** Owned by a process that opens a shared memory region.
*/
typedef struct shm_mapping {
	/* pointer to the corresponding region descriptor */
	struct shm *shm;

	/* pointer to the region's virtual mapping in the process owning this descriptor */
	void *ptr;
} Shm_mapping;

/*
** Collection of shared memory mappings.
**
** Used to track all of a process's shared memory usage.
*/
typedef struct shminfo {
	Uint32 num_mappings;
	struct shm_mapping *mappings;
} Shminfo;


/*
** Module init
*/
void _shm_init(void);

/*
** Release mappings on process shutdown
*/
Status _shm_cleanup(struct pcb *pcb);


/*
** Syscall implementations
*/

/*
** Create a new shared memory object and map it into the calling user
** process's address space. *ptr is the beginning of the virtual mapping.
*/
Status _shm_create(struct pcb *pcb, const char *name, Uint32 size, Uint32 flags, void **ptr);

/*
** Map an existing named shared memory region into the caller's address
** space. *ptr is the beginning of the mapping.
*/
Status _shm_open(struct pcb *pcb, const char *name, void **ptr);

/*
** Release a shared memory region and unmap it from the address space.
*/
Status _shm_close(struct pcb *pcb, const char *str);

/*
** Copy a process's shared memory information for fork()
*/
Status _shm_copy(struct pcb *new, struct pcb *pcb);

/*
** Helpers
*/

/*
** Find a shared memory object in the system list by name.
*/
Status _shm_find(const char *name, struct shm **out);

/*
** Find a mapping object in a user process's list by name.
*/
Status _shm_find_mapping(struct pcb *pcb, const char *name, struct shm_mapping **out);

/*
** Delete an unused shared memory object and free the physical memory.
*/
Status _shm_delete(struct shm *shm);

#endif

#endif
