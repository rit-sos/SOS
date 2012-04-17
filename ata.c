
/*
**
** File:	ata.c
**
** Author:	Andrew LeCain
**
** Contributor:
**
** Description:	ata module
*/

#include "ata.h"
#include <startup.h>
#include "sio.h"


/*
**IO ports
*/

//#define DEVICE_CONTROL	0x3F6
#define DEVICE_CONTROL	0x374

#define PRIMARY 	0x01F0
#define SECONDARY  	0x0170

#define MASTER		0xA0
#define SLAVE		0xB0

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
**Commands
*/

#define IDENTIFY	0xEC


Uint8 block[1024];


void _ata_reset(){
	__outb(DEVICE_CONTROL, 0x02);
}


int _ata_identify(int slave, Uint32 base){

	//detect a floating drive
	if( __inb( base+REGULAR_STATUS ) != 0xFF){ //High if there is no drive

		//select the master/slave drive as appropriate
		__outb(base+DRIVE_HEAD_PORT, MASTER | slave<< 4);	

		for (int i = 0 ; i < 10; i++){
			//read the control register 5 times to give it time to settle
			__inb(DEVICE_CONTROL);

		}

		Uint8 cl = __inb(base+CYLINDER_LOW);
		Uint8 ch = __inb(base+CYLINDER_HIGH);
		
		volatile unsigned char status = 0;
		status =__inb(base+REGULAR_STATUS);

		__outb(base+SECTOR_COUNT,0);	
		__outb(base+LBA_LOW,0);	
		__outb(base+LBA_MID,0);	
		__outb(base+LBA_HIGH,0);	

		//send the identify command	
		__outb(base+COMMAND,IDENTIFY);


		for (int i = 0 ; i < 10; i++){
			//read the control register 5 times to give it time to settle

			status =__inb(base+REGULAR_STATUS);

		}
		status =__inb(base+REGULAR_STATUS);

		if (status == 0){
			c_puts("drive doesn't exist on specified bus\n");
			return -1;
		}else{
			c_printf("drive actually exists. Status %d cl: %d ch:%d\n",status, cl, ch);

		}

		if( status & ERR){
			c_printf("identify error.. SATA drive?\n");
			return -1;
		}

		while( (status = __inb(base+REGULAR_STATUS)) & BSY){
			//read status until busy bit clears
		}

		if (__inb(base+LBA_MID) || __inb(base+LBA_HIGH)){
			//check if the drive is non ATA. If so, end
			c_puts("Non ata!");
			Uint8 cl = __inb(base+CYLINDER_LOW);
			Uint8 ch = __inb(base+CYLINDER_HIGH);

			c_printf(" status %d cl: %d ch:%d\n",status, cl, ch);
		}
		while( (status = __inb(base+REGULAR_STATUS))){
			//read status until error or DRQ
			if (status & DRQ) break;
			if (status & ERR) break;
		}
		if(status & ERR){
			//there was an error requesting
			c_puts("read error");
			return -1;
		}
		c_puts("IDENTIFY\n");

		for(int i =0; i < 256; i++){
			block[i] = __inw(base+DATA);
		}

		if (block[83]& (1<<10) ){
			c_puts("48 bit mode supported\n");
			c_printf("sectors: %d %d %d %d",block[103], block[102],block[101],block[100] );
			}else{
				c_printf("sectors: %d %d", block[61],block[60] );
			}

		}else{
			c_puts("Floating bus!\n");
		}


		return 0;

	}


	void _ata_init(void){
		_ata_identify(0, PRIMARY); //hda
		_ata_identify(1, PRIMARY); //hdb
		_ata_identify(0, SECONDARY); //hdc
		_ata_identify(1, SECONDARY); //hdd

		while(1){};
		c_puts( " ATA" );
	}
