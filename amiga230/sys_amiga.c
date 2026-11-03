/*
** sys_amiga.c
**
** Quake for Amiga M68k and PowerPC
**
** Written by Frank Wille <frank@phoenix.owl.de>
**
*/

#pragma amiga-align
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include <graphics/gfxbase.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#ifndef __amigaos4__
#include <clib/alib_protos.h>
#endif
#pragma default-align

#include <ctype.h>
#include <stdarg.h>
#include "quakedef.h"
#include "sys_gui.h"

#define MAX_HANDLES     10
#define CLOCK_PAL       3546895
#define CLOCK_NTSC      3579545


struct Device *TimerBase = NULL;
struct Library *UtilityBase = NULL;
struct Library *LowLevelBase = NULL;
struct GfxBase *GfxBase = NULL;

#ifdef POWERUP
struct Library *_UtilityBase;
struct GfxBase *_GfxBase;
struct ExecBase *_SysBase;
struct DosLibrary *_DOSBase;
#endif

#ifdef __amigaos4__
struct TimerIFace *ITimer;
struct UtilityIFace *IUtility;
struct LowLevelIFace *ILowLevel;
struct GraphicsIFace *IGraphics;
#endif

#ifndef NO_PAULA
#ifdef __PPC__
struct Library *MathIeeeDoubBasBase;
#endif
qboolean no68kFPU = false; /* for PowerUp-systems with LC040/LC060 CPU */
#endif
qboolean isDedicated = false;

static const char *_ver = "$VER: QuakeAmiga " AMIGA_VERSTRING "\r\n";
unsigned long __stack = 0x40000;  /* program requires 256k stack */

/* these are for snd_amiga.c */
long sysclock = CLOCK_PAL;
int FirstTime = 0;
int FirstTime2 = 0;

static FILE *qstdout;
static BPTR amiga_stdin,amiga_stdout;
static BPTR sys_handles[MAX_HANDLES];
static struct timerequest *timerio;
static int membase_offs = 32;
static int verbose=0, coninput=0;

#define ARGVBUFSIZE 1024
#define ARGVMAX 127
static char gui_argvbuf[ARGVBUFSIZE];
static char *gui_argv[ARGVMAX+1];
static int gui_argc;
static char *gui_argvptr;



static void cleanup(int rc)
{
  int i;

  if (host_initialized)
    Host_Shutdown();

  if (coninput)
    SetMode(amiga_stdin,0);  /* put console back into normal CON mode */

  if (host_parms.membase)
    FreeMem((byte *)host_parms.membase-membase_offs,host_parms.memsize+3*32);

  Sys_Cleanup();

  if (TimerBase) {
#ifdef __amigaos4__
    if (ITimer)
      DropInterface((struct Interface *)ITimer);
#endif
    if (!CheckIO((struct IORequest *)timerio)) {
      AbortIO((struct IORequest *)timerio);
      WaitIO((struct IORequest *)timerio);
    }
    CloseDevice((struct IORequest *)timerio);
    DeletePort(timerio->tr_node.io_Message.mn_ReplyPort);
    DeleteExtIO((struct IORequest *)timerio);
  }

#if defined(__PPC__) && !defined(NO_PAULA)
  if (no68kFPU) {
    if (MathIeeeDoubBasBase)
      CloseLibrary(MathIeeeDoubBasBase);
  }
#endif

  if (GfxBase) {
#ifdef __amigaos4__
    if (IGraphics)
      DropInterface((struct Interface *)IGraphics);
#endif
    CloseLibrary((struct Library *)GfxBase);
  }
  if (LowLevelBase) {
#ifdef __amigaos4__
    DropInterface((struct Interface *)ILowLevel);
#endif
    CloseLibrary(LowLevelBase);
  }
  if (UtilityBase) {
#ifdef __amigaos4__
    DropInterface((struct Interface *)IUtility);
#endif
    CloseLibrary(UtilityBase);
  }

  for (i=1; i<MAX_HANDLES; i++) {
    if (sys_handles[i])
      Close(sys_handles[i]);
  }
  if (qstdout != stdout) {
    Delay(100);
    fclose(qstdout);
  }

  exit(rc);
}


/*
===============================================================================

FILE IO

===============================================================================
*/

static int findhandle (void)
{
  int i;
  
  for (i=1 ; i<MAX_HANDLES ; i++)
    if (!sys_handles[i])
      return i;
  Sys_Error ("out of handles");
  return -1;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
  BPTR fh;
  struct FileInfoBlock *fib;
  int     i = -1;
  int flen = -1;
  
  *hndl = -1;
  if (fib = AllocDosObjectTags(DOS_FIB,TAG_DONE)) {
    if (fh = Open(path,MODE_OLDFILE)) {
      if (ExamineFH(fh,fib)) {
        if ((i = findhandle()) > 0) {
          sys_handles[i] = fh;
          *hndl = i;
          flen = (int)fib->fib_Size;
        }
        else
          Close(fh);
      }
      else
        Close(fh);
    }
    FreeDosObject(DOS_FIB,fib);
  }
  return flen;
}

int Sys_FileOpenWrite (char *path)
{
  BPTR fh;
  int  i;
  
  if ((i = findhandle ()) > 0) {
    if (fh = Open(path,MODE_NEWFILE)) {
      sys_handles[i] = fh;
    }
    else {
      char ebuf[80];
      Fault(IoErr(),"",ebuf,80);
      Sys_Error ("Error opening %s: %s", path, ebuf);
      i = -1;
    }
  }
  return i;
}

void Sys_FileClose (int handle)
{
  if (sys_handles[handle]) {
    Close(sys_handles[handle]);
    sys_handles[handle] = 0;
  }
}

void Sys_FileSeek (int handle, int position)
{
  Seek(sys_handles[handle],position,OFFSET_BEGINNING);
}

int Sys_FileRead (int handle, void *dest, int count)
{
  return (int)Read(sys_handles[handle],dest,(LONG)count);
}

int Sys_FileWrite (int handle, void *data, int count)
{
  return (int)Write(sys_handles[handle],data,(LONG)count);
}

int Sys_FileTime (char *path)
{
  BPTR lck;
  int  t = -1;

  if (lck = Lock(path,ACCESS_READ)) {
    t = 1;
    UnLock(lck);
  }

  return t;
}

void Sys_mkdir (char *path)
{
  BPTR lck;

  if (lck = CreateDir(path))
    UnLock(lck);
}


/*
===============================================================================

SYSTEM Functions

===============================================================================
*/

/*
================
usleep
================
*/
void usleep(unsigned long timeout)
{
  timerio->tr_node.io_Command = TR_ADDREQUEST;
  timerio->tr_time.tv_secs = timeout / 1000000;
  timerio->tr_time.tv_micro = timeout % 1000000;
  SendIO(&timerio->tr_node);
  WaitIO(&timerio->tr_node);
}


void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
}


void Sys_DebugLog(char *file, char *fmt, ...)
{
  va_list argptr; 
  static char data[1024];
  BPTR fh;

  va_start(argptr, fmt);
  vsprintf(data, fmt, argptr);
  va_end(argptr);
  if (fh = Open(file,MODE_READWRITE)) {
    Seek(fh,0,OFFSET_END);
    Write(fh,data,(LONG)strlen(data));
    Close(fh);
  }
}

void Sys_Error (char *error, ...)
{
  va_list argptr;

  fprintf(qstdout,"I_Error: ");   
  va_start(argptr,error);
  vfprintf(qstdout,error,argptr);
  va_end(argptr);
  fprintf(qstdout,"\n");

  if (cls.state == ca_connected) {
    fprintf(qstdout,"disconnecting from server...\n");
    CL_Disconnect ();   /* leave server gracefully (phx) */
  }
  cleanup(1);
}

void Sys_Printf (char *fmt, ...)
{
  if (verbose) {
    va_list         argptr;

    va_start(argptr,fmt);
    vfprintf(qstdout,fmt,argptr);
    va_end(argptr);
  }
}

void Sys_Quit (void)
{
  cleanup(0);
}

char *Sys_ConsoleInput (void)
{
  if (coninput) {
    static const char *backspace = "\b \b";
    static char text[256];
    static int len = 0;
    char ch;

    while (WaitForChar(amiga_stdin,10) == DOSTRUE) {
      Read(amiga_stdin,&ch,1);  /* note: console is in RAW mode! */
      if (ch == '\r') {
        ch = '\n';
        Write(amiga_stdout,&ch,1);
        text[len] = 0;
        len = 0;
        return text;
      }
      else if (ch == '\b') {
        if (len > 0) {
          len--;
          Write(amiga_stdout,(char *)backspace,3);
        }
      }
      else {
        if (len < sizeof(text)-1) {
          Write(amiga_stdout,&ch,1);
          text[len++] = ch;
        }
      }
    }
  }
  return NULL;
}

void Sys_Sleep (void)
{
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}


//=============================================================================


static void guiopt(char *optname,char *optfmt,...)
{
  va_list vl;
  unsigned long occup = gui_argvptr - gui_argvbuf;

  if ((gui_argc < ARGVMAX) && ((occup+strlen(optname)) < ARGVBUFSIZE)) {
    gui_argv[gui_argc++] = gui_argvptr;
    gui_argvptr += sprintf(gui_argvptr,"%s",optname);
    *gui_argvptr++ = '\0';
    if (optfmt) {
      occup = gui_argvptr - gui_argvbuf;
      if ((gui_argc < ARGVMAX) && ((occup+strlen(optfmt)+128) < ARGVBUFSIZE)) {
        gui_argv[gui_argc++] = gui_argvptr;
        va_start(vl,optfmt);
        gui_argvptr += vsprintf(gui_argvptr,optfmt,vl);
        va_end(vl);
        *gui_argvptr++ = '\0';
      }
    }
    gui_argv[gui_argc] = NULL;
  }
}


static void build_gui_args(void)
{
  int i;
  char *p;

  gui_argc = 0;
  gui_argvptr = gui_argvbuf;
  guiopt("quake",0);  /* dummy for program name */

  for (p=tt_gamedir; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      break;
    }
  }
  if (strcmp(tt_gamedir,"id1") != 0) guiopt("-game","%s",tt_gamedir);
  if (tt_dedicated) guiopt("-dedicated",0);
  if (tt_verbose) guiopt("-verbose",0);
  if (tt_coninp) guiopt("-console",0);
  if (tt_no68kfpu) guiopt("-no68kfpu",0);
  if (tt_nomouse) guiopt("-nomouse",0);
  if (tt_mem != 16) guiopt("-mem","%lu",tt_mem);
  if (tt_sound == 0) guiopt("-nosound",0);
#ifndef NO_PAULA
  else if (tt_sound == 1) guiopt("-ahi",0);
#endif
  if (tt_swapchans) guiopt("-swapchannels",0);
  if (tt_gfx) {
    guiopt("-aga",0);
    switch (tt_agamode) {
      case 0: guiopt("-ntsc",0); break;
      case 1: guiopt("-pal",0); break;
      case 2: guiopt("-dblntsc",0); break;
      case 3: guiopt("-dblpal",0); break;
    }
  }
  else {
    guiopt("-cgfx",0);
    guiopt("-modeid","0x%08lx",tt_modeid);
    if (tt_forcewpa8) guiopt("-forcewpa8",0);
  }
  if (tt_nocdaudio)
    guiopt("-nocdaudio",0);

  Sys_Printf("Command line arguments:\n");
  for (i=0; i<gui_argc; i++)
    Sys_Printf("%s ",gui_argv[i]);
  Sys_Printf("\n--------------------------------\n");
}


int main (int argc, char *argv[])
{
  extern int vcrFile;
  extern int recording;
  quakeparms_t parms;
  double time, oldtime, newtime;
  struct MsgPort *timerport;
  char cwd[128];
  int i;

#ifdef __amigaos4__
  if ((UtilityBase = OpenLibrary("utility.library",36)) == NULL ||
      (LowLevelBase = OpenLibrary("lowlevel.library",40)) == NULL)
    Sys_Error("Cannot open utility/lowlevel. OS4.0 required");
  IUtility = (struct UtilityIFace *)
              GetInterface((struct Library *)UtilityBase,"main",1,0);
  ILowLevel = (struct LowLevelIFace *)
               GetInterface((struct Library *)LowLevelBase,"main",1,0);
  if (IUtility==NULL || ILowLevel==NULL)
    Sys_Error("Cannot get utility/lowlevel interfaces. OS4.0 required");
  if (!(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",36)))
    Sys_Error("Can't open graphics.library V36");
  if (!(IGraphics = (struct GraphicsIFace *)
        GetInterface((struct Library *)GfxBase,"main",1,0)))
    Sys_Error("Can't get graphics interface");
#else
  if (!(UtilityBase = OpenLibrary("utility.library",36)))
    Sys_Error("Can't open utility.library V36");
  if (!(GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",36)))
    Sys_Error("Can't open graphics.library V36");
  LowLevelBase = OpenLibrary("lowlevel.library",40);
#ifdef POWERUP
  /* define lib bases for 68k calls */
  _SysBase = SysBase;
  _DOSBase = DOSBase;
  _GfxBase = GfxBase;
  _UtilityBase = UtilityBase;
#endif
#endif

  if (argc == 0) {  /* started from WB */
    int rc;

    if (!(qstdout = fopen("CON:0/0/640/200/Quake output/CLOSE","w")))
      exit(1);
    rc = Sys_OptionsGUI(argv);
    if (rc == 0)
      Sys_Error("Options GUI failed! Try to start from shell");
    else if (rc < 0)
      Sys_Error("Options GUI was aborted!");
    build_gui_args();
    COM_InitArgv(gui_argc,gui_argv);
  }
  else {
    qstdout = stdout;
    COM_InitArgv(argc, argv);    /* parse command string */
  }

  memset(&parms,0,sizeof(parms));
  parms.memsize = 16*1024*1024;  /* 16MB is default */
  parms.argc = com_argc;
  parms.argv = com_argv;
  host_parms.membase = parms.cachedir = NULL;

  /* Amiga Init */
  amiga_stdin = Input();
  amiga_stdout = Output();

#if defined(__PPC__) && !defined(NO_PAULA)
  no68kFPU = COM_CheckParm("-no68kfpu") != 0;
  if (no68kFPU) {
     if (!(MathIeeeDoubBasBase = OpenLibrary("mathieeedoubbas.library",36)))
       Sys_Error("mathieeedoubbas.library is missing");
  }
#endif

  isDedicated = COM_CheckParm("-dedicated") != 0;
  verbose = COM_CheckParm("-verbose");
  coninput = COM_CheckParm("-console");
  if (coninput)
    SetMode(amiga_stdin,1);  /* put console into RAW mode */

  /* open timer.device */
  if (timerport = CreatePort(NULL,0)) {
    if (timerio = (struct timerequest *)
                   CreateExtIO(timerport,sizeof(struct timerequest))) {
      if (OpenDevice(TIMERNAME,UNIT_MICROHZ,
                     (struct IORequest *)timerio,0) == 0) {
        TimerBase = timerio->tr_node.io_Device;
#ifdef __amigaos4__
        if (TimerBase) {
          ITimer = (struct TimerIFace *)
                    GetInterface((struct Library *)TimerBase,"main",1,0);
          if (ITimer == NULL)
            Sys_Error("Can't get timer interface");
        }
#endif
      }
      else {
        DeleteExtIO((struct IORequest *)timerio);
        DeletePort(timerport);
      }
    }
    else
      DeletePort(timerport);
  }

  if (!TimerBase)
    Sys_Error("Can't open timer.device");
  usleep(1);  /* don't delete, otherwise we can't do timer.device cleanup */

  Sys_Init();  /* OS specific initializations */

  if (i = COM_CheckParm("-mem"))
    parms.memsize = (int)(Q_atof(com_argv[i+1]) * 1024 * 1024);

  /* alloc 16-byte aligned quake memory */
  parms.memsize = (parms.memsize+15)&~15;
  if ((parms.membase = AllocMem((ULONG)parms.memsize,MEMF_ANY)) == NULL)
    Sys_Error("Can't allocate %ld bytes\n", parms.memsize);
  if ((ULONG)parms.membase & 8)
    membase_offs = 40;  /* guarantee 16-byte aligment */
  else
    membase_offs = 32;
  parms.membase = (char *)parms.membase + membase_offs;
  parms.memsize -= 3*32;

  /* get name of current directory */
  GetCurrentDirName(cwd,128);
  parms.basedir = cwd;

#ifndef NO_PAULA
  /* set the clock constant */
  if (GfxBase->DisplayFlags & REALLY_PAL)
    sysclock = CLOCK_PAL;
  else
    sysclock = CLOCK_NTSC;
#endif

  Host_Init (&parms);
  oldtime = Sys_FloatTime () - 0.1;

  while (1) {
    newtime = Sys_FloatTime ();
    time = newtime - oldtime;

    if (cls.state == ca_dedicated) {
      if (time < sys_ticrate.value && (vcrFile == -1 || recording)) {
        usleep(1);
        continue;
      }
      time = sys_ticrate.value;
    }

    if (time > sys_ticrate.value*2)
      oldtime = newtime;
    else
      oldtime += time;

    Host_Frame (time);
  }

  return 0;
}
