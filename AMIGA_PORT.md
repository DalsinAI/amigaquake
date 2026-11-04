# Amiga port intake: Quake original GPL engine

- Port Atlas: [g-quake-original-gpl-engine](https://www.amicradle.com/ports/#g-quake-original-gpl-engine)
- Priority: `P3`
- Atlas decision: `defer`
- Upstream source: `https://github.com/id-Software/Quake`
- Atlas source pin: `bf4ac424ce754894ac8f1dae6a3981954bc9852d`
- Imported commit: `bf4ac424ce754894ac8f1dae6a3981954bc9852d`
- Import scope: `complete upstream tree`
- Rights state: `review`

## Target contract

Primary target: AmigaOS 3.x / AmigaChrome AC090 / Motorola 68040 + FPU.

Use the OPEN platform libraries where applicable: OpenGPU, OpenAudio/OpenAHI, OpenInput, OpenSocket and measured OpenMulticore work. Preserve a correct 68040 fallback and do not assume Roadshow.

No build, runtime, playability or release claim is made by this source-intake commit. Upstream submodules are not initialised. Original commercial game data remains external.

## Rights and data boundary

review; Confirm source/data licences before release.

## Base: Quake for Amiga 2.30

`amiga230/` is the Amiga Quake source 2.30 by Frank Wille and Steffen
Häuser (Aminet game/shoot/Quake_src), id's GPL Quake with their AmigaOS
ports and 68k assembly. See `amiga230/README.md` for what was left out of
it and why.

## Building

    make -f amiga/Makefile.amigaos STOVE=/path/to/os32-gcc16/prefix \
         NDK=/path/to/gfopensdk/OpenNDK/Include_H \
         ACGAME=/path/to/amicradle-guest/gameports/acgame

GCC builds amiga230's engine as it is (C only for now: the 68k assembly
is in PhxAss/Devpac syntax and needs vasm, which is not in the stove yet),
with `amiga/` in place of the vbcc system, video, input and sound files:
the 320x200 picture on an ACGame screen (AGA or RTG) in the middle with a
black border, Amiga raw keys and mouse look, Quake's sound DMA ring played
through ahi.device, the E-clock for the time, the hunk from exec. CD audio
and the network are id's null drivers (`WinQuake/cd_null.c`,
`net_none.c`). `package/build-amiga.sh` makes the Quake drawer with its
GlowIcons (the program, the drawer and its id1 drawer).

## Tested

In the lab (68040, OpenKick) with the shareware data (archive.org
Quake_802, quake106; lab only, never committed or shipped):

- `quake +timedemo demo1`: 969 frames, 39.7 fps on RTG, 47.9 fps on AGA
  (the lab's emulated 68040, faster than a 50 MHz one).
- The demos with their sound through AHI (continuous), the console and
  its typing, the menu, quitting.

Not tested: playing a level by hand, saving, the full game's data, a real
68040 or 68060's speed, the assembly routines.

Fixed in test4, both found with `quake +timedemo demo2` (E1M4, where the
demo goes under water):

- In water, slime or lava the view is drawn into a warp buffer first.
  amiga230's `r_warpbuffer` was allocated by its vid_amiga.c, which this
  build replaces, so it stayed NULL and the view went to address 0: the
  68k's vectors and ExecBase were overwritten and the whole Amiga stopped
  (the "spinning in system code, interrupts masked" stop seen before, and
  the crash on falling into lava). `vid_acgame.c` now has its own buffer.
- Built with `-ffast-math`, the edge sort (`R_StepActiveU`) then looped
  for ever a moment later in the same demo. Built without it, demo2 plays
  to the end, and demo1 is no slower.

## OpenUpQuake (GLQuake on OpenGPU)

    make -f amiga/openup/Makefile.amigaos STOVE=... NDK=... ACGAME=... \
         OPENGPU=/path/to/OpenGPU-SDK

id's GLQuake (`WinQuake/` with `-DGLQUAKE`, built `-fpermissive -fcommon`
for its 1996 C) drawn through MiniGL (minigl.library, OpenGL 1.1 on
GFOpenGPU's 3D). `amiga/openup/gl_vidmgl.c` is the video layer (from
gl_vidlinux.c: the palette as R, G, B, A in memory for the big-endian 68k)
and reads the keys and mouse from MiniGL's window; the time and the sound
are the software build's. RTG and OpenGPU only.

In the lab (OpenGPU drawing its 3D on the CPU, no host GPU):
`openupquake +timedemo demo1` 15.5 fps at 640x480 (the 68k all busy:
the fill is the cost), 43.6 fps at 320x240 (the software build: 39.7).
With OpenGPU on a graphics chip the fill is not the 68k's; not measured.
