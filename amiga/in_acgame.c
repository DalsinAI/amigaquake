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
// in_acgame.c -- keyboard and mouse for AmigaOS 3.x on ACGame (from in_null.c)
//
// Keys come from ACGame's raw keys as Quake's key codes, the mouse buttons
// as K_MOUSE1-3, the mouse movement as the PC version's mouse look. The
// AmiCradle Team.

#include "quakedef.h"
#include <acgame/platform.h>

/* Amiga raw key code to Quake's key code; 0 for none. */
static const int rawkeys[0x68] = {
	'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\', 0, K_INS,	/* 0x00 */
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, K_END, K_DOWNARROW, K_PGDN,	/* 0x10 */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0, 0, K_LEFTARROW, 0, K_RIGHTARROW,	/* 0x20 */
	0, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, K_DEL, K_HOME, K_UPARROW, K_PGUP,	/* 0x30 */
	K_SPACE, K_BACKSPACE, K_TAB, K_ENTER, K_ENTER, K_ESCAPE, K_DEL, 0, 0, 0, '-', 0, K_UPARROW, K_DOWNARROW, K_RIGHTARROW, K_LEFTARROW,	/* 0x40 */
	K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, 0, 0, '/', '*', '+', K_F11,	/* 0x50 */
	K_SHIFT, K_SHIFT, 0, K_CTRL, K_ALT, K_ALT, K_AMIGA, K_AMIGA	/* 0x60 */
};

/* The Amiga version's mouse and joystick settings, which host.c saves with
 * the configuration (in_amigamouse.c, in_amigajoy.c). */
cvar_t m_filter = {"m_filter","1"};
cvar_t m_speed = {"m_speed","16"};
cvar_t joy_onlypsx = {"joy_onlypsx","0",true};
cvar_t joy_advanced = {"joy_advanced","0",true};
cvar_t joy_forwardthreshold = {"joy_forwardthreshold","0.15",true};
cvar_t joy_forwardsensitivity = {"joy_forwardsensitivity","-1",true};
cvar_t joy_sidethreshold = {"joy_sidethreshold","0.15",true};
cvar_t joy_sidesensitivity = {"joy_sidesensitivity","-1",true};
cvar_t joy_pitchthreshold = {"joy_pitchthreshold","0.15",true};
cvar_t joy_pitchsensitivity = {"joy_pitchsensitivity","1",true};
cvar_t joy_yawsensitivity = {"joy_yawsensitivity","-1",true};
cvar_t joy_forward_reverse = {"joy_forward_reverse","0",true};
cvar_t joy_side_reverse = {"joy_side_reverse","0",true};
cvar_t joy_up_reverse = {"joy_up_reverse","0",true};
cvar_t joy_force0 = {"joy_force0","AUTOSENSE",true};
cvar_t joy_force1 = {"joy_force1","AUTOSENSE",true};
cvar_t joy_force2 = {"joy_force2","AUTOSENSE",true};
cvar_t joy_force3 = {"joy_force3","AUTOSENSE",true};

static uint8_t	lastkeys[0x68];
static int	mouse_dx, mouse_dy;
static int	lastbuttons;

void IN_Init (void)
{
	memset (lastkeys, 0, sizeof(lastkeys));
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

/* Read ACGame once a frame: key changes go to Key_Event, the movement is
 * kept for IN_Move. */
void IN_SendKeyEvents (void)
{
	acgame_input_state	input;
	const uint8_t	*raw;
	int		i, b;

	acgame_platform_poll (&input);
	raw = acgame_platform_rawkeys ();
	for (i = 0; raw && i < (int)sizeof(lastkeys); i++)
	{
		uint8_t down = raw[i] ? 1 : 0;
		if (down == lastkeys[i])
			continue;
		lastkeys[i] = down;
		if (rawkeys[i])
			Key_Event (rawkeys[i], down);
	}
	mouse_dx += input.mouse_dx;
	mouse_dy += input.mouse_dy;
	for (b = 0; b < 3; b++)
	{
		int bit = 1 << b;
		if ((input.mouse_buttons ^ lastbuttons) & bit)
			Key_Event (K_MOUSE1 + b, (input.mouse_buttons & bit) != 0);
	}
	lastbuttons = input.mouse_buttons;
}

void IN_Move (usercmd_t *cmd)
{
	float	mouse_x, mouse_y;

	mouse_x = mouse_dx * sensitivity.value;
	mouse_y = mouse_dy * sensitivity.value;
	mouse_dx = mouse_dy = 0;

// add mouse X/Y movement to cmd
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
