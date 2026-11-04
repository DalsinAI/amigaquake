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
// vid_acgame.c -- the video driver for AmigaOS 3.x on ACGame (from vid_null.c)
//
// Quake draws 320x200 in 8 bits; the picture goes on an ACGame screen (AGA
// or RTG), in the middle with a black border, through sys_amiga.c. The
// AmiCradle Team.

#include "quakedef.h"
#include "d_local.h"
#include "sys_amiga.h"

viddef_t	vid;				// global video state

/* The Amiga version's mode settings, which host.c registers (vid_amiga.c). */
cvar_t	vid_mode = {"vid_mode","0",false};
cvar_t	_vid_default_mode = {"_vid_default_mode","0",true};

#define	BASEWIDTH	320
#define	BASEHEIGHT	200

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	surfcache[256*1024];

/* What the view is drawn into under water, slime or lava before
   D_WarpScreen waves it onto vid_buffer. amiga230's r_warpbuffer is a
   pointer its vid_amiga.c allocated; without it the view went to address
   0 and overwrote the 68k's vectors and ExecBase. */
static byte	warpbuffer[BASEWIDTH*BASEHEIGHT] __attribute__((aligned(4)));

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

static byte	vid_palette[256*3];

void	VID_SetPalette (unsigned char *palette)
{
	memcpy (vid_palette, palette, sizeof(vid_palette));
	sys_palette (vid_palette);
}

void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette (palette);
}

void	VID_Init (unsigned char *palette)
{
	vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;

	d_pzbuffer = zbuffer;
	r_warpbuffer = warpbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));

	if (sys_screen (BASEWIDTH, BASEHEIGHT) != 0)
		Sys_Error ("Could not open a %dx%d screen", BASEWIDTH, BASEHEIGHT);
	sys_hide_pointer ();
	VID_SetPalette (palette);
}

void	VID_Shutdown (void)
{
}

void	VID_Update (vrect_t *rects)
{
	sys_present (vid_buffer, vid_palette, 0);
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}
