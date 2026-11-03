/*
** powerup.c
**
** Quake for Amiga M68k and PowerPC
** Written by Frank Wille <frank@phoenix.owl.de>
**
** PowerUp specific system calls
*/

#pragma amiga-align
#include <exec/memory.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <powerup/ppclib/time.h>
#include <powerup/gcclib/powerup_protos.h>
#pragma default-align

#include "sys.h"
#include "keys_amiga.h"



struct ciatimer {
  void *resource;
  void *interrupt;
  unsigned long control;
  unsigned long low;
  unsigned long high;
  unsigned char stopmask;
  unsigned char startmask;
  unsigned short icrbit;
  unsigned long eclock;
};

static char gap1[32];
static struct ciatimer ctim;
static char gap2[32];
static ULONG TicksPerSec[2];


/* from sys_amiga.c */
extern int FirstTime,FirstTime2;

/* from powerup68k.s */
extern int ReserveCIA(struct ciatimer *);  /* in a0 */
extern void FreeCIA(struct ciatimer *);  /* in a0 */

/* from powerupPPC.s */
extern unsigned long MeasureBusClock(struct ciatimer *);
extern unsigned long CorrectBusClock(unsigned long);
extern ULONG PPC_mftbu(void);
extern ULONG PPC_mftbl(void);
extern void PPCDivu64p(int *,int *);
extern void PPCModu64p(int *,int *);
extern void PPCMulu64p(int *,int *);



static void InitSysTimePPC(void)
{
  static ULONG one[2] = { 1,0 };
  struct Caos MyCaos;
  unsigned long clk;

  PPCCacheFlush(&ctim,sizeof(struct ciatimer));
  MyCaos.caos_Un.Function = (APTR)ReserveCIA;
  MyCaos.a0 = (ULONG)&ctim;
  MyCaos.M68kCacheMode = IF_CACHEFLUSHALL;
  MyCaos.PPCCacheMode = IF_CACHEFLUSHALL;
  if (PPCCallM68k(&MyCaos)) {  /* ReserveCIA() */
    /* This should run in supervisor mode, for a better precision, */
    /* but it seems sufficient for Quake... */
    clk = MeasureBusClock(&ctim);
    MyCaos.caos_Un.Function = (APTR)FreeCIA;
    MyCaos.a0 = (ULONG)&ctim;
    MyCaos.M68kCacheMode = IF_CACHEFLUSHALL;
    MyCaos.PPCCacheMode = IF_CACHEFLUSHALL;
    PPCCallM68k(&MyCaos);  /* FreeCIA() */
  }
  TicksPerSec[0] = 0;
  TicksPerSec[1] = CorrectBusClock(clk) >> 2; /* PPC timer counts every 4th */
}


static void GetSysTimePPC(struct timeval *tv)
{
  static ULONG mill[2] = { 0,1000000 };
  ULONG r[2],secs[2];

  do {
    r[0] = PPC_mftbu();
    r[1] = PPC_mftbl();
  } while (r[0] != PPC_mftbu());
  secs[0] = r[0];
  secs[1] = r[1];
  PPCDivu64p((int *)secs,(int *)TicksPerSec);
  tv->tv_secs = secs[1];
  PPCModu64p((int *)r,(int *)TicksPerSec);
  PPCMulu64p((int *)r,(int *)mill);
  PPCDivu64p((int *)r,(int *)TicksPerSec);
  tv->tv_micro = r[1];
}


int Sys_Init(void)
{
  struct timeval tv;

  /* init GetSysTimePPC() emulation for PowerUp */
  InitSysTimePPC();

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
  return PPCAllocVec(size,attr);
}


void Sys_Free(void *p)
{
  PPCFreeVec(p);
}


const char *Sys_TargetName(void)
{
  return "PowerUp";
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
  extern int _GetMessages68k();
  struct Caos MyCaos;

  MyCaos.caos_Un.Function = (APTR)_GetMessages68k;
  MyCaos.a0 = (ULONG)msgarray;
  MyCaos.a1 = (ULONG)port;
  MyCaos.d0 = arraysize;
  MyCaos.M68kCacheMode = IF_CACHEFLUSHALL;
  MyCaos.PPCCacheMode = IF_CACHEFLUSHALL;
  return (int)PPCCallM68k(&MyCaos);
}


int Sys_OptionsGUI(void *wbmsg)
{
  extern int _options_gui();
  struct Caos MyCaos;

  MyCaos.caos_Un.Function = (APTR)_options_gui;
  MyCaos.a0 = (ULONG)wbmsg;
  MyCaos.M68kCacheMode = IF_CACHEFLUSHALL;
  MyCaos.PPCCacheMode = IF_CACHEFLUSHALL;
  return (int)PPCCallM68k(&MyCaos);
}
