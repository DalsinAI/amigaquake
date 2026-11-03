/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_acgame.c -- the sound DMA for AmigaOS 3.x through ahi.device
//
// Quake mixes into a ring of 16-bit stereo at 22050 Hz (snd_dma.c); the
// sound process of sys_amiga.c plays it, moving the "DMA" position as it
// goes. The AmiCradle Team.

#include "quakedef.h"
#include "sys_amiga.h"

#define	RING_FRAMES	8192		/* a power of two: about 0.37 s */
#define	RING_RATE	22050

static int16_t	*ring;
static volatile int	ring_pos;	/* frames played, modulo RING_FRAMES */
static qboolean	started;

/* In the sound process: hand AHI the next frames of the ring. */
static void ring_fill (void *arg, int16_t *stereo, int frames)
{
	int	pos = ring_pos;

	while (frames-- > 0)
	{
		*stereo++ = ring[pos * 2];
		*stereo++ = ring[pos * 2 + 1];
		pos = (pos + 1) & (RING_FRAMES - 1);
	}
	ring_pos = pos;
}

qboolean SNDDMA_Init (void)
{
	ring = calloc (RING_FRAMES * 2, sizeof(int16_t));
	if (!ring)
		return false;
	shm = &sn;
	shm->splitbuffer = false;
	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = RING_RATE;
	shm->samples = RING_FRAMES * 2;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)ring;
	ring_pos = 0;
	if (sys_audio_start (RING_RATE, ring_fill, NULL) != 0)
	{
		free (ring);
		ring = NULL;
		shm = NULL;
		return false;
	}
	started = true;
	return true;
}

int SNDDMA_GetDMAPos (void)
{
	shm->samplepos = ring_pos * 2;
	return shm->samplepos;
}

void SNDDMA_Shutdown (void)
{
	if (started)
		sys_audio_stop ();
	started = false;
	free (ring);
	ring = NULL;
}

void SNDDMA_Submit (void)
{
}

/* The AHI effects of the Amiga version (an echo under water): none here. */
void SNDDMA_EnterWater (void)
{
}

void SNDDMA_LeaveWater (void)
{
}
