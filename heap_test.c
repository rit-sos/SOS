#include "headers.h"

void main(void) {
	void *ptr1, *ptr2, *ptr3, *ptr4;

	puts("hello from heap_test\n");

	msleep(5000);

Alloc:

	puts("good morning\n");

	ptr1 = malloc(1000);

	if (ptr1) {
		puts("ptr1 is ");
		putx((unsigned int)ptr1);
	} else {
		puts("ptr1 is null");
	}
	
	puts("\n");

	ptr2 = malloc(1000);

	if (ptr2) {
		puts("ptr2 is ");
		putx((unsigned int)ptr2);
	} else {
		puts("ptr2 is null");
	}

	puts("\n");

	ptr3 = malloc(1000);

	if (ptr3) {
		puts("ptr3 is ");
		putx((unsigned int)ptr3);
	} else {
		puts("ptr3 is null");
	}

	puts("freeing ptr2\n");
	free(ptr2);

	ptr4 = malloc(1000);

	if (ptr4) {
		puts("ptr4 is ");
		putx((unsigned int)ptr4);
	} else {
		puts("ptr4 is null");
	}

}
