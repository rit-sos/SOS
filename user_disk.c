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
	int fd;
	int c, i,j,k;
	char *data;

	int sectorStart;


	Status status;
	fputs(SIO_FD, "hello from USER_DISK. Where would you like me to clobber??\n");

	data = malloc(sizeof(char)*512);

	sectorStart= readInt(SIO_FD);
	for(i =0; i< 1000;i++){

		status = fopen(sectorStart,1,&fd);
		if( status != SUCCESS ) {
			puts("open failed!");
			exit();
		}

		for(j=0;j<512;j++){
			status = read(fd, &c);
			data[j] = c;
		}

		for( j = 0; j < DELAY_STD; ++j )
			continue;
		for(j=0;j<512;j++){
			status = write(fd, (data[j]+1) );
		}
		if( status != SUCCESS ) {
			puts( "Disk user, write failed");
		}


		fclose(fd);
	}
	puts("USER_DISK done\n");

}
