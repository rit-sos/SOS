/*
 * File:	user_shell.h
 * Author:	James Letendre
 *
 * User shell for interacting with the OS
 */
#ifndef USER_SHELL_H
#define USER_SHELL_H

#include "umap.h"

#define MK_COMMAND(name)	{ #name, name##_ID }
#define END_COMMAND			{ NULL, -1 }

typedef struct
{
	const char *cmd;
	int			id;
} ShellProg;

// commands
ShellProg shell_commands[] =
{
	MK_COMMAND(user_disk),
	MK_COMMAND(writer),
	MK_COMMAND(cat),
	MK_COMMAND(window_test),
	MK_COMMAND(heap_test),
	MK_COMMAND(mman_test),
	MK_COMMAND(bad_user),
	MK_COMMAND(shm_test),

	// must be at the end
	END_COMMAND
};


#endif

