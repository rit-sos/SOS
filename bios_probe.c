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
	char *new_bios = PMBios;
	int i;
	char checksum;

	// copy bios
	while(bios < (char*)BIOS_END)
	{
		*new_bios++ = *bios;
	}

	// scan it
	*new_bios = PMBios;
	while( new_bios < PMBios + BIOS_SIZE )
	{
		if( new_bios[0] == 'P' &&
			new_bios[1] == 'M' &&
			new_bios[2] == 'I' &&
			new_bios[3] == 'D')
		{
			checksum = 0;
			for( i = 0; i < 20; i++ )
				checksum += new_bios[i];

			if( checksum == 0 )
				return (void*)new_bios;
		}

		new_bios++;
	}

	return (void*)NULL;
}
