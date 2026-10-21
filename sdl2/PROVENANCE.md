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
- `patches/0101-os3-relative-mouse.patch` (this port, 2026-10-20): SDL's
  relative mouse mode for the amigaos3 video driver. The driver had none, and
  its `WarpMouse` is a stub, so SDL's fallback (warping the pointer back to the
  centre) did nothing: the hidden pointer stopped at the screen's edge, and so
  did a game's mouse look. Now the event pump sets `IDCMP_DELTAMOVE` on its
  windows while the mode is on, and sends Intuition's movement deltas as
  relative motion.
