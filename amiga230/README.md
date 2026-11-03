# Quake for Amiga 2.30 (the base of this port)

This directory is the Amiga Quake source release 2.30 (Aminet
`game/shoot/Quake_src`, 2 October 2005), unchanged except for the files
listed below. It is id Software's Quake, ported to AmigaOS 3.x, AmigaOS 4,
MorphOS, WarpOS and PowerUp by **Frank Wille** and **Steffen Häuser**, with
the ProQuake extensions of J.P. Grossman (see `HISTORY`). The Quake source is
published under the GNU General Public License, version 2: `../gnu.txt`,
id's own copy.

## Left out

These files of the release are not under the GPL, so they are not here:

- `twfsound_cd.c`, `twfsound_cd.h`: Christopher Page's CD audio code ("may
  not be distributed, reproduced or altered ... without written
  permission"). `cd_amiga.c` needs them; the AmiCradle build uses
  `cd_null.c` instead.
- `psxport.h`: Joseph Fenton's psxport.device definitions ("All rights
  reserved"), and `in_amigapsx.c`, the PlayStation pad support that needs
  it. `in_amiga.c` calls it; the AmiCradle build has its own input.

## Kept, with their own terms

- `SDI_compiler.h`, `SDI_hook.h`: the SDI headers, public domain.
- `vid_c2p_040.s`: the chunky-to-planar routine by James McCoull and Peter
  McGavin, released freely; the AmiCradle build uses ACGame's instead.
