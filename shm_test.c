#include "headers.h"
#include "umap.h"
#include "shm.h"

typedef struct {
	int x, y, z;
	char str1[256];
	char str2[256];
} shared_object;

void main(void) {
	unsigned int pid;
	unsigned int status;
	shared_object *ptr;

	char *local_str1 = "Hello from the parent\n";
	char *local_str2 = "Hello from the child\n";

	status = fork(&pid);

	if (status != SUCCESS) {
		/* failure */
		puts("shm_test: fork: ");
		putx(status);
		puts("\n");
		exit();
	} else if (pid == 0) {
		/* child */
		msleep(2500);
		puts("shm_test: child: good morning\n");

		status = shm_open("test", (void**)&ptr);

		if (status != SUCCESS) {
			puts("shm_test: shm_open: ");
			putx(status);
			puts("\n");
			exit();
		}

		putx(ptr->x); puts(" "); putx(ptr->y); puts(" "); putx(ptr->z); puts("\n");
		puts("shared str1: ");
		puts(ptr->str1);

		ptr->x++; ptr->y++; ptr->z++;

		memcpy(ptr->str2, local_str2, strlen(local_str2) + 1);

		status = shm_close("test");

		ptr->x++;

		exit();
	} else {
		/* parent */
		puts("shm_test: parent: creating shm region \"test\"\n");

		status = shm_create("test", 0x4000, SHM_WRITE, (void**)&ptr);

		if (status != SUCCESS) {
			puts("shm_test: shm_create: ");
			putx(status);
			puts("\n");
			exit();
		}

		ptr->x = 0x10;
		ptr->y = 0x20;
		ptr->z = 0x30;

		memcpy(ptr->str1, local_str1, strlen(local_str1) + 1);

		msleep(5000);

		puts("shm_test: parent: good morning\n");
		putx(ptr->x); puts(" "); putx(ptr->y); puts(" "); putx(ptr->z); puts("\n");
		puts("shared str2: ");
		puts(ptr->str2);

		exit();
	}
}

