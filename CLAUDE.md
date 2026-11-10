# Doom 3 for AmigaOS 3: notes for whoever continues

Doom 3 (d3es-multithread, pinned in `sources.sh`) ported to AmigaOS 3 on a PiStorm
(Emu68, Raspberry Pi 4), drawing through ogles2.library (OpenGL ES 2.0 on the Pi's V3D).

**Read first:** [`PLAN.md`](PLAN.md): §4 (where things live), §8 (phases), §10 (performance
work) and §14 (status, newest last; its "Next, in order" list is the work queue). Every
measurement and decision is in §14 with the name of its results folder.

## Build machine

Ubuntu 24.04 in WSL on Windows; this repository on the Windows side (`C:\- CPP\doom3`),
clones and builds in WSL's home:
- `~/d3es` (the game, reset and patched by every build), `~/d3-sdl2` (SDL2_68k),
  `~/doom3-build` (output), `~/doom3-data/demo/demo00.pk4` (the free demo's data, §4).
- Needs ogles2's toolchain and build: GCC 16.2 for m68k in `~/amiga-gcc162-install`, and
  `~/ogles2-build` (ogles2.library, `v3dglslc`, `libogles2.a`, its headers) from the ogles2
  repository (its `dev/host-setup` and `CONTRIBUTING.md`).
- Order: `sources.sh` (clones, pinned), `build_deps.sh` (zlib, the allocator, libnix shims),
  `build_sdl2.sh`, `build_al.sh` (the OpenAL subset; after `build_deps.sh`, which clears its
  folder), then `build.sh` (A1200) and `build_host.sh` (x86-64 Linux, for reference) —
  or `tools/build_all.sh` for the last two.

## Changing the game (d3es)

Changes to d3es are patches (`patches/NNNN-name.patch`, applied in order), made in the WSL
clone, never in a Windows checkout:
1. `bash tools/patch/reopen.sh NNNN` (the clone with the patches before NNNN staged and NNNN
   open), or `reopen.sh 9999` for a new patch;
2. edit in `~/d3es`;
3. `bash tools/patch/export.sh NNNN-name` (writes the patch; keeps the old one in
   `~/doom3-build/patch-backup`);
4. `bash tools/patch/checkrest.sh NNNN` (the later patches must still apply);
5. `bash tools/build_all.sh`.

New files of the port (not d3es's) live in `src/` (the AmigaOS system layer `src/sys`, the
OpenAL subset `src/al`, the libnix additions `src/amiga`); SDL2 changes in `sdl2/patches`
(`sdl2/PROVENANCE.md`: 0001–0002 are ogles2's, 0100 on are the port's).

## Testing on the A1200

- `tools/a1200.py` is ogles2's amictl client (it needs ogles2's `tools/amictl/station.local`,
  the station's address and token, which is never committed). `python tools/a1200.py runpkg
  <package folder> -a "KEY=VALUE ..." -t <seconds>` uploads a package, runs its `Run-*`
  script and copies `results/` back into `tests/a1200/results/<package>-<time>/`.
- Packages (`tests/a1200/<name>`, built into `~/doom3-build/m68k/pkg/<name>`):
  - `bench`: builds compared (`tools/bench_pkg.sh "a:..." "b:..."`: fps, game, front end and
    back end per frame, stages, counts); `PCSAMPLE=1` adds the sampling profiler
    (`tools/pcsample_report.py`), `DUMP=1` the sound output dump (`tools/sndcmp.py`);
  - `play`: the game to play, until quit (`W= H= HZ= QUALITY=low|game SOUND=0|1`);
  - `map`: a screenshot WAIT frames in (`tools/map_pkg.sh`, `tools/tga2png.py`);
  - `start`, and the probes `clock-probe`, `io-probe`, `pc-sample`, `cxx-probe`.
- The game data is on the Amiga once (`Stuff:doom3/demo`), never in packages or commits.
- `tools/check_fpconv.py` checks the build for the float-to-int code GCC 6.5 got wrong.

## Rules

- Commit only when asked; the commit message ends with the agent attribution line.
- Never commit `station.local` or screenshots (`.gitignore` keeps pictures out of `results`).
- Before rebooting the Amiga or changing its system files, ask the developer.
- Do not modify sources outside this repository: d3es and SDL2_68k change through patches.
- ogles2 is a separate project: its plan and docs do not mention this port; change it only
  when the developer asks, and then in its own repository's way (its `CLAUDE.md`).
- When something is done, measured or decided, add it to `PLAN.md` §14 (and the section it
  belongs to); keep the "Next" list current.
- Since patch 0034 the Amiga build's tick rate always follows the frame rate (20-60 Hz) and
  loads DDS textures at every setting; `com_gameHz*` and `image_usePrecompressedTextures`
  change nothing there. Benchmarks before it ran at a fixed rate (30 Hz until 2026-10-29,
  then 20): compare across it with care. The Pi was overclocked on 2026-10-20 (§14):
  compare only within one clock.

## Traps that cost time

- PowerShell mangles `$`, `\` and quotes passed to `wsl bash -c`; Git Bash turns `/mnt/...`
  arguments into Windows paths. Write a script file and run `wsl -d Ubuntu-24.04 bash <file>`.
- `build.sh` and `build_host.sh` reset `~/d3es`: export edits first. `export.sh` on a clone
  not prepared by `reopen.sh` writes the wrong lines into the patch (a backup is kept).
- amictl does not upload empty files, and `-a` values may not contain `/` (a map name:
  edit `.def MAP` in the package's script).
- SDL2_68k's `SDL_getenv` sees only `SDL_setenv`'s values, and libnix's `getenv` only the
  shell's local variables (`Set`), neither `ENV:` (`SetEnv`): use dos.library's `GetVar`.
- The executables keep only global symbols: a profile's function also holds the static
  functions after it (`pcsample_report.py` prints each symbol's span).
- A fresh package is a first start (no `save/base/config.spec`): since patch 0033 the Amiga build
  then sets Low (Doom 3's preset, then `low.cfg` if the save directory has it), 1280x720, the
  frame rate shown, 12 lights, 20 Hz (before: "Ultra" detected from the memory). Settings on
  the command line override it. A quality is `+set com_machineSpec N +execMachineSpec` (the
  preset, then the port's `low/medium/high/ultra.cfg`); those files must not run
  `execMachineSpec` themselves. The game writes `dhewm.cfg` at every start's first frame.
- Each run's sound system is restarted once during start-up (the detection does it).
- Patch 0032 (the data prepared at the first start, PLAN.md §16) is off by default: the game
  loads the original pk4s and never asks. `+set fs_prepare 2 +set fs_usePrepared 1` prepares
  and loads `<game folder>/prepared/` (the A1200's `Stuff:doom3/base/prepared` has one).
