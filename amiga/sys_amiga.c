/*
 * The Amiga side of Quake's system, video, input and sound layers
 * (sys_acgame.c, vid_acgame.c, in_acgame.c, snd_acgame.c), for AmigaOS 3.x on
 * ACGame: an 8-bit screen shown through
 * ACGame on AGA or RTG, keys from ACGame's raw keys, timing from
 * timer.device, and sound through ahi.device.
 *
 * Sound runs in a process of its own: the driver fills 16-bit stereo, which
 * is folded to mono and played double buffered through linked CMD_WRITE
 * requests. Without AHI the game plays silently.
 *
 * The AmiCradle Team, on the same terms as the game.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <acgame/platform.h>

#include "sys_amiga.h"

/* ---- timing ---- */

static struct SignalSemaphore sndlock;   /* the sound mixer's, from sys_init on */

struct Device *TimerBase;
static struct MsgPort *timerPort;
static struct timerequest *timerReq;
static ULONG eclockHz;
static uint64_t eclockStart;

static uint64_t
eclock_now(void)
{
    struct EClockVal v;

    ReadEClock(&v);
    return ((uint64_t)v.ev_hi << 32) | v.ev_lo;
}

static int
timer_open(void)
{
    struct EClockVal v;

    timerPort = CreateMsgPort();
    if (!timerPort)
        return -1;
    timerReq = (struct timerequest *)CreateIORequest(timerPort, sizeof(struct timerequest));
    if (!timerReq || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                                (struct IORequest *)timerReq, 0) != 0)
        return -1;
    TimerBase = timerReq->tr_node.io_Device;
    eclockHz = ReadEClock(&v);
    eclockStart = ((uint64_t)v.ev_hi << 32) | v.ev_lo;
    return 0;
}

static void
timer_close(void)
{
    if (TimerBase)
        CloseDevice((struct IORequest *)timerReq);
    TimerBase = NULL;
    if (timerReq)
        DeleteIORequest((struct IORequest *)timerReq);
    timerReq = NULL;
    if (timerPort)
        DeleteMsgPort(timerPort);
    timerPort = NULL;
}

double
sys_seconds(void)
{
    return (double)(eclock_now() - eclockStart) / (double)eclockHz;
}

uint32_t
sys_ticks(void)
{
    return (uint32_t)((eclock_now() - eclockStart) * 1000 / eclockHz);
}

/*
 * A timer request can come back well after the time asked for (a tick
 * late, on some machines); the game sleeps a few ms a frame, so that would
 * slow it to a crawl. So the lateness is measured as it goes, and taken off
 * the next sleep; a sleep shorter than it is not taken at all.
 */
static uint32_t lateness;

void
sys_sleep(uint32_t ms)
{
    uint32_t start, took;

    if (ms <= lateness)
        return;
    start = sys_ticks();
    ms -= lateness;
    timerReq->tr_node.io_Command = TR_ADDREQUEST;
    timerReq->tr_time.tv_secs = ms / 1000;
    timerReq->tr_time.tv_micro = (ms % 1000) * 1000;
    DoIO((struct IORequest *)timerReq);
    took = sys_ticks() - start;
    /* An average over the last few sleeps, kept below 50 ms. */
    if (took > ms) {
        uint32_t late = took - ms;

        lateness = (3 * lateness + (late > 50 ? 50 : late)) / 4;
    }
    else
        lateness = (3 * lateness) / 4;
}

/* ---- screen ---- */

/*
 * The game's picture (gameW x gameH) is drawn centred in a frame the size of
 * the whole screen ACGame opened, and everything round it is black: pen 0,
 * which is also the colour AGA shows outside the screen. The game's palette
 * can make pen 0 any colour, so where a picture uses it, its pixels move to
 * a pen the picture leaves free, which takes pen 0's colour, and pen 0 is
 * set to black.
 */
static int screenOpen, gameW, gameH, frameW, frameH, originX, originY;
static int movedPen;         /* where pen 0 went, 0 if it stayed */

int
sys_init(void)
{
    static int done;

    if (done)
        return 0;
    done = 1;
    InitSemaphore(&sndlock);
    if (timer_open() != 0) {
        puts("quake: timer.device did not open");
        return -1;
    }
    /* The game's error() exits; the screen and the sound go with it. */
    atexit(sys_shutdown);
    return 0;
}

static int
open_frame(int w, int h)
{
    acgame_video_config config;

    memset(&config, 0, sizeof(config));
    config.width = (unsigned)w;
    config.height = (unsigned)h;
    config.refresh_hz = 50;
    config.mode = ACGAME_VIDEO_INDEXED8;
    config.display = ACGAME_DISPLAY_AUTO;
    return acgame_platform_init(&config);
}

int
sys_screen(int w, int h)
{
    acgame_platform_info info;
    int tries;

    if (screenOpen && w == gameW && h == gameH)
        return 0;
    if (screenOpen)
        acgame_platform_shutdown();
    screenOpen = 0;
    /* 320 x 240 at least: a mode every display has. A smaller screen can
     * sit in the corner of a bigger display mode, with the rest of it out
     * of our reach. Then again for the whole screen that gave, if larger,
     * so the frame covers it all and the border is ours to colour. */
    frameW = w > 320 ? (w + 7) & ~7 : 320;
    frameH = h > 240 ? h : 240;
    for (tries = 0; tries < 3; tries++) {
        if (open_frame(frameW, frameH) != 0) {
            printf("quake: could not open a %dx%d screen\n", frameW, frameH);
            return -1;
        }
        if (acgame_platform_get_info(&info) != 0 ||
            ((int)info.screen_width == frameW && (int)info.screen_height == frameH) ||
            (info.screen_width & 7u) || (int)info.screen_width < w || (int)info.screen_height < h)
            break;
        acgame_platform_shutdown();
        frameW = (int)info.screen_width;
        frameH = (int)info.screen_height;
    }
    screenOpen = 1;
    gameW = w;
    gameH = h;
    originX = (frameW - w) / 2;
    originY = (frameH - h) / 2;
    /* Black at once: the screen opens in the system's colours, grey, and
     * the game's first picture can be a while coming. */
    {
        acgame_indexed_surface *frame = acgame_platform_begin_indexed_frame();

        if (frame) {
            memset(frame->pixels, 0, (size_t)frame->pitch * frame->height);
            memset(frame->palette, 0, sizeof(frame->palette[0]) * 256);
            acgame_platform_present_indexed(frame);
        }
    }
    return 0;
}

/*
 * For games that draw their own pointer: a blank one on ACGame's window,
 * which is the active window once the screen is open (a backdrop with no
 * border, which a Workbench window is not).
 */
static struct Window *pointerWindow;
static UWORD *blankPointer;

void
sys_hide_pointer(void)
{
    struct Window *w = IntuitionBase->ActiveWindow;
    const ULONG flags = WFLG_BACKDROP | WFLG_BORDERLESS;

    if (!screenOpen || pointerWindow || !w || (w->Flags & flags) != flags)
        return;
    blankPointer = AllocMem(6 * sizeof(UWORD), MEMF_CHIP | MEMF_CLEAR);
    if (!blankPointer)
        return;
    pointerWindow = w;
    SetPointer(w, blankPointer, 1, 16, 0, 0);
}

void
sys_shutdown(void)
{
    sys_audio_stop();
    if (pointerWindow)
        ClearPointer(pointerWindow);
    pointerWindow = NULL;
    if (blankPointer)
        FreeMem(blankPointer, 6 * sizeof(UWORD));
    blankPointer = NULL;
    if (screenOpen)
        acgame_platform_shutdown();
    screenOpen = 0;
    timer_close();
}

/* A pen for pen 0's pixels when the picture uses pen 0: one the picture
 * does not use. 0 when it does not need moving, or no pen is free. */
static int
pen_for_zero(const uint8_t *pixels)
{
    static uint8_t used[256];
    int i, n = gameW * gameH;

    memset(used, 0, sizeof(used));
    for (i = 0; i < n; i++)
        used[pixels[i]] = 1;
    if (!used[0])
        return 0;
    for (i = 255; i > 0; i--)
        if (!used[i])
            return i;
    return 0;
}

static void
copy_palette(acgame_rgb8 *out, const uint8_t *pal)
{
    int i;

    for (i = 0; i < 256; i++) {
        out[i].r = pal[3 * i];
        out[i].g = pal[3 * i + 1];
        out[i].b = pal[3 * i + 2];
    }
    if (movedPen)
        out[movedPen] = out[0];
    out[0].r = out[0].g = out[0].b = 0;
}

void
sys_present(const uint8_t *pixels, const uint8_t *pal, int shake)
{
    acgame_indexed_surface *frame;
    int x, y;

    if (!screenOpen)
        return;
    frame = acgame_platform_begin_indexed_frame();
    if (!frame)
        return;
    movedPen = pen_for_zero(pixels);
    for (y = 0; y < frameH && y < (int)frame->height; y++) {
        uint8_t *dst = frame->pixels + (size_t)y * frame->pitch;
        int sy = y - originY - shake;

        if (sy < 0 || sy >= gameH || y < originY) {
            memset(dst, 0, (size_t)frameW);
            continue;
        }
        memset(dst, 0, (size_t)originX);
        memcpy(dst + originX, pixels + (size_t)sy * gameW, (size_t)gameW);
        if (movedPen)
            for (x = originX; x < originX + gameW; x++)
                if (!dst[x])
                    dst[x] = (uint8_t)movedPen;
        memset(dst + originX + gameW, 0, (size_t)(frameW - originX - gameW));
    }
    copy_palette(frame->palette, pal);
    acgame_platform_present_indexed(frame);
}

void
sys_palette(const uint8_t *pal)
{
    acgame_rgb8 out[256];

    if (!screenOpen)
        return;
    copy_palette(out, pal);
    acgame_platform_set_palette(out);
}

void
sys_fade(const uint8_t *pal)
{
    acgame_rgb8 dark[256];
    int step, i;

    for (step = 15; step >= 0; step--) {
        for (i = 0; i < 256; i++) {
            dark[i].r = pal[3 * i] * step / 16;
            dark[i].g = pal[3 * i + 1] * step / 16;
            dark[i].b = pal[3 * i + 2] * step / 16;
        }
        if (movedPen)
            dark[movedPen] = dark[0];
        dark[0].r = dark[0].g = dark[0].b = 0;
        acgame_platform_set_palette(dark);
        sys_sleep(30);
    }
}

/* ---- input ---- */

const uint8_t *
sys_keys(int *quit, uint32_t *pad)
{
    acgame_input_state input;

    acgame_platform_poll(&input);
    *quit = input.quit_requested;
    *pad = input.pad.connected ? input.pad.buttons : 0;
    return acgame_platform_rawkeys();
}

/* ---- sound ---- */

/* ahi.device's request (AHI's devices/ahi.h, interface version 4). */
struct XAHIRequest {
    struct IOStdReq ahir_Std;
    UWORD ahir_Version;
    UWORD ahir_Pad1;
    ULONG ahir_Private[2];
    ULONG ahir_Type;
    ULONG ahir_Frequency;
    LONG ahir_Volume;            /* Fixed, 0x10000 = full */
    LONG ahir_Position;          /* Fixed, 0x8000 = centre */
    struct XAHIRequest *ahir_Link;
};
#define XAHI_DEFAULT_UNIT 0
#define XAHIST_M16S 1UL          /* 16-bit signed mono */

#define MIX_SAMPLES 1024
#define MIX_BYTES (MIX_SAMPLES * 2)

static sys_fill_fn fillFn;
static void *fillArg;
static int mixRate;
static int16_t stereo[MIX_SAMPLES * 2];

static struct Process *mixer;
static struct Task *owner;
static volatile LONG mixer_ok;
static int audioActive;

/* Mix into out. Runs in the sound process. */
static void
mix(int16_t *out)
{
    int i;

    memset(stereo, 0, sizeof(stereo));
    ObtainSemaphore(&sndlock);
    fillFn(fillArg, stereo, MIX_SAMPLES);
    ReleaseSemaphore(&sndlock);
    for (i = 0; i < MIX_SAMPLES; i++)
        out[i] = (int16_t)(((int32_t)stereo[2 * i] + stereo[2 * i + 1]) >> 1);
}

/*
 * The sound process: opens ahi.device, then keeps one request playing and
 * the next one linked behind it until Ctrl-C.
 */
static void
mixer_main(void)
{
    struct MsgPort *port;
    struct XAHIRequest *req[2] = { NULL, NULL };
    struct XAHIRequest *link = NULL;
    int16_t *buf[2] = { NULL, NULL };
    ULONG portsig, sigs;
    int i = 0, opened = 0, quit = 0;

    port = CreateMsgPort();
    if (port)
        req[0] = (struct XAHIRequest *)CreateIORequest(port, sizeof(struct XAHIRequest));
    if (req[0]) {
        req[0]->ahir_Version = 4;
        opened = OpenDevice((CONST_STRPTR)"ahi.device", XAHI_DEFAULT_UNIT,
                            (struct IORequest *)req[0], 0) == 0;
    }
    if (opened) {
        req[1] = AllocVec(sizeof(struct XAHIRequest), MEMF_PUBLIC);
        buf[0] = AllocVec(MIX_BYTES, MEMF_PUBLIC);
        buf[1] = AllocVec(MIX_BYTES, MEMF_PUBLIC);
    }
    if (!req[1] || !buf[0] || !buf[1]) {
        mixer_ok = -1;
        goto out;
    }
    CopyMem(req[0], req[1], sizeof(struct XAHIRequest));
    mixer_ok = 1;
    Signal(owner, SIGBREAKF_CTRL_F);

    portsig = 1UL << port->mp_SigBit;
    while (!quit) {
        mix(buf[i]);
        req[i]->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
        req[i]->ahir_Std.io_Command = CMD_WRITE;
        req[i]->ahir_Std.io_Data = buf[i];
        req[i]->ahir_Std.io_Length = MIX_BYTES;
        req[i]->ahir_Std.io_Offset = 0;
        req[i]->ahir_Type = XAHIST_M16S;
        req[i]->ahir_Frequency = (ULONG)mixRate;
        req[i]->ahir_Volume = 0x10000;
        req[i]->ahir_Position = 0x8000;
        req[i]->ahir_Link = link;
        SendIO((struct IORequest *)req[i]);

        if (link) {
            while (!CheckIO((struct IORequest *)link)) {
                sigs = Wait(portsig | SIGBREAKF_CTRL_C);
                if (sigs & SIGBREAKF_CTRL_C) {
                    quit = 1;
                    break;
                }
            }
            if (quit)
                AbortIO((struct IORequest *)link);
            WaitIO((struct IORequest *)link);
        }
        else if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            quit = 1;
        link = req[i];
        i ^= 1;
    }
    if (link) {
        AbortIO((struct IORequest *)link);
        WaitIO((struct IORequest *)link);
    }

out:
    if (buf[1]) FreeVec(buf[1]);
    if (buf[0]) FreeVec(buf[0]);
    if (req[1]) FreeVec(req[1]);
    if (opened) CloseDevice((struct IORequest *)req[0]);
    if (req[0]) DeleteIORequest((struct IORequest *)req[0]);
    if (port) DeleteMsgPort(port);
    if (mixer_ok == 0)
        mixer_ok = -1;
    Forbid();  /* the owner frees nothing of ours until we are gone */
    Signal(owner, SIGBREAKF_CTRL_F);
}

int
sys_audio_start(int rate, sys_fill_fn fill, void *arg)
{
    fillFn = fill;
    fillArg = arg;
    mixRate = rate;
    owner = FindTask(NULL);
    mixer_ok = 0;
    SetSignal(0, SIGBREAKF_CTRL_F);
    mixer = CreateNewProcTags(NP_Entry, (ULONG)mixer_main,
                              NP_Name, (ULONG)"Quake sound",
                              NP_Priority, 5,
                              NP_StackSize, 65536,
                              TAG_DONE);
    if (!mixer)
        return -1;
    Wait(SIGBREAKF_CTRL_F);
    if (mixer_ok != 1) {
        mixer = NULL;
        puts("quake: ahi.device did not open, playing silently");
        return -1;
    }
    audioActive = 1;
    return 0;
}

void
sys_audio_stop(void)
{
    if (!audioActive)
        return;
    audioActive = 0;
    SetSignal(0, SIGBREAKF_CTRL_F);
    Signal((struct Task *)mixer, SIGBREAKF_CTRL_C);
    Wait(SIGBREAKF_CTRL_F);
    mixer = NULL;
}

/* Always taken, so a lock taken before the sound starts is still one. */
void
sys_audio_lock(void)
{
    ObtainSemaphore(&sndlock);
}

void
sys_audio_unlock(void)
{
    ReleaseSemaphore(&sndlock);
}
