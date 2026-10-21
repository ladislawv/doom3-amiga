# SDL2 for this port: where these files come from

- `patches/0001-gles2-on-ogles2.patch`, `patches/0002-os3-fixes-issue5.patch` and
  `src/video/amigaos3/SDL_os3opengles2.[ch]` are copies of ogles2's `ports/sdl2`
  at ogles2 commit `7f4b91d` (2026-10-19), unchanged. They apply to SDL2_68k
  (Steffen Haeuser) at `3632f48dc4d085355954322ae452ecc7b007ebc6`, which
  `sources.sh` clones into `~/d3-sdl2`.
- Licence: zlib, as SDL.
- This port builds them with GCC 16.2 and libnix (`Makefile.libnix`) in
  `build_sdl2.sh`; ogles2 builds them with GCC 6.5 and clib2. Changes this port
  needs go into new patches here (`0100-...`), not into the copies, so a newer
  ogles2 version can replace the copies.
- `patches/0003-os3-relative-mouse.patch` (SDL's relative mouse mode on AmigaOS, so mouse look
  turns freely) is ogles2's copy at commit `6455a35` (2026-10-21), unchanged. It was this port's
  0101 before; ogles2 took it over.
- `patches/0004-os3-ahi-format.patch` (this port's 0103 of 2026-10-21, now ogles2's copy at
  commit `33b96b1`, its comment without this port's name): the AHI audio
  driver's format search (`OS3AHI_OpenDevice`) kept the format after the one it
  matched: its `for` loop tested `!found` in the condition, so the increment ran
  once more after a match, and `AUDIO_S16MSB` became `AUDIO_S16LSB`. SDL then
  converted the application's big-endian samples to little-endian, and AHI,
  told `AHIST_S16S`, played them byte-swapped: noise at every level, on every
  AHI unit (a capture of what the driver sent with `CMD_WRITE` read as a clean
  tone only little-endian). The loop now stops at the match. SDL's Paula driver
  was not affected. ogles2's port had the same bug; it took the fix.
