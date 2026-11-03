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
