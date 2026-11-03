/*
** sys_gui.c
**
** Quake for Amiga M68k and PowerPC
**
** Written by Frank Wille <frank@phoenix.owl.de>
**
*/

#pragma amiga-align
#include <dos/dos.h>
#include <graphics/displayinfo.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <proto/icon.h>
#include <proto/utility.h>
#ifndef __amigaos4__
#include <clib/alib_protos.h>
#endif
#pragma default-align

#include <stdarg.h>
#include "quakedef.h"
#include "SDI_hook.h"

#ifdef __PPC__
#if !defined(__MORPHOS__) && !defined(__amigaos4__)
#define POWERUPBOARD 1
#endif
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

struct Library *MUIMasterBase = NULL;
struct Library *IconBase = NULL;
#ifdef POWERUP
struct IntuitionBase *IntuitionBase = NULL;
#else
extern struct IntuitionBase *IntuitionBase;
#endif

#ifdef __amigaos4__
struct MUIMasterIFace *IMUIMaster;
struct IconIFace *IIcon;
extern struct IntuitionIFace *IIntuition;
#endif

enum { ID_SAVE=0x8000, ID_USE, ID_CYCLEGFX, ID_CDAUDIO };

/* default settings: */
char tt_gamedir[128];            /* "id1" */
ULONG tt_dedicated = FALSE;
ULONG tt_verbose = FALSE;
ULONG tt_coninp = FALSE;
ULONG tt_no68kfpu = FALSE;
ULONG tt_nomouse = FALSE;
ULONG tt_mem = 16;               /* 16MB is default */
#ifdef NO_PAULA
ULONG tt_sound = 1;              /* AHI */
#else
ULONG tt_sound = 2;              /* Paula */
#endif
ULONG tt_swapchans = FALSE;
ULONG tt_gfx = 0;                /* CyberGfx */
ULONG tt_modeid = 0;             /* CGfx screen mode id */
ULONG tt_forcewpa8 = FALSE;
ULONG tt_agamode = 0;            /* NTSC */
ULONG tt_nocdaudio = TRUE;
static char tt_cddev[80];        /* scsi.device */
static ULONG tt_cdunit = 0;

static Object *app,*win,*gamedirasl;
static Object *dedictoggle,*verbtoggle,*coninptoggle,*no68kfputoggle;
static Object *nomousetoggle,*memslider;
static Object *sndcycle,*swapchtoggle;
static Object *gfxcycle,*cgxgroup,*wpa8toggle,*agagroup;
static Object *usebutton,*savebutton,*quitbutton;
static Object *nocdatoggle,*cdagroup,*cddevstr,*cdunitnum;

static char cgxmodename[64];

static char *Pages[] = {
  "System", "Graphics", "Sound", "CD-Audio", NULL
};

static char *gfxtype[] = {
  "CyberGfx", "AGA", NULL
};

static char *agamodes[] = {
  "NTSC", "PAL", "DblNTSC", "DblPAL", NULL
};

static char *soundtype[] = {
  "None",
  "AHI",
#ifndef NO_PAULA
  "Paula",
#endif
  NULL
};



static int vsprintf_mini(char *s,const char *format,va_list args)
{
  int cnt = 0;
  char c;
  char *cp;
  unsigned long ul;
  char buf[32];

  while (c = *format++) {
    if (c=='%' && *format!='\0') {
      c = *format++;
      switch (c) {
        case 's':
          cp = va_arg(args,char *);
          while (*cp) {
            *s++ = *cp++;
            cnt++;
          }
          break;
        case 'u':
          ul = va_arg(args,unsigned long);
          cp = buf;
          do {
            *cp++ = '0' + ul%10;
            ul /= 10;
          }
          while (ul);
          for (--cp; cp>=buf; cp--) {
            *s++ = *cp;
            cnt++;
          }
          break;
      }
    }
    else {
      *s++ = c;
      cnt++;
    }
  }
  return cnt;
}


static int sprintf_mini(char *s,const char *format,...)
{
  va_list args;
  int ret;

  va_start(args,format);
  ret = vsprintf_mini(s,format,args);
  va_end(args);
  return ret;
}


static int sscanf_mini(char *s,long *res)
{
  int neg = 0;
  int ok = 0;
  long v = 0;

  while (*s<=' ' && *s!=0)
    s++;
  if (*s == '-') {
    neg = 1;
    s++;
  }
  else if (*s == '+')
    s++;
  while (*s>='0' && *s<='9') {
    ok = 1;
    v *= 10;
    v += (long)(*s++ - '0');
  }
  if (ok)
    *res = neg ? -v : v;
  return ok;
}


static void write_env(int saveglob)
{
  char buf[80];

  if (!tt_nocdaudio) {
    SetVar("quake1/cd_device",tt_cddev,-1,
           (saveglob?GVF_GLOBAL_ONLY:0)|GVF_SAVE_VAR);
    sprintf_mini(buf,"%u",tt_cdunit);
    SetVar("quake1/cd_unit",buf,-1,
           (saveglob?GVF_GLOBAL_ONLY:0)|GVF_SAVE_VAR);
  }
}


static void get_numtooltype(struct DiskObject *dob,char *ttname,ULONG *var)
{
  STRPTR val;
  long num;

  if (val = FindToolType(dob->do_ToolTypes,ttname)) {
    if (sscanf_mini(val,&num))
      *var = (ULONG)num;
  }
}


static void read_tooltypes(struct DiskObject *dob)
{
  STRPTR val;
  long num;
  int i;
  struct NameInfo ni;
  char buf[80];

  if (val = FindToolType(dob->do_ToolTypes,"GAMEDIR"))
    strcpy(tt_gamedir,val);
  else
    strcpy(tt_gamedir,"id1/");

  get_numtooltype(dob,"DEDICATED",&tt_dedicated);
  get_numtooltype(dob,"VERBOSE",&tt_verbose);
  get_numtooltype(dob,"CONINPUT",&tt_coninp);
#ifdef POWERUPBOARD
  get_numtooltype(dob,"NO68KFPU",&tt_no68kfpu);
#endif
  get_numtooltype(dob,"NOMOUSE",&tt_nomouse);
  get_numtooltype(dob,"MEM",&tt_mem);

  if (val = FindToolType(dob->do_ToolTypes,"SOUND")) {
    if (!Stricmp(val,soundtype[0]))
      tt_sound = 0;
    else if (!Stricmp(val,soundtype[1]))
      tt_sound = 1;
#ifndef NO_PAULA
    else if (!Stricmp(val,soundtype[2]))
      tt_sound = 2;
#endif
  }
  get_numtooltype(dob,"SWAPCH",&tt_swapchans);

  if (val = FindToolType(dob->do_ToolTypes,"GFXTYPE")) {
    if (!Stricmp(val,gfxtype[0]))
      tt_gfx = 0;
    else if (!Stricmp(val,gfxtype[1]))
      tt_gfx = 1;
  }
  get_numtooltype(dob,"MODEID",&tt_modeid);
  if (!tt_modeid)
    tt_modeid = BestModeID(BIDTAG_NominalWidth,640,
                           BIDTAG_NominalHeight,480,
                           BIDTAG_Depth,8,
                           BIDTAG_DIPFMustNotHave,DIPF_IS_AA,
                           TAG_DONE);
  if (GetDisplayInfoData(0,(UBYTE *)&ni,sizeof(ni),DTAG_NAME,tt_modeid) > 0)
    sprintf_mini(cgxmodename,"%s        ",ni.Name);  /* @@@ hmm... */
  get_numtooltype(dob,"FORCEWPA8",&tt_forcewpa8);

  if (val = FindToolType(dob->do_ToolTypes,"AGAMODE")) {
    for (i=0; i<4; i++) {
      if (!Stricmp(val,agamodes[i])) {
        tt_agamode = i;
        break;
      }
    }
  }

  get_numtooltype(dob,"NOCDAUDIO",&tt_nocdaudio);

  if (GetVar("quake1/cd_device",buf,79,0) > 0)
    strcpy(tt_cddev,buf);
  else
    strcpy(tt_cddev,"scsi.device");

  if (GetVar("quake1/cd_unit",buf,79,0) > 0) {
    if (sscanf_mini(buf,&num))
      tt_cdunit = num;
  }
}


static char *newtooltype(STRPTR **ttptr,char *p,char *ttfmt,...)
{
  va_list vl;

  *(*ttptr)++ = p;
  *(*ttptr) = NULL;
  va_start(vl,ttfmt);
  p += vsprintf_mini(p,ttfmt,vl);
  va_end(vl);
  *p++ = '\0';
  return p;
}


static void write_tooltypes(STRPTR name,struct DiskObject *dob)
{
  char buf[1024];
  STRPTR tooltypes[64];
  char *p;
  STRPTR *ttptr;

  p = &buf[0];
  ttptr = &tooltypes[0];

  p = newtooltype(&ttptr,p,"GAMEDIR=%s",tt_gamedir);
  p = newtooltype(&ttptr,p,"DEDICATED=%u",tt_dedicated);
  p = newtooltype(&ttptr,p,"VERBOSE=%u",tt_verbose);
  p = newtooltype(&ttptr,p,"CONINPUT=%u",tt_coninp);
#ifdef POWERUPBOARD
  p = newtooltype(&ttptr,p,"NO68KFPU=%u",tt_no68kfpu);
#endif
  p = newtooltype(&ttptr,p,"NOMOUSE=%u",tt_nomouse);
  p = newtooltype(&ttptr,p,"MEM=%u",tt_mem);
  p = newtooltype(&ttptr,p,"SOUND=%s",soundtype[tt_sound]);
  p = newtooltype(&ttptr,p,"SWAPCH=%u",tt_swapchans);
  p = newtooltype(&ttptr,p,"GFXTYPE=%s",gfxtype[tt_gfx]);
  p = newtooltype(&ttptr,p,"MODEID=%u",tt_modeid);
  p = newtooltype(&ttptr,p,"FORCEWPA8=%u",tt_forcewpa8);
  p = newtooltype(&ttptr,p,"AGAMODE=%s",agamodes[tt_agamode]);
  p = newtooltype(&ttptr,p,"NOCDAUDIO=%u",tt_nocdaudio);

  dob->do_ToolTypes = &tooltypes[0];
  PutDiskObject(name,dob);
  write_env(TRUE);
}


static void read_settings(void)
{
  /* read from objects which were not handled during main loop */
  char *p;

  get(gamedirasl,MUIA_String_Contents,&p);
  strcpy(tt_gamedir,p);
  get(dedictoggle,MUIA_Selected,&tt_dedicated);
  get(verbtoggle,MUIA_Selected,&tt_verbose);
  get(coninptoggle,MUIA_Selected,&tt_coninp);
#ifdef POWERUPBOARD
  get(no68kfputoggle,MUIA_Selected,&tt_no68kfpu);
#endif
  get(nomousetoggle,MUIA_Selected,&tt_nomouse);
  get(memslider,MUIA_Numeric_Value,&tt_mem);
  get(sndcycle,MUIA_Cycle_Active,&tt_sound);
  get(swapchtoggle,MUIA_Selected,&tt_swapchans);
  get(wpa8toggle,MUIA_Selected,&tt_forcewpa8);
  get(cddevstr,MUIA_String_Contents,&p);
  strcpy(tt_cddev,p);
  get(cdunitnum,MUIA_Numeric_Value,&tt_cdunit);
}


HOOKPROTONH(scrmode_init,ULONG,
            Object *popasl,struct TagItem *tags)
{
#if 0 /*@@@ ASLSM_Initial... tags don't seem to work and taglist is empty,
        opposing to the mui_popasl autodocs? why??? */
  static struct TagItem modetags[3] = {
    ASLSM_InitialDisplayWidth,0,
    ASLSM_InitialDisplayHeight,0,
    TAG_DONE,0
  };
  struct TagItem *lasttag=tags, *ti;
  char *modestr;

  get(popasl,MUIA_Text_Contents,&modestr);
  if (sscanf(modestr,"%lux%lu",
             &modetags[0].ti_Data,&modetags[1].ti_Data) == 2) {
    while (ti = NextTagItem(&tags))
      lasttag = ti;
    lasttag++;
    lasttag->ti_Tag = TAG_MORE;
    lasttag->ti_Data = (ULONG)modetags;
  }
#endif
  return TRUE;
}
MakeStaticHook(scrmodeini_hook,scrmode_init);


HOOKPROTONH(scrmode_select,ULONG,
            Object *popasl,struct ScreenModeRequester *smr)
{
  struct NameInfo ni;

  if (GetDisplayInfoData(0,(UBYTE *)&ni,sizeof(ni),
                         DTAG_NAME,smr->sm_DisplayID) > 0) {
    strcpy(cgxmodename,ni.Name);
    set(popasl,MUIA_Text_Contents,cgxmodename);
    tt_modeid = smr->sm_DisplayID;
  }
  return TRUE;
}
MakeStaticHook(scrmodesel_hook,scrmode_select);


static Object *create_mui_app(void)
{
  Object *app = ApplicationObject,
    MUIA_Application_Title, "Quake Options GUI",
    MUIA_Application_Version, "$VER: Quake Amiga " AMIGA_VERSTRING,
    MUIA_Application_Copyright, "Copyright ©2005 Frank Wille",
    MUIA_Application_Author, "Frank Wille",
    MUIA_Application_Base, "QUAKEGUI",

    SubWindow, win = WindowObject,
      MUIA_Window_Title, "Quake Options",
      MUIA_Window_ID, MAKE_ID('Q','G','U','I'),
      WindowContents, VGroup,

        Child, ColGroup(2),
          Child, Label2("Game Dir:"),
          Child, PopaslObject,
            MUIA_Popasl_Type, ASL_FileRequest,
            MUIA_Popstring_String, gamedirasl = StringObject, StringFrame,
              MUIA_String_Contents, tt_gamedir,
              MUIA_String_MaxLen, 64,
              End,
            MUIA_Popstring_Button, PopButton(MUII_PopDrawer),
            ASLFR_DrawersOnly, TRUE,
            End,
          End,

        Child, RegisterGroup(Pages),
          MUIA_Register_Frame, TRUE,

          Child, VGroup,
            Child, ColGroup(2),
              Child, dedictoggle = CheckMark(tt_dedicated),
              Child, LLabel1("Dedicated server"),
              Child, verbtoggle = CheckMark(tt_verbose),
              Child, LLabel1("Verbose"),
              Child, coninptoggle = CheckMark(tt_coninp),
              Child, LLabel1("Console input"),
#ifdef POWERUPBOARD
              Child, no68kfputoggle = CheckMark(tt_no68kfpu),
              Child, LLabel1("Do not use 68k FPU"),
#endif
              Child, nomousetoggle = CheckMark(tt_nomouse),
              Child, LLabel1("Disable mouse control"),
              End,
            Child, ColGroup(2),
              Child, LLabel1("Amount of memory to allocate"),
              Child, memslider = SliderObject,
                MUIA_Numeric_Min, 6,
                MUIA_Numeric_Max, 64,
                MUIA_Numeric_Value, tt_mem,
                MUIA_Numeric_Format, "%lu MB",
                MUIA_FixWidthTxt,"XXXXXXXXXXXXXXXX",
                End,
              End,
            End,

          Child, VGroup,
            Child, ColGroup(2),
              Child, Label1("Gfx type"),
              Child, gfxcycle = CycleObject,
                MUIA_Cycle_Entries, gfxtype,
                MUIA_Cycle_Active, tt_gfx,
                End,
              End,
            Child, HGroup,
/*              MUIA_Group_SameHeight, TRUE,*/
              Child, cgxgroup = VGroup, GroupFrameT("CyberGfx"),
                MUIA_Disabled, tt_gfx!=0,
                Child, PopaslObject,
                  MUIA_Popasl_Type, ASL_ScreenModeRequest,
                  MUIA_Popstring_String, TextObject,
                    MUIA_Text_Contents, cgxmodename,
                    MUIA_Frame, MUIV_Frame_Text,
                    End,
                  MUIA_Popstring_Button, PopButton(MUII_PopUp),
                  MUIA_Popasl_StartHook,(ULONG)&scrmodeini_hook,
                  MUIA_Popasl_StopHook,(ULONG)&scrmodesel_hook,
                  ASLSM_MinWidth, 320,
                  ASLSM_MinHeight, 200,
                  ASLSM_MinDepth, 8,
                  ASLSM_MaxDepth, 8,
                  End,
                Child, ColGroup(2),
                  Child, wpa8toggle = CheckMark(tt_forcewpa8),
                  Child, LLabel1("Use OS for screen update"),
                  End,
                End,
              Child, agagroup = VGroup, GroupFrameT("AGA"),
                MUIA_Disabled, tt_gfx!=1,
                Child, ColGroup(2),
                  Child, Label1("Mode"),
                  Child, CycleObject,
                    MUIA_Cycle_Entries, agamodes,
                    MUIA_Cycle_Active, tt_agamode,
                    End,
                  End,
                End,
              End,
            End,

          Child, VGroup,
            Child, ColGroup(2),
              Child, Label1("Sound"),
              Child, sndcycle = CycleObject,
                MUIA_Cycle_Entries, soundtype,
                MUIA_Cycle_Active, tt_sound,
                End,
              End,
            Child, ColGroup(2),
              Child, swapchtoggle = CheckMark(tt_swapchans),
              Child, LLabel1("Swap left/right channel"),
              End,
            End,

          Child, VGroup,
            Child, ColGroup(2),
              Child, nocdatoggle = CheckMark(tt_nocdaudio),
              Child, LLabel1("Disable CD audio playback"),
              End,
            Child, cdagroup = VGroup, GroupFrameT("CD Audio"),
              MUIA_Disabled, tt_nocdaudio!=0,
              Child, ColGroup(2),
                Child, Label2("CD device"),
                Child, cddevstr = StringObject, StringFrame,
                  MUIA_String_Contents, tt_cddev,
                  MUIA_String_MaxLen, 80,
                  MUIA_FixWidthTxt,"xxxxxxxxxxxxxxxxxxxxxxxx",
                  End,
                Child, Label2("CD unit"),
                Child, cdunitnum = NumericbuttonObject,
                  MUIA_Numeric_Min, 0,
                  MUIA_Numeric_Max, 15,
                  MUIA_Numeric_Value, tt_cdunit,
                  End,
                End,
              End,
            End,

          End,

        Child, ColGroup(3),
          Child, usebutton = SimpleButton("Use"),
          Child, savebutton = SimpleButton("Save"),
          Child, quitbutton = SimpleButton("Quit"),
          End,
        End,
      End,
    End;

  return app;
}


int options_gui(REG(a0,struct WBStartup *wbmsg))
{
  BPTR orgdir;
  struct WBArg *wba;
  struct DiskObject *dob;
  ULONG id,sigs=0;
  int rc = 0;

  /*
   * open libraries
   */
  if (!(MUIMasterBase = OpenLibrary("muimaster.library",19)))
    return 0;
#ifdef __amigaos4__
  IMUIMaster = (struct MUIMasterIFace *)
                GetInterface((struct Library *)MUIMasterBase,"main",1,0);
#endif

  if (!(IconBase = OpenLibrary("icon.library",36))) {
#ifdef __amigaos4__
    DropInterface((struct Interface *)IMUIMaster);
#endif
    CloseLibrary(MUIMasterBase);
    return 0;
  }
#ifdef __amigaos4__
  else
    IIcon = (struct IconIFace *)
             GetInterface((struct Library *)IconBase,"main",1,0);
#endif

  if (IntuitionBase == NULL) {
    if (!(IntuitionBase = (struct IntuitionBase *)
        OpenLibrary("intuition.library",36))) {
#ifdef __amigaos4__
      DropInterface((struct Interface *)IIcon);
      DropInterface((struct Interface *)IMUIMaster);
#endif
      CloseLibrary(IconBase);
      CloseLibrary(MUIMasterBase);
      return 0;
    }
#ifdef __amigaos4__
    else
      IIntuition = (struct IntuitionIFace *)
                    GetInterface((struct Library *)IntuitionBase,"main",1,0);
#endif
  }

  /*
   * find icon and read its tooltypes
   */
  wba = wbmsg->sm_ArgList;
  orgdir = CurrentDir(wba->wa_Lock);
  if (dob = GetDiskObject(wba->wa_Name)) {
    read_tooltypes(dob);
    FreeDiskObject(dob);
  }

  /*
   * create MUI application
   */
  if (app = create_mui_app()) {
    DoMethod(win,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,app,2,
             MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
  	DoMethod(gfxcycle,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,
             MUIV_Notify_Application,2,
             MUIM_Application_ReturnID,ID_CYCLEGFX);
  	DoMethod(nocdatoggle,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,
             MUIV_Notify_Application,2,
             MUIM_Application_ReturnID,ID_CDAUDIO);
    DoMethod(quitbutton,MUIM_Notify,MUIA_Pressed,FALSE,
             MUIV_Notify_Application,2,
             MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
    DoMethod(savebutton,MUIM_Notify,MUIA_Pressed,FALSE,
             MUIV_Notify_Application,2,MUIM_Application_ReturnID,ID_SAVE);
    DoMethod(usebutton,MUIM_Notify,MUIA_Pressed,FALSE,
             MUIV_Notify_Application,2,MUIM_Application_ReturnID,ID_USE);

    /*
     * open application window and handle events
     */
    set(win,MUIA_Window_Open,TRUE);

    for (;;) {
      LONG val;

      id = DoMethod(app,MUIM_Application_NewInput,&sigs);
      if (sigs) {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
          break;
      }
      if (id == MUIV_Application_ReturnID_Quit) {
        rc = -1;
        break;
      }
      else if (id == ID_USE) {
        rc = 1;
        break;
      }
      else if (id == ID_SAVE) {
        rc = 2;
        break;
      }
      else switch (id) {
        case ID_CYCLEGFX:
          get(gfxcycle,MUIA_Cycle_Active,&val);
          set(cgxgroup,MUIA_Disabled,val!=0);
          set(agagroup,MUIA_Disabled,val!=1);
          tt_gfx = val;
          break;
        case ID_CDAUDIO:
          get(nocdatoggle,MUIA_Selected,&val);
          set(cdagroup,MUIA_Disabled,val!=0);
          tt_nocdaudio = val;
          break;
      }
    }

    /*
     * destroy application window
     */
    read_settings();
    set(win,MUIA_Window_Open,FALSE);
    MUI_DisposeObject(app);
  }

  if (rc == 2) {
    /*
     * write new settings into the icon's tooltypes
     */
    if (dob = GetDiskObject(wba->wa_Name)) {
      write_tooltypes(wba->wa_Name,dob);
      FreeDiskObject(dob);
    }
  }
  else if (rc == 1)
    write_env(FALSE);

  /*
   * cleanup
   */
  CurrentDir(orgdir);
#ifdef __amigaos4__
  DropInterface((struct Interface *)IIntuition);
  DropInterface((struct Interface *)IIcon);
  DropInterface((struct Interface *)IMUIMaster);
#endif
  CloseLibrary((struct Library *)IntuitionBase);
  IntuitionBase = NULL;
  CloseLibrary(IconBase);
  CloseLibrary(MUIMasterBase);
  return rc;
}
