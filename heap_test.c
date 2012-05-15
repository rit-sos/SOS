#include "headers.h"

void main(void) {
	void *ptr1, *ptr2, *ptr3, *ptr4;
	int i;

	puts("hello from heap_test\n");

	msleep(5000);

Alloc:

	puts("good morning\n");

	ptr1 = malloc(1000);

	if (ptr1) {
		puts("ptr1 is ");
		putx((unsigned int)ptr1);
		puts("\n");
	} else {
		puts("ptr1 is null\n");
	}
	
	puts("\n");

	ptr2 = malloc(1000);

	if (ptr2) {
		puts("ptr2 is ");
		putx((unsigned int)ptr2);
		puts("\n");
	} else {
		puts("ptr2 is null\n");
	}

	puts("\n");

	ptr3 = malloc(1000);

	if (ptr3) {
		puts("ptr3 is ");
		putx((unsigned int)ptr3);
		puts("\n");
	} else {
		puts("ptr3 is null\n");
	}

	puts("freeing ptr2\n");
	free(ptr2);

	ptr4 = malloc(1000);

	if (ptr4) {
		puts("ptr4 is ");
		putx((unsigned int)ptr4);
		puts("\n");
	} else {
		puts("ptr4 is null\n");
	}

	puts("sleeping\n");
	msleep(1000);

	puts("malloc 1MB\n");
	ptr2 = malloc(0x100000);
	if (ptr2) {
		puts("ptr2 is ");
		putx((unsigned int)ptr2);
		puts("\n");
	} else {
		puts("ptr2 is null\n");
	}

	puts("write to 1MB\n");
	for( i = 0; i < 0x100000; i++ )
	{
		if( (((unsigned int)ptr2 + i) & 0xFFF) == 0)
		{
			puts("writing to: ");
			putx((unsigned int)ptr2+i);
			puts("\n");
		}

		((char*)ptr2)[i] = 0xAA;
	}
	puts("write successful\n");

	if( 0 )
		goto Alloc;
}
