# Doom 3 for AmigaOS 3 on ogles2.library: port plan

| | |
|---|---|
| Target | Doom 3 (later Resurrection of Evil) on 68k AmigaOS 3.2 with a PiStorm (Raspberry Pi 4, Emu68), drawn with ogles2.library 1.3 on the Pi's V3D 4.2 GPU |
| Base | [emileb/d3es-multithread](https://github.com/emileb/d3es-multithread) at `be1fa3f04eb2618c94ab0d502ab72f9af6267a04` (2024-04-20): dhewm3 1.5.3 WIP with a native GLSL ES 2.0 renderer |
| Licence | GPLv3 with id Software's additional terms, as the base. Game data is not included |
| Status | Planned (2026-10-20, second pass). Nothing built yet. §14 is the work queue |

---

## 1. Summary

- **The port uses d3es-multithread unchanged where it can and patches it where it must.**
  - It is dhewm3 with the d3wasm GLSL ES 2.0 back end, and the only Doom 3 renderer found that needs nothing beyond core OpenGL ES 2.0:
    - 16-bit indices;
    - stencil shadow volumes with `glStencilOpSeparate`, including Carmack's reverse;
    - user clip planes in the shader;
    - no depth clamp, no depth bounds.
  - Its only optional extension, `OES_packed_depth_stencil`, is in ogles2.library.
  - It asks SDL2 for an ES 2.0 context, which is exactly how ogles2's SDL2 backend is selected.
  - It builds the base game and Resurrection of Evil, can link the game code into the executable (`HARDLINK_GAME`), and leaves its render thread off by default.
- **Other GPLv3 trees supply what d3es lacks:**
  - [dhewm3](https://github.com/dhewm/dhewm3) 1.5.5: the precompressed (DDS) texture loader that d3wasm removed, and later fixes.
  - [d3wasm](https://github.com/gabrielcuvillier/d3wasm): running everything in one task (NOMT).
  - [ldhewm3-rearmed](https://github.com/Rinnegatamante/ldhewm3-rearmed) (PS Vita, built on d3es): CPU savings for a weak processor.
  - [HunoPPC's AmigaOS 4 dhewm3](https://github.com/HunoPPC-NG/Dhewm3-for-EGL_Wrap-AmigaOS4) and [DIII4A](https://github.com/glKarin/com.n0n3m4.diii4a): GLSL for heat haze and glass.
- **Why not the alternatives:**
  - HunoPPC's port needs his unpublished EGL_Wrap library, shared-object game libraries and pthreads.
  - dhewm3 through gl4es puts a translation layer on every draw. Quake III ran at 43 fps through gl4es and 80 fps on ogles2 directly.
  - RBDOOM-3-BFG needs GL 4 or Vulkan.
- **The main risk is CPU time.**
  - Natively on a Pi 400 (four ARM cores, Mesa), dhewm3 reaches about 13 fps in `timedemo demo1` at 1024x768.
  - Here everything runs as 68k code translated by Emu68's JIT on one ARM core, on the generic (non-SIMD) code paths: game logic, skinning, shadow silhouettes, collision, scripts and sound mixing.
  - The plan therefore gets a frame-time measurement on the A1200 as early as possible (phase 4) and makes it a go/no-go gate before the larger work.
- **Each problem is first solved on the PC.** An x86-64 build of the same tree in WSL, with the same options (one task, game linked in, the port's sound library), runs on Mesa's software renderer (llvmpipe). It separates "the port's changes are wrong" from "the 68k or big-endian build is wrong", and it provides the reference pictures.

## 2. Scope

- **In:**
  - Doom 3 single player at the 1.3.1 data level;
  - Resurrection of Evil as a second binary;
  - fullscreen on a 32-bit RTG screen, with SDL2 input;
  - sound through AHI.
- **Out:**
  - multiplayer and networking;
  - the editors and tools;
  - mods that need their own game code;
  - the BFG Edition data;
  - an AmigaOS 4 or MorphOS build.

## 3. Relationship to ogles2

- **ogles2 is an external dependency, used as released. This project never changes it.** It uses:
  - the installed ogles2.library and `LIBS:v3dglslc`;
  - `libogles2.a` and the headers from ogles2's build (`~/ogles2-build`) or its release SDK;
  - the `amictl` test station client.
- **The SDL2 patches are copied, not shared.** This project keeps its own copy of the SDL2 ogles2 backend files and patches (`sdl2/`, with their source commit noted). It builds them for libnix in its own SDL2 clone.
- **Requests to ogles2.** If a measurement shows that something only ogles2 can fix (a missing feature, a slow path), it goes into §13 as a request for the developer, with the measurement. This plan does not edit ogles2.

## 4. Where things live

- **Windows: `C:\- CPP\doom3`** (this folder) holds everything that is not a clone or a build:
  - `PLAN.md`, and `CLAUDE.md` (the short handbook: build order, the patch workflow, testing, rules, traps);
  - `sources.sh`, `build_deps.sh`, `build_sdl2.sh`, `build_al.sh`, `build.sh`, `build_host.sh`, `run_host.sh`, `bench_host.sh`;
  - `patches/` (changes to d3es, made against the WSL clone, LF line ends; `tools/patch/reopen.sh`, `export.sh`, `checkrest.sh`);
  - `src/` (new files: the AmigaOS 3 system layer `src/sys`, the OpenAL subset `src/al`, the libnix additions `src/amiga`);
  - `sdl2/` (SDL2 patches, `PROVENANCE.md`);
  - `tests/a1200/<package>/` and `tests/a1200/results/`;
  - `tools/` (`a1200.py`, `build_all.sh`, `bench_pkg.sh`, `map_pkg.sh`, `pcsample_report.py`, `sndcmp.py`, `tga2png.py`, `check_fpconv.py`);
  - `config/` (the default `autoexec.cfg`; not written yet: §10.4).
- **WSL (Ubuntu 24.04):**
  - `~/d3es`: the clone at the pinned commit, reset and patched on every build;
  - `~/d3es-ref`: dhewm3 1.5.5, for reference;
  - `~/d3-sdl2`: SDL2_68k at the commit ogles2's `ports/sdl2/build.sh` names;
  - `~/doom3-build`: output, with `m68k/` and `host/`.
- **Working rules:**
  - Patches are made against the WSL clone, never a Windows checkout (Windows git writes CRLF).
  - WSL commands run from a script file (`wsl -d Ubuntu-24.04 bash <script>`).
  - A1200 test scripts start with `Avail FLUSH` and record `List LIBS:ogles2#? LIBS:v3dglslc`.
  - Screenshots and the station token are never committed.
  - Ask before rebooting the Amiga or changing its system files.
- **Game data:** `base/pak000.pk4` to `pak008.pk4` from Doom 3 1.3.1 (the version GOG and Steam sell), and `d3xp/pak000.pk4`, `pak001.pk4`.
  - The developer copies them to the Amiga and to WSL (for the reference build) once.
  - They are never committed or put into packages.
  - The BFG Edition uses different archive and data formats and is not supported.
  - **Where to get it:**
    - **GOG's "DOOM 3"** (DRM-free) includes the original 2004 game with Resurrection of Evil, besides the BFG Edition. Use the original's `base/` and `d3xp/` folders.
    - **Steam's original "DOOM 3"** comes fully patched (dhewm3 FAQ). Its data can be copied from a Windows install or fetched with SteamCMD.
    - **The 2004 CDs/DVD** carry `base/pak000`–`pak004` and `d3xp/pak000`. The rest comes from the official 1.3.1 patch; the dhewm3 FAQ shows how to extract it from the Linux patch.
  - **Checking it:** compare the MD5 sums against the dhewm3 FAQ's list (https://github.com/dhewm/dhewm3/wiki/FAQ).
  - **Free demo:** dhewm3 1.5.1 and newer can run the free 2004 demo's data, so d3es (1.5.3 WIP) should too. It could serve for the first start-up tests, but its maps and demos differ, so the `demo1` measurements need the full game.
    - **Download:** `doom3-linux-1.1.1286-demo.x86.run` (485,257,318 bytes; SHA256 `b42260fd08feb13c2f035a3746f8c1e3472151f0f781b8a2d1da231dae818083`), for example from the GWDG mirror of id's FTP: https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/doom3/linux/
    - **Extracting it:** `sh doom3-linux-1.1.1286-demo.x86.run --tar xf demo/` in WSL gives `demo/demo00.pk4` (dhewm3.org).
    - **Where it goes:** d3es looks in `demo/` by itself when `base/` has no `default.cfg` (`FileSystem.cpp`). The full game's 1.3.1 pk4s must not be added; dhewm3 warns that they break the demo.
    - **Contents of `demo00.pk4`** (483,535,485 bytes, MD5 `70c2c63ef1190158f1ebd6c255b22d8e`):
      - 12,234 files;
      - three maps (`game/demo_mars_city1`, `demo_mars_city2`, `demo_mc_underground`);
      - 4,079 `dds/` textures (so the precompressed path can be tested), 3,771 TGAs;
      - `default.cfg`;
      - **no recorded demos.**

      Until the full game is here, a demo recorded on the host build (`recordDemo`) can serve as the benchmark.
- **Data locations:** WSL: `~/doom3-data/demo/` (and later `base/`, `d3xp/`); the downloaded installer stays in `~/downloads/doom3-demo/`. Amiga: next to the executable, copied in phase 3. amictl's `put` takes at most 64 MB per file, so a pk4 goes up in pieces joined with `Join` there, or is copied by hand.

## 5. Platform facts the plan relies on

Checked for this plan (2026-10-20) or taken from ogles2's documentation:

- **GCC 16.2.0b with libnix and libstdc++ (`~/amiga-gcc162-install`):**
  - It has C++. Both toolchains are built `--enable-threads=no` (thread model "single"), so libstdc++ has no `std::thread` or `std::mutex`.
  - ogles2's dEQP-GLES2 port (C++17, exceptions not disabled) is built with it and runs on the A1200. Doom 3 needs exceptions: `common->Error` throws `idException`.
  - GCC 16.1 miscompiles and must not be used. `-fno-strict-aliasing` is required.
- **Float-to-int conversion:**
  - With `-m68020 -m68881`, GCC 16.2 converts with `fintrz` (checked).
  - With `-m68040` it switches the FPU's rounding mode around every conversion: three FPCR moves, because the real 68040 lacks `fintrz`. Emu68 implements `fintrz`.
  - Doom 3 converts floats to ints constantly, so the port uses `-m68020 -m68881`.
  - GCC 6.5 has an optimiser pass that deletes the `fintrz` (ogles2's notes). Whether 16.2 has it is checked on the build's assembly (phase 3).
- **The Amiga hunk format cannot align a static object beyond 8 bytes.** d3es uses `ALIGN16` (`__attribute__((aligned(16)))`) about 270 times. The generic SIMD code does not need 16 bytes, so the port defines `ALIGN16` as 8-byte alignment. `Mem_Alloc16` and `_alloca16` align by hand and are unaffected.
- **SDL2_68k** (Steffen Haeuser's, the one ogles2 uses):
  - It has threads, timers, AHI and Paula audio, RTG screens and input.
  - Its `SDL_endian.h` sets `SDL_BIG_ENDIAN` for `__m68k__`, which is what idlib's byte swapping and the savegame header key on.
  - It ships `Makefile.libnix`, and ogles2's SDL2 patch already covers that makefile. A libnix build is therefore the same patch built with GCC 16.2.
- **ogles2.library 1.3:**
  - GLSL compiled on the Amiga: about 1 s to start the compiler once, then about 0.2 s per program. Shader records are kept and loaded later, and a program's shipped records are found in `PROGDIR:shaders/` first.
  - S3TC (DXT1, DXT3, DXT5) sampled directly; `glCompressedTexSubImage2D` on 4x4 block bounds.
  - `OES_packed_depth_stencil`, depth textures, `OES_element_index_uint`, `OES_mapbuffer`, anisotropic filtering, MSAA 4x.
  - `glCopyTexSubImage2D` done on the GPU (used for `_currentRender`: heat haze, glass).
  - Vertex data byte-swapped per attribute layout. Buffers marked `GL_STREAM_DRAW` or `GL_DYNAMIC_DRAW` are converted per draw for the vertices read; `GL_STATIC_DRAW` buffers get one converted copy per layout, of the whole buffer.
- **Machine:** about 1.9 GB fast RAM, all GPU-visible, no virtual memory. Quake III (ioquake3's GLES2 renderer, 640x480) runs at 80 fps, CPU-bound, and at 56 fps with sound.

## 6. What d3es needs changed (read from its source)

| Area | d3es today | Port |
|---|---|---|
| Threads | Render thread optional (`r_multithread`, default 0); the 60 Hz async tic runs in an `SDL_AddTimer` callback (`framework/Common.cpp`, `AsyncTimer`); background download thread | All in the main task: the async tic called from the main loop, as d3wasm's NOMT does; download thread compiled out |
| Game code | DLLs, or `HARDLINK_GAME` (one game per binary) | `HARDLINK_GAME`; two binaries |
| System layer | `sys/posix`, `sys/linux`; also `sys/aros` (Kalamatee's AROS port, about 1,500 lines on `dos.library` and `exec.library`) | `src/sys/amigaos3/` from the AROS files, with `sys/glimp.cpp`, `sys/events.cpp` and `sys/threads.cpp` kept (SDL2) |
| Sound | OpenAL 1.1 with optional EFX: about 25 core `al*`/`alc*` calls (sources, buffers, streaming queues, listener, gain and pitch), EFX through `alGetProcAddress` | Own small OpenAL subset (D7) |
| Textures | TGA to RGBA8 or RGBA4444, optional ETC1 encoding on the CPU; no DDS path | dhewm3's `CheckPrecompressedImage`/`UploadPrecompressedImage` restored for `dds/` images as S3TC (D6). The interaction shader already reads the normal's x from alpha (`.agb`), as Doom 3's RXGB normal maps store it, so no shader change |
| Vertex upload | Per-frame temporary buffers (2 MB each), filled on the CPU and sent once a frame in `BeginBackEnd` with `glBufferData(..., GL_STREAM_DRAW)`, only the bytes used. Static model data goes through `Position()` with `GL_STATIC_DRAW` (`renderer/VertexCache.cpp`) | Nothing at first: ogles2 converts stream buffers per draw for the vertices read and static ones once per layout. Measured in phase 4 (§10.1) |
| Alignment | `ALIGN16` on about 270 declarations | 8-byte alignment (§5) |
| Debug drawing | `tr_rendertools.cpp`'s desktop GL calls (`qglBegin` and others) | Nothing: they are inside `#if 0` (checked) |
| Depth copy | `CopyDepthbuffer` calls `glCopyTexImage2D(GL_DEPTH_COMPONENT)`, which ES 2.0 does not allow | Nothing: nothing calls it (checked) |
| Machine spec | `com_machineSpec` from RAM size: 1.9 GB gives "Ultra" | Fixed defaults in the port's `autoexec.cfg` (§10.4) |

## 7. Decisions

| # | Question | Decision |
|---|---|---|
| D1 | Base | **d3es-multithread**, pinned. dhewm3 code is taken as patches where needed |
| D2 | Compiler, C library, flags | **GCC 16.2, libnix (`-noixemul`)**, `-O2 -mcpu=68020 -m68881 -fno-strict-aliasing`, C++ with exceptions and RTTI as d3es needs. If idlib needs it, `int32_t` as `int`, as dEQP does. `-m68060` and `-O3` on hot files tried in phase 5 |
| D3 | System layer | **SDL2 (libnix build)** for window, GL context, input, timers and audio; `src/sys/amigaos3/` from d3es's AROS code for the rest |
| D4 | Threads | **None in the engine.** `r_multithread 0`; async tic from the main loop. The only other task is SDL's audio task, which touches only the sound library's mixer state |
| D5 | Game code | **Linked in**: `doom3` (base) and `doom3-roe` (d3xp) |
| D6 | Textures | **Precompressed DDS as S3TC**; TGA as RGBA8 for the rest; ETC1 off |
| D7 | Sound | **A small OpenAL 1.1 subset written for the port** (`src/al/`; written 2026-10-20, §14): exactly the calls d3es uses, mixing in SDL2's audio callback (AHI) at 22,050 Hz stereo by default, with distance attenuation, stereo panning, pitch, looping and queued streaming (Ogg music and speech). EFX reports itself unavailable, which d3es supports (`s_useEAXReverb` 0). Why: OpenAL Soft needs `std::thread`/`std::mutex`, which this libstdc++ lacks, and does far more per sample than this CPU can spare. Sound stays off (`s_noSound 1`, which skips OpenAL) until phase 7 |
| D8 | Picture size | **640x480 32-bit screen**, rendered at `r_framebufferWidth`/`Height` (for example 320x240 or 480x360) and scaled up when the frame time asks for it |
| D9 | Verification | **The x86-64 host build first** for every functional change; the A1200 for byte order, speed and the real GPU |

## 8. Phases

Effort: S (days), M (one to two weeks), L (longer). Each phase ends with its exit check passing and its results written into §14.

### Phase 1: the host build (M)

1. `sources.sh`: d3es at the pinned commit into `~/d3es`, dhewm3 1.5.5 into `~/d3es-ref`, SDL2_68k into `~/d3-sdl2`.
2. `build_host.sh`: d3es for x86-64 Linux with:
   - `HARDLINK_GAME`, `BASE` only, `r_multithread` off;
   - the async tic in the main loop;
   - SDL2 from Ubuntu, an ES 2.0 context on Mesa's llvmpipe (`~/mesa-install`, or Ubuntu's Mesa).
   - The build machine's WSL is version 1 on Windows 10, with no display. The host build therefore runs under `xvfb-run`, and its pictures come from Doom 3's `screenshot` command.
   - Packages: `xvfb libsdl2-dev libopenal-dev` (OpenAL Soft only for the host reference sound).

   This is the build the port's patches are written against first.
3. Patches made and tested here:
   - `0001-single-task`: the async tics from the main loop, no render or download thread, CMake option `SINGLE_TASK`;
   - the DDS loader;
   - the stubs (rendertools, depth copy).

   `ALIGN16` is an Amiga-only change and comes with the platform patch in phase 3.
4. **Exit:**
   - `timedemo demo1` runs to the end on the host, or, with the demo data only, a demo recorded on the host;
   - screenshots at fixed demo frames are saved as the reference set (`tests/reference/`, not committed if they are large; their place noted);
   - the DDS path is shown working (`listImages` names `dds/` sources).

### Phase 2: libraries and the C++ probe on the A1200 (S)

1. **SDL2 for libnix:** `~/d3-sdl2` with `sdl2/` patches (copied from ogles2's `ports/sdl2`, the commit noted), built with `Makefile.libnix` and GCC 16.2. If its MiniGL backend needs PiStorm3D's SDK, it is used as ogles2's build uses it, or the MiniGL backend is left out (the port needs only ES).
2. **zlib** built with GCC 16.2 and libnix.
3. **C++ probe** (`tests/a1200/cxx-probe`), which:
   - throws an exception through several frames;
   - uses `alloca` of 1 MB;
   - sets an 8 MB stack through libnix's `__stack` and checks it;
   - times `new` and `delete` for 100,000 small blocks;
   - times `fopen`/`fseek`/`fread` of a 300 MB pk4 in 64 KB reads;
   - opens an SDL2 ES window that clears to a colour;
   - plays a one-second tone through SDL2 audio.
4. **Exit:** the probe passes on the A1200, and its timings are in §14.

### Phase 3: it builds and starts on the A1200 (M)

1. **`build.sh`** for 68k:
   - a CMake toolchain file (GCC 16.2, libnix, D2 flags);
   - `HARDLINK_GAME=ON`, `BASE=ON`, `D3XP=OFF`, `DEDICATED=OFF`, no curl;
   - OpenAL replaced by `src/al/` (at first a stub that fails `alcOpenDevice`).

   If d3es's CMake fights the cross-compiler, the script compiles its file list itself.
2. **`src/sys/amigaos3/`:**
   - `main`;
   - paths (`fs_basepath` = `PROGDIR:`, `fs_savepath` = `PROGDIR:save`);
   - `Sys_Milliseconds` through SDL;
   - memory size from `AvailMem(MEMF_FAST)`;
   - CPU reported generic;
   - console to `stdout` and a log file;
   - no clipboard, URL or network (single player);
   - a stack-size check at start.
3. **Assembly check:** the build compiles to assembly too and fails on a float-to-int conversion without `fintrz`. The check is the same idea as ogles2's `tools/check_fpconv.py`, copied into this project.
4. **Exit:**
   - `doom3 +set s_noSound 1` starts on the A1200, mounts the pk4s, runs `listImages`, `com_showFPS 1` and `quit` from the command line;
   - free memory after quitting is within 1 MB of before (after `Avail FLUSH`).

### Phase 4: pictures and the CPU gate (M–L)

1. **The menu draws** on a 640x480 32-bit screen with depth 24 and stencil 8.
   - The 20 GLSL programs (`neo/renderer/glsl/`) compile on the Amiga, which should take under 10 s on a first run.
   - Their records go into `PROGDIR:shaders/` for the release.
   - A program the compiler refuses is logged and goes to §13.
2. **Maps load:** `map game/mars_city1`, then `game/alphalabs1`.
3. **Byte-order check:** A1200 screenshots at the reference frames compared with the host's (phase 1). Differences are explained or fixed.
4. **Measure** `timedemo demo1` at 640x480, and at 320x240 through `r_framebufferWidth`, with:
   - `com_speeds 1` (game, renderer front end, back end);
   - `r_skipBackEnd 1` (the CPU side alone);
   - ogles2's log for its draw time and the GPU time.
5. **The gate,** after the cheap fixes of §10.1 and §10.4:
   - **at least 15 fps** at 640x480, or at least 20 fps at 320x240: continue;
   - **below 8 fps** in both: stop, and write down where the time goes and what would be needed;
   - **in between:** phase 5 first, then decide again.

### Phase 5: performance (L)

The work of §10, each change measured alone with `timedemo demo1` and kept only if it helps.

### Phase 6: textures, memory, loading (M)

1. The DDS path on the A1200: texture memory before and after, and map load time.
2. Free fast memory logged at start, after each map load and after `disconnect`, over five map changes. If libnix's allocator is slow (phase 2) or the memory fragments, a dlmalloc-style allocator over a few large `MEMF_FAST` blocks goes behind idlib's `Mem_Alloc`.
3. Load time:
   - larger file reads;
   - the shader records shipped;
   - optionally a script the user runs once to repack the pk4s uncompressed.

### Phase 7: sound (M)

1. `src/al/` written and tested on the host first, against the same d3es build and against OpenAL Soft's output for the same demo.
2. On the A1200:
   - `demo1` with sound, the frame time compared;
   - mixing rate, the sound library's voice limit and its resampling quality set so mixing costs at most about 3 ms a frame.
3. Music and speech (Ogg Vorbis through stb_vorbis, already in d3es) decoded in small pieces each frame, so no frame stalls.

### Phase 8: completeness and release (M)

1. **Effects d3es does not draw** (stages written as ARB fragment programs: heat haze, glass, the enviro-suit view): GLSL ported from HunoPPC's `base/gl2progs` or DIII4A, checked on the host first.
2. **Resurrection of Evil:** `doom3-roe` (`D3XP=ON`, `BASE=OFF`), with its maps loaded and its timedemo measured.
3. **Release archive:**
   - both executables, the shader records in `shaders/`, `autoexec.cfg`, icons (from d3es's AROS `.info` sources), a readme;
   - the stated requirements (ogles2.library version, the 32-bit screen mode, the stack);
   - instructions for copying `base/` and `d3xp/` from the user's own Doom 3.

   Source under GPLv3. Check the licence terms of the libraries linked into the binary (libnix, SDL2, `libogles2.a`) as was done for Quake III.
4. **Tests on the A1200:**
   - `demo1` (the benchmark, short);
   - each map loaded and held about a second, then the next (the watchable test);
   - a game saved and loaded.

## 9. Testing

- **Host (WSL):** the reference build of phase 1. Every patch is made here first, and the reference screenshots and the reference sound output come from here.
- **A1200 packages** (`tests/a1200/<name>`, run with `amictl runpkg`):
  - `cxx-probe` (phase 2);
  - `start` (phase 3);
  - `demo1` (timedemo at the chosen sizes, with `com_speeds`, the log copied back);
  - `maps` (each map held about a second, screenshots at fixed points);
  - `save` (save, quit, start, load).

  The game data is already on the Amiga; packages carry only the binaries, configuration and scripts. Benchmarks run short. Visible tests hold each state about a second and say on the Shell what to look for.
- **Results** in `tests/a1200/results/<package>-<date>/`, summarised in §14.

## 10. Performance work

### 10.1 Vertex data

- d3es fills each frame's dynamic geometry (skinned models, deforms, GUI surfaces, shadow volumes) into 2 MB temporary buffers and sends them once a frame with `glBufferData(..., GL_STREAM_DRAW)`, only the bytes used (`idVertexCache::BeginBackEnd`).
  - ogles2 keeps no converted copy of a stream buffer: each draw converts only the vertices it reads.
  - Static model data (`Alloc`, uploaded in `Position()` with `GL_STATIC_DRAW`) is converted once per attribute layout and kept.
  - (The first version of this plan read the static path as the per-frame one and planned a patch; reading `BeginBackEnd` showed it is not needed.)
- An interaction draw reads up to six attributes of `idDrawVert`: position, texture coordinates, normal, two tangents and colour.
- **Measure:**
  - client arrays (`r_useVertexBuffers 0`) against both;
  - how often the same vertices are converted twice (the interaction pass reads a surface once per light).

  If that cost is large, the request to ogles2 (§13) is a whole-buffer conversion for layouts that are all 32-bit words. The port would store `idDrawVert`'s colour bytes reversed, so the buffer is uniform.

### 10.2 CPU savings from ldhewm3-rearmed

Its commits were reviewed on 2026-10-20 (§14, "the Vita port reviewed": what a 68k can use, in three groups, and patch 0020). These are ported without its `__vita__`, vitaGL and NEON parts:
- dormant entities behind closed portals;
- lower animation rates for distant entities;
- cached shadow volumes for static lights and models;
- cached pathfinding;
- buffered file reads.

### 10.3 Hot spots

To be profiled before any change:
- `R_CreateShadowVolume` and the turbo-shadow path (`r_useTurboShadow`);
- `idSIMD_Generic` skinning (`TransformJoints`, `TransformVerts`), `DeriveTangents` and `CreateShadowCache`;
- `idClip` and collision;
- the script interpreter;
- float-to-int conversions.

Emu68 charges per memory access: in ogles2's measurements `movem` moved four longs about five times faster than single moves. Tight copy and transform loops are the place for that.

### 10.4 Settings

The port's `autoexec.cfg` sets:
- `com_machineSpec` to a fixed value;
- `image_downSize`, `r_framebufferWidth`/`Height`;
- `r_multithread 0`, `r_useETC1 0`;
- `s_numberOfSpeakers 2`;
- vsync off for measurements, on for play.

## 11. Byte order checklist

- **Engine data files:** pk4 (zip), TGA, DDS headers, WAV, ROQ, demos and savegames go through idlib's `LittleLong`/`LittleShort`/`LittleFloat`. These were written for the PowerPC Mac build and kept by dhewm3.
- **Code d3es added since dhewm3:** the GLSL back end (uniforms, attribute pointers), `framebuffer.cpp`, the ETC1 encoder (off) and the vertex cache. Grepped in phase 3 for raw reads of little-endian data and for byte views of words.
- **Vertex data:** `idDrawVert` is 15 floats and 4 colour bytes; shadow vertices are 4 floats. ogles2 swaps floats per attribute and leaves bytes alone, so no change is needed.
- **Savegames** record `SDL_BYTEORDER` and refuse the other order: saves do not move between PC and Amiga.
- **Test:** the A1200 pictures against the host's at the same demo frames (phase 4).

## 12. Risks

| Risk | Effect | Mitigation |
|---|---|---|
| CPU too slow (JIT, no SIMD, one core) | Unplayable | Measured in phase 4 before the large work; §10; low render size |
| The libnix SDL2 build differs from the tested clib2 one | Phase 2 delay | Same patch, already covering `Makefile.libnix`; fall back to an own context on `agl*` for the GL part only |
| libnix allocator slow or fragmenting | Stalls, failed loads | Timed in phase 2; own allocator (phase 6) |
| Large `alloca`, deep recursion | Crash on image load or in AI | 8 MB stack from the start; checked at start-up |
| Code d3es added never ran big-endian | Wrong pictures or data | §11; host reference pictures |
| Float-to-int code from the compiler | Wrong values (the GCC 6.5 pass) or slow (FPCR switching) | `-m68020 -m68881`; the assembly check in every build |
| Vertex conversion cost in ogles2 | High back-end time | §10.1; request to ogles2 only with a measurement |
| A shader the on-target compiler refuses | Missing surfaces | Logged; 20 small programs, checked in phase 4 |
| Own OpenAL subset sounds wrong | Poor sound | Compared against OpenAL Soft on the host |
| About 1.5 GB of data on the SD card | Slow loads | DDS textures; optional uncompressed pk4s |

## 13. Requests to ogles2

Requests go here only with a measurement behind them; the developer decides whether ogles2 takes them.

1. **(2026-10-20) Per-draw CPU time with Doom 3's draw pattern: 25.6 µs a draw, 27.3 ms a frame.** Measured with probe P: 1,066 draws a frame, mostly `glDrawElements` with 16-bit indices from a `GL_STREAM_DRAW` buffer re-sent once a frame, or from static buffers, 5–7 attributes, 5 textures. The largest stages per frame:
   - uniforms 7.4 ms: the interaction program has about 13 uniforms, most of them changed every draw;
   - vertex data 5.8 ms;
   - checks 5.3 ms.

   Anything that lowers the fixed cost per draw helps this game directly. Raw data: `tests/a1200/results/bench-*/results/ogles2.log` (the `OGLES2PROBE=1` runs).
2. **(2026-10-20) A swap without waiting for SDL's full-screen windows.**
   - **The cost:** SDL2's ogles2 backend creates its context with `OGLES2_CCT_WINDOW`, also for a full-screen window, which is a borderless backdrop window covering its own screen. Window mode renders into an offscreen bitmap and `ClipBlit`s it at the swap, so the swap waits for the finished frame. On the A1200 that is 16.2 ms of Doom 3's 128 ms frame with vsync off, 13.6 ms of it the render wait.
   - **The request:** when a window's bitmap is its screen's and nothing can cover it (a borderless backdrop window the size of its screen, as SDL opens for full screen), could ogles2 store straight into the screen bitmap and return at submission, as `OGLES2_CCT_MODEID` does? Every SDL2/SDL3 full-screen program would gain.
   - **The alternative:** the port's copy of the SDL backend could use `OGLES2_CCT_MODEID` itself and take SDL's input from ogles2's window (`OGLES2_CCT_PISTORM_GET_WINDOW`). That is more intricate: two screens, and SDL's event code reading another window.
   - **Done in ogles2 (2026-10-20, uncommitted there):** a window that is all of its private 32-bit screen is rendered into directly; the swap returns at submission (ogles2's §7.2; `ENV:OGLES2/WindowCopy 1` keeps the copy). The SDL backend needs no change. Results under §14, "the swap without the copy".

## 14. Status and next steps

**Done (2026-10-20): survey and plan.**
- **Candidates read:**
  - d3es-multithread, d3wasm, ldhewm3-rearmed, DIII4A, dante, dhewm3 1.5.5, RBDOOM-3-BFG;
  - HunoPPC's AmigaOS 4 dhewm3: gl4es through EGL_Wrap to AmigaOS 4's ogles2.library, about 30–40 fps at 1080p on an X5000, source up to 1.5.3 beta (2022).
- **d3es-multithread chosen (D1).** §6 was read from its source at the pinned commit.
- **Checked in WSL:**
  - both toolchains have C++ with a single-thread libstdc++;
  - GCC 16.2 emits `fintrz` with `-m68020 -m68881` and FPCR switching with `-m68040`;
  - SDL2_68k's endian header and `Makefile.libnix`;
  - ogles2's SDL2 patch covers that makefile.

**Done (2026-10-20): repository.** This folder is the private GitHub repository `ladislawv/doom3-amiga` (branch `main`, LF line ends by `.gitattributes`; game data, builds and screenshots ignored).

**Done (2026-10-20): demo data in WSL.**
- `doom3-linux-1.1.1286-demo.x86.run` came from the GWDG mirror; its SHA256 matched, and so did the payload's MD5 in the installer header.
- The installer script was not run: the gzip tar after its 373-line header was unpacked with `tar`.
- `demo/demo00.pk4` is in `~/doom3-data/demo/` (§4).
- Not yet on the Amiga: the station did not answer.

**Done (2026-10-20): implementation started; phase 1 written, phase 2 built.**
- **Scripts:**
  - `sources.sh`: d3es `be1fa3f`, dhewm3 1.5.5 `455b88e`, SDL2_68k `3632f48`;
  - `build_host.sh`, `run_host.sh` (xvfb-run, llvmpipe);
  - `build_sdl2.sh`;
  - `tests/a1200/build_cxx_probe.sh`.
- **The clones are reset with `git reset --hard` and patched in order.** Each finished patch is staged in the clone, so `git diff` shows only the next one.
- **`patches/0001-single-task.patch`** (CMake option `SINGLE_TASK`, `D3_SINGLE_TASK`):
  - `Async()` is called from `Frame()` and `GUIFrame()`, with no `SDL_AddTimer`;
  - the session's wait for the next tic (`Session.cpp`, which waited on `TRIGGER_EVENT_ONE`) runs `Async()` itself;
  - no background download thread (only multiplayer pak downloads use it), and its `Sys_DestroyThread` is guarded;
  - `multithreadActive` is always false.
- **`patches/0002-precompressed-textures.patch`:**
  - `CheckPrecompressedImage`/`UploadPrecompressedImage` come from dhewm3 1.5.5, reduced to S3TC: DXT1/3/5, and RXGB for normal maps. A non-RXGB normal map, an uncompressed, palettized or BC7 file, or a GPU without S3TC falls back to the TGA.
  - Also added:
    - `glConfig.textureCompressionAvailable` (`GL_EXT_texture_compression_s3tc`, or DXT1 plus ANGLE's DXT3/DXT5);
    - the cvar `image_usePrecompressedTextures` (1);
    - DXT sizes in `StorageSize`, DXT names in `listImages`.
  - ES 2.0 has no `GL_TEXTURE_MAX_LEVEL`, so a mip chain that stops above 1x1 uses level 0 only.
  - Not compiled yet (no SDL2 development files in Ubuntu-24.04). Every changed file except `Common.cpp` passes `g++ -fsyntax-only`.
- **Corrections to this plan from reading the code:**
  - the per-frame vertex buffers are already `GL_STREAM_DRAW` (§10.1);
  - the depth copy and the desktop GL debug drawing need no stubs (§6).
- **SDL2 for libnix** (`build_sdl2.sh`): `libSDL2.a` (1 MB) built with GCC 16.2.
  - `sdl2/` holds copies of ogles2's two patches and its backend (`sdl2/PROVENANCE.md`, ogles2 `7f4b91d`), plus this port's `0100-gcc16-libnix.patch`. SDL2_68k's libnix makefile had not been built with GCC 14 or later. The patch:
    - defines `HAVE_STDLIB_H` (calls to undeclared stdlib functions are errors now, and an implicit `malloc` would be wrong code on 68k);
    - guards the `u_int32_t` typedef in `libm/math_private.h` against newlib's;
    - adds `<clib/alib_protos.h>` (`BeginIO`) and `SDL_timer.h` (`SDL_GetTicks`).
  - The GCC 16.2 toolchain has no AHI or CyberGraphX headers. The script copies them from the GCC 6.5 toolchain into the build's include folder, without changing either toolchain.
  - The remaining warnings are `printf` formats given `long` for `int` (`int32_t` is `long` on this toolchain; both are 32 bits).
- **C++ probe** (`tests/a1200/cxx-probe`, package in `~/doom3-build/m68k/pkg/cxx-probe`):
  - Checks: C++ runtime, FPU, byte order, the 8 MB stack and a 1 MB `alloca`, `new`/`delete` timing, a 256 MB `malloc`, large-file reads, an SDL2 ES window clearing red, green and blue for a second each, and a one-second AHI tone.
  - Found while building it:
    - libstdc++ needs `__xpg_strerror_r`, which libnix lacks (`src/amiga/libnix_shims.c`);
    - libnix's `swapstack.o`, which honours `__stack`, is linked only with `-Wl,-u,___stkinit`. Without it `__stack` silently does nothing.
  - **Run in vamos** (`-C 68040`; `-C 68020` stops at libnix's FPU alert): static constructors, an exception through three frames with destructors, `dynamic_cast`/`typeid`, STL, float-to-int truncation, maths, byte order and sizes all pass. The stack, SDL and GL checks need the A1200: vamos leaves `tc_SPLower`/`tc_SPUpper` at 0, which is also why the stack swap crashes there, and has no `CreateNewProc`.
- **Waiting on:**
  - `xvfb libsdl2-dev libopenal-dev` in Ubuntu-24.04 (they went into Ubuntu-22.04, WSL's default);
  - the A1200 station, for the probe;
  - a wrapper for `amictl.py` (used read only), so that Doom 3 packages' results land in this repository rather than ogles2's.

**Done (2026-10-20): the host build runs, with seven patches.**
- **Unpatched d3es** (`NOPATCH=1`) builds and draws `demo_mars_city1` on llvmpipe under Xvfb.
  - Trap: WSL 1 passes the Windows `PATH` through, and CMake found curl in `C:\- CPP\vcpkg-master`. `build_host.sh` now drops `/mnt/*` from `PATH` and disables curl.
  - d3es writes screenshots as RGBA data under a 24-bit TGA header. `tools/tga2png.py` reads both that and the fixed layout.
- **The patches:**

  | Patch | What it does |
  |---|---|
  | `0001-single-task` | One task: no timer, render or download thread. Checked: the opening's monitor screen advances, so game time runs from the main loop |
  | `0002-precompressed-textures` | The DDS loader (described above), now also reporting with `developer 1` why it declines an image |
  | `0003-screenshot-tga` | Screenshots packed into the 24-bit BGR their header says; d3es's row-stride formula was wrong for odd widths |
  | `0004-image-formats` | `GL_RGBA` (what d3es stores for TGA images) and `GL_RGB` (framebuffer copies) known to `StorageSize`/`listImages`; framebuffer copies and cinematics record their format; a texture name with nothing uploaded counts 0 bytes. Without it `listImages` stopped with "BAD FORMAT" |
  | `0005-demo-data-loading-gui` | The free demo's data has no `guis/map/loading.gui`: demo playback crashed on a NULL pointer |
  | `0006-timedemo-time-limit` | `com_timeDemoSeconds N` ends a timedemo after N seconds and reports the frames rendered by then (for the retail `demo1`) |
  | `0007-benchmark-seconds` | `com_benchmarkSeconds N`: from the first frame after a map has loaded, count frames for N seconds, print `benchmark: F frames rendered in S seconds = X fps`, quit. Needs no demo file; `bench_host.sh` runs it |
- **Precompressed textures are switched off by Doom 3's "Ultra" preset.** `com_machineSpec` 3, which any machine with 1 GB or more gets (and the Amiga's 1.9 GB would too), sets `image_usePrecompressedTextures 0` (`Common.cpp`). The port's `autoexec.cfg` must set it to 1 (§10.4). With it on:
  - `demo_mars_city1`: 2,147 images, 2,062 from DDS (998 DXT1, 611 DXT3, 453 DXT5);
  - `demo_mc_underground`: **106.2 MB of textures against 458.0 MB** from TGAs, a factor of 4.3. The two pictures look the same, so the RXGB normal maps are read correctly.
- **Host benchmark** (`bench_host.sh`, 30 s of `demo_mars_city1`'s opening, 640x480, llvmpipe on 32 threads): **1,269 frames in 30.0 s = 42.3 fps.** Map load 1.7 s; 34 s in all.
- **Open: d3es's render demos do not play back.**
  - A demo recorded by this build (`recordDemo`) stops at once with "demo hash index out of range", in `playDemo` and `timeDemo` alike.
  - The demo code is dhewm3's apart from the version-5 "nospecular" header that d3es lacks, so the fault is more likely in what d3es's split front end and back end write.
  - Whether id's retail `demo1` plays is not known until the full data is here. Until then `com_benchmarkSeconds` measures instead.

**Done (2026-10-20): the C++ probe on the A1200; phase 2's exit check met.**
- **`tools/a1200.py`:** ogles2's `amictl.py` used as it is, with results written here (`tests/a1200/results/`). Run with `python tools/a1200.py runpkg <package> -t <s>`.
- **The packages carry `libs/ogles2.library`:** the test machine has none in `LIBS:`.
- **Run 1 (stack swap linked):** the Amiga crashed and rebooted. Its output was lost, because it was buffered.
- **Run 2, in steps with unbuffered output:**
  - Without the swap, everything passes except the stack (200,000 bytes).
  - With libnix's swap (`-Wl,-u,___stkinit`), every check passes, including 8 MB and a 1 MB `alloca`, but the program hangs after its last line, in libnix's stack-swap exit code. It ignores Ctrl-C and stays as a process until the next reboot.
- **Decision:** no libnix stack swap. The Shell's `Stack 9000000` (and, later, the icon's stack) gives the stack; the program checks it at start.
- **Run 3** (`tests/a1200/results/cxx-probe-2-20261020-133252`), 14.3 s, every check passes:
  - C++ runtime: static constructors, an exception through three frames with destructors, `dynamic_cast`/`typeid`, STL;
  - float-to-int truncation, `sqrtf`/`sinf`/`atan2f`, byte order, sizes;
  - stack 9,000,000 bytes and a 1 MB `alloca`;
  - a 256 MB `malloc`;
  - SDL2's GLES 2.0 context: "V3D 4.2.14 (PiStorm), 8 QPUs, 64 KB VPM", depth 24, stencil 8, with red, green and blue cleared and shown;
  - SDL audio: 22 callbacks in one second at 22,050 Hz. **SDL chose its Paula driver, not AHI**: the sound phase must ask for AHI.
- **libnix's `free` is slow:** 100,000 small blocks take 79 ms to `new` and **4,358 ms to `delete`** in mixed order, 44 µs a block. Doom 3 allocates and frees all the time, so **its own allocator (§9) moves from "if needed" to required.**
- **Open:**
  - Free fast memory was 8.8 MB lower after the run than before. This may be ogles2's GPU set-up, which it never frees by design, or a leak; to be told apart by running twice.
  - AmigaDOS `If` has no `LE`. `Run-Probe` now uses `If NOT {FROM} GT n VAL`.

**Done (2026-10-20): zlib and the port's allocator.**
- **`build_deps.sh`** builds `~/doom3-build/m68k/deps`: zlib 1.3.1's `libz.a`, and `libd3amiga.a`, the port's support library.
  - `sources.sh` fetches zlib and dlmalloc 2.8.6 into `~/downloads/doom3-deps` and checks their SHA-256.
- **`libd3amiga.a`:**
  - `src/amiga/d3malloc.c`: dlmalloc with prefixed names, no locks, no mmap. It takes non-contiguous 4 MB segments from libnix's `malloc`, so libnix frees them at exit, and never gives them back before that.
  - `src/amiga/d3new.cpp`: every `operator new`/`delete` form on it.
  - `src/amiga/libnix_shims.c`.
- **SDL and libnix keep libnix's `malloc`,** so SDL's audio task and the game never share an allocator and no lock is needed.
- **Link traps:**
  - `-Wl,-u,___xpg_strerror_r` is needed, because libstdc++ comes after `-ld3amiga` on the link line.
  - dlmalloc's `MORECORE(0)` must return the end of the last segment, as `sbrk(0)` would. While it returned failure, the first `new` threw `bad_alloc` and the A1200 run aborted (`cxx-probe-3`).
- **Host test** (`tests/host/d3malloc_test.c`): 2,000,000 random operations including `realloc`, contents checked, no errors.
- **A1200** (`tests/a1200/results/cxx-probe-4-20261020-134239`), every check passes:

  | 100,000 small blocks | Allocate | Free (mixed order) |
  |---|---|---|
  | The port's allocator | 16 ms | 26–28 ms |
  | libnix | 72–75 ms | 4,334–4,464 ms |

  About 160 times faster freeing. The random load (200,000 operations of 1 byte to 64 KB, contents checked) takes 2.3 s.
- **The 8.8 MB:** each run ends 8.8 MB lower, but the next run starts higher than the last one ended, so the memory comes back. It is held a while after the run, probably by ogles2.library until it is expunged. Not a leak of the program's.

**Done (2026-10-20): phase 3, the 68k build links.** `build.sh` gives `~/doom3-build/m68k/game/dhewm3`: 7.9 MB, Release, base game linked in, no undefined references.
- **`patches/0008-qgl-only`:** `VertexCache.cpp` and `framebuffer.cpp` called about 60 GL functions directly; now all go through the `qgl*` pointers. On the Amiga every GL function comes from `SDL_GL_GetProcAddress`. Host benchmark unchanged: 39.7 fps at 640x480, 30.2 fps through the 320x240 framebuffer.
- **`patches/0009-amigaos3-build`:**
  - CMake:
    - AmigaOS detected from `CMAKE_SYSTEM_NAME`;
    - OpenAL's headers and d3es's `openal_stub.cpp` in place of the library;
    - no `GLESv2` library;
    - the system sources `sys/amigaos3/*`;
    - `AMIGA_LIBS`/`AMIGA_LDFLAGS` from the build;
    - minizip with `USE_FILE32API` (libnix's `stdio.h` does not declare `fseeko` to C);
    - no `-fvisibility`, no `-fPIC` on idlib.
  - `platform.h` gets an `__amigaos3__` block.
  - `fs_caseSensitiveOS` 0, `:` as a directory separator, and the CD-key path as on AROS.
  - `BuildOSPath` joins `Work:` + `base` without a `/`.
  - `RenderSystem.cpp` includes `<unistd.h>` (`usleep`).
- **`cmake/m68k-amigaos.cmake` and `cmake/Platform/AmigaOS.cmake`:**
  - GCC 16.2, `-mcpu=68020 -m68881 -noixemul`;
  - `-std=gnu++17`, because GCC 16 defaults to C++20, where `requires` is a keyword and Doom 3 uses it as a name;
  - no PIC flags, since `-fPIC` output (`@GOT(a4)`) does not assemble;
  - try-compiles without linking.
- **`src/sys/amigaos3`:**
  - `amiga_main.cpp`:
    - paths: data, configuration and saves next to the executable (`save/`), as `Sys_GetPath` gives them;
    - a stack check at start, with a requester when the stack is under 4 MB;
    - console to `stdout` and `save/dhewm3log.txt`, flushed every line;
    - `Sys_Error` closes SDL and shows a requester;
    - no DLLs, processes or URLs.
  - `amiga_os.c`/`amiga_os.h`: the AmigaDOS and exec calls behind a C interface, so the AmigaOS headers' macros (`Read`, `Write`, `Printf`) never meet Doom 3's names. It covers program directory and name, `Lock`/`ExNext` listing, `CreateDir`, dates, `AvailMem`, the stack size and `EasyRequest`.
  - `amiga_net.cpp`: no sockets (single player); loopback addresses only.
- **`tools/check_fpconv.py`** checks the disassembly per register. The game's 645 float-to-integer stores all truncate with `fintrz`. The 205 without it are in SDL2 (`-m68040`, which switches the FPU rounding mode instead: correct, slower).
- **Packages and data:**
  - `tests/a1200/build_pkg.sh <name>` adds the game and ogles2 release 1.3's `ogles2.library` and `v3dglslc` in `libs/`, where ogles2 looks when `LIBS:` has none.
  - `tests/a1200/start/Run-Start`: start, initialise, quit.
  - `tools/a1200_put_large.py` uploads a file of any size in 48 MB pieces and `Join`s them on the Amiga (amictl's PUT stops at 64 MB; AmigaOS 3.2 makes directories with `MakeDir ALL`). The game data goes to `Stuff:doom3`, which has 3.8 GB free.

**Done (2026-10-20): phase 3's exit check met: the game starts and quits on the A1200** (`tests/a1200/results/start-20261020-141241`, 94 s with sound off; the developer watching).
- **What start-up does on the A1200:**
  - mounts the demo pk4 from `Stuff:doom3` (12,234 files);
  - opens a 640x480 RTG screen through SDL2 with depth 24 and stencil 8;
  - creates the GL ES 2.0 context on ogles2: "V3D 4.2.14 (PiStorm)", with `EXT_texture_compression_s3tc`, `OES_packed_depth_stencil`, `OES_texture_npot` and anisotropy 16;
  - compiles all 11 GLSL programs on the Amiga (ogles2's `v3dglslc` from the package's `libs/`);
  - initialises the game: 142 classes; `script/doom_main.script` compiles in 7.1 s;
  - starts the session, shuts down cleanly and exits.
- **Two crashes found and fixed on the way:**
  1. **amiga.lib's `sprintf` in place of the C library's.** `-lamiga` was on the link line before libnix, and amiga.lib's `sprintf` is `RawDoFmt`, with 16-bit `%d`. The first `sprintf` crashed before `main` wrote anything; the `vamos` emulator showed it (`RawDoFmt` reading a bad string pointer). The driver already links amiga.lib after libnix, so `build.sh` and the probe no longer name it.
  2. **Static destructors in the wrong order at exit.** GCC on this target puts them in the DTOR list, which libnix runs in link order, whereas Linux uses `__cxa_atexit` (reverse construction order), and Doom 3's static objects rely on that. SmartCrash caught the crash after the game had shut down cleanly (a trace showed `exit()` reached and the `atexit` handlers starting). The fix: `src/amiga/cxa_atexit.c` in `libd3amiga.a`, a `__cxa_atexit` and `__dso_handle` that libnix lacks, run from one `atexit` handler, and `-fuse-cxa-atexit` for the game. After that, no crash at exit.
- **Also learnt:**
  - `vamos` runs the game as far as `SDL_Init`, which needs `CreateNewProc`; a stack size of 0 (`vamos`) now counts as unknown.
  - Each run ends 8.8 MB lower in free memory, as the probe does (ogles2 holding it until it is expunged).
  - My earlier reading of a "double shutdown" was two files' tails printed one after the other.

**Done (2026-10-20): the first map on the A1200, and the phase 4 measurement** (`tests/a1200/map`, demo data, sound off, the developer watching).
- **`demo_mc_underground` loads in 77–78 s** (4.6 s of it collision data); on the host it takes 1.7 s. The screen stays black with the mouse pointer while it loads, and the station does not answer meanwhile.
- **The A1200's screenshot matches the host's:** the door with normal maps, per-pixel lighting and the HUD. Byte order, the DDS textures and the GLSL programs are right on the real GPU.
- **Frame rate, 10 s counted after loading:**
  - 640x480: **81 frames = 8.0 fps** (`map-20261020-141750`);
  - rendered at 320x240 and scaled up: **79 frames = 7.9 fps** (`map-20261020-142230`).

  The picture size makes no difference: the game is CPU-bound.
- **Where a frame goes** (`com_speeds`, last 40 frames at 320x240):

  | Part | ms per frame |
  |---|---|
  | Game frame | 88 |
  | Renderer front end | 11 |
  | Renderer back end, including ogles2's draw calls | 42 |
  | Total | 150 |

  The game runs fixed 60 Hz tics (`USERCMD_HZ` is a constant in d3es). At about 125 ms a frame it runs 7–8 tics per frame to keep up, so game logic takes over half the CPU. dhewm3's `com_gameHz` exists only in its high-fps branch, commented out in 1.5.5.
- **The gate (§7, phase 4):** 8 fps is at the lower limit, in the "in between" band: phase 5 (performance) first, then decide again. The first target is the cost of one game tic, not rendering.

**Done (2026-10-20): the game tic profiled on the A1200** (`Run-Map PROFILE=1`: `g_timeEntities 2`, `g_frametime`, `com_showTics`; `map-20261020-142752`).
- **Right after loading, a tic costs 45–55 ms.** Two ragdolls lying in the map (`env_ragdoll_zsecm_0`, `env_ragdoll_maint_0`) take 6–7 ms each and the gibs 2 ms each, while they settle.
  - On the host the same entities show up in the first 7 tics only (`g_timeEntities 0.2`), then come to rest.
  - On the A1200 they had settled by game time 4.8 s too, so this is not a port fault.
- **Once settled, a tic costs about 30 ms of CPU** for 16.7 ms of game time, with 225 entities. The largest are `player1` (2–3 ms), `crane_main` (2 ms) and `swinglight_mover` (2 ms); the other entities cost about 0.1 ms each and add up to the rest.
  - The game logic alone needs about 1.8 times real time.
  - The engine then runs slower than real time (game time falls behind) rather than dropping tics.
- **The 10 s benchmark window covers only the first few seconds of game time,** because game time runs slower than real time. Measurements must start after the ragdolls have settled, or run longer.
- **Host, same map:** 54.5 fps (llvmpipe).

**Done (2026-10-20): phase 5, compiler flags measured; the game frequency made variable.**
- **Benchmark (patch 0007):**
  - `com_benchmarkWarmup` (seconds before counting starts) and a summary line: per frame the game, front-end and back-end times, and game tics per second. `com_speeds` prints these every frame, which costs time on the Amiga; the summary does not.
  - The renderer's times reach it only when `EndFrame` is given somewhere to put them, so `UpdateScreen` now passes them while the benchmark runs too.
- **`tests/a1200/bench`:**
  - runs each `dhewm3-<build>` in the package in turn (`bench-one`, one line per build written by `List LFORMAT`, since AmigaDOS has no loops);
  - a `<build>.args` file adds console commands for that build;
  - `build.sh` takes `AMIGA_OPT` (in place of d3es's Release flags, through patch 0009) and `OUT_NAME`;
  - `build_pkg.sh bench <builds...>`.
- **Compiler flags on the A1200** (`demo_mc_underground`, 10 s warm-up, 30 s counted):

  | Build | fps | Game time per frame | Game tics a second |
  |---|---|---|---|
  | d3es Release (`-O2 -fno-math-errno -fno-trapping-math -fomit-frame-pointer`) | 4.9 | 114.9 ms | 48.9 |
  | `-O2 -ffast-math -fomit-frame-pointer` | 4.9 | 113.2 ms | 49.2 |
  | `-O3` with d3es's other flags (1 MB larger) | 5.0 | 107.4 ms | 49.8 |

  `-O3` saves about 6% of game time and `-ffast-math` 1.5%; neither is the lever. (`-ffast-math` turns `pow(2, x)` into `exp2f`, which libnix lacks; it is in `libnix_shims.c` now.)
- **A tic costs about 11.5 ms once the map has settled:** 49 tics a second at about 10 per frame. The earlier 30 ms included `g_timeEntities`' printing. About 90 ms of each ~200 ms frame is rendering.
- **`patches/0010-game-hz`:** dhewm3's `kickstart-my-hertz` branch (`com_gameHz` 10–480).
  - The branch's diff since its merge base, without d3xp, ImGui and its settings menu, applied with `--reject`. Of 20 rejected hunks, the ones about soft particles, the version string, display refresh rates and macOS clocks were left out.
  - In `Common.cpp`, the `com_gameHz` check and `Com_UpdateFrameTime` were put in by hand. The branch's async thread replaces the SDL timer, kept under `#ifndef D3_SINGLE_TASK`.
  - `Sys_MillisecondsPrecise`/`Sys_SleepUntilPrecise` are new per-platform functions (`Sys_Milliseconds` is now inline on them). The POSIX ones come with the branch; the Amiga ones are in `amiga_main.cpp`, on SDL's performance counter.
  - Host: `com_gameHz 30` gives 29.9 tics a second and 29.1 fps (frames are drawn when a tic is ready); 60 Hz gives 58.4 tics and 38.3 fps.

**Done (2026-10-20): 30 Hz game measured on the A1200; shadows; a run-order effect.**
- **`com_gameHz` 60 against 30** (one build under two names; both ran after three other builds in the same package):

  | `com_gameHz` | fps | Game | Front end | Back end | Game tics a second |
  |---|---|---|---|---|---|
  | 60 | 4.8 | 116.8 ms | 17.2 ms | 69.6 ms | 48.1 |
  | 30 | 7.1 | 49.2 ms | 17.1 ms | 71.9 ms | 30.0 |

  30 Hz gives 48% more frames, and the game keeps up with real time.
- **In a package of their own, with `hz30` first:**
  - `hz30`: **9.6 fps** (game 35.3, front end 12.1, back end 53.4 ms);
  - `r_shadows 0`: **10.5 fps** (31.2 / 11.6 / 49.3 ms).

  Stencil shadows cost only about 9%.
- **The same build ran 7.1 fps in one package and 9.6 fps in another.** It was not the run order: the developer was moving the mouse during some runs, and the camera's direction changes the frame rate a lot. Since then `bench-one` turns mouse look off (`sensitivity 0`; d3es has no `in_mouse`). Comparisons made before that point carry this uncertainty.
- `tools/a1200.py runpkg` now deletes the package's directory on the Amiga first: amictl uploads into it without removing an earlier package's files, and one bench ran the previous package's builds too.
- **Host:** about 1,080 draws a frame in this scene (`r_showPrimitives 2`). At ogles2's roughly 30 µs a draw, that accounts for most of the back end. Each interaction draw makes 11–13 uniform calls (four of them per-light values that do not change between surfaces) and 5 texture binds.
- `Run-Bench OGLES2PROBE=1` turns on ogles2's probe P: every 256 frames, the draws, their time and its stages, in `results/ogles2.log`.

**Done (2026-10-20): measurements repeat; the baseline.**
- With mouse look off, two identical runs of the 30 Hz build gave 7.0 fps each:

  | Run | fps | Game | Front end | Back end |
  |---|---|---|---|---|
  | `hz30a` | 7.0 | 49.5 ms | 17.2 ms | 72.3 ms |
  | `hz30b` | 7.0 | 49.8 ms | 17.3 ms | 72.5 ms |

- **Run order does not matter; the camera does.** The faster runs before that (9.1–10.5 fps) had the camera turned towards a cheaper view, and the 9% for shadows is uncertain for the same reason.
- A task list (`tools/amiga/tasks.c`: every exec task and process with its state, which `Status` does not show) found nothing left running after a run.
- **The baseline for comparisons:** `demo_mc_underground`, 30 Hz, mouse look off, 7.0 fps.

**Done (2026-10-20): the back end split by ogles2's probe P** (`Run-Bench OGLES2PROBE=1 SECS=90`; 30 Hz, 6.8 fps). Steady state, frames 256–768, per frame:
- **The frame:** 146 ms (6.8 fps), 1,066 draws.
- **ogles2, inside `glDrawElements`: 27.3 ms** (25.6 µs a draw):

  | Stage | ms per frame |
  |---|---|
  | Uniforms | 7.4 |
  | Vertex data | 5.8 |
  | Checks | 5.3 |
  | Binning list | 2.5 |
  | Texture records | 2.4 |
  | Indices | 1.6 |
  | Shader record | 1.4 |
  | Shaders | 0.7 |

- **Waiting for the GPU** (ogles2's "render wait" per job): **13.6 ms**.
- `glBufferData` 0.8 ms; texture uploads none.
- **That leaves about 32 ms of the back end's ~74 ms in Doom's own back end** and its other GL calls: 11–13 uniform calls, 5 texture binds and the attribute set-up per interaction, about 20,000 GL calls a frame through ogles2's dispatch.
- The first report (frames 0–256) includes the loading screen, so a measurement needs at least 512 frames.

**Done (2026-10-20): `patches/0011-uniform-cache`.**
- The GLES2 back end's `GL_Uniform*` helpers remember, per program, the last value of each uniform field (`glColor` to `clipPlane`, 24 slots of up to 16 floats in `shaderProgram_t`) and skip calls that would not change it.
- Per-light values (light origin, light projection, view origin) and the colour modulation were sent again for every surface.
- The cache is cleared when a program's locations are fetched (load and `reloadGLSLprograms`). The sampler uniforms, set directly, are not cached fields.
- Host: the same picture.

**Done (2026-10-20): the uniform cache, the back end's stages, vsync** (A1200, 30 Hz, mouse look off; baseline 7.0 fps):

| Change | fps | Game | Front end | Back end |
|---|---|---|---|---|
| Uniform cache (patch 0011) | 7.1 | 49.0 | 17.3 | 71.2 |
| Uniform cache, rendered at 320x240 | 7.3 | 48.0 | 17.3 | 68.6 |
| Uniform cache, `r_swapInterval 0` | **7.8** | 45.4 | 17.8 | 60.6 |

- **The uniform cache saves about 1 ms;** the GL calls are not where Doom's back end spends its time.
- **`patches/0012-backend-stage-timers`:** `rb_stageMsec` around the stages of `RB_RenderView` and the swap, reported by the benchmark per frame. With vsync on (d3es's `r_swapInterval` defaults to 1):

  | Stage | ms per frame |
  |---|---|
  | Interactions (with stencil shadows) | 32.4 |
  | **Swap** | **28.5** |
  | Shader passes | 4.9 |
  | Depth fill | 4.5 |
  | View set-up, fog, post-processing | 0.5 |

- **With vsync off the swap is 16.2 ms.** That is about ogles2's render wait (13.6 ms) plus the copy.
  - SDL's ogles2 backend uses ogles2's **window mode**, an offscreen bitmap copied with `ClipBlit`. ogles2's own §7.2 says its swaps wait for the finished frame, while its full-screen mode (`OGLES2_CCT_MODEID`) returns at submission and overlaps the next frame's CPU work with the render.
  - So a full-screen path could save most of the 16 ms (§13, request 2).
- **`r_swapInterval 0` belongs in the port's defaults:** at these frame rates vsync only adds waiting.
- **Per frame, at 7.8 fps:**
  - game 45 ms (3.85 tics of 11.8 ms);
  - front end 18 ms;
  - interactions 34 ms;
  - swap 16 ms;
  - the rest of the back end 10 ms.

**Done (2026-10-20): the swap without the copy** (§13, request 2, done in ogles2). The same `dhewm3-novsync` build (`com_gameHz 30`, `r_swapInterval 0`), demo_mc_underground, 10 s warm-up, 30 s counted, ogles2 built from its working tree:

| ogles2 path | fps | game | front end | back end | swap |
|---|---|---|---|---|---|
| copy (`ENV:OGLES2/WindowCopy 1`), `bench-20261020-170441` | 8.1 | 43.1 ms | 17.8 ms | 58.5 ms | 16.3 ms |
| direct, `bench-20261020-170942` | **9.8** | 36.1 ms | 17.6 ms | 44.4 ms | **1.7 ms** |

- ogles2's log says "application window, all of its screen (no copy), 640x480".
- The game's time fell too (43 to 36 ms), probably because the CPU work now overlaps the render.
- The Emu68 beta's video memory is allocated dynamically: its size setting (32 or 128 MB) makes no difference (the developer).

**Done (2026-10-20): the game tic profiled by class; the clock made 11 times cheaper.**
- **`patches/0013-game-profile`:** `g_profileThink 1` sums, per game tic, the parts of `idGameLocal::RunFrame` (before the think, the think, `idEvent::ServiceEvents`, the rest) and, nested inside them, `idEntity::RunPhysics`, `idAnimatedEntity::UpdateAnimation`, `idEntity::Present` (the render world's entity updates) and `idThread::Execute` (scripts), counted once when nested (`neo/game/GameProfile.h`). Every think is timed and summed by entity class (one clock read per entity). The game's `gameProfile reset|print` command; the benchmark resets the profile when it starts counting and prints it before it quits. The benchmark also prints the clock reads per frame (Amiga only).
- **The first profile run measured mostly the clock:** 2.6 fps, 321 ms of game per frame, because one read of SDL's performance counter cost 20.7 µs.
- **`tests/a1200/clock-probe`** (`build_clock_probe.sh`, `Run-Clock`; `clock-probe-20261020-173807`), 10,000 calls each:

  | Clock | µs a read |
  |---|---|
  | Emu68's free-running counter (ARM `CNTPCT`, 54 MHz), `MOVEC` 0xe1/0xe2 through exec's `Supervisor()` | **1.7** |
  | timer.device `ReadEClock` (SDL2_68k's `SDL_GetPerformanceCounter`) | 19.7 |
  | timer.device `GetSysTime` | 20.4 |

  The counter agreed with the E-clock over `Delay(50)` (1,005 ms each) and never went backwards. Emu68 documents these control registers in `docs/internals/ControlRegisters.md`; `MOVEC` is privileged there as on a 68k.
- **`src/sys/amigaos3`:** `Sys_MillisecondsPrecise` (and so `Sys_Milliseconds`) reads Emu68's counter (`amiga_counter_hz`/`amiga_counter_read` in `amiga_os.c`; SDL's counter when `devicetree.resource`, Emu68's, is missing).
- **A1200, `bench-20261020-174123`** (same settings as above):

  | Build | fps | game | front end | back end | clock reads per frame |
  |---|---|---|---|---|---|
  | `ReadEClock` (before) | 9.9 | 36.1 ms | 17.4 ms | 44.5 ms | (not counted) |
  | Emu68 counter | **10.3** | 33.5 ms | 16.8 ms | 43.3 ms | 130 |
  | Emu68 counter, `g_profileThink 1` | 9.3 | 44.7 ms | 16.7 ms | 43.5 ms | 3,302 |

- **Where a game tic goes** (the profile run: 13.8 ms a tic, about 2 ms of it the profile's own clock reads at 2 µs each; 3 tics a frame):
  - the entities' think 13.4 ms; before it, the events and the rest together 0.4 ms;
  - inside the think: physics 6.8 ms, render world updates 1.1, animation 0.5, scripts 0.3;
  - by class: `idMover` 5.1 ms (45 thinks a tic, 0.115 ms each), `idPlayer` 2.8 ms, `idLight` 2.2 ms (14 a tic, 0.16 ms each: lights bound to moving machinery, updated in the render world every tic), `idAI` 1.2 ms (51 thinks, 0.02 each), `idAFAttachment` 0.8 ms, every other class under 0.3 ms.
  - So in this map the tic is the moving machinery (movers and the lights on them) and the player. ldhewm3-rearmed's animation and AI savings (§10.2) would win little here; its mover savings are the ones that later needed fixes there (movers stuck in callbacks, airlocks out of step).

**Done (2026-10-20): the profile by entity; render world updates only before a frame is drawn.**
- **The profile by entity** (patch 0013 prints the 30 entities with the most think time; `bench-20261020-175816`), per tic: `player1` 2.7 ms; `swinglight_mover` 1.9 ms; `underground_window_security_1` (`idAI`, the guard behind the window) 0.9 ms; then about 40 movers and lights at 0.1–0.2 ms each (`fly_around*`, `canismover*`, pistons, `crane_main`, the lights bound to them).
  - `swinglight_mover` is a brush mover carrying `env_swinglight_long_wbulbs` (`idAFEntity_Generic`: 5 bodies, 5 universal joints, self-collision), a light and two effects. A mover runs its whole team's physics, so the articulated figure's simulation is charged to it, and the figure never rests because the mover keeps swinging. It is the map's content; left as it is.
- **`patches/0014-present-last-tic`:** at 30 Hz a frame runs about three game tics, and every moving entity and light updated its render world definitions on each of them, though only the last update is ever drawn. `com_presentLastTic 1` (the default; 0 as before):
  - the session sets `com_moreTicsBeforeDraw` (`framework/Game.h`) for every tic but the last of a frame;
  - `idEntity::DeferPresent`, asked first by `idEntity::Present` and every override that updates the render world (`idLight`, `idItem`, `idMultiModelAF`, `idAFEntity_Gibbable`, `idSecurityCamera`), returns at once on those tics; the entity keeps `TH_UPDATEVISUALS`, so it stays active and its Think presents it on the last tic;
  - for the entities whose own Think does not present (some are presented by direct calls, e.g. `target_*` entities), `idGameLocal::PresentDeferred` presents the remembered ones still waiting at the end of the frame's last tic, and also at the end of a tic that ends the frame early (a skipped cinematic);
  - not through `gameLocal.isNewFrame`, which also gates one-shot effects (sounds, decals) in multiplayer prediction.
- **A1200** (`bench-20261020-180854`, one build, the same settings as above):

  | `com_presentLastTic` | fps | game | front end | back end |
  |---|---|---|---|---|
  | 0 (every tic) | 9.9 | 36.0 ms | 17.1 ms | 44.9 ms |
  | 1 | **10.9** | **27.2 ms** | 16.8 ms | 45.0 ms |

  An entity or light update frees its derived render data (interactions, shadows) and links it into the areas again, so the saving (8.8 ms a frame) is larger than the profile's "render world updates" (1.1 ms a tic) suggested.
- **The picture:** `Run-Map` now takes `WAIT` (frames before its screenshot) and reads `dhewm3.args` like `bench-one`. The game's own screenshots 150 frames in, with 0 and with 1 (`map-20261020-182949`, `map-20261020-182514`), show the same scene: 0.74% of the bytes differ, 175 of 921,600 by more than 16 levels (animation at slightly different game times); nothing is missing. amictl's screenshots of the running game are black in both settings (the loading screen comes out): its capture, not the game.
- Not explained yet: in the map's first 10 seconds (`Run-Map`, no warm-up) the game made 4,882 clock reads a frame, against about 130 later.

**Done (2026-10-20): the front end's time by stage.**
- **`patches/0015-frontend-stage-timers`:** `rf_stageMsec` (`tr_local.h`) around the stages of `R_RenderView` (the outermost view only: a subview's is inside its parent's `R_GenerateSubViews`), and inside them around `InstantiateDynamicModel`, `idInteraction::CreateInteraction`, the deferred `R_CreateLightTris`, and, in `R_AddModelSurfaces`' entity loop, the entity scissors, the ambient draw surfaces and the active interactions. The benchmark prints two lines of them. The timers read the clock only while the benchmark counts (`rf_timeStages`; about 500 reads a frame, 1 ms), so a game played normally does not pay for them.
- **A1200** (`bench-20261020-183746` with the first timers: 11.1 fps, front end 17.0 ms; `bench-20261020-184346` with the finer ones, which add about 480 clock reads a frame, about 1.4 ms), per frame:

  | Front end | ms |
  |---|---|
  | model surfaces (`R_AddModelSurfaces`) | 15.6 |
  | … active interactions added | 9.6 |
  | … … of which interactions created (light triangles, shadow volumes) | 2.3 |
  | … ambient draw surfaces | 5.4 |
  | … … of which dynamic model snapshots (skinning, deforms) | 0.9 |
  | … entity scissors | 0.3 |
  | light surfaces | 1.2 |
  | visible lights and entities | 0.8 |
  | the rest (frustum, sort, draw command) | 0.7 |

  So most of the front end is the per-frame walk over interactions and surfaces that already exist (about 7 ms of interactions and 4.5 ms of ambient surfaces), not rebuilding them. In `AddActiveInteraction` that walk is the view-frustum cull and shadow scissor of each shadowing entity (`CullInteractionByViewFrustum`, `CalcInteractionScissorRectangle`), a box cull per surface (`R_CullLocalBox`), and linking the surfaces into the light's lists; in the ambient surfaces, a box cull and material register evaluation per surface.

**Done (2026-10-20): the front end's interactions counted; interaction scissors; texture bindings cached.**
- **Counts** (patch 0015, `bench-20261020-185727`), per frame: 264 interactions visited, 193 of them through the view-frustum cull and shadow scissor (`CullInteractionByViewFrustum`, `CalcInteractionScissorRectangle`: 4.3 ms, about 18 µs each net of the timers); 577 light surfaces and 99 shadow surfaces linked; 253 ambient draw surfaces (about 4.5 ms, 18 µs each).
- **`r_useInteractionScissors`** (one build, `bench-20261020-190250`): 2 (the default) 10.5 fps, front end 19.3 ms (with the timers); 1 the same; **0: 10.8 fps, front end 16.5 ms**, back end 0.3 ms more (125 shadow surfaces against 99). On this machine a draw costs CPU, not fill rate, so the looser scissor costs little: a candidate for the port's defaults (§10.4).
- **`patches/0016-texture-bind-cache`:** d3es had dropped Doom 3's texture-binding cache, so every `idImage::Bind` called `glBindTexture`, and an interaction draw made 5 `glActiveTexture` and 5 `glBindTexture` calls, each a library call into ogles2, though the light's falloff and projection textures stay the same for all of its surfaces. `backEnd.glState.tmu2D`/`tmuCube` remember each unit's binding; `Bind`/`BindFragment` skip what is bound; `GL_BindImage(unit, image)` selects the unit only when its texture changes, and the interaction draw uses it. Forgotten (`GL_ForgetTextureBindings`) where GL changes bindings outside it: a deleted texture (`PurgeImage`), the framebuffer's own texture (`framebuffer.cpp`), the default GL state. The game's screenshot 150 frames in matches the one before (`map-20261020-192749`: 0.68% of the bytes, 142 by more than 16 levels, as between any two runs). `r_useBindCache 0` binds as before, for comparison. **A1200** (`bench-20261020-195925`, one build): `r_useBindCache 1` **10.8 fps**, interactions 31.2 ms a frame; `0` 10.6 fps, 32.3 ms. About 1 ms of the back end; the rest of an interaction draw's 54 µs (577 light surfaces) is ogles2's draw (about 26 µs, §13) and the back end's own work per surface (light and view origins and light planes into the surface's space, the stage loops, uniforms, six attribute pointers).

**Done (2026-10-20): map load 240 s to 92 s; compressed textures confirmed.**
- **Compressed textures are used:** the runs set `image_usePrecompressedTextures 1` (§10.4: the "Ultra" preset would turn them off); 1,789 of the map's 1,801 images load from DDS (S3TC), uploaded and kept compressed by ogles2 (the TMU samples BC1–BC3). Their upload takes 0.4 s in all.
- **Where the time went** (`com_timestampPrints 2`, `map-20261020-191732`): from the start to a playable map 239.7 s, of which **69.8 s opening `demo00.pk4`** (484 MB, 12,234 files, all deflated), 17.7 s compiling the game's scripts, the map load 136.5 s.
- **`tests/a1200/io-probe`** (`build_io_probe.sh`, `Run-IO`; `io-probe-20261020-194304`; `Stuff:` is PFS3), on the pk4: open and close 0.30–0.36 ms; dos.library random Seek and a 64-byte Read **0.47 ms**, libnix stdio's **3.2 ms** (its buffer is 64 KB, refilled after every seek); a random seek and a 64 KB read 3.1 ms; reading straight on 21–26 MB/s. minizip reads the central directory, and each file's local header, a few bytes at a time after a seek: with stdio, each of those cost a 64 KB disk read.
- **`patches/0017-zip-read-cache`** (`neo/framework/minizip/ioapi.c`): the zip streams read through a cache of their own: a seek only moves the position, small reads come from a block read at once, reads of a block or more go straight to the caller. On AmigaOS the file is read through dos.library (8 KB blocks, no Seek when a read goes on from the last), and the streams on one file share its handle (one task; each read seeks for itself), so a file opened in a pk4 costs no Open. Elsewhere stdio with 64 KB blocks. Streams opened for writing are unchanged.
- **Patch 0002** opens a DDS file once (its timestamp from the open file) instead of once to check it and once to read it.
- **`patches/0018-load-times`:** after "msec to load", one line with the time and count of images (DDS, image files, uploads), models (with their images), sounds and files opened in pk4s, and the time of each step of the map load's end (`EndLevelLoad`s, the 10 settling game frames).
- **A1200:**

  | From the start to | before | first cache (64 KB, stdio) | now |
  |---|---|---|---|
  | pk4 opened | 69.8 s | 0.7 s | 0.7 s |
  | map initialisation | 103.1 s | 30.7 s | 18.6 s |
  | **playable map** | **239.7 s** | 144.0 s | **91.5 s** |

  Now (`map-20261020-194658`): the map load 72.9 s; 1,789 DDS images 16.4 s (9.2 ms each), models 14.7 s with their images, 3,075 files opened in the pk4 in 3.4 s (4,838 in 17.4 s before patch 0002's change and the shared handle); spawning the entities about 33 s, the render world (`.proc`) 8 s, collision 4 s, the game's scripts 9.5 s at start. The host loads the same pk4 (checksum 0xfe75bbef as before) and the map; the game's screenshot 150 frames in matches the earlier ones (0.68%).
- **What is left in a DDS image** (9.2 ms for about 59 KB): reading about 30 KB compressed takes about 2 ms at the measured rates, so most of it is inflating on the 68k. A pk4 repacked without compression (about 1.1 GB; the files' CRCs, and so the pk4's checksum, stay the same) would trade that for twice the reading; not tried.

**Done (2026-10-20): the physics split; a PC sampler; the Vita port reviewed.**
- **Patch 0013's new parts:** `idPush::ClipPush`, `idClipModel::Link` and idClip's queries (`Translation`, `Rotation`, `Motion`, `Contacts`, `Contents`), each with its count of calls. **A1200** (`bench-20261020-200426`), per tic (12.5 ms): physics 7.6 ms, of which pushes 1.0 ms (47 calls), clip links 0.5 ms (104), **clip queries 2.1 ms in only 20 calls** (about 0.1 ms each). `swinglight_mover` alone costs 1.9 ms a tic: an articulated figure (`env_swinglight_long_wbulbs`) is bound to it, and since its master keeps moving it never comes to rest, so the AF solver and its collision run every tic.
- **`tests/a1200/pc-sample`: a sampling profiler** (`build_pc_sample.sh [bench]`, `tools/pcsample_report.py`). `pc_sample` wakes every millisecond (timer.device, above the game's priority) and reads the PC exec saved for the preempted game process. `frame_probe` measured exec's frame on this machine (`pc-sample-20261020-200955`): at `tc_SPReg` a 0xFFFF word, the FPU's control registers, fp0–fp7 and its fsave frame, then PC, SR and d0–a6 (PC at +0x74). Emu68 stops a task at the end of a translated block, so a sample is a block's start: fine for functions. It samples while `ENV:d3bench` is `counting` (**`patches/0019-pc-sample-mark`**: the benchmark sets it, through `amiga_set_env` in `src/sys/amigaos3`). `bench-one` starts it when the package carries it. The report maps the game's PCs with `nm` (the packages' executables keep their symbols), others to the library whose vectors are just below them (ogles2's code lies about 10 KB above its stubs), and sums the game's functions by engine area. Sampling costs: 7.9 fps against 10.8.
- **Where the frame goes** (`bench-20261020-201504`, 25,216 samples in 30 s, none waiting): ogles2.library 27%; the game 67%, of which the renderer's front end 17%, its back end (the game's side, with the GL stubs) 16%, collision 10%, physics 6%, game logic 4%, animation and skinning 4%, GUIs 2%, C library 1%, scripts 1%; exec 2%. The functions are spread thinly: `RB_RenderView` (the back end inlined into it) 7%, `RB_ComputeMVP` 1.8%, then the collision model's traces (about 1% each), `R_CornerCullLocalBox` and `R_RadiusCullLocalBox` 1.6%, `idFrustum`'s scissor and cull 2.2%, `PushVolumeIntoTree_r` 0.9%.
- **ldhewm3-rearmed (the Vita port, `~/ldhewm3-rearmed`, last commit 2026-09-14) reviewed** for what a 68k without NEON can use:
  1. the same picture with less work: box culling against one corner per plane (9392d5d); an entity pushed into the areas as a box (0a7957b); a static entity's GUIs only in the player's PVS (a724256); `idBounds::PlaneSide` (e4dd2a1: not hot here, not taken);
  2. small visual trade-offs, behind cvars: an animated model's light triangles not culled per triangle (395ba95); animation at a lower rate for distant or unseen entities (8f06287, 2272689, c164170); `r_maxLights` (7f43e0d, 8 on the Vita);
  3. game behaviour, riskier: movers outside the PVS thinking every second or fourth tic (85ae4cb, with three later fixes; its README still lists stuck movers); dormant AFs and FX (f968490); AI planning less often (9ca74fa, 0a0122b, a4b383e).
  Not taken: the NEON code; 099af34, whose test reads inverted (`!r_shadows`).
- **`patches/0020-vita-culling`** (group 1):
  - `R_CullLocalBox`: each plane into the model's space (`R_GlobalPlaneToLocal`, exact for any linear model matrix, so scaled models too), then the corner nearest it; the same answer as eight corners in world space. The radius test before it went (it assumed no scale). `r_useCulling 3` keeps the old tests for comparison.
  - `R_CreateEntityRefs`: the bounds pushed down the area tree as a box (half axes from the model matrix), with the corners' rule (in front when the farthest corner is at or in front of the plane, behind when the nearest is behind it); `r_useEntityBoxRefs 0` as before.
  - `idStaticEntity::Think`: its GUIs' state only while it is in the player's PVS.
  - Host (`demo_mc_underground`): the same counts with the new and the old tests (264 interactions visited, 193 culled or scissored, 99 shadow surfaces, 254 ambient surfaces; 578 against 576 light surfaces, as between runs).
  - **A1200** (`bench-20261020-202120`): **10.8 fps, front end 18.7 ms**; with `r_useCulling 3 r_useEntityBoxRefs 0` 10.7 fps, 19.7 ms. The same counts in both.
- **Tried and dropped: a model view projection cache.** `RB_ComputeMVP` looked like 1.8% of the frame, so `viewEntity_t` kept the product per view (`r_useMVPCache`). **A1200** (`bench-20261020-202647`): no difference beyond the runs' noise (10.9 fps with it, 11.1 without; back end 43.1 against 42.7 ms). The samples were not its own: **the toolchain keeps no symbols for static functions**, not even in the object files, so a global function's span runs on over the static ones after it in its file. `RB_ComputeMVP`'s 2.4 KB hold `RB_GLSL_DrawInteraction`, `RB_GLSL_CreateSingleDrawInteractions`, `RB_GLSL_CreateDrawInteractions` and `RB_T_GLSL_Shadow`; `RB_RenderView`'s 7% are the same kind. The report now prints each symbol's span.
- **`patches/0021-vita-tradeoffs`** (group 2): **`r_cullDynamicLightTris 0` (the default)** makes an animated model's light surface its whole ambient surface. Without it, every animated model's triangles are tested against each light (facing, light volume) every frame. The light's falloff and projection keep the outside dark, and back faces lie in the model's own stencil shadow. **A1200** (`bench-20261020-202647`): front end 17.5 ms against 18.4 ("interactions created" 1.4 against 2.2 ms), back end unchanged, 11.1 fps against 10.9. **The picture** (the game's screenshots 150 frames in, `map-20261020-203415` with 0 and `map-20261020-203607` with 1): 0.48% of the bytes differ, 351 by more than 16 levels, all on the guard behind the window (whose pose differs between runs) and the HUD's animated corner; his lighting and shading are the same, with no lit back faces.

- **A PC sample with stacks** (`bench-20261020-203955`, defaults after patch 0021; 8.0 fps while sampling). `pc_sample` now keeps, per sample, the first 8 values on the stack that point into the game's code; `build_pc_sample.sh bench` keeps the executables by MD5 (`~/doom3-build/pcsample-bin/`, named in `results/build-ids.txt`), so the report always uses the binary that ran. The shares are as before (ogles2 26%; the game's back end 17%, front end 16%, collision 10%, physics 6%).
  - **ogles2's time by the game function that called it:** `RB_DrawElementsWithCounters` 41% (the draws), code inlined into `RB_RenderView` 25% (state and uniform calls), the interaction draw (`RB_ComputeMVP`'s span) 10%, the swap 4%, `GL_BindImage` 3%.
  - **Collision's callers:** none found in half the samples (its recursion fills the 8 addresses kept); the player's `idPhysics_Player::CheckGround` 11%; movers (`idPhysics_Parametric::Evaluate`, their pushes) 10%; the swinging light's AF only 1%.
  - **The inclusive sums are noisy:** values left on the stack by frequent calls count too (`Sys_MillisecondsPrecise`, the clock of the stage timers, shows in 29% of the samples). They are read as hints, the callers of one area more than the totals.

- **Patch 0021, animation level of detail** (8f06287, 2272689, c164170): **`g_animLOD 1`** animates an entity 512 units from the view every 64 ms, from 768 units or out of the PVS or the view every 96 ms (every second and third tic at 30 Hz), with a phase per entity. Between updates `UpdateRenderEntity` keeps the last frame, so the renderer keeps the skinned model (an MD5 model is re-skinned only when its callback reports a new frame; a missing one is rebuilt from the last joints). Off by default. **A1200** (`bench-20261020-204520`): 11.0 fps against 10.8, game 26.6 against 27.3 ms, skinning unchanged (0.9 ms): this view's animated models (the guard, the swinging light) are near and in sight. To be measured where monsters are far or out of sight before it becomes a default. The host runs both demo maps with it.
- **Not taken: the Vita port's mover throttle** (85ae4cb and its fixes): its README still lists a mover that can get stuck after its last fix, the bridge in the early game that the player must cross (a save and load frees it). A stuck mover can block the game.
- The sampler's stack window is wider now (24 addresses from up to 8 KB of stack), for collision's callers.

**Done (2026-10-20): playing on the A1200; mouse look fixed.**
- **`tests/a1200/play`** (`Run-Play`): a map played with mouse look, no benchmark; it runs until the player quits (console: `quit`). `W=`/`H=` for the size, `QUALITY=low` (the default) runs `low.cfg` from the game's save directory with `+exec`: Doom 3's Low preset (`com_machineSpec 0`, half-size textures, no anisotropy), and shadows, post-process effects, Phong, interaction scissors, decals, blood, double vision, muzzle flashes, projectile lights and ejected shells off. The game's own detection picks "Ultra" from the memory (1 GB or more). `QUALITY=game` leaves the game's settings. The command line would pass 800 characters with these settings as `+set`s, hence the file.
- **Sizes played by the developer** (`play-20261020-*`): 320x200, 480x360, 640x480, 800x600, 1280x720; each opens a full-screen window and a framebuffer of that size, loads in 71 s and quits cleanly.
- **Mouse look stopped after part of a turn:** SDL2_68k's amigaos3 driver has no relative mouse mode and its `WarpMouse` is a stub, so SDL's fallback (warping the pointer back to the centre) did nothing, and the hidden pointer stopped at the screen's edge. **`sdl2/patches/0003-os3-relative-mouse.patch` (now ogles2's, `ports/sdl2`)**: the event pump sets `IDCMP_DELTAMOVE` on the windows while the mode is on and sends Intuition's movement deltas as relative motion (moves queued across a switch are dropped).

- **The size costs almost nothing** (`bench-20261020-214544`, `low.cfg`, which `bench-one` now runs when the package carries it as `bench.cfg`):

  | Size | fps | game | front end | back end | swap |
  |---|---|---|---|---|---|
  | 640x480 | **13.4** | 22.5 ms | 13.6 ms | 35.6 ms | 0.7 ms |
  | 1280x720 | **13.0** | 23.2 ms | 13.6 ms | 37.2 ms | 0.9 ms |
  | 1920x1080 | **12.8** | 23.7 ms | 13.9 ms | 37.3 ms | 1.3 ms |

  **The Pi 4 overclocked** (by the developer, 2026-10-20; `bench-20261020-220642`, 1920x1080, `low.cfg`): **15.6 fps** against 12.8; game 17.5 ms (−26%), front end 12.3 ms (−12%), back end 31.4 ms (−16%), swap unchanged (1.3 ms). The CPU's parts gain, the GPU's wait does not move. Measurements before this point are at the old clock; comparisons stay within one clock.

  6.75 times the pixels cost 4%: the V3D has fill rate to spare; the frame is CPU work (the game, the front end, ogles2's cost per draw), which does not depend on the size. The developer saw the same while playing. The low settings themselves: 13.4 fps at 640x480 against about 11 with the detected "Ultra" settings (front end 13.6 against about 18 ms, back end 35.6 against about 43 ms), mostly the shadows (their volumes and draws).

**Done (2026-10-20): the DOOM 3 BFG Vita port reviewed** (`github.com/Rayz0rum/doom3bfg-vita`, cloned as `~/doom3bfg-vita`; its Vita work is one commit, `f55c068`, 2026-10-19, on chocolate-doom3-bfg). A different engine generation (BFG), so ideas, not patches. Its own notes (`docs/PERFORMANCE.md`, `TECHNICAL.md`): four cores and NEON for the game, the front end, shadow volumes and skinning; dynamic resolution; a lighter interaction shader; "culls lights and shadows hard". What maps onto d3es:
- **Picture and behaviour unchanged:**
  1. **with `r_shadows 0`, d3es still did the shadows' front-end work** (found while checking where the Vita's shadow culling would go): `idInteraction::HasShadows` ignored `r_shadows`, so every interaction of a shadowing light went through the view-frustum cull and shadow scissor, and the lights collected the entities out of sight as shadow casters (and skinned hidden monsters). **`patches/0023-no-shadow-front-end`**: `HasShadows` false and no hidden casters with `r_shadows 0`; `r_useShadowsFrontEnd 1` as before;
  2. **the player's walk IK while his body cannot be seen** (single player, no player shadow, first person, no cinematic): it rebuilt the body's skeleton and traced 2–3 boxes every tic for feet nobody sees. **`patches/0022-bfg-vita-game`**: skipped (`g_playerWalkIK 0`, the default); `g_ikDistance` (0, off) skips it for actors farther than that;
  3. a hand-written tree solve for the AF solver (`idAFTree::Solve`/`Response` for matrices up to 6x6, bit-identical; not taken yet);
  4. the flashlight's monster alert traced only when its beam touches something that reacts (exact; not taken yet);
  5. exact AAS speedups (a floor-edge cache, `SortWallEdges` without restarts; not taken yet).
- **Behaviour or picture changed, each behind a cvar if taken:** articulated figures at half rate, the skipped tic simulated in the next one's double step (the swinging light, 1.9 ms a tic); far AI at half rate, with animation frame commands always run (the Vita's half rate once skipped the command that unlocks mc_underground's checkpoint door); lights small on screen dropped (0.25% of the view, with hysteresis); an adaptive light cap; at most 3 lights on an animated model; world GUIs and the HUD at 15 Hz; thinner particles; ragdolls and debris settled sooner.
- **Not applicable:** threads, NEON, memory pools, the tile-GPU image work (MSAA, Toksvig, sharpening), dynamic resolution (the size costs almost nothing here), one game tic per frame (here the game would run at a third of real time).

- **A1200** (`bench-20261020-222618`, 1920x1080, `low.cfg`, the new clock):

  | Build | fps | game | front end | back end | interactions visited |
  |---|---|---|---|---|---|
  | `r_useShadowsFrontEnd 1 g_playerWalkIK 1` (as before) | 15.6 | 17.6 ms | 12.0 ms | 31.7 ms | 260 (189 culled or scissored) |
  | `r_useShadowsFrontEnd 1` | 15.8 | 16.8 ms | 11.9 ms | 31.7 ms | 260 |
  | **defaults (0022 and 0023)** | **16.7** | 16.0 ms | **9.4 ms** | 31.7 ms | 130 (none culled) |

  The same 605 light surfaces in all three (the picture, with shadows off, is the same). The walk IK 0.8 ms of game a frame; the shadows' front end 2.5 ms.

- **20 game tics a second** (`bench-20261020-223235`, 1920x1080, `low.cfg`; patch 0010 takes 10–480 Hz and rescales the 60 Hz values): **18.2 fps** against 16.7 at 30 Hz; game 10.8 against 16.0 ms a frame (per tic 9.8 against 8.9 ms: the longer steps cost a little more each). `Run-Play HZ=20` to play it.

- **Decided (the developer, 2026-10-20): 20 game tics a second for play**, after playing it (`play-20261020-230014`): "20hz is ok". `Run-Play`'s default is `HZ=20`; the port's `autoexec.cfg` (§10.4) gets `com_gameHz 20`. The benchmarks keep 30 Hz so that they compare with the earlier ones.

- The developer played the game's own "Ultra" settings at 1920x1080 and 20 Hz (`play-20261020-230719`; still with precompressed textures, which the Ultra preset turns off: on the command line).

**Done (2026-10-20): sound, phase 7 started: the port's OpenAL subset.**
- **`src/al/al.c`** (D7): the OpenAL 1.1 calls d3es makes. Buffers of 8 or 16-bit PCM, mono or stereo; sources with gain, pitch, position (relative or not), looping, play, pause, stop, rewind and queued buffers (d3es streams long sounds through three queued buffers); a listener (gain, position, orientation).
  - The mixer runs in SDL2's audio callback, 22,050 Hz stereo 16-bit by default. On AmigaOS it uses AHI through SDL2_68k's driver (SDL's default there is Paula), opened with `SDL_AudioInit("ahi")` unless `SDL_AUDIODRIVER` says otherwise.
  - Each source is resampled to the output rate: linearly, or as the average of two at exactly half rate (44 kHz sounds). It is panned by its direction from the listener at equal power (stereo sounds play as they are), and its gain is ramped over each block so changes don't click.
  - d3es does the distance attenuation itself (`AL_ROLLOFF_FACTOR 0` on every source), so distance models, rolloff, cones and Doppler are accepted and ignored. EFX and the other extensions are reported absent, and d3es turns its reverb off.
  - State changes take SDL's audio device lock. Settings come from the environment: `D3AL_VOICES` (32; d3es keeps 8 for stereo sounds), `D3AL_RATE`, `D3AL_SAMPLES` (1024).
- **`build_al.sh`** builds `libd3al.a` for the A1200 (into `~/doom3-build/m68k/deps/lib`, after `build_deps.sh`, which clears that directory) and for the host (`~/doom3-build/host-al`). `build.sh` links it when it is there; **`patches/0024-amiga-openal`** drops d3es's OpenAL stub in that case (`AMIGA_OPENAL_LIBRARY`). `AL=1 bash build_host.sh` builds the host game with the subset in place of OpenAL Soft. `run_host.sh` now leaves `SDL_AUDIODRIVER` alone if set.
- **Host, against OpenAL Soft** (20 s of `demo_mars_city1`'s opening). The subset's output went through SDL's `disk` driver, OpenAL Soft's through its `wave` writer. The loudness per second has the same course in both (silent while loading, then the same rises and falls), with no clipping in either. The subset is 15–25% louder (its centre is 0.707 on each side), with peaks of 5,931 against 6,903.
- `Run-Play` plays with sound now (`SOUND=0` for none); the benchmarks keep `s_noSound 1` so that they compare with the earlier ones.

**Open (2026-10-21): the sound is noise on the A1200.**
- **The developer** (`play-20261020-232653`, `-234347`): noise on AHI, its loudness following the game's sounds (a door opening), much better at 10% of the game's volume, with occasional crackle and noise even then.
- **Checked:**
  - **The output:** SDL uses its AHI driver (`SDL_AudioInit("ahi")`) at 22,050 Hz, stereo, 1,024 frames a block, `AUDIO_S16MSB` (`0x9010`). The driver hands that to AHI as `AHIST_S16S` and double-buffers its writes; its audio thread runs at priority 10, above the game's 0.
  - **`idMath::FtoiFast`** is `(int)f` here. stb_vorbis's fast float-to-int is the same in both byte orders.
  - **The host's output with the subset** is clean (compared with OpenAL Soft, above).
- **Found and fixed:**
  - **`patches/0025-wav-big-endian`:** a WAV file's 16-bit PCM was handed on little-endian; it is now swapped after reading (`LittleShort`). That did not change what the developer heard; most of Doom 3's ".wav" sounds are OGG files under that name. *(Wrong, and removed on 2026-10-21: `idWaveFile::Read` already swapped them; this swapped them back. Below.)*
  - **SDL2_68k's `SDL_getenv` sees only what `SDL_setenv` set** (its own table, not `ENV:`), so the subset's `D3AL_*` settings never reached it on the Amiga. It reads them with the C library's `getenv` now *(which does not see `ENV:` either: `GetVar` since 2026-10-21, below)*. It also prints its set-up on the console ("OpenAL subset: SDL audio driver …").
  - **amictl does not upload empty files**, and `-a` takes no `/` in a value. `Run-Bench` skips `.dump` markers now (`dhewm3-~(#?.(args|dump))`).
- **The diagnostic `D3AL_DUMP=PREFIX`** (`bench-one` sets it, global, when `<build>.dump` is in the package, non-empty): the first 30 s of the mixed output to `PREFIX.raw` and one line per `alBufferData` (format, rate, frames, peak, mean level, first samples) to `PREFIX.txt`, written when the device closes. Its host reference for 20 s of `demo_mars_city1`'s opening is in `~/doom3-build/sndtest/host.raw` and `host.txt`. The comparison is `tools/sndcmp.py`: buffers line by line, loudness per second, and whether the output reads as byte-swapped.
- **The first run with the dump on the A1200 hung the machine** (2026-10-21 00:2x; the station no longer answered). That dump wrote its files through `SDL_RWFromFile`, new on this machine; it uses stdio now and runs on the host. Not known yet: whether the dump, the sound or the overclock hung it.
- **Load time with sound:** 161 s against 62 s (`play-20261020-232653`). 862 sounds took 53 s (about 61 ms each: OGG decoded at load, for all shorter than `s_decompressionLimit`, 6 s); images 24.8 against 16 s and twice the pk4 opens (6,280), as the sounds come from the same pk4.
- **The developer's idea, to follow:** sounds decoded on the PC once, in an extra pk4 of PCM (big-endian, so the 68k need not swap), which the loader prefers to the OGG. That saves the 53 s, takes the 68k's decoder out of the question, and shows whether the noise comes from the decoding.

**Done (2026-10-21): the noise found: patch 0025 swapped WAV samples twice.** (A second machine: `C:\GitHub\doom3`, WSL distro `Ubuntu`; `build_sdl2.sh` now finds ogles2 and the PiStorm3D SDK next to this repository.)
- **libnix's `getenv` sees only the shell's local variables** (`Set`), not `ENV:` (`SetEnv`), which `bench-one` uses (`tests/a1200/results/envprobe-20261021-142315`: a probe printing `getenv` and `GetVar` for both kinds). So no `D3AL_*` setting had ever reached the subset on the A1200, nor the dump (`bench-20261021-141822`: no "output dumped" on its console). `src/al/al.c` reads them with `GetVar` on AmigaOS now (local, then global); its `Lock`/`Unlock` are `LockMixer`/`UnlockMixer`, clear of `dos.library`'s.
- **The dump run** (`bench-20261021-142503`, `demo_mars_city1`, sound on, no hang) against the host's: **110 of 550 buffers arrived byte-swapped** (mean level about 16,000 against the host's 1,000–5,000; host 2675 = `0x0A73`, A1200 29450 = `0x730A`). They are the WAV files of real PCM: `idWaveFile::Read` already swaps them to native order (`LittleRevBytes`), and patch 0025 swapped them back. **Patch 0025 removed** (§14 above described it as found and fixed; it was the fault). The 440 OGG sounds were right all along.
- **After the fix** (`bench-20261021-143016`): 550 of 550 buffers alike. **The mixed output** (`bench-20261021-143536`; the dump now starts at the first sound, not at the device's opening, as the A1200 spends minutes loading in silence) follows the host's loudness second by second (483/488, 549/557, 1058/1150, … 1292/1292), peak 8,467, nothing clipped; read byte-swapped it would be about 19,000 throughout. The mixer is right on the 68k; what AHI makes of it is checked by ear.
- The dump's host reference is in `~/doom3-build/sndtest/host.raw` and `host.txt` (`SDL_AUDIODRIVER=disk`, which needs a regular file in `SDL_DISKAUDIOFILE`, not `/dev/null`).

- **Listened (the developer, 2026-10-21):**
  - **AHI** (`play-20261021-143945`): still noise at the game's full volume, clean and correct at its minimum. The mixed output at full volume is clean (above), so AHI makes the noise.
  - **SDL2_68k's Paula driver** (`play-20261021-144958`; `SDL_AUDIODRIVER=paula`, set with `SetEnv` in the package's copy of `Run-Play`; 8-bit through audio.device, bypassing AHI): **clean at full volume.**
- **AHI on the test A1200:** unit 0 (which SDL2_68k's driver opens) plays through a **USB sound device** (mode `0x003B0001`, "USB PnP Sound Device: 16 bit stereo++", driver `usbaudio`, registered at run time, so not in `DEVS:AudioModes`); units 2 and 3 through Paula HiFi 14-bit. The developer hears Paula and the USB device mixed. (Unit 0 was set to Paula for a while on 2026-10-21 on the wrong belief that the mode was missing; the original prefs are back, the backup in `ENVARC:Sys/ahi.prefs.bak-20261021`. AHI did not reread the prefs meanwhile.)
- **Found: SDL2_68k's AHI driver sent little-endian samples.** Tests, each a 440 Hz tone (`tests/a1200/sound-probes`, built by `tests/a1200/build_sound_probes.sh`; results `tone-*`, `ahiprobe-*`, `ahistream-*` of 2026-10-21):
  - SDL → AHI unit 0 (USB) and unit 2 (Paula): noise at every level, 3% to 98%, with no game running; callbacks on time.
  - AHI's low-level API (`AHI_AllocAudio` with mode `0x0002000D`), and a plain `CMD_WRITE` streamer with SDL's driver logic (request priority 60, task priority 10, its order of waits): clean.
  - A debug SDL copying each buffer it sent: the samples were the tone **little-endian**. The driver's format loop kept the format after the one it matched (`AUDIO_S16LSB`), SDL converted to it, and AHI played `AHIST_S16S` byte-swapped. **`sdl2/patches/0103-os3-ahi-format`** stops the loop at the match. After it the capture is the tone big-endian, peak 7,999, and the developer heard it clean through unit 0 (USB, `tone-20261021-153303`).
  - The earlier "loud is noise, quiet is fine" was this swap: a small sample swapped is a loud one, but at the game's minimum volume everything is near silence.
- **Picasso96's native video at 31 kHz** (the developer tried it): AHI's Paula output stuttered; back at 15 kHz it is clean. Our 22,050 Hz needs no more than 15 kHz modes allow (about 28 kHz).

- **Done: the game's sound is clean on the A1200** (`play-20261021-153434`: `demo_mc_underground`, 1920x1080, low, AHI unit 0 through the USB sound device with the developer's own prefs, the game's full volume, 12 minutes; the developer listening).

- **The cost of sound** (`bench-20261021-160146`: `demo_mc_underground`, 1920x1080, `low.cfg`, 30 Hz, 10 s warm-up, 30 s counted; one executable, `bench-one`'s new `<build>.audio` choosing SDL's driver):

  | build | fps | frame | game | front end | back end |
  |---|---|---|---|---|---|
  | silent (`s_noSound 1`) | 7.8 | 128 ms | 31.7 | 9.5 | 84.8 |
  | Paula (SDL's driver, 8-bit) | 5.9 | 169 ms | 47.1 | 11.3 | 96.1 |
  | AHI (unit 0, the USB device) | 3.0 | 339 ms | 152.0 | 18.9 | 134.0; 28.7 tics a second |

  - Sound takes time from every stage, as the mixer and AHI run in other tasks: **with Paula about 41 ms a frame (a quarter of the CPU)**, which is nearly all ours (the subset's mixer, OGG streams decoded while playing, d3es's sound updates; SDL's 16-to-8-bit conversion is small). **AHI through USB costs about 170 ms a frame more** (AHI's mixing at 44.1 kHz in a non-HiFi mode with 16 channels, the USB stack): over half the CPU.
  - Silent 7.8 fps here against 18.2 in the phase 5 notes: different settings (30 against 20 game tics a second) and not checked against the Pi's clock; compare only within one run.

- **Why this machine is half as fast as 2026-10-20 (silent 7.8 against 16.7 fps): the ogles2 library.** Every stage but the swap is the same as `bench-20261020-222618` (the game per tic 8.3 against 8.9 ms, front end 9.5 against 9.4 ms, the back end's draw stages within 1 ms); **the swap is 53 ms against 1.3**. Checked and ruled out:
  - the GPU and display: ogles2 1.3's Boing 2026 (`boing-20261021-162636`, 640x480, MSAA 4x) 56.0 fps in a window and 61.4 full screen, against 46.05 on 2026-10-18 before the overclock; the Pi at 2,200 MHz, 56 °C;
  - this machine's builds: **the developer's own executable of 2026-10-20** (from the Amiga's `map` package) is as slow (7.4 fps, swap 53.9 ms; `bench-20261021-162113/results/dhewm3-dev20`);
  - the ogles2 1.3 release library against `~/ogles2-build/m68k`'s (both 7.8 fps), the generated headers and `libogles2.a` (identical to 1.3's SDK), the Ultra detection (textures still DDS), the framebuffer (none at 1920x1080), the Workbench screen (640x480 both days).
  - **The cause:** the swap without the copy (§13, request 2; `bench-20261020-170942`) was made in ogles2's working tree on the developer's other machine and is not committed there. Neither ogles2 checkout here has it (`WindowCopy` appears nowhere in `C:\GitHub\ogles2` or `ogles3.1`), so SDL's full-screen window is copied again: the swap waits for the finished frame and `ClipBlit`s 1920x1080 (16 ms at 640x480 then; 22 ms at 640x480 and 53 ms at 1920x1080 now). The last fast run was `bench-20261021-000848` (1.1 ms); the A1200 was reset after the dump's hang and the station's files come from this machine since.
  - Also: on this machine `~/ogles2-build/m68k` was rebuilt on 2026-10-21 at 14:05 from `C:\GitHub\ogles3.1` (its `ogles3-make.log`), which shares that folder; `tools/bench_pkg.sh` packages from it.
  - **All of 2026-10-21's A1200 numbers above (the cost of sound included) are with the copy**; to be measured again with the change.

**Next, in order:**
1. **ogles2's swap without the copy on this machine:** committed in ogles2 on the developer's other machine and pulled here (ogles2 changes in its own repository, by its own rules), its library built and packaged; then the benchmarks of 2026-10-21 again.
2. **The cost of sound, split:** the PC sampler on the Paula build (which of the mixer, OGG decoding, d3es's sound updates), then the cheap cuts: fewer voices (`D3AL_VOICES`), an 11,025 Hz output (`D3AL_RATE`), long sounds decoded at load (`s_decompressionLimit`), or decoded on the PC (above). And AHI on a Paula unit at 22,050 Hz against the USB unit, to see how much is the USB path.
2. A PC sample with the wider stacks: collision's callers.
2. `r_maxLights` from the Vita port (a setting that drops the least important lights), for the port's lower settings.
3. Collision (10% of the frame): who makes the expensive queries (the swinging light's AF, the player).
4. The front end's per-frame walk: what can be kept from one frame to the next for entities and lights that did not move.
5. **Later (the developer, 2026-10-20): the load.**
   - **A pk4 without compression:** `demo00.pk4` repacked with every file stored (about 1.1 GB against 484 MB; `Stuff:` has 3.2 GB free), copied to the Amiga, so that no file is inflated on the 68k (most of a DDS image's 9.2 ms). The files' CRCs stay the same, so the pk4's checksum does too. A tool to repack (`tools/`), then the load measured against `map-20261020-194658`.
   - Or a faster inflate on the 68k, if the repacked pk4 is not wanted.
   - The entities' spawn (33 s, models 14.7 s with their images), the render world (8 s), the scripts' compile at start (9.5 s).
2. Performance (phase 5), cheapest first, each step measured with `Run-Bench`:
   - compiler flags for the game: `-O3`, `-mcpu=68060` (scheduling), `-fno-exceptions` is not possible (Doom 3 throws);
   - the game tic: a 30 Hz game rate (the idea of dhewm3's high-fps branch, which makes the tic length a variable), which would halve the logic's cost per second; dormant entities and animation level of detail from ldhewm3-rearmed (§10.2);
   - the back end's 42 ms: where it goes in ogles2 (its log probes), a request to ogles2 only with numbers (§13).
2. Map load time (77 s): where it goes (pk4 inflate, images, collision, AAS, scripts).
2. The developer gets the 1.3.1 data (§4: GOG, Steam or CD plus the patch), checks the MD5 sums, and copies `base/` and `d3xp/` to WSL and to the Amiga.
3. Phase 3 (§8): the 68k build of the game.
