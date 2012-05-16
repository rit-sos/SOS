#include "headers.h"

int somefunc(void) {
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
	int *numbers, *recv;
	int sum;
	int i;
	unsigned int status;

	numbers = malloc(20000);

	if (!numbers) {
		puts("mman_test: malloc returned null\n");
	}

	sum = 0;

	for (i = 0; i < 5000; i++) {
		numbers[i] = i;
		sum += i;
	}

	status = sys_sum(numbers, 5000);

	if (status != 0) {
		puts("mman_test: sys_sum: ");
		putx(status);
		puts("\n");
		exit();
	}

	puts("The sum is ");
	putx(sum);
	puts("\n");

	recv = malloc(20000);

	if (!recv) {
		puts("mman_test: second malloc returned null\n");
	}

	status = set_test(recv, 5000);

	if (status != 0) { 
		puts("mman_test: set_test: ");
		putx(status);
		puts("\n");
		exit();
	}

	for (i = 0; i < 5000; i++) {
		if (recv[i] != i) {
			puts("mman_test: set_test: recv[");
			putx(i);
			puts("] = ");
			putx(recv[i]);
			puts("\n");
			exit();
		}
	}

	puts("mman_test: recv ok\n");

	free(numbers);
	free(recv);

	exit();

}
