/*
** vid_amiga.c
**
** Amiga Video Drivers
**
** AGA and CGFX
**
** Written by Frank Wille <frank@phoenix.owl.de>
**         Steffen Häuser <magicsn@birdland.es.bawue.de>
**    and Jarmo Laakkonen <jami.laakkonen@kolumbus.fi>
**
** FIXME: Support for DirectRect is missing
**
*/

#pragma amiga-align
#include <exec/memory.h>
#include <exec/libraries.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#pragma default-align

#include "quakedef.h"
#include "SDI_compiler.h"
#include "d_local.h"
#include "vid_amiga.h"
#include "keys_amiga.h"


/* Library bases used by the video drivers */
struct Library *CyberGfxBase = NULL;
struct Library *IntuitionBase = NULL;
#ifdef __amigaos4__
struct CyberGfxIFace *ICyberGfx;
struct IntuitionIFace *IIntuition;
#endif

/* will be assigned by the video driver */
struct Screen *QuakeScreen = NULL;
struct Window *Window = NULL;

viddef_t vid;                    /* global video state */
byte *vid_buffer;
static byte *vid_buf_base = NULL;
static short *zbuffer = NULL;
static int config_notify = 0;
int shutdown_keyboard = 0;
static byte *surfcache;
static int surfcachesize;

static byte vid_current_palette[768];
static int vid_modenum;
static int agamode;
static int firstopen = 1;

unsigned short d_8to16table[256];
unsigned d_8to24table[256];

unsigned char rawkeyconv[] = {
  '`','1','2','3','4','5','6','7','8','9','0','-','=','\\',0,K_INS,
  'q','w','e','r','t','y','u','i','o','p','[',']',0,K_END,K_DOWNARROW,K_PGDN,
  'a','s','d','f','g','h','j','k','l',';','\'',0,0,K_LEFTARROW,'5',K_RIGHTARROW,
  0,'z','x','c','v','b','n','m',',','.','/',0,K_DEL,K_HOME,K_UPARROW,K_PGUP,
  K_SPACE,K_BACKSPACE,K_TAB,K_ENTER,K_ENTER,K_ESCAPE,K_F11,0,
  0,0,'-',0,K_UPARROW,K_DOWNARROW,K_RIGHTARROW,K_LEFTARROW,
  K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,'(',')','/',K_PAUSE,'+',K_F12,
  K_SHIFT,K_SHIFT,0,K_CTRL,K_ALT,K_ALT,K_AMIGA,K_AMIGA,
  K_MOUSE1,K_MOUSE2,K_MOUSE3,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,K_MWHEELUP,K_MWHEELDOWN,0,0,0,0
};

cvar_t vid_mode={"vid_mode","1",false};
cvar_t _vid_default_mode={"_vid_default_mode","1",true};



/*
**
*/

int modearray_x[11]={320,320,320,400,480,512,640,640,800,1024,1280};
int modearray_y[11]={200,240,400,300,384,384,400,480,600, 768,1024};

typedef struct {
  char *name;
  char *header;
  int   width;
  int   height;
} vmode_t;

static const vmode_t vmodes[] = {
  {  "320*200",  "LowRes I",   320,  200  },
  {  "320*240",  "LowRes II",  320,  240  },
  {  "320*400",  "LowRes III", 320,  400  },
  {  "400*300",  "MedRes I",   400,  300  },
  {  "480*384",  "MedRes II",  480,  384  },
  {  "512*384",  "MedRes III", 512,  384  },
  {  "640*400",  "HiRes I",    640,  400  },
  {  "640*480",  "HiRes II",   640,  480  },
  {  "800*600",  "HiRes III",  800,  600  },
  { "1024*768",  "HiRes IV",  1024,  768  },
  { "1280*1024", "HiRes V",   1280, 1024  }
};

#define NUM_VIDMODES ((int)(sizeof(vmodes)/sizeof(vmodes[0])))

typedef struct {
  int   modenum;
  char *desc;
  int   iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE 11
#define MAX_MODEDESCS   33
static modedesc_t modedescs[MAX_MODEDESCS];

static int vid_wmodes, vid_line, vid_column_size;

static char *VID_GetModeDescription (int modenum)
{
  if ((modenum < 0) || (modenum > (NUM_VIDMODES-1)))
    Sys_Error ("VID_GetModePtr: invalid modenum");

  return vmodes[modenum].name;
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
  int     i, j, column, row;
  char    temp[100], *ptr;
  qpic_t *p;

  p = Draw_CachePic ("gfx/vidmodes.lmp");
  M_DrawPic ( (320-p->width)/2, 4, p);

  vid_wmodes = 0;

  for (i=0; i < NUM_VIDMODES ; i++) {
    if (vid_wmodes < MAX_MODEDESCS) {
      modedescs[vid_wmodes].modenum = i;
      modedescs[vid_wmodes].desc = VID_GetModeDescription (i);
      modedescs[vid_wmodes].iscur = (i == vid_modenum ? 1 : 0);

      vid_wmodes++;
    }
  }

  vid_column_size = (vid_wmodes + 2) / 3;

  column = 16;
  row = 36;

  for (i=0; i < vid_wmodes; i++) {
    if (modedescs[i].iscur)
      M_PrintWhite (column, row, modedescs[i].desc);
    else
      M_Print (column, row, modedescs[i].desc);

    row += 8;

    if ((i % vid_column_size) == (vid_column_size - 1)) {
      column += 13*8;
      row = 36;
    }
  }

  M_Print (9*8, 36 + MAX_COLUMN_SIZE * 8 + 8*3, "Press Enter to set mode");
  ptr = VID_GetModeDescription (vid_modenum);
  sprintf (temp, "D to make %s the default", ptr);
  M_Print (6*8, 36 + MAX_COLUMN_SIZE * 8 + 8*5, temp);
  ptr = VID_GetModeDescription ((int)_vid_default_mode.value);

  if (ptr) {
    sprintf (temp, "Current default is %s", ptr);
    M_Print (7*8, 36 + MAX_COLUMN_SIZE * 8 + 8*6, temp);
  }

  M_Print (15*8, 36 + MAX_COLUMN_SIZE * 8 + 8*8, "Esc to exit");

  row = 36 + (vid_line % vid_column_size) * 8;
  column = 8 + (vid_line / vid_column_size) * 13*8;

  M_DrawCharacter (column, row, 12+((int)(realtime*4)&1));
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
  switch (key) {
    case K_ESCAPE:
      S_LocalSound ("misc/menu1.wav");
      M_Menu_Options_f ();
      break;

    case K_UPARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line--;

      if (vid_line < 0)
        vid_line = vid_wmodes - 1;
      break;

    case K_DOWNARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line++;

      if (vid_line >= vid_wmodes)
      vid_line = 0;
      break;

    case K_LEFTARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line -= vid_column_size;

      if (vid_line < 0) {
        vid_line += ((vid_wmodes + (vid_column_size - 1)) /
                     vid_column_size) * vid_column_size;

        while (vid_line >= vid_wmodes)
               vid_line -= vid_column_size;
      }
      break;

    case K_RIGHTARROW:
      S_LocalSound ("misc/menu1.wav");
      vid_line += vid_column_size;

      if (vid_line >= vid_wmodes) {
        vid_line -= ((vid_wmodes + (vid_column_size - 1)) /
                     vid_column_size) * vid_column_size;

        while (vid_line < 0)
          vid_line += vid_column_size;
      }
      break;

    case K_ENTER:
      S_LocalSound ("misc/menu1.wav");
      Cvar_SetValue("vid_mode",modedescs[vid_line].modenum);
      vid_modenum = modedescs[vid_line].modenum;
      config_notify = 1;
      break;

    case 'D':
    case 'd':
      S_LocalSound ("misc/menu1.wav");
      Cvar_SetValue ("_vid_default_mode", modedescs[vid_line].modenum);
      vid_modenum = modedescs[vid_line].modenum;
      break;

    default:
      break;
  }
}


void VID_SetPalette (unsigned char *palette)
{
  ULONG rgbtab[3*256+2];
  ULONG *rt = rgbtab;
  ULONG x;
  int i;

  /* @@@ improve this??? */
  if (palette != vid_current_palette)
    Q_memcpy(vid_current_palette,palette,sizeof(vid_current_palette));

  *rt++ = 256<<16;
  for (i=0; i<256; i++)
  {
    x = (ULONG)*palette++;
    *rt++ = (x<<24)|(x<<16)|(x<<8)|x;
    x = (ULONG)*palette++;
    *rt++ = (x<<24)|(x<<16)|(x<<8)|x;
    x = (ULONG)*palette++;
    *rt++ = (x<<24)|(x<<16)|(x<<8)|x;
  }
  *rt = 0;
  LoadRGB32(&(QuakeScreen->ViewPort),rgbtab);
}


void VID_ShiftPalette (unsigned char *palette)
{
  VID_SetPalette(palette);
}


void VID_Init (unsigned char *palette)
{
  char *module = "VID_Init: ";

  if (firstopen) {
    firstopen = 0;
    vid_modenum = _vid_default_mode.value;
    Cvar_SetValue("vid_mode",vid_modenum);
    vid_mode.value = _vid_default_mode.value;
  }

  if (!(IntuitionBase = OpenLibrary("intuition.library",36)))
    Sys_Error("%sCan't open intuition.library V36",module);
#ifdef __amigaos4__
  if (!(IIntuition = (struct IntuitionIFace *)
        GetInterface((struct Library *)IntuitionBase,"main",1,0)))
    Sys_Error("%sCan't get intuition interface",module);
#endif

#ifdef __amigaos4__
  if (!(CyberGfxBase = (struct Library *)
                        OpenLibrary("cybergraphics.library",0)))
    Sys_Error("%sCan't open cybergraphics.library",module);
  if (!(ICyberGfx = (struct CyberGfxIFace *)
        GetInterface((struct Library *)CyberGfxBase,"main",1,0)))
    Sys_Error("%sCan't get cybergraphics interface",module);
  agamode = FALSE;  /* no AGA support for OS4 */

#else
  if (CyberGfxBase = (struct Library *)
                      OpenLibrary("cybergraphics.library",0))
    agamode = FALSE;
  else
    agamode = TRUE;

  if (COM_CheckParm("-cgfx")) {
    agamode = FALSE;
  }
  else if (COM_CheckParm("-aga")) {
    agamode = TRUE;
  }
  else {
    char strMode[256];

    if (GetVar("quake1/gfxmode",strMode,255,0) < 0)
    {
      if (!Q_strcasecmp(strMode,"cybergfx"))
        agamode = FALSE;
      else if (!Q_strcasecmp(strMode,"agac2p"))
        agamode = TRUE;
    }
  }
  if (!agamode && CyberGfxBase==NULL) {
    Sys_Error("%sNo CyberGfx available",module);
    agamode = TRUE;
  }
#endif

  if (agamode)
    VID_Init_AGA(palette);
  else
    VID_Init_CGFX(palette);

  vid.colormap = host_colormap;
  vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

  if (!(vid_buf_base = (byte *)Sys_Alloc(vid.width * vid.height + 16,
                                         MEMF_FAST|MEMF_PUBLIC)))
    Sys_Error("%sNot enough memory for video buffer",module);

  /* Make sure vid_buffer is 16-bytes aligned */
  vid_buffer = (byte *)((ULONG)(vid_buf_base + 15) & ~15);

  if (!(zbuffer = (short *)Sys_Alloc(vid.width * vid.height * 2,
                                     MEMF_FAST|MEMF_PUBLIC)))
    Sys_Error("%sNot enough memory for z-buffer",module);

  if (!(r_warpbuffer = (byte *)Sys_Alloc(vid.width * vid.height,
                                         MEMF_FAST|MEMF_PUBLIC)))
    Sys_Error("%sNot enough memory for warp buffer",module);

  vid.conbuffer = vid.buffer = vid_buffer;
  d_pzbuffer = zbuffer;

  surfcachesize = D_SurfaceCacheForRes(vid.width,vid.height);

  if (!(surfcache = (byte *)Sys_Alloc(surfcachesize,MEMF_FAST)))
    Sys_Error("%sNot enough memory for surface cache",module);

  D_InitCaches (surfcache, surfcachesize);

  if (agamode) {
    vid_menudrawfn = NULL;
    vid_menukeyfn = NULL;
  }
  else {
    vid_menudrawfn = VID_MenuDraw;
    vid_menukeyfn = VID_MenuKey;
  }
}


void VID_Shutdown (void)
{
  D_FlushCaches();
  if (surfcache)
    Sys_Free(surfcache);
  if (r_warpbuffer)
    Sys_Free(r_warpbuffer);
  if (zbuffer)
    Sys_Free(zbuffer);
  if (vid_buf_base)
    Sys_Free(vid_buf_base);

  if (agamode)
    VID_Shutdown_AGA();
  else
    VID_Shutdown_CGFX();

  if (CyberGfxBase) {
#ifdef __amigaos4__
    if (ICyberGfx)
      DropInterface((struct Interface *)ICyberGfx);
#endif
    CloseLibrary((struct Library *)CyberGfxBase);
  }
  if (IntuitionBase) {
#ifdef __amigaos4__
    if (IIntuition)
      DropInterface((struct Interface *)IIntuition);
#endif
    CloseLibrary(IntuitionBase);
  }
}


void VID_Update (vrect_t *rects)
{
  if (config_notify) {
    D_FlushCaches();
    vid.recalc_refdef = 1;

    if (vid_buf_base)
      Sys_Free(vid_buf_base);
    vid_buf_base = NULL;

    if (zbuffer)
      Sys_Free(zbuffer);
    zbuffer = NULL;

    if (r_warpbuffer)
      Sys_Free(r_warpbuffer);
    d_viewbuffer = r_warpbuffer;
    r_warpbuffer = NULL;

    if (surfcache)
      Sys_Free(surfcache);
    surfcache = NULL;

    if (!agamode)
      VID_Shutdown_CGFX();

    VID_Init(vid_current_palette);
    shutdown_keyboard = 1;
    config_notify = 0;
    Con_CheckResize();
    Con_Clear_f();
  }
  else {
    if (agamode)
      VID_Update_AGA(rects);
    else
      VID_Update_CGFX(rects);
  }
}


void Sys_SendKeyEvents (void)
{
  if (Window) {
    struct MsgStruct events[50];
    int messages = Sys_GetKeyEvents(Window->UserPort,events,50);
    int i;

    for (i=0; i<messages; i++) {
      if (events[i].Class==IDCMP_RAWKEY || events[i].Class==IDCMP_MOUSEBUTTONS) {
        int kn = (int)rawkeyconv[events[i].Code & 0x7f];
        ULONG code = events[i].Code;

        if (code & IECODE_UP_PREFIX)
          Key_Event(kn, false);
        else
          Key_Event(kn, true);

        if (code==NM_WHEEL_UP || code==NM_WHEEL_DOWN)
          Key_Event(kn, false);
      }
      if (shutdown_keyboard) {
        shutdown_keyboard = 0;
        return;
      }
    }
  }
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
