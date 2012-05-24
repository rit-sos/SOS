/*
 * File:	user_shell.c
 * Author:	James Letendre
 * Contributors: Corey Bloodstein (added the c_io half of shm_test)
 *
 * A basic shell to launch other applications
 */

#include "headers.h"
#include "ulib.h"
#include "shm.h"
#include "user_shell.h"

char line[256];

int getLine( char *line, int size )
{
	if( !line ) return -1;

	int idx = -1;

	while( ++idx < size - 1 )
	{
		int c;
		if( read( CIO_FD, &c ) != SUCCESS )
		{
			windowing_print_str("\nuser_shell: read() Error");
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
	windowing_print_str("\n");
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
		windowing_print_str("$: ");

		if( getLine( line, 256 ) )
		{
			windowing_print_str("Error reading line\n");
			continue;
		}

		ShellProg *prog = shell_commands;

		if( strcmp( line, "help" ) == 0 )
		{
			// print out list of programs
			windowing_print_str("Programs:\n");

			while( prog->cmd != NULL )
			{
				windowing_print_str(prog->cmd);
				windowing_print_str("\n");
				prog++;
			}
			// don't look for "help" program

			windowing_print_str("\n"
				"shm commands:\n"
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
		}
		// have a command
		int done = 0;

		while( prog->cmd != NULL && !done )
		{
			if( strcmp( prog->cmd, line ) == 0 )
			{
				windowing_print_str("user_shell: launching command: \"");
				windowing_print_str(prog->cmd);
				windowing_print_str("\"\n");

				// found our program
				status = fork( &pid );
				if( status != SUCCESS ) {
					windowing_print_str("fork failed\n");
					//prt_status( "user_shell: can't fork(), %s\n", status );
				} else if( pid == 0 ) {
					status = exec( prog->id );
					windowing_print_str("exec failed\n");
					//prt_status( "user_shell: can't exec(), status %s\n", status );
					exit();
				}
				done = 1;
			}
			prog++;
		}

		// not a command or empty string
		if( !done && (strcmp( line, "" ) != 0) )
		{
			// no such command
			windowing_print_str("Error no such command: \"");
			windowing_print_str(line);
			windowing_print_str("\"\n");
		}
	}

	windowing_cleanup();
	exit();
}
