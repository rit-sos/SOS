
/*
**
** File:	ata.h
**
** Author:	Andrew LeCain
**
** Contributor:
**
** Description:	ata module header file.
*/

#ifndef _ATA_H
#define _ATA_H

#include "headers.h"

typedef struct Drive{
	Uint64 sectors;
	Uint32 base; //base IO port address
	Uint8 master; //true if master, false if slave		
}Drive;	


void _ata_init(void);

#endif
