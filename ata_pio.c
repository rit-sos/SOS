#define	__KERNEL__20113__

#include "headers.h"


#include "ata.h"
#include "ata_pio.h"

#include "c_io.h"
#include <startup.h>
#include "support.h"

Uint16 *block;


void send_read (Ata_fd_data *dev_data){
	switch(dev_data->d->type){
		case LBA48:
			lba_set_sectors(dev_data->d, dev_data->read_sector, 1); //request only one sector
			__outb (dev_data->d->base+COMMAND, READ_SECTORS_EXT);
			dev_data->read_sector++;
			break;
		case LBA28:
			c_puts("mode not supported yet!");
			break;
		case INVALID_DRIVE:
			c_puts("invalid!");
			break;
		default:
			c_puts("what?");
			break;
	}
}

void send_write(Ata_fd_data *dev_data){
	switch(dev_data->d->type){
		case LBA48:
			lba_set_sectors(dev_data->d, dev_data->write_sector, 1); //request only one sector
			__outb (dev_data->d->base+COMMAND, WRITE_SECTORS_EXT);
			dev_data->write_sector++;
			break;
		case LBA28:
			c_puts("mode not supported yet!");
			break;
		case INVALID_DRIVE:
			c_puts("invalid!");
			break;
		default:
			c_puts("what?");
			break;
	}
}

void read_block(Uint16 *block, Drive *d){
	int i;
	for(i =0; i < 256; i++){
		block[i] = __inw(d->base+DATA);
	}
}
void write_block(Uint16 *block, Drive *d){
	int i;
	for(i =0; i < 256; i++){
		__outw(d->base+DATA,block[i]);
	}
}

void write_fd_block(Fd* fd){
	int i;
	Uint8 *data = (Uint8 *) block;
	Ata_fd_data *dev_data = (Ata_fd_data *)fd->device_data;

	selectDrive(dev_data->d,LBA_MODE); //select the drive before transferring data

	for(i =0; i < 512; i++){
		data[i]= _fd_getTx(fd) ; //grab the next byte
	}
	write_block(block, dev_data->d);

	//if we just wrote to the sector we last read from, we made our FD dirty
	if(dev_data->write_sector == dev_data->read_sector-1 && fd->write_index > fd->read_index){
		//we just overwrote the sector we are reading from. Update said data
		_fd_flush_rx(fd);
		for (i=fd->read_index % 512; i < 512; i++){
			_fd_readBack(fd,data[i]);
		}
	}
	dev_data->write_sector++;
}

void selectDrive(Drive *d, Uint8 mode){
	//select the master/slave drive as appropriate
	__outb(d->base+DRIVE_HEAD_PORT, mode | (~(d->master))<< 4);	
	delay(d->control);
}

void read_busmaster(Bus *b){
	__inb(b->busmaster_base + 2);
	__outb(b->busmaster_base + 2,0x04); //mark primary busmaster as IRQ handled
	__inb(b->busmaster_base + 8 + 2);
	__outb(b->busmaster_base + 8 + 2,0x04); //mark secondary

}

void disableIRQ(Drive *d){
	if(d->type != INVALID_DRIVE){
		selectDrive(d,LBA_MODE);
		__outb(d->control,0);
		__outb(d->control,NIEN);
	}
}
void enableIRQ(Drive *d){
	if(d->type != INVALID_DRIVE){
		selectDrive(d,LBA_MODE);
		__outb(d->control,0);
	}
}

void _ata_reset(Uint32 control){
	__outb(control, SRST );
	__outb(control, 0x00 );
	delay(control);
	//disable interrupts
}


void lba_set_sectors(Drive *d, Uint64 sector, Uint16 sectorcount){
	Uint8 *sector_b= (Uint8 *)&sector;
	Uint8 *sectorcount_b = (Uint8 *)&sectorcount;

	selectDrive(d,LBA_MODE);
	__outb (d->base+SECTOR_COUNT, sectorcount_b[1]);//sectorcount high byte
	__outb (d->base+LBA_LOW, sector_b[3]);
	__outb (d->base+LBA_MID, sector_b[4]);
	__outb (d->base+LBA_HIGH,sector_b[5]);
	__outb (d->base+SECTOR_COUNT, sectorcount_b[0]);//sectorcount low byte
	__outb (d->base+LBA_LOW, sector_b[0]);
	__outb (d->base+LBA_MID, sector_b[1]);
	__outb (d->base+LBA_HIGH, sector_b[2]);
}

void send_command(Drive *d, Uint8 cmd){
	__outb(d->base+COMMAND,cmd);
}

char read_status(Drive *d){
	selectDrive(d,LBA_MODE);
	return __inb(d->base+REGULAR_STATUS);
}

void delay(Uint32 control){
	Uint8 i;
	for (i = 0 ; i < 5; i++){
		//read the control register 5 times to give it time to settle
		__inb(control);
	}
}

void flush_cache(Drive *d){
	__outb (d->base+COMMAND, FLUSH_CACHE);
}

int _ata_identify(int master, Uint32 base, Uint32 control, Drive* d ){

	d->type=INVALID_DRIVE;
	d->base=base;
	d->control=control;
	d->master=master;

	if(base == 0x00 || control == 0x00){
		c_puts("invalid bus. Skipping");
		return -1;
	}

	delay(control);

	selectDrive(d,CHS); //chs mode for identification
	//detect a floating drive
	if( __inb( base+REGULAR_STATUS ) != 0xFF){ //High if there is no drive
		int i;

		volatile unsigned char status = 0;
		do{
			status =__inb(base+REGULAR_STATUS);
		}while (status & BSY);
		//load 0 into the follow addresses. If these change, we messed up.
		__outb(base+SECTOR_COUNT,0);
		__outb(base+LBA_LOW,0);
		__outb(base+LBA_MID,0);
		__outb(base+LBA_HIGH,0);

		//send the identify command
		__outb(base+COMMAND,IDENTIFY);
		delay(control);
		status =__inb(base+REGULAR_STATUS);
		if (status == 0){ //no device
			return -1;
		}

		if( status & ERR || status & DF){
			c_printf("identify error.. SATA drive? Status:%x\n", status);
			_ata_reset(control);
			return -1;
		}

		while( (status = __inb(base+REGULAR_STATUS)) & BSY){
			//read status until busy bit clears
		}

		if (__inb(base+LBA_MID) || __inb(base+LBA_HIGH)){
			//check if the drive is non ATA. If so, end
			Uint8 cl = __inb(base+CYLINDER_LOW);
			Uint8 ch = __inb(base+CYLINDER_HIGH);
			if (cl == 0x14 && ch == 0xEB){
				c_printf("ATAPI drive detected but not supported\n");
				__outb(control, 0);
				__outb(control, NIEN);
				flush_cache(d);
			}
			return -1;
		}
		do{
			status = __inb(base+REGULAR_STATUS); //read status until error or DRQ
		}while(!(status & (DRQ | ERR | DF)));

		if(status & ERR || status &DF){
			//there was an error requesting
			c_printf("error reading the identify block");
			return -1;
		}
		if(!(status & DRQ)){
			return -1;
		}
		//read in the identify block
		for(i =0; i < 256; i++){
			block[i] = __inw(base+DATA);
		}

		Identify_packet *ip=(Identify_packet *) block;

		for (i=0;i<40;i+=2){
			d->model[i]=ip->model[i+1];
			d->model[i+1]=ip->model[i];
			if(d->model[i] < ' ' || d->model[i] > '~')
				break;
		}
		c_printf("%s",d->model);

		//check sector count
		if (block[83] & (1<<10) ){
			d->type=LBA48;
			d->sectors=*(Uint64* ) &block[100];
			c_printf("48 bit mode sectors: %d \n",*(Uint64* ) &block[100] );
			//c_printf("sectors: %d %d %d %d\n",block[103], block[102],block[101],block[100] );
		}else{
			d->type=LBA28;
			d->sectors=*(Uint32* ) &block[60];
			c_printf("sectors: %d %d\n", block[61],block[60] );
		}

		//Ask the user if we should use this drive.
		c_puts("Should we use this drive?");
		int c='j';
		while((c = c_getchar()) != 'y' && c !='n');
		if (c=='n'){
			c_puts(" Okay. Marking as invalid.");
			d->type=INVALID_DRIVE;
			disableIRQ(d);
		}else{
			_ata_primary=d;
			enableIRQ(d);
		}
		c_puts("\n");
	}
	return 0;
}


