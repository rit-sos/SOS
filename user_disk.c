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

	data = malloc(sizeof(int) *512 *3);
	if(data == NULL){
		puts("error mallocing..");
		exit();
	}

	Status status;
	puts("hello from USER_DISK\n");
	status = write(CIO_FD, 'D' );
	if( status != SUCCESS ) {
		//prt_status( "User A, write 1 status %s\n", status );
	}
	status = fopen(0,5,&fd);
	if( status != SUCCESS ) {
		puts("open failed!");
		exit();
	}
	puts("Fd value:");
	putx((Uint32)fd);


	for(i=0;i<512*2;i++){
		status = read(fd, &c);
		data[i] = c;
		status = write(SIO_FD, c);
	}


	status = write(SIO_FD, '\n');

	for( i = 0; i < 512 + 256 ; ++i ) {
		for( j = 0; j < DELAY_STD; ++j )
			continue;
		if( status != SUCCESS ) {
			//prt_status( "Disk user, read status %s\n", status );
		}
		status = write(fd, data[i] +1);
		if( status != SUCCESS ) {
			puts( "Disk user, write failed");
		}

	}
	read(CIO_FD,&c);

	for(i=0;i<512*2-1;i++){
		status = read(fd,&c);
		status = write(SIO_FD, c);
	}

	read(CIO_FD,&c);

	fclose(fd);
	puts("===File closed===");
	//status = read(fd,&c);
	if( status != SUCCESS ) {
		puts("reading from closed fd failed!\n");
	}

	status = fopen(0,5,&fd);
	if( status != SUCCESS ) {
		puts("reopening closed fd failed!\n");
		exit();
	}
	puts("reopening closed fd success!\n");
	for( i = 0; i < 512 *3 ; ++i ) {
		for( j = 0; j < DELAY_STD; ++j )
			continue;
		status = read(fd,&c);
		status = write(SIO_FD, c);
	}

	puts("waiting\n");
	for( i = 0; i < 20 ; ++i ) {
		status = read(CIO_FD,&c);
		status = write(CIO_FD, c);
	}
	puts("USER_DISK done\n");

}
