

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

#define IDENTIFY			0xEC
#define READ_SECTORS		0x20
#define WRITE_SECTORS		0x30
#define READ_SECTORS_EXT 	0x24
#define WRITE_SECTORS_EXT 	0x34
#define FLUSH_CACHE			0xE7

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

/*
 * _ata_pio_lba48_set_sectors -> set the sector to read or write on an LBA48 drive
 *
 *  usage: _ata_pio_lba48_set_sectors(drive, sectorstart, 1);
 */
void _ata_pio_lba48_set_sectors(Drive *d, Uint64 sector, Uint16 sectorcount);
/*
 * _ata_pio_lba28_set_sectors -> set the sector to read or write on an LBA28 drive
 *
 *  usage: _ata_pio_lba28_set_sectors(drive, sectorstart, 1);
 */
void _ata_pio_lba28_set_sectors(Drive *d, Uint sector, Uint8 sectorcount);
/*
 * _ata_pio_send_read-> send a read command to a drive
 *
 *  usage: _ata_pio_send_read((Ata_fd_data *) fd->device_data);
 */
void _ata_pio_send_read(Ata_fd_data *dev_data);
/*
 * _ata_pio_send_write -> send a write command of the appropriate type to the drive
 *
 *  usage: _ata_pio_send_write((Ata_fd_data *) fd->device_data);
 */
void _ata_pio_send_write(Ata_fd_data *dev_data);
/*
 * _ata_pio_read_block -> transfer one sector of size SECTOR_SIZE from the drive into block
 *
 *  usage: _ata_pio_read_block(buf, drive);
 */
void _ata_pio_read_block(Uint16 *block, Drive *d);
/*
 * _ata_pio_write_block ->transfer one sector of size SECTOR_SIZE from the block into the drive.
 *
 *  usage: _ata_pio_write_block(buf, drive);
 */
void _ata_pio_write_block(Uint16 *block, Drive *d);
/*
 * _ata_pio_write_fd_block -> transfer one sector from the file descriptor buffer to the drive it's attached to.
 *
 *  usage: _ata_pio_write_fd_block(fd);
 */
void _ata_pio_write_fd_block(Fd* fd);
/*
 * _ata_pio_flush_cache -> send the cache flush command to the drive
 *
 *  usage: _ata_pio_flush_cache(drive);
 */
void _ata_pio_flush_cache(Drive *d);
/*
 * _ata_pio_delay -> delay 500ns by reading the control register 5 times.
 *
 *  usage: _ata_pio_delay(drive->control)
 */
void _ata_pio_delay(Uint32 control);
/*
 * _ata_pio_selectDrive -> select a drive in the specified mode
 *
 *  usage: _ata_pio_selectDrive(drive, LBA48);
 */
void _ata_pio_selectDrive(Drive *d, ata_type mode);
/*
 * _ata_pio_disableIRQ -> disable the IRQ on a specified drive.
 *
 *  usage: _ata_pio_disableIRQ(drive);
 */
void _ata_pio_disableIRQ(Drive *d);
/*
 * _ata_pio_enableIRQ -> enable the irq on the specified drive
 *
 *  usage: _ata_pio_enableIRQ(drive);
 */
void _ata_pio_enableIRQ(Drive *d);
/*
 * _ata_pio_reset -> reset the ata bus
 *
 *  usage: _ata_pio_reset(drive->control);
 */
void _ata_pio_reset(Uint32 control);
/*
 * _ata_pio_send_command -> send a command to the drive
 *
 *  usage: _ata_pio_send_command (drive, <command>);
 */
void _ata_pio_send_command(Drive *d, Uint8 cmd);
/*
 * _ata_pio_read_status -> read the status byte from the drive.
 *
 *  usage: char status = _ata_pio_read_status(drive);
 */
char _ata_pio_read_status(Drive *d);
/*
 * _ata_pio_read_busmaster -> reads the busmaster status bytes on a bus,
 *              telling the device the interrupt is handled
 *
 *  usage: _ata_pio_read_busmaster(_busses[current_bus]);
 */
void _ata_pio_read_busmaster(Bus *b);
/*
 * _ata_pio_identify -> runs the drive identify routine and fills d with appropirate information
 *
 *  usage: _ata_pio_identify( master_not_slave, bar[0], bar[1]+2, &drive);
 */
int _ata_pio_identify(int master, Uint32 base, Uint32 control, Drive* d );

#endif
