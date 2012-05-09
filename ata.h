
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

/*
** General (C and/or assembly) definitions
*/

#ifndef __ASM__20113__

/*
** Start of C-only definitions
*/

#define ATA_MAX_BUSSES 3
#define ATA_DRIVES_PER_BUS 4 
#define DEBUG_ATA
/*
** Types
*/
typedef enum ata_type{
	INVALID_DRIVE,
	CHS,
	LBA48,
	LBA28
}ata_type;


typedef struct Drive{
	ata_type type;
	Uint64 sectors;
	Uint32 base; //base IO port address
	Uint32 control; //control register IO port address
	char model[44]; //44 bytes for model name
	Uint8 master; //true if master, false if slave		
}Drive;	

typedef struct Bus{
	Drive drives[ATA_DRIVES_PER_BUS];
	Uint32 primary_base; 
	Uint32 primary_control; 
	Uint32 secondary_base; 
	Uint32 secondary_control;
	Uint8 interrupt_line; 
}Bus;

/*
** Globals
*/

Bus busses[ATA_MAX_BUSSES];

/*
** prototypes
*/


void _ata_init(void);

/*
**Read sectors from the drive.
**
** d -- drive residing in the golbal "busses"
** sector -- the starting sector to read from
** sectorcount -- the number of sectors to read. Note: buffer must be 256 words * this size
** buf -- the word buffer that will hold the data
*/
int _ata_read(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf );

#endif

#endif
