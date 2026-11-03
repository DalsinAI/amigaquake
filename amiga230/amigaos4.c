/*
** amigaos4.c
**
** Quake for Amiga M68k and PowerPC
** Written by Frank Wille <frank@phoenix.owl.de>
**
** AmigaOS4 specific system calls
*/

#include <exec/memory.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/timer.h>

#include "sys.h"
#include "keys_amiga.h"


/* from sys_amiga.c */
extern int FirstTime,FirstTime2;

/* from sys_gui.c */
extern options_gui(void *);



int Sys_Init(void)
{
  struct timeval tv;

  GetSysTime(&tv);
  FirstTime2 = FirstTime = tv.tv_secs;

  return 1;
}


void Sys_Cleanup(void)
{
}


void *Sys_Alloc(unsigned long size,unsigned long attr)
{
  attr &= ~(MEMF_CHIP|MEMF_FAST);  /* AmigaOne has no Chip/Fast-RAM! */
  return AllocVec(size,attr);
}


void Sys_Free(void *p)
{
  FreeVec(p);
}


const char *Sys_TargetName(void)
{
  return "AmigaOS4";
}


double Sys_FloatTime(void)
{
  struct timeval tv;

  GetSysTime(&tv);
  return ((double)(tv.tv_secs-FirstTime) + (((double)tv.tv_micro) / 1000000.0));
}


long Sys_Milliseconds(void)
{
  struct timeval tv;

  GetSysTime(&tv);
  return (tv.tv_secs-FirstTime)*1000 + tv.tv_micro/1000;
}


int Sys_GetKeyEvents(void *port,void *msgarray,int arraysize)
{
  return GetMessagesNat((struct MsgPort *)port,
                        (struct MsgStruct *)msgarray,arraysize);
}


int Sys_OptionsGUI(void *wbmsg)
{
  return options_gui(wbmsg);
}


void *Sys_CreateExtIO(void *port,long size)
{
  if (port) {
    struct IORequest *ior;

    if (ior = AllocMem(size,MEMF_CLEAR|MEMF_PUBLIC)) {
      ior->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
      ior->io_Message.mn_Length = size;
      ior->io_Message.mn_ReplyPort = (struct MsgPort *)port;
      return ior;
    }
  }
  return NULL;
}


void Sys_DeleteExtIO(void *ioReq)
{
  struct IORequest *ior;

  if (ior = (struct IORequest *)ioReq) {
    ior->io_Message.mn_Node.ln_Succ = (void *)-1;
    ior->io_Device = (struct Device *)-1;
    FreeMem(ior,ior->io_Message.mn_Length);
  }
}
