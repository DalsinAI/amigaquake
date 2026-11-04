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
// gl_vidmgl.c -- OpenUpQuake's video and input: GLQuake on MiniGL
// (minigl.library, OpenGL 1.1 on GFOpenGPU's 3D), from gl_vidlinux.c.
//
// MiniGL opens the screen and its window; the keys and the mouse are read
// from that window's IDCMP (raw keys, mouse buttons, mouse movement), as
// in_acgame.c reads ACGame's. The AmiCradle Team.

#include "quakedef.h"
#include <exec/types.h>
#include <intuition/intuition.h>
#include <devices/inputevent.h>
#include <proto/exec.h>
#include <proto/intuition.h>

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

unsigned short	d_8to16table[256];
unsigned		d_8to24table[256];
unsigned char	d_15to8table[65536];

cvar_t	vid_mode = {"vid_mode","0",false};
cvar_t	vid_redrawfull = {"vid_redrawfull","0",false};
cvar_t	vid_waitforrefresh = {"vid_waitforrefresh","0",true};

static int	scr_width, scr_height;
static float	vid_gamma = 1.0;
static struct Window	*window;
static qboolean	mgl_open, mgl_context;

float	gldepthmin, gldepthmax;
cvar_t	gl_ztrick = {"gl_ztrick","1"};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean is8bit = false;
qboolean isPermedia = false;
qboolean gl_mtexable = false;

int		texture_mode = GL_LINEAR;
int		texture_extension_number = 1;

/*-----------------------------------------------------------------------*/

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_Shutdown (void)
{
	/* Also called by Sys_Error before VID_Init has opened anything. */
	if (mgl_context)
		mglDeleteContext ();
	if (mgl_open)
		MGLTerm ();
	mgl_context = mgl_open = false;
	window = NULL;
}

void VID_ShiftPalette (unsigned char *p)
{
}

void VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		k;
	unsigned short i;
	unsigned	*table;
	int dist, bestdist;

//
// 8 8 8 encoding, in memory order R, G, B, A
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (r<<24) + (g<<16) + (b<<8) + 255;	/* big-endian: R first */
		*table++ = v;
	}
	d_8to24table[255] &= 0xffffff00;	// 255 is transparent

	for (i=0; i < (1<<15); i++) {
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,bestdist=10000*10000; v<256; v++,pal+=4) {
			r1 = (int)r - (int)pal[0];
			g1 = (int)g - (int)pal[1];
			b1 = (int)b - (int)pal[2];
			dist = (r1*r1)+(g1*g1)+(b1*b1);
			if (dist < bestdist) {
				k=v;
				bestdist = dist;
			}
		}
		d_15to8table[i]=k;
	}
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	gl_vendor = (const char *)glGetString (GL_VENDOR);
	Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = (const char *)glGetString (GL_RENDERER);
	Con_Printf ("GL_RENDERER: %s\n", gl_renderer);
	gl_version = (const char *)glGetString (GL_VERSION);
	Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = (const char *)glGetString (GL_EXTENSIONS);
	Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	glClearColor (0,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

void GL_EndRendering (void)
{
	mglSwitchDisplay ();
}

qboolean VID_Is8bit (void)
{
	return is8bit;
}

void VID_Init8bitPalette (void)
{
}

static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0)
		vid_gamma = 0.7;
	else
		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

void VID_Init (unsigned char *palette)
{
	int i;
	char	gldir[MAX_OSPATH];
	int width = 640, height = 480;

	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_redrawfull);
	Cvar_RegisterVariable (&vid_waitforrefresh);
	Cvar_RegisterVariable (&gl_ztrick);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if ((i = COM_CheckParm("-width")) != 0)
		width = atoi(com_argv[i+1]);
	if ((i = COM_CheckParm("-height")) != 0)
		height = atoi(com_argv[i+1]);

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
		vid.conwidth = 640;
	vid.conwidth &= 0xfff8;
	if (vid.conwidth < 320)
		vid.conwidth = 320;
	vid.conheight = vid.conwidth*3 / 4;
	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	if (!MGLInit ())
		Sys_Error ("minigl.library did not open (OpenGPU's MiniGL is needed)");
	mgl_open = true;
	mglChooseZBufferDepth (16);
	mglChooseNumberOfBuffers (2);
	if (!mglCreateContext (0, 0, width, height))
		Sys_Error ("MiniGL could not open a %dx%d screen", width, height);
	mgl_context = true;
	mglEnableSync (COM_CheckParm ("-vsync") ? GL_TRUE : GL_FALSE);
	mglClearPointer ();

	window = (struct Window *)mglGetInputWindowHandle ();
	if (!window)
		window = (struct Window *)mglGetWindowHandle ();
	if (window)
	{
		window->Flags |= WFLG_RMBTRAP;
		ModifyIDCMP (window, IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_DELTAMOVE);
		ReportMouse (TRUE, window);
	}

	scr_width = width;
	scr_height = height;

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

	GL_Init();

	sprintf (gldir, "%s/glquake", com_gamedir);
	Sys_mkdir (gldir);

	Check_Gamma(palette);
	VID_SetPalette(palette);

	Con_SafePrintf ("Video mode %dx%d initialized.\n", width, height);

	vid.recalc_refdef = 1;				// force a surface cache flush
}

/*-------------------------------------------------------------- input */

/* Amiga raw key code to Quake's key code; 0 for none. */
static const int rawkeys[0x68] = {
	'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\', 0, K_INS,	/* 0x00 */
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, K_END, K_DOWNARROW, K_PGDN,	/* 0x10 */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0, 0, K_LEFTARROW, 0, K_RIGHTARROW,	/* 0x20 */
	0, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, K_DEL, K_HOME, K_UPARROW, K_PGUP,	/* 0x30 */
	K_SPACE, K_BACKSPACE, K_TAB, K_ENTER, K_ENTER, K_ESCAPE, K_DEL, 0, 0, 0, '-', 0, K_UPARROW, K_DOWNARROW, K_RIGHTARROW, K_LEFTARROW,	/* 0x40 */
	K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, 0, 0, '/', '*', '+', K_F11,	/* 0x50 */
	K_SHIFT, K_SHIFT, 0, K_CTRL, K_ALT, K_ALT, 0, 0	/* 0x60 */
};

static int	mouse_dx, mouse_dy;

void IN_Init (void)
{
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

/* The window's messages: keys and mouse buttons to Key_Event, the movement
 * kept for IN_Move. */
void IN_SendKeyEvents (void)
{
	struct IntuiMessage	*msg;

	if (!window)
		return;
	while ((msg = (struct IntuiMessage *)GetMsg (window->UserPort)) != NULL)
	{
		ULONG	cls = msg->Class;
		UWORD	code = msg->Code;
		WORD	mx = msg->MouseX, my = msg->MouseY;

		ReplyMsg ((struct Message *)msg);
		if (cls == IDCMP_RAWKEY)
		{
			int raw = code & 0x7f;
			if (raw < (int)(sizeof(rawkeys) / sizeof(rawkeys[0])) && rawkeys[raw])
				Key_Event (rawkeys[raw], (code & IECODE_UP_PREFIX) == 0);
		}
		else if (cls == IDCMP_MOUSEMOVE)
		{
			mouse_dx += mx;
			mouse_dy += my;
		}
		else if (cls == IDCMP_MOUSEBUTTONS)
		{
			switch (code)
			{
			case SELECTDOWN: Key_Event (K_MOUSE1, true); break;
			case SELECTUP: Key_Event (K_MOUSE1, false); break;
			case MENUDOWN: Key_Event (K_MOUSE2, true); break;
			case MENUUP: Key_Event (K_MOUSE2, false); break;
			case MIDDLEDOWN: Key_Event (K_MOUSE3, true); break;
			case MIDDLEUP: Key_Event (K_MOUSE3, false); break;
			}
		}
	}
}

void IN_Move (usercmd_t *cmd)
{
	float	mouse_x, mouse_y;

	mouse_x = mouse_dx * sensitivity.value;
	mouse_y = mouse_dy * sensitivity.value;
	mouse_dx = mouse_dy = 0;

	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (in_mlook.state & 1)
		V_StopPitchDrift ();

	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if (cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if (cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}
	else
	{
		if ((in_strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}
}
