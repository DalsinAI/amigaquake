/*
** vid_aga.c
**
** AGA video driver using c2p
**
** Written by Frank Wille <frank@phoenix.owl.de>
** PAL/DblNTSC/DblPAL support by Christian Michael <v2615a@groenjord.dk>
**
*/

#pragma amiga-align
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#pragma default-align

#include "quakedef.h"
#include "vid_amiga.h"
#include "SDI_compiler.h"

#ifdef __PPC__
#define C2P_8 c2p_8_ppc
extern void C2P_8(UBYTE *,PLANEPTR,UBYTE *,ULONG);
#else
#define C2P_8 c2p_8_040
extern void ASM C2P_8(REG(a0,UBYTE *),REG(a1,PLANEPTR),REG(a2,UBYTE *),REG(d1,ULONG));
#endif

extern struct Screen *QuakeScreen;
extern struct Window *Window;

static unsigned short *dummypointer;
static int BytesPerRow;

static struct RastPort video_rastport;
static struct BitMap video_bitmap;
static PLANEPTR video_raster = NULL;  /* contiguous bitplanes */
static UBYTE *video_compare_buffer = NULL;

/* use fixed values for now: NTSC-AGA:320x200 */
static int DispID=0x11000, ScrWidth=320, ScrHeight=200, ScrDepth=8;



#ifdef __amigaos4__
void VID_Init_AGA (unsigned char *palette)
{
}

void VID_Shutdown_AGA (void)
{
}

void VID_Update_AGA (vrect_t *rects)
{
}


#else
void VID_Init_AGA (unsigned char *palette)
{
  char *module = "AGA_Init: ";
  int i;

  if (COM_CheckParm("-ntsc"))
    DispID=0x11000, ScrWidth=320, ScrHeight=200, ScrDepth=8;
  if (COM_CheckParm("-pal"))
    DispID=0x21000, ScrWidth=320, ScrHeight=256, ScrDepth=8;
  if (COM_CheckParm("-dblntsc"))
    DispID=0x91000, ScrWidth=320, ScrHeight=200, ScrDepth=8;
  if (COM_CheckParm("-dblpal"))
    DispID=0xA1000, ScrWidth=320, ScrHeight=256, ScrDepth=8;

  if ((video_raster = (PLANEPTR)
      AllocRaster (ScrWidth,ScrDepth*ScrHeight)) == NULL)
    Sys_Error("%sAllocRaster() failed",module);
  memset(video_raster,0,ScrDepth*RASSIZE(ScrWidth,ScrHeight));

  InitBitMap(&video_bitmap,ScrDepth,ScrWidth,ScrHeight);
  for (i=0; i<ScrDepth; i++)
    video_bitmap.Planes[i] = video_raster + i * RASSIZE(ScrWidth,ScrHeight);

  if (!(video_compare_buffer = malloc(ScrWidth*ScrHeight)))
    Sys_Error("%sAllocation of C2P video compare buffer failed",module);
  memset(video_compare_buffer,0,ScrWidth*ScrHeight);

  InitRastPort (&video_rastport);
  video_rastport.BitMap = &video_bitmap;
  SetAPen (&video_rastport,(1<<ScrDepth)-1);
  SetBPen (&video_rastport,0);
  SetDrMd (&video_rastport,JAM2);

  if (!(QuakeScreen = OpenScreenTags(NULL,
                                     SA_Quiet,TRUE,
                                     SA_Width,ScrWidth,
                                     SA_Height,ScrHeight,
                                     SA_Depth,ScrDepth,
                                     SA_DisplayID,DispID,
                                     SA_BitMap,&video_bitmap,
                                     /* custom bitmap, contiguous planes */
                                     TAG_DONE)))
    Sys_Error("%sCan't open AGA screen",module);

  BytesPerRow = ScrWidth/8;
  vid.maxwarpwidth = vid.width = vid.conwidth = ScrWidth;
  vid.maxwarpheight = vid.height = vid.conheight = ScrHeight;
  vid.rowbytes = vid.conrowbytes = ScrWidth;
  if (COM_CheckParm("-pal") || COM_CheckParm("-dblpal"))
    vid.aspect = 1.28;
  else
    vid.aspect = 1.0;
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
    Sys_Error("%sCan't open the AGA window",module);

  if (dummypointer = (unsigned short *)Sys_Alloc(16,MEMF_CLEAR))
    SetPointer(Window,dummypointer,1,1,0,0);

  Con_Printf("graphics.library AGA %d x %d x %d\n",ScrWidth,ScrHeight,8);
  Con_Printf(" %d bpr planar CLUT\n",BytesPerRow);
}


void VID_Shutdown_AGA (void)
{
  if (dummypointer)
    Sys_Free(dummypointer);
  if (Window)
    CloseWindow(Window);
  if (QuakeScreen)
    CloseScreen(QuakeScreen);
  if (video_compare_buffer)
    free(video_compare_buffer);
  if (video_raster)
    FreeRaster(video_raster,ScrWidth,ScrDepth*ScrHeight);
}


void VID_Update_AGA (vrect_t *rects)
{
  C2P_8(vid_buffer,video_raster,video_compare_buffer,(ScrWidth*ScrHeight)>>3);
}
#endif
