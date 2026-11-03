/*
** warpos.c
**
** Quake for Amiga M68k and PowerPC
** Written by Frank Wille <frank@phoenix.owl.de>
**
** WarpOS specific system calls
*/

#pragma amiga-align
#include <exec/memory.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <powerpc/powerpc.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/powerpc.h>
#pragma default-align

#include "sys.h"
#include "keys_amiga.h"

extern int FirstTime,FirstTime2;


int Sys_Init(void)
{
  struct timeval tv;

  GetSysTimePPC(&tv);
  FirstTime = tv.tv_secs;
  GetSysTime(&tv);
  FirstTime2 = tv.tv_secs;

  return 1;
}


void Sys_Cleanup(void)
{
}


void *Sys_Alloc(unsigned long size,unsigned long attr)
{
  return AllocVecPPC(size,attr,0);
}


void Sys_Free(void *p)
{
  FreeVecPPC(p);
}


const char *Sys_TargetName(void)
{
  return "WarpOS";
}


double Sys_FloatTime (void)
{
  struct timeval tv;

  GetSysTimePPC(&tv);
  return ((double)(tv.tv_secs-FirstTime) + (((double)tv.tv_micro) / 1000000.0));
}


long Sys_Milliseconds(void)
{
  struct timeval tv;

  GetSysTimePPC(&tv);
  return (tv.tv_secs-FirstTime)*1000 + tv.tv_micro/1000;
}


int Sys_GetKeyEvents(void *port,void *msgarray,int arraysize)
{
  extern int GetMessages68k();
  struct PPCArgs args;

  args.PP_Code = (APTR)GetMessages68k;
  args.PP_Offset = 0;
  args.PP_Flags = 0;
  args.PP_Stack = NULL;
  args.PP_StackSize = 0;
  args.PP_Regs[PPREG_A0] = (ULONG)msgarray;
  args.PP_Regs[PPREG_A1] = (ULONG)port;
  args.PP_Regs[PPREG_D0] = arraysize;
  Run68K(&args);
  return args.PP_Regs[PPREG_D0];
}


int Sys_OptionsGUI(void *wbmsg)
{
  extern int options_gui();
  struct PPCArgs args;

  args.PP_Code = (APTR)options_gui;
  args.PP_Offset = 0;
  args.PP_Flags = 0;
  args.PP_Stack = NULL;
  args.PP_StackSize = 0;
  args.PP_Regs[PPREG_A0] = (ULONG)wbmsg;
  Run68K(&args);
  return args.PP_Regs[PPREG_D0];
}
