/*
** File:	bios_probe.c
**
** Author:	James Letendre
**
** Contributor:
**
** Description:	Probe the bios to find PMIB for VBE
*/
#define	__KERNEL__20113__

#include "headers.h"

#include "c_io.h"
#include "bios_probe.h"

/* new area for BIOS */
char PMBios[BIOS_SIZE];	// 32KB

void* bios_probe(void)
{
	// pointer to video bios
	char *bios = (char*)BIOS_START;
	int i;
	int idx;
	char checksum;

	// copy bios
	for( idx = 0; idx < BIOS_SIZE; idx++ )
	{
		PMBios[idx] = bios[idx];
	}

	// scan it
	for( idx = 0; idx < (BIOS_SIZE - 20); idx++ )
	{
		if( PMBios[idx+0] == 'P' &&
			PMBios[idx+1] == 'M' &&
			PMBios[idx+2] == 'I' &&
			PMBios[idx+3] == 'D')
		{
			checksum = 0;
			for( i = 0; i < 20; i++ )
				checksum += PMBios[idx+i];

			c_printf("\nFound 0x%x\n", checksum);

			if( checksum == 0 )
				return (void*)(PMBios + idx);
		}
	}

	return (void*)NULL;
}
