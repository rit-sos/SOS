#include "headers.h"
#include "shm.h"
#include "windowing.h"

char line[256];

int getLine( char *line, int size )
{
	if( !line ) return -1;

	int idx = -1;

	while( ++idx < size - 1 )
	{
		int c;
		if( read( SIO_FD, &c ) != SUCCESS )
		{
			windowing_print_str("\nshm_shell: read() Error");
			return -1;
		}

		line[idx] = c & 0xFF;
		windowing_print_char( line[idx] );

		if( line[idx] == '\r' || line[idx] == '\n' )
			break;
	}

	// terminate the string
	line[idx] = '\0';
	return 0;
}

int strcmp( const char *a, const char *b )
{
	if( !a || !b )
		return -1;	// lets say a less than for a null string

	while( *a != '\0' && *b != '\0' && *a == *b )
	{
		a++;
		b++;
	}
	return *a-*b;
}

const char name_prompt[] = "Name? ";
const char string_prompt[] = "String? ";
const char already_sharing[] = "A mapping is already open\n";
const char not_sharing[] = "No mapping is open\n";
const char error[] = "Error: ";
char *share;

void do_create(void) {
	int status;

	if (share) {
		windowing_print_str(already_sharing);
		return;
	}

	windowing_print_str(name_prompt);
	getLine(line, sizeof(line));

	if ((status = shm_create(line, 0x1000, SHM_WRITE, (void**)&share)) != SUCCESS) {
		windowing_print_str(error);
		putx(status);
		windowing_print_str("\n");
	} else {
		*share = '\0';
	}
}

void do_open(void) {
	int status;

	if (share) {
		windowing_print_str(already_sharing);
		return;
	}

	windowing_print_str(name_prompt);
	getLine(line, sizeof(line));

	if ((status = shm_open(line, (void**)&share)) != SUCCESS) {
		windowing_print_str(error);
		putx(status);
		windowing_print_str("\n");
	}
}

void do_close(void) {
	int status;

	if (!share) {
		windowing_print_str(not_sharing);
		return;
	}

	windowing_print_str(name_prompt);
	getLine(line, sizeof(line));

	if ((status = shm_close(line)) != SUCCESS) {
		windowing_print_str(error);
		putx(status);
		windowing_print_str("\n");
	} else {
		share = NULL;
	}
}

void do_read(void) {
	if (!share) {
		windowing_print_str(not_sharing);
		return;
	}

	windowing_print_str(share);
}

void do_write(void) {
	if (!share) {
		windowing_print_str(not_sharing);
		return;
	}

	windowing_print_str(string_prompt);
	getLine(line, sizeof(line));

	memcpy(share, line, sizeof(line));
}

void main(void)
{
	Uint pid;
	Status status;

	share = NULL;

	puts("user_shell: Starting\n");
	if( windowing_init( WIN_AUTO_FLIP ) != SUCCESS )
	{
		puts("user_shell: Error intializing window\n");
		exit();
	}

	while(1)
	{
		windowing_print_str("[shm]$ ");

		if( getLine( line, 256 ) )
		{
			windowing_print_str("Error reading line\n");
			continue;
		}

		if (strcmp(line, "help") == 0) {
			windowing_print_str("\n"
				"Commands:\n"
				"create\n"
				"open\n"
				"close\n"
				"read\n"
				"write\n");
			continue;
		} else if (strcmp(line, "create") == 0) {
			do_create();
			continue;
		} else if (strcmp(line, "open") == 0) {
			do_open();
			continue;
		} else if (strcmp(line, "read") == 0) {
			do_read();
			continue;
		} else if (strcmp(line, "close") == 0) {
			do_close();
			continue;
		} else if (strcmp(line, "write") == 0) {
			do_write();
			continue;
		} else if (!line[0]) {
			continue;
		}

		windowing_print_str("No such command\n");
	}
}
