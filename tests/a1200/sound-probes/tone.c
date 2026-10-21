/* AHI level test: a 440 Hz tone through SDL2_68k's audio driver (argv[1], default ahi),
   22050 Hz stereo S16 (the game's format), in steps getting louder: each step 2 s of tone
   and 1 s of silence. Prints the steps as they start and how late the callbacks came. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <proto/dos.h>

/* tone [driver] [frames a block] [amplitude ...] */
static int amps[16] = { 1000, 4000, 8000, 16000, 24000, 32000 };
static int NSTEPS = 6;
static volatile int step = -1;
static unsigned long pos;
static Uint32 last, maxGap, calls, late;

static void SDLCALL cb(void *u, Uint8 *s, int len) {
    Sint16 *o = (Sint16 *)s;
    int i, n = len / 4;
    Uint32 now = SDL_GetTicks();
    (void)u;
    if (calls++ && now - last > maxGap) maxGap = now - last;
    if (calls > 1 && now - last > 70) late++;
    last = now;
    for (i = 0; i < n; i++, pos++) {
        unsigned long t = pos % (22050 * 3);
        int st = (int)(pos / (22050 * 3));
        int v = 0;
        if (st < NSTEPS && t < 22050 * 2)
            v = (int)(amps[st] * sin(2.0 * M_PI * 440.0 * pos / 22050.0));
        o[i * 2] = o[i * 2 + 1] = (Sint16)v;
        step = st;
    }
}

int main(int argc, char **argv) {
    const char *drv = argc > 1 ? argv[1] : "ahi";
    SDL_AudioSpec w, h;
    SDL_AudioDeviceID d;
    int shown = -1, samples = argc > 2 ? atoi(argv[2]) : 1024, i;
    if (argc > 3) {
        for (NSTEPS = 0, i = 3; i < argc && NSTEPS < 16; i++) amps[NSTEPS++] = atoi(argv[i]);
    }
    if (SDL_AudioInit(drv) < 0) { printf("SDL_AudioInit(%s): %s\n", drv, SDL_GetError()); return 20; }
    SDL_zero(w);
    w.freq = 22050; w.format = AUDIO_S16SYS; w.channels = 2; w.samples = (Uint16)samples; w.callback = cb;
    d = SDL_OpenAudioDevice(NULL, 0, &w, &h, 0);
    if (!d) { printf("open: %s\n", SDL_GetError()); return 20; }
    {
        char v[16] = "";
        LONG r = GetVar((CONST_STRPTR)"SDL_AHI_UNIT", (STRPTR)v, sizeof(v), 0);
        printf("SDL_AHI_UNIT as GetVar sees it in this task: %s\n", r > 0 ? v : "(not set)");
    }
    printf("driver %s: %d Hz, %d ch, format 0x%x, %d frames a block\n", SDL_GetCurrentAudioDriver(), h.freq, h.channels, h.format, h.samples);
    fflush(stdout);
    SDL_PauseAudioDevice(d, 0);
    while (step < NSTEPS) {
        if (step != shown && step >= 0 && step < NSTEPS) {
            shown = step;
            printf("step %d: amplitude %d of 32767 (%.0f%%)\n", step + 1, amps[step], amps[step] * 100.0 / 32767);
            fflush(stdout);
        }
        SDL_Delay(50);
    }
    SDL_CloseAudioDevice(d);
    printf("callbacks %lu, longest gap %lu ms (a block is %d ms), gaps over 70 ms: %lu\n",
           (unsigned long)calls, (unsigned long)maxGap, h.samples * 1000 / 22050, (unsigned long)late);
    SDL_AudioQuit();
    return 0;
}
