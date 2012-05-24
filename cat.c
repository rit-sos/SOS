/*
**
** File:	user_disk.c
**
** Author:	Andrew LeCain
**
** Contributor:
**
** Description:	User routines.
*/

#include "headers.h"

#define DELAY_STD	25000

void main( void );


void main( void ) {
	int i, j;
	int fd;
	int c;
	int *data;
	
	int sectorStart;

	Status status;
	fputs(SIO_FD, "Where would you like to start reading?\n");
	sectorStart= readInt(SIO_FD);

	status = fopen(sectorStart,1,&fd);
	if( status != SUCCESS ) {
		puts("open failed!\n");
		fclose(fd);
		exit();
	}
	puts("Fd value:");
	putx((Uint32)fd);
	puts("\n");

	while(1){
		status = read(fd, &c );
		if (c == 0x04) break; //wait for EOT
		if( status != SUCCESS ) {
			puts( "cat: read failed\n");
			break;
		}
		status = write(SIO_FD, c);
	}

	fclose(fd);

	puts("===File closed===\n");

}
