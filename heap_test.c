#include "headers.h"

void main(void) {
	void *ptr1, *ptr2, *ptr3;

	puts("hello from heap_test\n");

	msleep(5000);

	puts("good morning\n");

	ptr1 = malloc(1000);

	if (ptr1) {
		puts("ptr1 is ");
		putx((unsigned int)ptr1);
	} else {
		puts("ptr1 is null");
	}

	puts("\n");
}
