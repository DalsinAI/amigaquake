// Main Module for Input Code for Amiga Version, can be easily expanded
// to support other Init-Devices

/*
            Portability
            ===========

            This file should compile fine under all Amiga-Based Kernels,
            including WarpUP, 68k and PowerUP.

            Steffen Haeuser (MagicSN@Birdland.es.bawue.de)
*/

#include "quakedef.h"
#include "in_amiga.h"

int psxused;

static void Force_CenterView_f(void)
{
  cl.viewangles[PITCH] = 0;
}

void IN_Init (void)
{
  Cmd_AddCommand ("force_centerview", Force_CenterView_f);
  IN_InitMouse();
  psxused = IN_InitPsx();
#ifdef __amigaos4__ /* FIXME - OS4 lowlevel joysticks don't work? */
  IN_InitJoy();
#endif
}

void IN_Shutdown (void)
{
#ifdef __amigaos4__ /* FIXME - OS4 lowlevel joysticks don't work? */
  IN_ShutdownJoy();
#endif
  if (psxused) IN_ShutdownPsx();
  IN_ShutdownMouse();
}

void IN_Commands (void)
{
  IN_MouseCommands();
}

void IN_Move (usercmd_t *cmd)
{
  IN_MouseMove(cmd);
  if (psxused) IN_PsxMove(cmd);
#ifdef __amigaos4__ /* FIXME - OS4 lowlevel joysticks don't work? */
  IN_JoyMove(cmd);
#endif
}
