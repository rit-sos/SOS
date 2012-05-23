
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
#include "fd.h"

/*
** General (C and/or assembly) definitions
*/

#ifndef __ASM__20113__

/*
** Start of C-only definitions
*/

#define ATA_MAX_BUSSES 3
#define ATA_DRIVES_PER_BUS 4
#define ATA_MAX_REQUESTS 16
#define DEBUG_ATA
/*
** Types
*/

//forward declarations

typedef enum ata_type{
	INVALID_DRIVE,
	CHS,
	LBA48,
	LBA28
}ata_type;

typedef enum state{
	IDLE,
	READING,
	WRITING
}ata_state;

typedef struct drive{
	Uint64 sectors;
	Uint32 base; //base IO port address
	Uint32 control; //control register IO port address
	ata_type type;
	ata_state state;
	char model[44]; //44 bytes for model name
	Uint8 master; //true if master, false if slave
}Drive;

typedef struct ata_fd_data{
	Drive *d;
	Uint64 sector_start;
	Uint64 sector_end;
	Uint64 read_sector;
	Uint64 write_sector;
}Ata_fd_data;

typedef struct bus{
	Drive drives[ATA_DRIVES_PER_BUS];
	Uint32 primary_base; 
	Uint32 primary_control; 
	Uint32 secondary_base; 
	Uint32 secondary_control;
	Uint32 busmaster_base;
	Uint8 interrupt_line; 
}Bus;

/*
** Globals
*/

Bus _busses[ATA_MAX_BUSSES];
Drive *_ata_primary;


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
Fd* _ata_fopen(Drive *d, Uint64 sector, Uint16 len, Flags flags);
Status _ata_fclose(Fd *fd);
Status _ata_flush(Fd *fd);
Status _ata_read_blocking(Fd* fd);
Status _ata_read_start(Fd* fd);
Status _ata_read_finish(Fd* fd);
Status _ata_write_blocking(Fd* fd);
Status _ata_write_start(Fd* fd);
int read_raw_blocking(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf );
int write_raw_blocking(Drive* d, Uint64 sector, Uint16 sectorcount, Uint16 *buf );

#endif

#endif
