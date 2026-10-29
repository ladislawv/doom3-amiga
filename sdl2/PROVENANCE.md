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
- `patches/0102-os3-text-input.patch` (this port, 2026-10-21): text input. The
  amigaos3 video driver sent no `SDL_TEXTINPUT`, and SDL2 programs take typed
  text only from it (Doom 3's console got Return but no letters). A key's press
  now also goes through the user's keymap (`keymap.library`'s `MapRawKey`, with
  the message's qualifiers and previous keys for dead keys); its printable
  characters (ISO 8859-1) go to SDL as UTF-8 while SDL's text input is active.
  Typing in Doom 3's console froze the A1200 after this patch: that was the
  spinlock livelock 0104 fixes, not this patch; with 0104, typing works.
- `patches/0103-os3-paula-no-spin.patch` (this port, 2026-10-22): the Paula
  driver's `WaitDevice` always sleeps, about a buffer's time at least, when
  audio.device hands a request back at once (an error) or no device is open,
  and the errors are reported when the device closes. Written for the menu
  freeze before its cause was known (0104); sound on its own.
- `patches/0104-os3-atomics-no-spin.patch` (this port, 2026-10-22): SDL's
  atomics without spinlocks, and `SDL_AtomicLock`'s wait a real sleep. The
  build has no native atomics (`HAVE_GCC_ATOMICS` off), so `SDL_atomic.c`
  emulated every atomic operation with one of 32 spinlocks, whose waiter loops
  on `SDL_Delay(0)`; exec always runs the highest-priority ready task, so
  `Delay(0)` gives nothing to a holder of lower priority. When the audio thread
  (priority 10) wanted the lock the game's task (0) held, it spun for ever and
  everything under priority 10 stopped: Doom 3's main menu froze the whole
  A1200 at random, 20 s to 14 min in (doom3 PLAN.md section 14, caught by a
  flight recorder). On AmigaOS 3 the atomics now run under `Forbid()`, atomic
  against every task on its one CPU, and `SDL_AtomicLock` sleeps a tick
  (`SDL_Delay(1)`) between tries. ogles2's `ports/sdl2` took the same fix as
  its `0005-os3-atomics-no-spin.patch` (2026-10-22, its comments without this
  port's name); when this port takes ogles2's copies again, 0104 goes.
- `ogles3/patches/0001-gles2-on-ogles2.patch`, `ogles3/patches/0002-os3-fixes-issue5.patch`
  and `ogles3/src/video/amigaos3/SDL_os3opengles2.[ch]` (2026-10-28) are copies of
  ogles3's `ports/sdl2` (the private ogles3.1 repository, to be github.com/ladislawv/ogles3)
  at commit `94eb81b` (ogles3.library 1.0-beta1), unchanged: ogles2's 0001, 0002 and backend
  with ogles3's names (`proto/ogles3.h`, `OGLES3Open`, `OGLES3_CCT_*`, `OGLES3_CFLAGS`), and
  OpenGL ES 3.0 and 3.1 contexts when a program asks for them. `OGLES=3 build_sdl2.sh` uses
  them in place of 0001, 0002 and `src/`; ogles3's 0003 and 0004 are the same files as
  this port's, its 0005 is this port's 0104 (comments aside), so the rest of `patches/`
  applies as it is, with the two ogles2 names in 0100's context lines renamed while it is
  applied.
