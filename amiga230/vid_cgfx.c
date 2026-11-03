/*
** vid_cgfx.c
**
** cybergraphics.library
**
** Written by Frank Wille <frank@phoenix.owl.de>
**
*/

#pragma amiga-align
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#ifdef CGFX_V4
#include <cybergraphx/cybergraphics.h>
#else
#include <cybergraphics/cybergraphics.h>
#endif
#include <proto/cybergraphics.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#pragma default-align

#include "quakedef.h"
#include "vid_amiga.h"
#include "SDI_compiler.h"


extern struct GfxBase *GfxBase;
extern struct Screen *QuakeScreen;
extern struct Window *Window;

static struct RastPort TempRP;
static struct BitMap TempBM;
static unsigned short *dummypointer;
static int BytesPerRow;
static byte *GfxAddr;

#ifndef _68881
#ifdef __PPC__
#define TurboUpdate TurboUpdatePPC
extern void TurboUpdate(byte *,byte *,int,int,int,int,int);
#else
#define TurboUpdate TurboUpdate68k
extern void ASM TurboUpdate(REG(a0,byte *), REG(a1,byte *),
                            REG(d4,int), REG(d0,int), REG(d1,int),
                            REG(d2,int), REG(d3,int));
#endif
static int turbogfx=TRUE;
#endif



void VID_Init_CGFX (unsigned char *palette)
{
  char *module = "CGfx_Init: ";
  static short ColorModel[] = {PIXFMT_LUT8,-1};
  static struct TagItem CyberModeTags[] = {
    CYBRMREQ_CModelArray,(ULONG)ColorModel,
    CYBRMREQ_MinWidth,320,
    CYBRMREQ_MinHeight,200,
    TAG_DONE,0
  };
  int DispID=-1, ScrWidth, ScrHeight, i;

#ifdef TurboUpdate /* 040/060/PPC only */
  if (COM_CheckParm("-forcewpa8"))
    turbogfx = FALSE;
#endif

  if (i = COM_CheckParm("-modeid")) {
    if (!(sscanf(com_argv[i+1],"%i",&DispID)))
      DispID = -1;
  }
  
  if (DispID==-1) {
    if (!(DispID = CModeRequestTagList(NULL,CyberModeTags)))
      Sys_Error("%sCan't open the screenmode requester",module);
    if (DispID==-1)
      Sys_Error("%sScreenmode requester aborted",module);
  }

  ScrWidth = GetCyberIDAttr(CYBRIDATTR_WIDTH,DispID);
  ScrHeight = GetCyberIDAttr(CYBRIDATTR_HEIGHT,DispID);
  if (!(QuakeScreen = OpenScreenTags(NULL,
                                     SA_Quiet,TRUE,
                                     SA_Width,ScrWidth,
                                     SA_Height,ScrHeight,
                                     SA_Depth,8,
                                     SA_DisplayID,DispID,
                                     TAG_DONE)))
    Sys_Error("%sCan't open the screen",module);
  BytesPerRow = GetCyberMapAttr(QuakeScreen->RastPort.BitMap,CYBRMATTR_XMOD);
  GfxAddr = (byte *)GetCyberMapAttr(QuakeScreen->RastPort.BitMap,CYBRMATTR_DISPADR);
#if 1
  /*phx - This allows high-resolution under water */
  vid.maxwarpwidth = vid.width = vid.conwidth = ScrWidth;
  vid.maxwarpheight = vid.height = vid.conheight = ScrHeight;
#else
  vid.maxwarpwidth = WARP_WIDTH;
  vid.maxwarpheight = WARP_HEIGHT;
#endif
  vid.rowbytes = vid.conrowbytes = ScrWidth;
  vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
  vid.numpages = 1;
  VID_SetPalette(palette);

  if (!(Window = OpenWindowTags(NULL,
              WA_Left,0,
              WA_Top,0,
              WA_Width,ScrWidth,
              WA_Height,ScrHeight,
              WA_Activate,TRUE,
              WA_Borderless,TRUE,
              WA_RMBTrap,TRUE,
              WA_IDCMP, IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY,
              WA_CustomScreen,QuakeScreen,
              TAG_DONE,0)))
    Sys_Error("%sCan't open the window",module);

  if (dummypointer = (unsigned short *)Sys_Alloc(16,MEMF_CLEAR))
     SetPointer(Window,dummypointer,1,1,0,0);
  InitRastPort(&TempRP);
  if ((GfxBase->LibNode.lib_Version)>=39)
  {
    if (!(TempRP.BitMap = AllocBitMap(ScrWidth,1,8,BMF_MINPLANES,
                                      QuakeScreen->RastPort.BitMap)))
      Sys_Error("%sCan't allocate temporary bitmap",module);
  }
  else
  {
    InitBitMap(TempRP.BitMap=&TempBM,8,ScrWidth,1);
    for (i=0;i<8;i++)
      if (!(TempRP.BitMap->Planes[i]=AllocRaster(ScrWidth,1)))
        Sys_Error("%sCan't allocate temporary bitmap",module);
  }
  Con_Printf("cybergraphics.library %d x %d x %d\n",ScrWidth,ScrHeight,8);
  Con_Printf(" %dbpr chunky CLUT\n",BytesPerRow);
}


void VID_Shutdown_CGFX (void)
{
  int i;

  if (GfxBase) {
    if ((GfxBase->LibNode.lib_Version)>=39) {
      if (TempRP.BitMap)
        FreeBitMap(TempRP.BitMap);
    }
    else {
      for (i=0;i<8;i++)
        if (TempRP.BitMap->Planes[i])
          FreeRaster(TempRP.BitMap->Planes[i],vid.width,1);
    }
  }
  if (dummypointer)
    Sys_Free(dummypointer);
  if (Window)
    CloseWindow(Window);
  if (QuakeScreen)
    CloseScreen(QuakeScreen);
}


void VID_Update_CGFX (vrect_t *rects)
{
  if (rects) {

#ifdef TurboUpdate
    if (turbogfx)
      do {
        TurboUpdate(vid_buffer,GfxAddr,BytesPerRow,
                    rects->x,rects->y,rects->width,rects->height);
      } while ((rects = rects->pnext));
    else
#endif
      do {
        WritePixelArray8(&QuakeScreen->RastPort,
                         rects->x,
                         rects->y,
                         rects->x+rects->width-1,
                         rects->y+rects->height-1,
                         vid_buffer,
                         &TempRP);
      } while ((rects = rects->pnext));

  }
}
