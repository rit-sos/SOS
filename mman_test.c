#include "headers.h"

int somefunc() {
	int i;
	int x = 0x840ad381;
	int y = 0x08dda830;

	for (i = 0; i < 1000; i++) {
		x ^= y;
		y = (y >> 8) | (x & 0xff000000);
	}

	return y;
}

void main(void) {
	int x = 'm';
	int y = 'X';
	int *p = (int*)0x40001234; // 1GB plus change
	int c;

	puts("hi, I'm mmap_test, and my main is at ");
	putx((Uint32)&main);
	puts("\n");

	read(CIO_FD,&c);

	*p = x;
	y = *p;

	write(CIO_FD,y);

	write(CIO_FD,'~');
}
