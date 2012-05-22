

#ifndef _ATA_PIO_H
#define _ATA_PIO_H

#define PRIMARY 	0xF0E0
#define SECONDARY  	0xF0C0

#define CHS_MODE	0xA0
#define LBA_MODE	0x40

/*
 **Port offsets
 */

#define DATA		0x0
#define FEATURES	0x1
#define SECTOR_COUNT	0x2
#define SECTOR_NUMBER	0x3
#define LBA_LOW		0x3
#define CYLINDER_LOW	0x4
#define LBA_MID		0x4
#define CYLINDER_HIGH	0x5
#define LBA_HIGH	0x5
#define DRIVE_HEAD_PORT	0x6
#define REGULAR_STATUS 	0x7
#define COMMAND		0x7

/*
 **Status bits
 */
#define ERR	(1<<0)
#define DRQ	(1<<3)
#define SRV	(1<<4)
#define DF	(1<<5)
#define RDY	(1<<6)
#define BSY	(1<<7)

/*
 **control register bit descriptions
 */
#define NIEN	0x02
#define SRST 	0x04
#define HOB  	0x08

/*
 **Commands
 */

#define IDENTIFY		0xEC
#define READ_SECTORS_EXT 	0x24
#define WRITE_SECTORS_EXT 	0x34
#define FLUSH_CACHE		0xE7

typedef struct Identify_packet{
	char devtype[2];
	char cylinders[(6-2)];
	char heads[(12-6)];
	char sectors[(20-12)];
	char serial[(54-20)];
	char model[(98-54)];
	char capabilities[(106-98)];
	char fieldvalid[(120-106)];
	char max_lba[(164-120)];
	char commandsets[(200-164)];
	char max_lba_ext[(256-200)];
}Identify_packet;

extern Uint16 *block;


void send_read(Ata_fd_data *dev_data);
void send_write(Ata_fd_data *dev_data);
void read_block(Uint16 *block, Drive *d);
void write_block(Uint16 *block, Drive *d);
void write_fd_block(Fd* fd);
void flush_cache(Drive *d);
void delay(Uint32 control);
void selectDrive(Drive *d, Uint8 mode);
void disableIRQ(Drive *d);
void enableIRQ(Drive *d);
void _ata_reset(Uint32 control);
void lba_set_sectors(Drive *d, Uint64 sector, Uint16 sectorcount);
void send_command(Drive *d, Uint8 cmd);
char read_status(Drive *d);
void read_busmaster(Bus *b);
int _ata_identify(int master, Uint32 base, Uint32 control, Drive* d );

#endif
