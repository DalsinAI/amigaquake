/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sys_acgame.c -- the system driver for AmigaOS 3.x on ACGame (from sys_null.c)
//
// The files as stdio (libnix), the time from the E-clock, the data in the
// program's drawer (PROGDIR:id1). The AmiCradle Team.

#include "quakedef.h"
#include "errno.h"
#include <sys/stat.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "sys_amiga.h"

qboolean isDedicated = false;

/*
===============================================================================

FILE IO

===============================================================================
*/

#define MAX_HANDLES             10
FILE    *sys_handles[MAX_HANDLES];

int             findhandle (void)
{
	int             i;
	
	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int filelength (FILE *f)
{
	int             pos;
	int             end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "rb");
	if (!f)
	{
		*hndl = -1;
		return -1;
	}
	sys_handles[i] = f;
	*hndl = i;
	
	return filelength(f);
}

int Sys_FileOpenWrite (char *path)
{
	FILE    *f;
	int             i;
	
	i = findhandle ();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;
	
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int     Sys_FileTime (char *path)
{
	FILE    *f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

void Sys_mkdir (char *path)
{
	mkdir (path, 0777);
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}

void Sys_DebugLog (char *file, char *fmt, ...)
{
}

void Sys_Error (char *error, ...)
{
	va_list         argptr;

	Host_Shutdown ();
	printf ("Sys_Error: ");
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;

	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
}

void Sys_Quit (void)
{
	Host_Shutdown ();
	exit (0);
}

double Sys_FloatTime (void)
{
	return sys_seconds ();
}

/* Amiga memory for the clipboard code (sys_amigaclip.c). */
void *Sys_Alloc (unsigned long size, unsigned long attr)
{
	return AllocVec (size, attr);
}

void Sys_Free (void *p)
{
	if (p)
		FreeVec (p);
}

const char *Sys_TargetName (void)
{
	return "AmigaOS 3.x 68040 (AmiCradle)";
}

long Sys_Milliseconds (void)
{
	return (long)sys_ticks ();
}

char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (void)
{
	sys_sleep (1);
}

void Sys_SendKeyEvents (void)
{
	IN_SendKeyEvents ();
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

//=============================================================================

int main (int argc, char **argv)
{
	static quakeparms_t    parms;
	double	time, oldtime, newtime;
	int		i;

	if (sys_init () != 0)
		return 20;
	/* Unbuffered, so the log is complete when something goes wrong. */
	setvbuf (stdout, NULL, _IONBF, 0);

	parms.memsize = 16*1024*1024;
	COM_InitArgv (argc, argv);
	i = COM_CheckParm ("-mem");
	if (i && i < com_argc-1)
		parms.memsize = (int)(Q_atof (com_argv[i+1]) * 1024 * 1024);
	/* The hunk from exec: libnix's malloc does not give one this large. */
	parms.membase = AllocVec (parms.memsize, MEMF_ANY);
	if (!parms.membase)
	{
		printf ("quake: not enough memory for %d MB (largest free block %lu KB)\n",
			parms.memsize / (1024*1024), (unsigned long)(AvailMem (MEMF_ANY | MEMF_LARGEST) / 1024));
		return 20;
	}
	/* The program's drawer by its full name ("Work:Games/Quake"): the game
	 * adds "/id1", and "PROGDIR:/id1" would be the drawer's parent. */
	{
		static char	basedir[MAX_OSPATH];

		if (NameFromLock (GetProgramDir (), basedir, sizeof(basedir)))
			parms.basedir = basedir;
		else
			parms.basedir = "PROGDIR:";
	}

	parms.argc = com_argc;
	parms.argv = com_argv;

	Host_Init (&parms);
	oldtime = Sys_FloatTime () - 0.1;
	while (1)
	{
		newtime = Sys_FloatTime ();
		time = newtime - oldtime;
		Host_Frame (time);
		oldtime = newtime;
	}
	return 0;
}
