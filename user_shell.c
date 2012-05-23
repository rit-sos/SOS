/*
 * File:	user_shell.c
 * Author:	James Letendre
 *
 * A basic shell to launch other applications
 */

#include "headers.h"
#include "ulib.h"
#include "user_shell.h"

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
			puts("\nuser_shell: read() Error");
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
		return -1;

	while( *a != '\0' && *b != '\0' && *a == *b )
	{
		a++;
		b++;
	}
	return *a-*b;
}

void main(void)
{
	char line[256];
	Uint pid;
	Status status;

	puts("user_shell: Starting\n");
	if( windowing_init( WIN_AUTO_FLIP ) != SUCCESS )
	{
		puts("user_shell: Error intializing window\n");
		exit();
	}

	while(1)
	{
		puts("$: ");
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
					puts("fork failed\n");
					//prt_status( "user_shell: can't fork(), %s\n", status );
				} else if( pid == 0 ) {
					status = exec( prog->id );
					puts("exec failed\n");
					//prt_status( "user_shell: can't exec(), status %s\n", status );
					exit();
				}
				done = 1;
			}
			prog++;
		}

		// not a command or empty string
		if( !done && strcmp( prog->cmd, "" ) != 0 )
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
