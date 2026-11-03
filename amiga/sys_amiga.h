/*
 * The Amiga side of the video and sound drivers: see sys_amiga.c.
 */

#ifndef SYS_AMIGA_H
#define SYS_AMIGA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sys_fill_fn)(void *arg, int16_t *stereo, int frames);

int sys_init(void);
int sys_screen(int w, int h);
void sys_shutdown(void);
/* Hide the system's mouse pointer on the game's screen. */
void sys_hide_pointer(void);
uint32_t sys_ticks(void);
/* Seconds since sys_init, from the E-clock. */
double sys_seconds(void);
void sys_sleep(uint32_t ms);
/* pixels is w x h from the last sys_screen, pal 256 RGB triples. */
void sys_present(const uint8_t *pixels, const uint8_t *pal, int shake);
/* A new palette for the picture on show, without drawing it again. */
void sys_palette(const uint8_t *pal);
/* Fade the picture on show to black over about half a second. */
void sys_fade(const uint8_t *pal);
/* Every key by its Amiga raw key code, 1 while held. */
const uint8_t *sys_keys(int *quit, uint32_t *pad);
int sys_audio_start(int rate, sys_fill_fn fill, void *arg);
void sys_audio_stop(void);
void sys_audio_lock(void);
void sys_audio_unlock(void);

#ifdef __cplusplus
}
#endif

#endif
