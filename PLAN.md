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
   - the game data prepared once by the game at its first start on the Amiga (stored pk4s, sounds as PCM, images without a DDS as DDS), with a progress screen: §16.

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

**Done (2026-11-10), patch 0033:** the first start (no `config.spec`) sets the port's settings in the game itself (Low quality, 1280x720, the frame rate shown, 12 lights, no vsync, 20 Hz, 2 speakers: no "Ultra" detection, no second detection pass); a quality (the menu's, or `com_machineSpec N` + `execMachineSpec`) keeps the screen mode and the precompressed textures and then runs the port's `low/medium/high/ultra.cfg`; the configuration is written at every start. `Doom3.script` passes only the options given, so the saved settings are used (§14). The plan below (an `autoexec.cfg`) was not needed.

The port's `autoexec.cfg` sets (the first plan):
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
3. **(2026-10-28, to ogles3) A GLES2 draw costs 44% more CPU in ogles3.library 1.0-beta1 than in ogles2.library 1.3.1: 34.6 against 24.1 µs.** Measured with probe P on `timeDemo demo1` (1920x1080, 237 draws a frame; §14, "the draw's stages"): the same game, the same work per draw, 16.3 fps against 17.6. Every stage is slower; most of all the shaders (0.55 → 2.2 µs a draw), textures (2.2 → 3.8), checks (4.6 → 6.8) and the binning list (2.0 → 3.1). The GPU's render is faster in ogles3 (2.0 against 3.3 ms a frame), but this game waits for the CPU. ogles3's ES 3 features could save this game CPU (uniform buffers, skinning in the vertex shader) only once a GLES2 draw is back near 1.3.1's cost. Raw data: `tests/a1200/results/bench-20261028-223954/results/ogles2.log` and `ogles3.log`. (**Note, 2026-10-29:** this and every `timeDemo` figure before 2026-10-29 22:31 streamed the demo from its pk4, value by value through zlib (`com_preloadDemos 0`), which cost about 3% at 1920x1080 and more where the GPU is not the limit; the bench now reads it into memory first, §14.)

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
  - **(2026-10-22) Fixed: `patches/0027-demo-playback`.** d3es's 16-bit `glIndex_t` broke the demos' indexes, which a demo stores as ints (id's demos and `WriteToDemo` alike): `idGuiModel::ReadFromDemo` read shorts (the stream fell 2 bytes behind per index; the next token was garbage: "Bad render demo token", and "demo hash index out of range" for this build's own demos), and `idRenderModelStatic::ReadFromDemoFile` read through an `int&` into a 16-bit index (the next index overwritten, the last one written past the end); its writer wrote 4 bytes from a 2-byte index. Each index is now read and written as an int and converted. Found by printing the tokens: the last before the error was `DC_GUI_MODEL`. Also a null check in `idSoundEmitterLocal::StopSound` (no sample with `s_noSound 1`); **silent timedemos still crash** (more sound paths assume loaded samples, e.g. `FindAmplitude` for sound-driven lights): timedemos run with sound on.
  - (**Note, 2026-10-29:** this and every `timeDemo` figure before 2026-10-29 22:31 streamed the demo from its pk4, value by value through zlib (`com_preloadDemos 0`), which cost about 3% at 1920x1080 and more where the GPU is not the limit; the bench now reads it into memory first, §14.) **Host:** `demo1` 2,148 frames in 80.9 s, 26.5 fps (llvmpipe). **A1200 (`timedemo-20261022-065136`): `timeDemo demo1 usecache` at 1920x1080, all lights, Paula: 2,148 frames in 208.9 s = 10.3 fps.** The demo's sounds did not reach the mixer (0 callbacks mixed sound). **640x480 with `r_maxLights 12`** (`timedemo-20261022-065930`): 143.3 s = **15.0 fps**. **Both at the game's detected Ultra settings, not `low.cfg`:** with the full game's data the game's save directory is `save/base/`, and `Run-Play` copied the settings file to `save/demo/` only (no "execing low.cfg" in their logs; the play session `play-20261022-070523` too). `Run-Play` and `bench-one` now copy it to both. **With `low.cfg` found (`timedemo-20261022-071225`): 1920x1080, `low.cfg`, `r_maxLights 12`, Paula: 2,148 frames in 126.8 s = 16.9 fps.** **The Pi's GPU overclocked** (the developer, 2026-10-22: `v3d_freq=650`, `gpu_freq=650`; ARM 2,200 MHz, 53.5 °C; `timedemo-20261022-073325`): the same 118.0 s = **18.2 fps** (+8%). (A run with `core_freq=700` was aborted.) The timedemo package: `tests/a1200/play`'s `Run-Play` with `+timeDemo demo1 usecache quit` in place of `+map`.
  - **(2026-10-22, before the fix) It does not either:** `timeDemo demo1 usecache` on the host (the full game's data) stops at once with "Bad render demo token" (`AdvanceRenderDemo`), and the error's cleanup then crashes in `idSoundEmitterLocal::StopSound` (`StopPlayingRenderDemo` → `StopAllSounds`). Patch 0006 now also takes `timeDemo <demo> usecache quit` (quits after the timed pass, as `timeDemoQuit`, which has no precaching pass), for when demos play.

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
  - A debug SDL copying each buffer it sent: the samples were the tone **little-endian**. The driver's format loop kept the format after the one it matched (`AUDIO_S16LSB`), SDL converted to it, and AHI played `AHIST_S16S` byte-swapped. **`sdl2/patches/0103-os3-ahi-format`** (now ogles2's `ports/sdl2` patch 0004, `sdl2/patches/0004-os3-ahi-format.patch`) stops the loop at the match. After it the capture is the tone big-endian, peak 7,999, and the developer heard it clean through unit 0 (USB, `tone-20261021-153303`).
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

- **Done (2026-10-21): the swap without the copy here.** ogles2 committed it (`f3704e2`), and took this port's SDL fixes (its patches 0003 and 0004, in place of this port's 0101 and 0103; doom3 `8ff7bb2`); synced here, ogles2 `33b96b1` built (`ogles2.library` 240,312 bytes, as the developer's), SDL and the game rebuilt. **Silent: 16.4 fps, swap 1.3 ms** (`bench-20261021-170203`), against 16.7 fps and 1.3 ms on 2026-10-20; the other stages as then.
  - `tests/a1200/build_pkg.sh` (the `play` package) still takes ogles2's 1.3 release (`~/ogles2-build/release`), which has the copy: play packages need `OGLES2_RELEASE=~/ogles2-build/direct-libs` (what `tools/bench_pkg.sh` copies) until ogles2 makes a release.

- **The cost of sound, with the no-copy swap** (`bench-20261021-170547`, the same three builds and settings as above):

  | build | fps | frame | game | front end | back end | swap |
  |---|---|---|---|---|---|---|
  | silent | **16.2** | 62 ms | 16.5 | 9.6 | 32.6 | 1.4 |
  | Paula (SDL's driver, 8-bit) | **11.1** | 90 ms | 27.1 | 11.4 | 40.8 | 1.5 |
  | AHI (unit 0, the USB device) | **4.6** | 216 ms | 105.3 | 18.5 | 75.7 | 1.6 |

  - As a share of the CPU (another task taking a share f stretches every frame by 1/(1−f)): **sound with Paula about 32%** (the subset's mixer, OGG streams decoded while playing, d3es's sound updates, SDL's 16-to-8-bit conversion), **sound through AHI and USB about 71%**; AHI's part alone, above Paula's, about 39 points.
  - The earlier table (with the copy) put the same order on it; its milliseconds were stretched by the 53 ms swap.

- **Paula only: the mixer in the device's format, and the cheap cuts** (`bench-20261021-172212`, 1920x1080, `low.cfg`, demo_mc_underground, Paula, cumulative from left to right):
  - **The mixer writes what the device plays.** SDL2_68k's Paula driver is 8-bit mono, so SDL converted the subset's 16-bit stereo through float on every sample. `src/al/al.c` now opens the device with `SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE` and mixes into its format (`AUDIO_S16SYS` or `AUDIO_S8`, stereo or mono; mono without panning). AHI and the host keep 16-bit stereo: the host's dump is the same as before (loudness per second identical, 99.8% of the samples alike, the rest real-time jitter).

    | build | fps | game | front end | back end |
    |---|---|---|---|---|
    | the mixer in 8-bit mono (`base`) | 11.1 | 27.0 | 11.4 | 40.4 |
    | + `s_useOcclusion 0`, `s_maxSoundsPerShader 1` (`cvars`) | 10.8 | 28.5 | 11.4 | 41.3 |
    | + `s_decompressionLimit 60`: nothing decoded while playing (`decomp`) | 11.7 | 26.6 | 11.8 | 41.5 |
    | + 16 voices (`D3AL_VOICES`, `v16`) | 12.4 | 24.4 | 11.5 | 40.1 |
    | + 11,025 Hz output in place of 16 voices (`D3AL_RATE`, `r11k`, 32 voices) | 13.1 | 22.0 | 11.0 | 38.8 |

    (silent 16.2 fps, `bench-20261021-170547`). SDL's conversion was not the cost (`base` is no faster than 11.1 before), and occlusion and the sounds per shader cost nothing measurable. The game's time grows with sound, 16.5 to 27 ms a frame: d3es's sound work in the game's own process, not the mixer's. `bench-one` takes `<build>.env` (`SetEnv` lines) for the subset's settings per build.
  - **The PC sampler on SDL's audio process hung the A1200** (a second `pc_sample` on `SDL_SDLAudioP2`, beside the one on the game; the station stopped answering during the run). The sampler stays on the game's own process.

- **The cost of sound, split** (Paula, the `decomp` settings):
  - **The mixer times itself** now (`src/al/al.c`: time in the callback, printed when the device closes): **2.96–3.17 ms a 46.4 ms block, 6.4–6.8% of the CPU** while sounds play (`bench-20261021-180937`, `-181715`).
  - **The PC sampler on the game's process** (`bench-20261021-180937`, silent against Paula): d3es's sound code 2.2% of the game's samples (`AddChannelContribution`, `MixLoop`, `ALStop`, `cexpf`), OGG decoding still 1.2% (`idSampleDecoderLocal::ClearDecoder`'s span, stb_vorbis); the rest grows with the game's work: collision, physics, game logic.
  - **With sound the level plays differently:** scripts wait for sounds that `s_noSound 1` makes zero-length, so the counted 30 s hold more (691 light surfaces against 605, 144 interactions against 130). `D3AL_NOMIX=1` (new: the subset outputs silence and mixes nothing) separates it (`bench-20261021-181715`):

    | build | fps | frame | game | front end | back end |
    |---|---|---|---|---|---|
    | silent (`s_noSound 1`) | 16.4 | 61 ms | 16.5 | 9.6 | 31.9 |
    | sound, mixer idle (`D3AL_NOMIX 1`) | 12.8 | 78 ms | 22.8 | 11.3 | 39.3 |
    | sound, mixed (Paula) | 11.7 | 85 ms | 26.3 | 12.0 | 41.9 |

    **The mixer and Paula: 7 ms a frame (8%).** The 17 ms between silent and the idle mixer are the other scene (its back end alone 7 ms more) and d3es's sound work; **sound is not 30% of the CPU, and silent fps do not compare with fps with sound.** Benchmarks of sound changes compare against `D3AL_NOMIX` or each other.
- **Play's defaults for Paula:** `low.cfg` adds `s_maxSoundsPerShader 1` (Doom 3's Low preset's) and `s_decompressionLimit 60` (nothing decoded while playing; 8 s more load: 94.6 to 102.9 s). `Run-Play` takes `AUDIO=` (default **paula**), `VOICES=` (default **32**) and `RATE=` (default 22050; 11025 gave +1.4 fps, the sound duller), set as global variables for the run. Occlusion stays on (off gained nothing measurable).
  - **16 voices lost the voice lines** (the developer, `demo_mars_city1`'s intro, `play-20261021-192254`): d3es keeps 8 of the voices for stereo sounds, so only 8 were left for the rest of a busy scene. With 32 they play (`play-20261021-193114`), and the mixer then cost 3.0% of the CPU in that scene (1.38 ms a 46.4 ms block, against 1.33 with 16): the voices' +0.7 fps in `bench-20261021-172212` was mc_underground's busier sound. **32 is the default again.**

- **Done (2026-10-21): typing in the console.** SDL2_68k's AmigaOS 3 driver sent no `SDL_TEXTINPUT` at all, and d3es on SDL2 takes typed characters only from it: the console saw Return (empty `]` lines in the log) but no letters. **`sdl2/patches/0102-os3-text-input`**: each key press also goes through the user's keymap (`MapRawKey`, with the qualifiers and the previous keys for dead keys); its printable characters (ISO 8859-1) go to SDL as UTF-8, while SDL's text input is active. The developer typed in the console with it.
- **Open (2026-10-21): starting a second map hangs the A1200.** Twice: `demo_mc_underground` played, then New Game (which loads `demo_mars_city1`): the log ends at "Game Map Shutdown", the station stops answering, a reset is needed (the logs fetched after the reset: `play-20261021-182700-hang`). `demo_mars_city1` as the first map loads and plays (all the dump runs). On the host, `+map A +wait 20 +map B` never reached the second `map` within 4 minutes (the first map loaded in 5 s), so the host test says nothing yet. Until it is found: start the map wanted directly (`Run-Play`'s `MAP`, edited in the package's copy: amictl's `-a` takes no `/`).

- **1280x720 against 1920x1080, as played** (`bench-20261021-194746`: Paula, 32 voices, `low.cfg` with its sound settings, demo_mc_underground, 30 Hz): **12.5 fps against 12.2**; back end 40.8 against 41.1 ms, its draw stages the same (interactions 29.4 ms both), swap 1.1 against 1.5 ms. The back end's time is the CPU's (ogles2's per-draw work, §13), not the pixels: the resolution costs almost nothing.

- **`patches/0026-max-lights`: the Vita port's `r_maxLights`** (ldhewm3-rearmed `7f43e0d`; §14 "the Vita port reviewed", group 2). At most `r_maxLights` lights in a view (0, the default: all); ambient and fog lights first, the others by their distance from the view against their volume and brightness, the rest dropped from the view's list and marked out of view (no interactions made for them). Changed from the Vita's: each score computed once and sorted (its sort recomputed them), no fixed array of 1,024, a light coloured by its material (shader parameters 0) counts as bright 1, not as dark. Host (`demo_mc_underground`'s benchmark view): light surfaces 340, 138 with 8, 53 with 4. **A1200** (`bench-20261021-195908`, 1920x1080, `low.cfg`, silent):

  | `r_maxLights` | fps | game | front end | back end | light surfaces |
  |---|---|---|---|---|---|
  | 0 (all) | **16.5** | 16.1 | 9.6 | 32.0 | 605 |
  | 12 | **20.3** | 13.8 | 8.4 | 24.3 | 358 |
  | 8 | **23.3** | 12.3 | 8.0 | 19.7 | 247 |
  | 6 | **26.1** | 11.0 | 7.2 | 17.5 | 136 |

  The back end (ogles2's cost per draw) falls with the light surfaces, and the game's time per frame with the frames' rate (more frames, fewer tics each). The picture: fewer lights, the dropped ones' surfaces dark, lights changing as the view moves; to be judged by the developer playing.

- **Played (the developer): lights missing or popping, at 8 and at 12.** The ranking reworked in patch 0026:
  - the distance from the view to the light's volume (`frustumTris`' bounds; 0 inside it) in place of its centre's, so a light the player stands in is kept;
  - the size from the same bounds: the Vita's took `lightRadius`, which projected lights do not have, so they counted as size 0 and went first;
  - hysteresis: a light kept in the last frame ranks `r_maxLightsKeep` (2) times better, against lights flickering at the limit (`idRenderLightLocal::maxLightsFrame`).

  **A1200** (`bench-20261021-203817`, `-204421`; 1920x1080, `low.cfg`, silent): all lights 16.4 fps; **`r_maxLights 12` 21.2 fps** (back end 23.0 ms, 298 light surfaces); **`r_maxLights 8` 24.0 fps** (back end 18.9 ms, 229 light surfaces).
- **`tests/a1200/play/high.cfg`** (`Run-Play QUALITY=high`): Doom 3's High preset (`com_machineSpec 2`, `execMachineSpec`: full-size textures, 8x anisotropy, precompressed textures) with shadows, post-processing, interaction scissors, decals, blood, projectile lights, double vision, muzzle flashes and shells on; sound as `low.cfg`. Played by the developer (`play-20261021-205847`, loads in 96 s against about 90). **A1200, `r_maxLights 12`** (`bench-20261021-210656`, 1920x1080, silent; high's settings on the command line after `low.cfg`):

  | settings | fps | game | front end | back end | swap | shadow surfaces |
  |---|---|---|---|---|---|---|
  | low | **21.3** | 13.2 | 8.0 | 23.1 | 3.5 | 0 |
  | high | **15.0** | 17.7 | 13.1 | 32.9 | 9.6 | 92 |

  High's cost is mostly the shadows: their front-end work (interactions culled and scissored, 3.0 ms) and draws, and a swap that now waits for the GPU (9.6 ms: the stencil shadows and the full-size, anisotropic textures are the first load here the V3D notices).
- **640x480 with `r_maxLights 12`** (`bench-20261021-222858`, low, silent): **24.4 fps** against 21.3 at 1920x1080; back end 19.0 against 23.1 ms (interactions 10.4 against 11.2, swap 0.5 against 3.5). With fewer draws the size starts to count a little (15%; it was 4% with all lights, 2026-10-20): the swap at 1920x1080 now waits for the GPU.
- **Open: the A1200 froze after typing in the console** (`r_maxLights 8` typed during a play session, 2026-10-21 ~20:20; the first session with patch 0102 hung too, typing and New Game). The benchmarks, which type nothing, run. Patch 0102 (text input) is suspected; not to be typed in until found.

**Done (2026-10-22): the full game's data (GOG, the original Doom 3 1.3.1)** in `~/doom3-data/base/` and `Stuff:doom3/base/` (copied by the developer): `pak000.pk4`–`pak008.pk4`, 1,556,064,554 bytes; all nine MD5 sums as the dhewm3 FAQ's, on the PC and on the Amiga (its `MD5Sum`). With `base/` present the game takes it in place of `demo/`. Resurrection of Evil (`d3xp/`) not copied: it needs a build with `D3XP`.

**Done (2026-10-22): the map switch's hang found and fixed** (patch 0001). `idSessionLocal::CompleteWipe` (the fade after a map load) waits in a loop for `com_ticNumber` to reach the wipe's end; the async timer task advanced it, and `D3_SINGLE_TASK` has none, so the loop ran for ever with the screen frozen. The first map at startup escaped (`com_ticNumber` 0: the wipe is skipped); every later map load (New Game after a map, a level change, a saved game) hung. The loop now runs `common->Async()` as patch 0001's other loops do. Host, the full game (`mars_city1`, then `map game/mc_underground` typed through stdin, then `quit`): before, hung in `CompleteWipe` after the first load (the earlier host tests too, which a `+map A +wait +map B` command line could not get past); after, both maps loaded (3.3 and 2.3 s) and the game quit cleanly.

**Done (2026-10-22): the game startable from the Amiga, in `Stuff:doom3`** (copied there by the developer's request):
- `dhewm3`, `libs/ogles2.library` and `libs/v3dglslc` (ogles2 1.3.1), `low.cfg`, `medium.cfg`, `high.cfg`, and **`tests/a1200/play/Doom3.script`**: the game at its main menu (or `MAP=`), `W=`/`H=` (1920x1080), `QUALITY=` (low), `LIGHTS=` (`r_maxLights`, 12), `AUDIO=` (paula), `VOICES=`, `RATE=`, `SOUND=`, `HZ=` (20); it copies the settings file to `save/base` and `save/demo`, sets the 9 MB stack and the subset's variables.
- **The `Doom3` icon:** Workbench cannot start a script from a tool icon, so `Doom3` is a small launcher (`tests/a1200/play/src/doom3_launcher.c`, 4 KB) that runs `Doom3.script` from its own drawer (`GetProgramDir`) without a Shell window; its icon is the system's `def_Tool.info`. (A `dhewm3.info` the developer had made started the game without the script's settings.)
- **The game asked for the volume `WORK:`:** SDL2_68k's `SDL_OS3_DEBUG` logging writes `WORK:sdl_log.txt` and `WORK:sdl_debug.log` (the audio driver's, at its open). ogles2's SDL patch 0002 removed the flag from SDL's `Makefile`, not from `Makefile.libnix`, which this port builds with; the station's processes suppress volume requesters, so it never showed in a package. **`sdl2/patches/0100-gcc16-libnix`** now drops it there too; no `WORK:` path is left in the game.
- **Settings files and the full game:** with `base/` the game's save directory is `save/base/`, and `Run-Play` and `bench-one` copied their settings to `save/demo/` only, so every run with the full game's data until the fix ran at the detected Ultra settings (the first timedemos, `timedemo-20261022-065136` and `-065930`, and `play-20261022-070523`). Both scripts now copy to both.

**Found (2026-10-22): the console at the start was no hang.** Doom 3 opens its main menu at start-up only when its command line holds no commands (`idCommonLocal::Init`: `if ( !AddStartupCommands() ) StartMenu( true )`), and `Doom3.script`'s `+exec <quality>.cfg` is one: the game sat at its full-screen console until Esc. The script then ended with `+disconnect` when no map is given (`Session_Disconnect_f`: `Stop()`, `StartMenu()`); since patch 0028 it ends with `+startMenu` (below). With the 11 programs shipped in `Stuff:doom3/shaders/`, a start compiled nothing (no `v3dglslc` running, nothing new in `T:`). **Still open:** why `v3dglslc` ran for minutes without output and ignored Ctrl-C in the two starts before (no shaders shipped; ogles2's log not yet caught).

**Earlier reading (2026-10-22), kept for the record: the full game's main menu hangs the A1200 in ogles2's shader compiler.** Started without a map (`Doom3.script`, from the icon or from a Shell alike), the game compiles its 11 GLSL programs at start-up (the same 11 as the play sessions, and the host's at its main menu: `R_ReloadGLSLPrograms`), then `v3dglslc` starts a 12th compile and never finishes (no output for minutes; it ignores Ctrl-C); the game waits at its console. The sessions that went straight into a map never needed a 12th program. The host's main menu compiles nothing more, so the 12th is presumably one of ogles2's own internal programs (e.g. its GPU copies), first needed by the menu: an ogles2 matter, to be shown to it with its log (`ENV:OGLES2/LogFile`: which program, which source). The shaders go to `T:` (no `OGLES2Cache:` assign on the test machine), so every boot compiles them again; **the 11 programs are now shipped in `Stuff:doom3/shaders/`** (30 files, copied from `T:` on 2026-10-22; ogles2 looks in a program's own `shaders/` first; to be compiled and copied again after an ogles2 or compiler update). Until then: start with `MAP=` (`Doom3.script MAP=game/mars_city1`), which reaches the game.

**Found (2026-10-22): the main menu's hang is ogles2's window drawn without the copy, with screen copies.**
- The menu opened with `+disconnect` (and with the new `startMenu` command, patch 0028: the start-up's own `StartMenu( true )`, without `disconnect`'s `Stop()` and unmuting) ran 20 s to a minute, then the game hung; the pointer still moved but nothing clicked, the station's network starved. Without sound it did not hang (timing, not the cause).
- **Test 1, `ENV:OGLES2/WindowCopy 1`** (ogles2's old path: an offscreen bitmap and `ClipBlit`): the menu ran 3 minutes, no hang.
- **Test 2, the direct path with `r_skipCopyTexture 1`** (no `glCopyTexSubImage2D`): no hang, with sound.
- **So: `glCopyTexSubImage2D` from a window ogles2 draws into directly hangs** (in all likelihood on the screen bitmap's lock, which Intuition then waits for too). An ogles2 matter (its `f3704e2`), to be reported there with these tests.
- **The port's workaround:** `r_skipCopyTexture 1` in `low.cfg` and `medium.cfg` (the copies feed only post-processing, off there). `high.cfg` keeps post-processing and so the copies: it can hang until ogles2 is fixed (or runs with `WindowCopy 1` at half the speed).
- The two earlier starts whose `v3dglslc` ran for minutes were probably the same: the game hung in a copy while the compiler waited for the screen.

**Open (2026-10-22): a quality preset chosen in the game's menu hangs the game** (the developer, Medium, at the main menu: the menu's music stopped too; Workbench still reacted, the station's network starved, as with the copy hang). The menu's preset runs `execMachineSpec` and then `vid_restart` (`Session_menu.cpp`), which closes and reopens the GL context and the full-screen window: on ogles2's direct path not tested yet; the first suspect. Until known: settings only before the start (`Doom3.script`'s `.def QUALITY`, or `QUALITY=` from a Shell), nothing in the game's video options. The developer set `QUALITY` to medium in the Amiga's copy of `Doom3.script`.

**Found (2026-10-22, later): the menu freeze follows the game's output going to `NIL:`; the two readings above were wrong.** Each of their tests was one run of about 4 minutes, while the freeze comes at random, 20 s to 2 min in: a clean run that short proved nothing. What the runs show, all at the main menu, untouched unless noted:
- **Output to `NIL:` froze, every time:** every start from the icon (its launcher runs `Doom3.script` with `NIL:` in and out), and from the station `Execute Doom3.script >NIL:` (nothing else changed: froze at 112–134 s, `doom3-nilout-20261022-170809`). Through the launcher started from the station: froze at 41–63 s (`doom3-icon-20261022-164647`), at medium, with screen copies off (`medium.cfg`'s `r_skipCopyTexture 1`), the Paula patch 0103 and ogles2's copy fix in. The last freezes stopped the whole Amiga: the network, and the mouse pointer (input.device, priority 20) — so no task of priority 10 (SDL's audio thread) and no wait for the screen's lock alone explains them.
- **Output to a RAM file ran clean, every time:** every station run (`Execute Doom3.script >RAM:...`, among them dummy audio, `WindowCopy 1`, `r_skipCopyTexture 1`, low with copies, medium); the longest 9 min 13 s (`doom3-fileout-20261022-171315`); and **the developer's own icon start with the script's output to `T:doom3-out.txt`: 10 min 49 s, still drawing** (`doom3-icon-fileout-20261022-172343`).
- **Not the printing:** with output to the file the game wrote 4,006 bytes up to the menu and not one byte more in 9 minutes (the size read every 20 s). So the output's speed cannot be it, and why `NIL:` matters is not known. libnix sets the standard streams up the same way for both (`__initstdio`: `IsInteractive`, stderr from `pr_CES` or `Open("*")`, else stdout); the game never reads its input (`Sys_ConsoleInput` returns `NULL`).
- **Not the GPU clock:** the developer had taken the overclock back long before.
- **Not a workaround after all:** `Doom3.script` sends the game's output to `T:doom3-out.txt` (`dhewm3 >T:doom3-out.txt ...`), also on the Amiga (its `QUALITY` medium kept). The developer's icon start with it ran clean for the watch's 10 min 49 s and then **froze at about 14 minutes** (the same session). So the output only correlates with how soon the freeze comes, if at all: the clean file runs were the long ones by chance or by rate, and the freeze's cause is still open. Timing tests cannot settle a freeze this random; the next step records what the game is doing when it comes (below).
- **Kept, but not shown to cure anything:** ogles2's copy fix (`ogles2/copy.c`, uncommitted there: the screen's lock given back before the TFU reads the screen; on display the bitmap does not move) and `sdl2/patches/0103-os3-paula-no-spin` (the Paula thread always sleeps in `WaitDevice`, and reports audio.device's errors when it closes). Both are sound on their own.
- **Also seen:** one boot came up without `LIBS:` assigned; the game then exits at once (`mathieeesingtrans.library failed to load`), and from the icon nothing shows. A clean reboot brought `LIBS:` back.

**Found and fixed (2026-10-22): the menu freeze is a livelock on SDL's spinlocks** (`sdl2/patches/0104-os3-atomics-no-spin`).
- **The flight recorder** (`tests/a1200/pc-sample/src/flight.c`, built by `build_pc_sample.sh`; `tools/flight_report.py` names the addresses): at priority 21 it appends, every 250 ms, the game process's state and PC, the return addresses on its stack, and every ready task with its priority, to a file closed after each line. `Doom3.script` on the Amiga starts it (only there, for now: `Stuff:doom3/flight.txt`).
- **What it caught** (`doom3-flight-20261022-175915`, the developer's icon start, frozen): the records went on through the freeze, 2,041 in all. In every one of the last, the game's task (priority 0) was ready but standing at the same PC, `SDL_AtomicLock+4c`, under `SDL_AtomicAdd`; and `SDL_SDLAudioP2`, SDL's audio thread (priority 10), was ready in every record: it never slept.
- **Why:** this SDL has no native atomics (`HAVE_GCC_ATOMICS` off in `SDL_config_amigaos3.h`), so `SDL_atomic.c` emulates every atomic operation (`SDL_AtomicGet`, `Add`, `CAS`; the audio thread's loop and the game's event and joystick code use them all the time) with one of 32 spinlocks. `SDL_AtomicLock` waits with `SDL_Delay(0)`, `Delay(0)`, and exec always runs the highest-priority ready task: no yield reaches a lower one. When the audio thread wanted the lock the game held, it spun at priority 10 for ever; the game never ran again to release it, and everything under 10 (the network at 5, the Workbench) stopped. At random, as often as the two met on a lock: 20 s to 14 min. Everything earlier read into the freeze (screen copies, the Paula driver, the output to `NIL:`) was chance.
- **The fix (SDL patch 0104):** on AmigaOS 3 the atomics run under `Forbid()`/`Permit()` (atomic against every task on one CPU; no spinlock, nothing waits), and `SDL_AtomicLock`, still used for a few rare locks, sleeps a tick (`SDL_Delay(1)`) between tries. `SDL_atomic.o` now needs only `SysBase`. The game rebuilt with it (`dhewm3` MD5 `4b251c06...`) is in `Stuff:doom3`; the old one's symbols are kept for the record (`~/doom3-build/flight-bin/dhewm3-ffaf39d6`).
- **Tested: the menu for 22 minutes, no freeze** (the developer's icon start, medium, 1920x1080; the freezes had come within 20 s to 14 min). The flight record of it (`doom3-flight-0104-20261022-182733`): 5,298 records; the game at 638 different PCs and never in `SDL_AtomicLock`; the audio thread ready in 240 records (4.5%: its wake-ups), where in the frozen run it was ready in every record.
- **Tested in the same session:** the in-menu quality preset (the game said it takes effect at the next start; no freeze), and typing in the console (a command accepted; it had frozen the A1200 since patch 0102: the same livelock, patch 0102 was not the cause).
- **Tested: screen copies on again** (`r_skipCopyTexture 1` out of `low.cfg` and `medium.cfg`, and `high.cfg`'s warning out): the developer's icon start, the main menu and a New Game, about 32 minutes, "perfect", quit from the menu (`doom3-flight-copies-20261022-191114`: 7,871 records, the game at 1,314 PCs, never in `SDL_AtomicLock`, the audio thread ready in 282). The flight recorder is out of the Amiga's script again; `Doom3.script` keeps the game's output in `T:doom3-out.txt`.
- **ogles2's copy change reverted** (`ogles2/copy.c`, the screen's lock given back before the TFU reads it): not the fix, so not kept (the developer's decision). `Stuff:doom3/libs/ogles2.library` is the 1.3.1 release's again (MD5 `dfa97bbc...`, as in `ogles2-1.3.1.lha`; a clean rebuild of ogles2 gives the same file).
- **Offered to ogles2:** the same fix as its SDL port's `patches/0005-os3-atomics-no-spin.patch` (built there with GCC 6.5 and clib2).

**Done (2026-10-22): release 0.1** (`release/`; `bash release/build_release.sh dist` → `release/dist/Doom3-Amiga-0.1.lha`, 8.5 MB, and its Aminet-style readme). The drawer `Doom3`: the game (`dhewm3` `4b251c06`, the build tested 32 min), the launcher `Doom3`, `Doom3.script`, the three settings files, `libs/` (ogles2 1.3.1's library and `v3dglslc`, from ogles2's committed archive), `shaders/` (the 11 programs, `release/shaders/`), empty `base/` and `demo/` with notes, `licences/`, the guide `ReadMe` (`release/ReadMe`) and icons (`release/icons/`, made on the A1200 by `release/src/mkicons.c`). Changes for it: the script runs in its current directory (no `Stuff:doom3` in it), the launcher switches to its own drawer and passes on options (the icon's Tool Types, at the defaults, or the Shell's arguments), and the script refuses an `ogles2.library` older than 1.3 in `LIBS:` (checked by file: `Version` by name searches `LIBS:` only, which first refused the release's own `libs/`). Tested from the unpacked archive in a new drawer with the full game's data: from a Shell with `QUALITY=medium` and from the icon with `QUALITY=high`, both to the main menu, each quality's file used. The source link in it (`github.com/ladislawv/doom3-amiga`) is public from the release on (the developer).

**Done (2026-10-28): the game builds on ogles3.library** (ogles3 1.0-beta1, `C:\GitHub\ogles3.1` at `94eb81b`: ogles2 with OpenGL ES 3.0 and 3.1, and ES 3.0's features as extensions in GLES2 contexts). The first step to measure what it would bring: the game unchanged, in its GLES2 context, against ogles2 1.3.1.
- **Why not compute shadows (read from ogles3's `docs/GLES3_PLAN.md`):** `glDispatchCompute` waits for its job (under the GPU lock), and a buffer the GPU wrote is used as vertices, indices or indirect commands only after the CPU has waited for it and swapped it back (its §8: "a full stall and a swap at every use"; indirect draws are read on the CPU). The shadows' CPU cost here is the draws and the per-interaction cull and scissor; building the volumes was about 2.3 ms with all lights (above). `glMultiDrawElementsEXT` is a loop of `glDrawElements`. Candidates instead, each to be measured: uniform buffers for the interaction draws (an ES 3.0 context; a draw passes the block's address), skinning in the vertex shader with the joints in a uniform buffer, occlusion queries.
- **`OGLES=3 bash build_sdl2.sh`** builds SDL with ogles3's copies of the backend (`sdl2/ogles3/`, `sdl2/PROVENANCE.md`) and the ogles3 release SDK's headers (`OGLES3_SDK`, default `~/ogles3-build/release/ogles3/SDK`), into `~/doom3-build/m68k/sdl2-ogles3`; the default build is the same file as before. **`OGLES=3 bash build.sh`** links `libogles3.a` and builds into `game-ogles3` (`dhewm3` MD5 `8e66e4ba...`).
- **Packages:** `build_pkg.sh` adds `ogles3.library` and `v3dglslc3` (the release's `Libs/`, `OGLES3_RELEASE`) when an executable opens ogles3.library; `bench_pkg.sh` takes `name=build:args` (`"o3=game-ogles3:..."`), so one bench runs both libraries; `Run-Bench` lists `LIBS:ogles3#?` (one installed there wins over the package's) and sets `ENV:OGLES3/Probe` and `LogFile` (`results/ogles3.log`) with `OGLES2PROBE=1`.
- **The bench package's map runs are void with the full game's data:** with `base/` in `Stuff:doom3` the game takes it in place of `demo/`, so `demo_mc_underground` is missing and the game stays at its main menu (intro and menu shown; the developer had to start a game from the menu by hand). Two runs measured that (`bench-20261028-221817`, `-222451`; the second shows the same figures for both builds, a front end of 1.7 ms: the menu). No map bench ran between the full game's data's arrival (2026-10-22) and these; the earlier ones had the demo's data only.
- **`Run-Bench DEMO=demo1`** (with `bench-one`): `timeDemo demo1 usecache quit` in place of the map, with sound through SDL's Paula driver (a silent timedemo crashes, "Next" 6 below), no menu, quits by itself; the fps from the timedemo's own line.
- **ogles3 against ogles2** (`bench-20261028-223033`: `timeDemo demo1 usecache`, 1920x1080, `low.cfg`, `r_maxLights 12`, Paula; the 0.1 release's game `4b251c06` on ogles2 1.3.1 against `game-ogles3` `8e66e4ba` on ogles3 1.0-beta1 in its GLES2 context, the shaders compiled and cached in the run before): **ogles2 17.6 fps (122.0 s), ogles3 16.3 fps (131.5 s), 8% slower.** The ogles2 figure repeats `timedemo-qualities-noshadows-20261024-154704`'s low (17.5). From the libraries' logs (`T:ogles2.log`, `T:ogles3.log`: both contexts 4,354 frames, the precache pass and the timed one, 237 draws a frame):

  | per frame | ogles2 1.3.1 | ogles3 1.0-beta1 |
  |---|---|---|
  | in the draws | 5,198 µs (21.9 µs a draw) | 7,435 µs (31.4 µs a draw) |
  | CPU work before each flush | 12,740 µs | 14,804 µs |
  | the GPU's render | 3,565 µs | 2,222 µs |
  | swap to swap | 47.4 ms | 49.6 ms |

  ogles3's draw costs 43% more CPU; its render is faster, but the GPU is not the limit here. Both contexts get D32F depth at 1920x1080. So ogles3's features would first have to win back 9.5 µs a draw.
- **The draw's stages** (`bench-20261028-223954`, the same timedemo with `OGLES2PROBE=1`: 17.4 and 16.0 fps, so the run repeats; probe P's whole-context sums, `results/ogles2.log` and `ogles3.log`, divided by the 237 draws a frame), µs a draw:

  | stage | ogles2 1.3.1 | ogles3 1.0-beta1 | |
  |---|---|---|---|
  | checks | 4.6 | 6.8 | ×1.46 |
  | shaders | 0.55 | 2.2 | ×3.9 |
  | textures | 2.2 | 3.8 | ×1.74 |
  | uniforms | 3.1 | 3.8 | ×1.20 |
  | vertex data | 9.1 | 11.8 | ×1.31 |
  | record | 1.1 | 1.5 | ×1.31 |
  | indices | 1.4 | 1.7 | ×1.25 |
  | binning list | 2.0 | 3.1 | ×1.56 |
  | **in all** | **24.1** | **34.6** | **×1.44** |

  The same work in both (39 uniform stream words, 20 bytes of vertex data and 174 indices a draw), and every stage slower: the most in the shaders (the draw-time variant keys of ogles3's stage A, presumably) and the textures (its sampler variants), but also in stages ES 3 did not change, which points to something general (code size, the JIT's cache, checks for ES 3 state on every draw). In the heaviest 256 frames (627 draws a frame) ogles2 spent 13.1 ms a frame in draws, ogles3 19.3. **Taken to ogles3 as §13 request 3.** Until it is answered, ogles3 is slower for this game, and its ES 3 features (uniform buffers, skinning on the GPU) are not worth starting.
- **2026-11-04, the full game played (`play-20261104-070458`, 1920x1080 low, 60 and 30 Hz): the in-game screens take no click.** `mars_city1`'s exit elevator (after Kelly's briefing; the sentry only leads the player there) opens only from its panel `func_static_4316` (`guis/doors/elevatorcall.gui`, targets `func_door_5`, `func_door_7`, `func_door_16`; inside, `trigger_relay_36` is the level's end, `nextMap game/mc_underground`). The developer's click did nothing at 20, 60 and 30 Hz, so the tic rate is not the cause; the console's `trigger func_door_5` and `trigger func_door_16` opened the doors. The click's path: `BUTTON_ATTACK` toggled while `focusUI` is set (`idPlayer::Weapon_GUI`), the focus from `UpdateFocus`'s GUI trace on the panel's model. The click arrives (the cursor shows, the panel plays `guisounds_error1`): the panel's screenshot (the developer's, `bind p screenshot`; the Amiga has no F12) shows "Elevator access denied / system malfunction", which the GUI shows only when `gui_parm5` is 2, and then only its `Button2` (the error sound) takes the click. Nothing in `mars_city1.map`, its script or the defs sets that panel's `gui_parm5`, A fresh `map game/mars_city1` shows the same, so not the save. **The cause: libnix's `strtod` and `atof` return an undefined value for a text without a number** ("", " "; "abc" gives 0), where C says 0. The first probe called `atof("")` first and got 0 by chance; called after others it returns leftovers (7.5e-262 in the probe, in the game the previous conversion's result). A GUI condition reads a string state value with `atof` (`WOP_TYPE_VARS`, `idWindow::EvaluateRegisters`): the panel's unset `gui_parm5` (and `networkState`) came out as the 2 of `gui_parm1` converted just before. Found with a diagnostic patch, not committed (`guiState <entity>`: the entity's gui spawn args, each GUI's state, its desktop's registers, ops, variables and children's visibility; `play-20261104-081743` to `-085414`, the game started with `dhewm3.args` `+map game/mars_city1 +wait 300 +guiState func_static_4316 +quit`) and the host build at the same point (r14 = 0, `Button1` shown). **Fixed:** `src/amiga/strtod_fix.c` (in `libd3amiga.a`), the game linked with `-Wl,--wrap=strtod -Wl,--wrap=atof` (`build.sh`): no number, 0 and `*endptr` the text; else libnix's conversion. On the A1200 the panel's registers and windows now match the host's (`play-20261104-085414`). Any other GUI or code that converts an empty string was hit the same way before.

- **The developer's timedemos at 1920x1080** (`timedemo demo1 usecache` from the console, the strtod fix in): `ultra.cfg` (Ultra, shadows off) at 20 Hz **14.8 fps** (`play-20261104-091711`), `low.cfg` at 30 Hz **15.2 fps** (`play-20261104-092511`). A render demo replays recorded frames, so the game's tic rate hardly enters; the settings' difference is 3%.

**Done (2026-10-29): ogles3's newer build against ogles2 1.3.2 — the same 17.6 fps** (ogles3's `docs/GLES3_PLAN.md`, "Performance, the evening of 2026-10-29": a GLES2 draw now cheaper than 1.3's in its own tests). `timeDemo demo1 usecache`, 1920x1080, `low.cfg`, `r_maxLights 12`, Paula, 20 Hz, both games built on 2026-10-29 from this repository (`game`, `game-ogles3`), ogles3 from `LIBS:` (its build of that evening), ogles2 1.3.2's release in the package: **ogles3 2148 frames in 122.0 s = 17.6 fps** (`bench-20261029-213644`), **ogles2 122.2 s = 17.6 fps** (`bench-20261029-214036`). 1.0-beta1 was 16.3. Request 3 is answered for this game: no longer slower; the game's own CPU is most of its frame, so the library's lead does not show in the frame rate.
- **A stale `deps/` on the build machine cost a night:** `~/doom3-build/m68k/deps/lib/libd3al.a` was from 2026-10-21 00:07, before the Paula and byte-order fixes (`da7a895`, `1dd881e`), and `build.sh` links what is there without rebuilding it. The games then ignored `SDL_AUDIODRIVER`, opened AHI (2 channels, 16 bits) and ran at 10.1-10.3 fps with either library, `doom_main.script` compiled in 15 s instead of 5.5 (`bench-20261029-203137` to `-212835`, void). The logs' "SDL audio driver" line and that compile time show it. **On a new or old build machine run `build_deps.sh` and `build_al.sh` before `build.sh`.**
- **`tools/bench_pkg.sh` gives 20 game tics a second now** (as `play`; the developer's choice): the timedemo's figures are the same at 20 and 30 Hz (`bench-20261029-210359` against `-204356`, both with the stale `deps/`).

**Done (2026-10-29, later): §15's step 0 and two patches; the demo read into memory; the GPU near its limit at 1920x1080.** (ogles3 only; the figures are `timeDemo demo1`, 1920x1080, `low.cfg`, `r_maxLights 12`, Paula, 20 Hz, unless said.)
- **Where the timed demo's CPU goes** (PC sampler over the timed part, `bench-20261029-215823`; the sampler's window opened 95 s in by a script, the package's copy of `bench-one` only): ogles3.library 25%; exec 13.7% (the C library's `malloc` and `free` from the vertex cache through exec's `Allocate`/`Deallocate`, 5.9%; samples after `Supervisor()`'s return, 7.4%, not cache maintenance: counted, about 20 `CacheClearE` a second, 65 µs a flush; the sampler's artefact seen in ogles3's own tests); the demo file's reading 14% (below); renderer front end 16%, back end 14%; animation and skinning about 3-4% (`TransformVerts`, `DeriveTangents`, `NormalizeTangents`, the bounds): GPU skinning (§15 step 4) could win little in this demo.
- **Patch 0029, `glDrawRangeElements`** (`r_useDrawRange 1`, where the library has it, ogles3 in every context): `RB_DrawElementsWithCounters` passes the surface's vertex range (its ambient cache block's size), so the library does not scan the indices: ogles3's per-draw checks 1.6 ms a frame at 1.0-beta1, 0.4 ms now.
- **Patch 0030, the vertex cache on dlmalloc:** `idVertexCache`'s CPU copies of its blocks (`frontEndMemory`) came from libnix's `malloc`, which takes each block from exec (idlib's heap does too for large blocks); now `dlmalloc`/`dlfree` (`src/amiga/d3malloc.c`). With 0029: **18.5 fps** (`bench-20261029-220736`; 17.6 before).
- **The demo streamed from the pk4:** with `com_preloadDemos 0` (the default) `idDemoFile` reads `demos/demo1.demo` from its compressed pk4 entry 4 bytes at a time (`ReadInt`, `ReadFloat`), each read a call into zlib (`unzReadCurrentFile`, `inflate`, the CRC): 14% of the sampled CPU. **`+set com_preloadDemos 1`** reads and inflates it once at the start: **19.0 fps** (`bench-20261029-223126`). `tools/bench_pkg.sh` sets it for every build now; the earlier timedemo figures (above, §13) streamed it. Play reads no demo.
- **The GPU near its limit at 1920x1080:** ogles3's probe P (`bench-20261029-221526`): the swap waits for the last frame's render 4.8 ms a frame on average, 0.56 ms for its binning. At 1280x720 (`-222135`): 22.7 fps against 18.3 (both with the probe), the render wait 0.03 ms. So at 1920x1080 the CPU's savings show only in part; what is left: a lower internal resolution (`r_framebufferWidth`/`Height`), the GPU overclocked (+8% on 2026-10-22), or ogles3's render list (§15; ogles3's own plan).
- **ogles3's render list for this game (the GPU side, the developer's request):** with `com_preloadDemos 1` the swap waits 7.8 ms a frame for the last render (`bench-20261029-225140`), so the GPU sets the pace at 1920x1080. ogles3's probe C (every 256th job, `bench-20261029-230320`): every frame's colour is cleared (d3es's `r_clear 2`, a black clear each frame: no load of the last picture into the tiles), early Z on. So the render list has nothing to drop: a patch discarding the colour at the frame's start (`glDiscardFramebufferEXT`, ogles3 now honours it for the default framebuffer) changed nothing (18.9 fps either way) and was not kept; bilinear mipmaps (`image_filter GL_LINEAR_MIPMAP_NEAREST`) neither (19.0). The GPU's time is the scene's shading at 1920x1080. What is left there: a lower internal resolution (`r_framebufferWidth`/`Height`: 1280x720 ran 24% faster), fewer lights (`r_maxLights`), the GPU overclocked.

**Done (2026-10-30): the tick rate follows the frame rate (patch 0031, the default).** Doom 3 renders one frame a game tic, so a fixed `com_gameHz` caps the frame rate at it: at 1280x720 the game sat at 20 fps at 20 Hz (the developer, `play-20261029-231958`), and a rate the A1200 cannot keep runs several tics a frame.
- **Patch 0031, `com_gameHzAuto`** (on in the Amiga build; `com_gameHzMin 20`, `com_gameHzMax 60`, the developer's range): every half second the frames rendered are counted and the tick rate set **5 Hz above them** (the developer's rule), within the range; while the frames keep up with the rate it rises 5 Hz a half second, where the A1200 cannot render faster it stays 5 Hz above what it renders. Changed only by 2 Hz or more, or to a bound; a half second with a frame of more than 500 ms (a load, a hitch) is not counted. The rate is set in effect (`Com_SetGameHz`: tic length, `game->SetGameHz`), not in `com_gameHz`, which stays the start value and is not written to the configuration at every change; game time adds up tic by tic at each tic's length (`Com_UpdateFrameTime`), so a change moves no clock. Each change prints "Running the game at com_gameHz = ..." to the console.
- **Tried first, on the developer's word:** fixed 25 and 30 Hz; 5 Hz steps from 15 to 60 Hz after two seconds that kept up (`play-20261029-234552`: 39 changes, map loads dropped it to 15); then half-second windows, 25 to 60 Hz, loads not counted (`play-20261029-235127`: it flipped between 25 and 30 Hz where the machine made 27-28 fps).
- **At 1920x1080** (`play-20261030-000850`, the rule above, Paula, low): 253 changes, mostly 20-31 Hz (the frames 15-26 a second), 40-53 Hz in quieter places.
- **Benchmarks keep a fixed rate:** `tools/bench_pkg.sh` passes `com_gameHzAuto 0` (with `com_gameHz 20`).

**Done (2026-11-10): the game data prepared at the first start (§16, `patches/0032-data-prepare`).**
- **The inventory** (`tools/prep/inventory.py <folder>`): the full game's 9 pk4s, 25,609 files, 1,556 MB; prepared (what a load reads stored, the 3,566 Oggs below 60 s as PCM, the 7,358 TGAs a DDS covers kept compressed) about 3.98 GB: 2.72 GB inflated (`.tga` not covered, `.dds`, `.proc` 537 MB, `.cm` 346 MB, `.map` 205 MB, `.md5anim` 218 MB, ...), 0.78 GB of PCM. Oggs: 3,425 of 3,608 are 22 kHz mono. The demo: 484 MB to 1.05 GB, 1,241 Oggs decoded.
- **The patch:** `framework/DataPrepare.cpp` (in `InitGame` after `InitRenderSystem`: the question, the progress screen, the conversion, the manifest); the mount in `AddGameDirectory`; `idSoundSample::DecodeOggToShorts` (Load's decode, shared) and `Sound_PrepareOgg` (`snd_decoder.cpp`); `Sys_GetDriveFreeSpace` real on the host (`statvfs`) and the Amiga (`Info()`, `src/sys/amigaos3`). Cvars: `fs_prepare` (0 never, 1 ask, 2 without asking; the Amiga build defaults to 1, the host's to 0), `fs_usePrepared` (1; 0 loads the originals), `fs_prepareSoundLimit` (60), `fs_prepareKeep` (`roq demo`). `tools/bench_pkg.sh` and `tools/map_pkg.sh` pass `fs_prepare 0`; a complete `prepared/` is still loaded by them.
- **Host** (WSL): the demo prepared in 4.4 s, the full game in 15 s. `demo_mars_city1`'s screenshot 150 frames in **byte-identical** from the original and the prepared pk4 (sound off); with sound, the same 606 sounds and warnings (`guisounds.wav` fails in both). Redone when the settings change (the prepared pk4 loaded at the time: the file system is shut down, rewritten, restarted) and when an original's date changes; the output is deterministic (the same checksum, 0xbf8e6ebe, from two preparations).
- **A1200, the full game** (`Stuff:doom3/base`, 25.6 GB free; `map-20261110-170954`, `fs_prepare 2`): **12 min 36 s** (the first estimate 19:04, from guessed rates): inflated and stored 2,241 MB in 383 s (5.85 MB/s), copied 706 MB in 131 s (5.4 MB/s), 3,537 sounds decoded to 741 MB of PCM in 209 s (3.55 MB/s), 42 kept Ogg; 3,703 MB written, no pk4 over 1.31 GB (no split). The estimate now uses these rates (the progress bar's weights too: at 7:02 it showed 13:25 left for a real 5:34). The progress screen's pictures: `shots/`.
- **`mars_city1` loaded** (sound on, `s_decompressionLimit 60`, `map` package):

  | | original pk4s (`-170702`, `-172807`) | prepared (`-172535`) |
  |---|---|---|
  | map load | 96.0, 94.8 s | **82.0 s** |
  | start to playable | 120.3, 113.5 s | **100.9 s** |
  | sounds (690) | 28.3, 27.8 s | **14.0 s** |
  | DDS images (2,066) | 16.4, 16.3 s | 14.8 s |
  | models (489, with their images) | 15.5, 15.5 s | 15.0 s |
  | pk4 file opens | 6,110 in 6.8 s | 5,076 in 5.1 s |

  The screenshots differ by as much between two original runs as between original and prepared (46% and 51% of the bytes, by 16 levels or less but for 5 and 122): with sound on the picture 150 frames in varies from run to run; the two prepared runs gave the same one.
- **What it shows:** decoding was half of the sounds' time; the rest (14 s for 690 sounds, about 20 ms each) is reading the PCM and making the buffers. **Inflating was not most of a DDS image's time** (§14, 2026-10-20, had assumed it): stored, a DDS still takes 7.2 ms (9.2 before), so the open, the reads and the upload's preparation weigh more. Images without a DDS (§16 step 3) cost 0.4 s of this load (27 images): not built. Not measured yet: the memory in use during play (`avail-after` is taken after the game quits). A PCM sound is held twice (the sample's `nonCacheData` and the OpenAL subset's copy), an Ogg sound as its Ogg file plus the copy, so the prepared data should take about the PCM size of the map's sounds more.
- **The Amiga's `Stuff:doom3/base/prepared/`** holds the full game's preparation (3.7 GB). The released 0.1 game does not know it.
- **Switched off (the developer, the same day): 95 to 82 s is not a significant speed-up.** Patch 0032 stays, with `fs_prepare 0` and `fs_usePrepared 0` by default on every build: the game starts as before (the original pk4s, no question; checked on the host with a complete `prepared/` present). `+set fs_prepare 2 +set fs_usePrepared 1` turns it on. The ReadMe, the packages and `tools/` carry nothing of it; `tools/prep/inventory.py` stays.

**Done (2026-11-10): 1280x720 and the frame rate by default; the settings saved and used (patch 0033, the developer's request).** Before, `Doom3.script` (and the icon's Tool Types, `W=1920 H=1080 QUALITY=low LIGHTS=12 ...`) passed every setting at every start, so a change in the game's menu was undone at the next start, and a quality from the menu changed the screen mode too (Doom 3's presets set `r_mode`).
- **Patch 0033** (`framework/Common.cpp`, AmigaOS only): the first start's port settings (`Com_AmigaFirstStart`: `r_mode 9`, full screen, `com_showFPS 1`, `r_swapInterval 0`, `r_maxLights 12`, `com_gameHz 20`, Low) in place of the memory's "Ultra"; the second detection pass (with 6 speakers and a sound restart) skipped; `SetMachineSpec` (the menu's "recommended") gives Low; `execMachineSpec` keeps `r_mode` and `image_usePrecompressedTextures 1`, then runs the port's file for the preset if the save directory has it; the archived settings marked changed after start-up, so `dhewm.cfg` is written at every start's first frame (the first start's settings and the launcher's options are otherwise saved only when something else changes).
- **`Doom3.script`:** W, H, QUALITY, LIGHTS, HZ empty by default and passed only when given (QUALITY as `+set com_machineSpec N +execMachineSpec`); `RESET` deletes the saved settings (`dhewm.cfg`, `config.spec`; the saved games stay); full screen and precompressed textures still on every start; the four `.cfg` files copied to the save directories. The `.cfg` files lost their `com_machineSpec`/`execMachineSpec` lines (the preset runs them now); `Run-Play` uses the preset the same way. `ultra.cfg` goes into the release. **The icon's Tool Types** are now in parentheses (off) but `AUDIO=paula` (`release/src/mkicons.c`; `release/icons/Doom3-tool.info` made again on the A1200). The ReadMe says so.
- **A1200** (`settest-20261110-175330`: the shipped `Doom3.script` seven times on a fresh `save/`, its data from `Stuff:doom3`, 300 frames at the menu each): the first start opened 1280x720 (`r_mode 9`) with the frame rate, Low; a console change (`com_showFPS 0`, `r_maxLights 8`) was saved and kept at the next plain start; `W=1920 H=1080 QUALITY=high` saved and kept (shadows and decals on); the menu's quality Medium (`com_machineSpec 1`, `execMachineSpec`) kept 1920x1080 and ran `medium.cfg`; `RESET` gave the first start again. (`settest-20261110-174901` was the same test with the console commands passed as one argument: not executed.)
- **For measurements:** a fresh bench or map package now starts from Doom 3's Low preset (`image_useNormalCompression 2`, `image_useCompression 1`, ...) where it started from Ultra's before, under whatever the package sets; the packages set their screen size and quality as before. Compare across this change with care.
- **The developer's own `save/`** keeps its settings (the last ones the old launcher forced, 1920x1080): `Doom3 RESET` once gives the new defaults.

**Done (2026-11-10): the tick rate always automatic, DDS textures at every setting (patch 0034, the developer's request); `Stuff:doom3` updated.**
- **Patch 0034** (AmigaOS only): `Com_AutoGameHz` always runs, in a fixed 20-60 Hz (`AMIGA_GAMEHZ_MIN/MAX`); the rate starts at 20; `com_gameHz`, `com_gameHzAuto`, `com_gameHzMin`/`Max` change nothing (console, command line, configuration). `idImage::ActuallyLoadImage` tries the DDS first whatever `image_usePrecompressedTextures` says (Ultra's preset sets it 0).
- **A1200** (`map-20261110-182023`, `mars_city1`, the package passing `com_gameHzAuto 0`, `com_gameHz 30`, `image_usePrecompressedTextures 0` and the Ultra preset): the rate started at 20 Hz and followed the frames (28 to 55 Hz, 13 changes in the 10 s counted, 42.1 fps at 640x480); 2,066 of 2,079 images from DDS (the other 13 have none).
- **Scripts:** `Doom3.script` lost its `HZ` option (still accepted, ignored, so an old icon or habit does not stop it) and no longer passes `image_usePrecompressedTextures`; the icon's `(HZ=20)` went (`release/icons/Doom3-tool.info` made again); `Run-Play`'s `HZ=` does nothing now; `tools/bench_pkg.sh` and `tools/map_pkg.sh` no longer set a rate. **Benchmarks from here on run at the automatic rate**: a timedemo replays recorded frames, so it hardly matters there; a map benchmark's game time per frame does change with it.
- **`Stuff:doom3` updated** from `release/build_release.sh`'s drawer: `dhewm3` (md5 `1ddd0b7b...`, checked after the copy), `Doom3`, `Doom3.info`, `Doom3.script`, the four `.cfg` files, `ReadMe` and its icon. The replaced files are in `Stuff:doom3/old-0.1/`; `libs/`, `shaders/`, `save/` and the data unchanged. Its `save/base` still holds the settings the old launcher forced (1920x1080): `Doom3 RESET` once for the new first start.

**Done (2026-11-10): the menu offers only the screen sizes that exist (patch 0035, the developer's request).** The system menu's size list was d3es's fixed table (`r_vidModes`, modes 3-23: up to 4096x2304), whatever the screen could do. Now, on AmigaOS, `R_GetVidModeListString`/`R_GetVidModeValsString` (filled into the menu by `ChoiceWindow.cpp`) keep only the sizes that have a 32-bit RTG mode (`Sys_ScreenModeAvailable` → `amiga_rtg_mode_exists`, `src/sys/amigaos3/amiga_os.c`: cybergraphics.library, 4 bytes a pixel, exactly that size, as SDL's `OS3_Find32BitMode` picks one; a local `CyberGfxBase`, since SDL owns the global one). Asked once; the list goes to the log. Without cybergraphics.library, or with no size found, the whole table stays. The custom entry (`r_mode -1`) stays. **A1200** (`settest-20261110-185134`): 12 sizes of 21: 640x480, 800x600, 1024x768, 1280x720, 1280x1024, 1366x768, 1400x1050, 1600x1200, 1680x1050, 1920x1080, 1920x1200, 2560x1440. A size the Amiga has but the table lacks (1280x800, 960x540, ...) is still only reachable through `W=`/`H=`. `Stuff:doom3/dhewm3` updated (md5 `09b6b35c...`).

**Done (2026-11-10): colour icons (the developer's request).** Original art drawn on the PC (`release/icons/art/make_icons.py`, Pillow and the DejaVu fonts, drawn at 4x and scaled to 64x64, each image mapped to 255 colours plus a transparent one): the launcher a riveted, cracked steel plate with hellfire behind it, "DOOM" and a burning "3" (selected: hotter); the drawer a sci-fi supply crate with hazard stripes and a "3" decal (selected: the drawer open, glowing inside); the guide a datapad with amber text (selected: brighter). Nothing from the game's data or id's artwork. The art goes as `.d3ic` files to `release/src/mkicons.c` (`mkicons DIR ART`), which builds them on the Amiga into the system's default icons as GlowIcons (`IconControl`: palette, transparent colour and pixels of both images, frameless; icon.library 44, OS 3.5 and later; the planar default image stays for older systems). The A1200's icon.library 51.4 wrote them; decoded back on the PC they hold the art unchanged. `release/icons/` updated; installed on `Stuff:doom3` (`Doom3.info`, `ReadMe.info`, `Stuff:doom3.info`; the old ones in `old-0.1/`, the drawer's as `drawer.info`). The Workbench there is Directory Opus Magellan, which caches icons: the new ones show once its window rereads the drawer.

**Done (2026-11-10): release 1.0-beta1** (the developer's version; `release/VERSION`; `bash release/build_release.sh dist` → `release/dist/Doom3-Amiga-1.0-beta1.lha`, 8.5 MB, SHA-256 `cc871a28...`, and its Aminet readme; 0.1's archive left `dist/`, as the script keeps only the current one, and stays in the history at `445e490`). Since 0.1: the first start's port settings and the settings saved (patch 0033), the tick rate always automatic and DDS textures at every setting (0034), only the screen sizes that exist in the menu (0035), colour icons, the credit "by R-Typer and Ladislawv", the data preparation off (0032). The game in it: `dhewm3` md5 `09b6b35c...` (the build tested on the A1200 today); ogles2.library 1.3.1. Not yet played through as a whole from the archive (0.1 was: unpacked into a new drawer, started from a Shell and from the icon).

**Done (2026-11-10, later): 1.0-beta1 ships ogles2.library 1.4-beta1 (the developer's `ogles2pi3`: the Pi 4 and the Pi 3), tested from its archive.**
- `release/build_release.sh` takes ogles2 from `../ogles2pi3/release/dist/ogles2-1.4-beta1.lha` (the same layout as 1.3.1's: `Libs/ogles2.library`, `Libs/v3dglslc`, `LICENSE`, `licences/`). The game's link library is unchanged (an import library from 1.3 opens 1.4). `Doom3.script` now asks for 1.4 in `LIBS:` (a Pi 4-only 1.3 there would be found first and fail on a Pi 3).
- **The shaders made again with 1.4** on the A1200 (the 11 programs' binaries cleared from `T:`, `demo_mars_city1` loaded with the drawer's 1.4): all 11 `.v3dp` differ from 1.3.1's, the 19 `.v3ds` records are the same. They are the Pi 4's; a Pi 3 compiles its own (`.vc4p`, `.vc4k`) at first use with the shipped compiler. The Pi 3 decodes S3TC on the CPU into RGBA8 (4-8 times the memory, ogles2pi3's `docs/PI3_PLAN.md`), has 1 GB and a slower Emu68: Doom 3 not tried on one; the ReadMe says so and suggests low and 640x480 there.
- **The archive tested on the A1200** (`Doom3-Amiga-1.0-beta1.lha`, 8.7 MB, SHA-256 `7fa09bd7...`): unpacked into a new drawer with the demo's data, the launcher started from a Shell with `MAP=game/demo_mars_city1`, the shaders' binaries removed from `T:` first: the first start set 1280x720, the frame rate and Low; the map loaded in 81 s; the intro played at 37 fps; quit from the console (rc 0); `save/demo/dhewm.cfg` held `r_mode 9`, `com_showFPS 1`, `com_machineSpec 0`; nothing compiled into `T:` (the shipped binaries used). Not tested from the icon (the Workbench there is Directory Opus).
- **`Stuff:doom3`** has the 1.4 library and compiler, the new shaders, `Doom3.script` and the ReadMe; its 1.3.1 `libs/` and old `shaders/` are in `old-0.1/`.

**Done (2026-11-10): ogles3 tested again — the same speed as ogles2 1.4-beta1.** `timeDemo demo1`, 1920x1080, `low.cfg`, `r_maxLights 12`, Paula, the tick rate automatic (patch 0034), the demo in memory (`bench-20261110-194351`): the game on **ogles2 1.4-beta1** (the release's library, `OGLES2_LIBS=` in `tools/bench_pkg.sh`) **19.0 fps** (2148 frames in 113.3 s); the same game built for ogles3 today (`OGLES=3 build.sh`, `game-ogles3`) on **ogles3 1.0-beta1** (its release archive; the copy in `LIBS:` of 1 Nov, version 1.0, was the one opened) **18.9 fps** (113.5 s). Both offer `glDrawRangeElements` now (patch 0029's path: before 1.4 only ogles3 had it, 2026-10-29: ogles2 1.3.2 17.6 against ogles3 19.0). So nothing to choose between them for this game at the moment; the release stays on ogles2 (the Pi 3 as well). The plain game at the bench's 640x480 ran 22.7 fps in the same package.

**Next, in order:**
0. ~~libnix's empty-string `strtod`~~ (§14, 2026-11-04): fixed; the developer clicked the `mars_city1` elevator panel with the fixed build and its doors opened. The diagnostic patch (`guiState`) is not committed: it was numbered 0029 before 0029-0031 arrived and would need renumbering.
0. **The game on ogles3's ES 3 features: §15's plan** (step 0 first: the frame's budget on ogles3). ~~ogles3's draw cost~~ (§13, request 3): answered, the same 17.6 fps as ogles2 1.3.2 (above). Earlier note: the developer's call; the bench package measures a newer ogles3 with `"o3=game-ogles3:..."` and `DEMO=demo1`. The bench package's map runs need the demo's data (`DATA=` a directory with `demo/` only) or a full-game map.
1. ~~Patch 0104 to ogles2's `ports/sdl2`~~: done (its 0005); ogles2's copy change reverted.
2. ~~The console's freeze while typing~~: the spinlock livelock (patch 0104), not patch 0102; typing works.
3. **The full game on the A1200:** New Game after a first map works (`play-20261022-062500`), the main menu works (with the copy workaround); a level change at a level's end and a saved game's load still to be played; the load times; the heavier levels.
4. **`r_maxLights`:** the developer plays with 12 (`Doom3.script`'s default; lights still pop at times): a default in `low.cfg`, or the ranking refined further.
5. **The developer listens** to `RATE=11025` (Paula's defaults, 32 voices at 22 kHz, sound fine: `play-20261021-193114`).
6. **Silent timedemos:** with `s_noSound 1` a render demo's sound commands still reach the sound world without loaded samples (`FindAmplitude` for sound-driven lights crashes); timedemos run with sound until then.
7. A PC sample with the wider stacks: collision's callers.
8. Collision (10% of the frame): who makes the expensive queries (the swinging light's AF, the player).
4. The front end's per-frame walk: what can be kept from one frame to the next for entities and lights that did not move.
5. **The load** (the developer, 2026-10-20 and 2026-11-10): ~~the game data prepared at the first start~~ (§16: `mars_city1` 95 to 82 s, switched off as not significant). What the measurement showed: inflating is not most of a DDS image's 7-9 ms. Next, if the load is taken up again: a PC sample over the load (`PCSAMPLE=1`) to find where a DDS image's time and the models' 15 s go, then the entities' spawn, the render world, the scripts' compile at start.
2. Performance (phase 5), cheapest first, each step measured with `Run-Bench`:
   - compiler flags for the game: `-O3`, `-mcpu=68060` (scheduling), `-fno-exceptions` is not possible (Doom 3 throws);
   - the game tic: a 30 Hz game rate (the idea of dhewm3's high-fps branch, which makes the tic length a variable), which would halve the logic's cost per second; dormant entities and animation level of detail from ldhewm3-rearmed (§10.2);
   - the back end's 42 ms: where it goes in ogles2 (its log probes), a request to ogles2 only with numbers (§13).
2. Map load time (77 s): where it goes (pk4 inflate, images, collision, AAS, scripts).
2. Resurrection of Evil: its data (`d3xp/`) and a build with `D3XP` (§8).

## 15. Plan: the game on ogles3.library's ES 3 features

**Why now (2026-10-29):** ogles3's GLES2 draw is no longer dearer than ogles2 1.3's (§14: `timeDemo demo1` 17.6 fps on both; ogles3's own tests: its calls 0.59-0.87 of ogles2's time, its draws 0.56-0.96, in its `docs/GLES3_PLAN.md`), so what ogles3 adds can now only gain. The goal: fewer CPU cycles a frame on the A1200, measured with `timeDemo demo1` (the bench package, `DEMO=demo1`, 1920x1080, `low.cfg`, `r_maxLights 12`, Paula, 20 Hz: 17.6 fps, 57 ms a frame), every step against the step before, kept only if it helps.

**What the frame is made of** (the last measurements; step 0 repeats them on ogles3):
- 237 draws a frame in the timedemo; the library's share of them 5.2 ms a frame on ogles2 1.3.1 (probe P, `bench-20261028-223954`), about 22 µs a draw: uniforms, vertex data and the index scan the largest stages. The GPU's render is 2-3.5 ms: the GPU is not the limit.
- The rest is the game's own CPU: the game tic, the front end (in the map bench: the interactions' walk and culling about 7 ms, ambient surfaces 4.5 ms, skinning 0.9 ms; §14), Doom's back end (11-13 uniform calls, 5 texture binds and up to 6 attribute pointers per interaction draw; §14).
- So the library's ES 3 features can win at most the library's share and the back end's call overhead (perhaps 10-15 ms of 57); the front end and the game are untouched by them. Each step below says what it can win.

**What ogles3 does with each feature** (its `docs/GLES3_PLAN.md`; the costs decide the order):
- Uniform blocks: a buffer's GPU copy (every word swapped) made at its first use and kept while its data does not change; a draw passes the bound range's address. A `glBufferSubData` into a buffer the open job reads renames it: the whole copy is made again at the next use. So a block's data should be written before the frame's draws read it, or into a range no earlier draw of the job reads (a ring of ranges, `glBindBufferRange`).
- Vertex array objects, instancing, `glDrawRangeElements` (a draw with the index range given skips the library's scan of the indices), integer attributes, `glMapBufferRange` (the mapping is the CPU shadow: no cheaper than `glBufferSubData`), sync objects and occlusion queries (a result is there once the job's render is done).
- Buffers the GPU writes (transform feedback, compute shaders, storage buffers) come back to the CPU's side only after a wait for the render and a byte swap; `glDispatchCompute` waits for its job. So GPU skinning or shadow volumes through transform feedback or compute would stall: not planned.
- An ES 3 feature in GLSL (uniform blocks, `gl_VertexID`, integer inputs) needs an OpenGL ES 3.0 context and GLSL ES 3.00 programs; the extensions ogles3 gives GLES2 contexts (`OES_vertex_array_object`, `EXT_instanced_arrays`, `glDrawRangeElements`, ...) need neither.

**Steps, cheapest and safest first.** Each is a d3es patch (`patches/`, §4's procedure) or SDL's (`sdl2/ogles3`), behind a cvar where it changes behaviour, measured with the bench package (`"o3=game-ogles3@paula:..."`, `DEMO=demo1`, `OGLES2PROBE=1` for the library's stages) against the build before.

0. **The frame's budget on ogles3** (no change): `com_speeds` (game, front end, back end) and probe P's stages on `demo1`, and a PC sample (`PCSAMPLE=1`) with the stacks: the baseline every step is compared with, and the check that the shares above hold in the timedemo. If the library and Doom's back end together are under 10 ms of the 57, steps 2-5 can win at most that, and the front end and the game (§14's "Next", §10.2) come first.
1. **`glDrawRangeElements` for every draw** (GLES2 context, no shader change): `RB_DrawElementsWithCounters` and the shadow variant pass `0` and the surface's `numVerts - 1` (the indices of a cached surface address its own vertices). The library then skips its index scan (in ogles3's tests about 2 µs a draw of 160 indices; `demo1`'s draws have 174 on average): about 0.5 ms a frame. Check first that every caller's indices really stay within `numVerts` (the shadow volumes' doubled vertices).
2. **Vertex array objects for static surfaces** (GLES2 context, `OES_vertex_array_object`): a surface whose vertices are in a static buffer gets its attribute set-up once, bound with one call per draw instead of up to 6 `glVertexAttribPointer` and the enables. First a drawbench test in ogles3 (`tests/perf/drawbench`: a VAO bind against 6 pointers), since ogles3's bind copies the object's arrays: only if it is cheaper there.
3. **The interaction's uniforms in blocks** (an ES 3.0 context, the interaction and shadow programs in GLSL ES 3.00):
   - a per-view block (projection, view origin) and a per-light block (the light's projection, falloff and colour: 4 of the 11-13 uniforms, unchanged between a light's surfaces), written once a frame before the back end's first draw into one buffer (no renaming), one `glBindBufferRange` per light;
   - the per-surface values (model-view-projection, local view and light origins, bump/diffuse/specular matrices and colours) stay `glUniform*` calls, through the existing uniform cache (§14);
   - needs: SDL asked for 3.0 (`SDL_GL_CONTEXT_MAJOR_VERSION 3`), the programs ported (`attribute`/`varying`/`texture2D` to `in`/`out`/`texture`), their binaries in the shader cache (`release/shaders`, made for ogles3's compiler);
   - first a drawbench test in ogles3: a draw reading a uniform block against the same draw with 4 `glUniform4fv` (ogles3 takes a block's address per draw through its rarer uniform path): only if it is cheaper there.
   - What it can win: the 4 per-light calls on about 200 interaction draws a frame (about 0.5 µs each in ogles3: 0.4 ms) plus the stream words written for them. Small unless step 0 shows the uniforms larger than that.
4. **GPU skinning** (ES 3.0 context, with `r_shadows 0`, `low.cfg`'s setting): skinned models' vertices static in a buffer (position, normal, tangents, joint indices and weights), the joint matrices in a uniform block per entity, the vertex shader blending them. It removes the CPU skinning (`TransformJoints`, `TransformVerts`, `DeriveTangents`: 0.9 ms in the map bench, more in the monsters' rooms) and the skinned vertices' copy into the frame's stream buffer and the library's conversion of them per draw and per light (the interaction pass reads a surface once a light). Shadow volumes need the CPU's skinned positions, so with `r_shadows 1` the CPU path stays. The largest change of the plan: only after step 0 shows skinning and its vertex data worth it.
5. **Occlusion queries for lights** (`GL_ANY_SAMPLES_PASSED`, the light's bounds drawn without colour or depth writes): a light whose last query found no visible pixel skips its interactions in the next frame (one frame late; a light switched on or the view moved fast shows a frame late). It can win front-end and back-end time where lights are hidden behind walls; ogles3 answers a query once its job is rendered, so the result is read the frame after, never waited for. After steps 0-3, and only if the front end's per-light share is large in the heavier maps.

**Not planned, and why:** transform feedback or compute for skinning or shadow volumes (the GPU's writes come back through a CPU wait, above); `glMapBufferRange` for the vertex cache (no cheaper than `glBufferData` in ogles3); instancing (Doom 3 draws no repeated meshes in numbers; particles are built on the CPU); MRT and float targets (the low settings use no post-process).

## 16. Plan: the game data prepared once, on the Amiga, at the first start

**Why (2026-11-10, the developer's request):** every map load on the 68k still converts data that never changes:
- **inflating:** every file comes out of a deflated pk4 entry; most of a DDS image's 9.2 ms is zlib on the 68k, not the disk (§14, 2026-10-20);
- **sound decoding:** every `.ogg` (most of Doom 3's `.wav` names are Ogg files) shorter than `s_decompressionLimit` is decoded with stb_vorbis at each load, always to 44 kHz floats, then cut back to its own rate and to 16 bits (`idSoundSample::Load`). 862 sounds took 53 s at the default limit of 6 s (`play-20261020-232653`); `low.cfg`'s limit of 60 s decodes all of them at load (8 s more);
- **images without a DDS:** cube maps, the images a DDS was declined for (non-RXGB normal maps, uncompressed DDS files), the rest of the TGAs: read, run through their image program (`heightmap`, `addnormals`, ...) and mip-mapped (`R_MipMap`) on the CPU (`GenerateImage`, `GenerateCubeImage`).

**The developer's decision:** the conversion is done **by the game on the Amiga, at its first start**, once, with a progress bar and a line saying what it is doing. The user copies the original pk4s as now and needs no PC tool. Every later start and every map load reads the prepared data and converts nothing.

**What the first start does:**
- **When:** in `idCommonLocal::InitGame`, right after `InitRenderSystem()` (the file system, the declarations and the GL context are up; the sound system and the game code are not yet). If the prepared set is missing, incomplete or stale (below), the preparation runs; afterwards `fileSystem->Restart()` mounts the prepared set and the start goes on as usual. Every later start finds the set valid and skips straight on (one manifest read).
- **Asked first:** one screen before anything is written: what will be done, the estimated time and the room it needs against the free room on the volume (dos.library `Info()`), and three choices: *prepare now*, *not now* (plays from the original pk4s this time, asks again next start), *never* (a cvar, `fs_prepare 0`, in the configuration). Too little room: it says how much is missing and offers only *not now*/*never*. On the command line `+set fs_prepare 2` prepares without asking (the test packages).
- **The screen while it works:** the start-up screen the game already draws (`PrintLoadingMessage`: the splash picture, one line of text), extended to:
  - a progress bar over the whole job, by bytes to process (known from the pk4s' directories before it starts);
  - the step and the pk4: "Step 2 of 3: sounds, pak002.pk4 (3 of 9)";
  - the current file's name, the counts ("1,214 of 4,610 sounds") and the megabytes written;
  - the time taken and an estimate of the time left (from the bytes done so far).
  The screen is redrawn at most four times a second (a frame and its swap cost tens of milliseconds); between redraws the work runs. The same lines go to the console and the log, a line per pk4 and step, so a run without the screen (a package's `stdout`) shows it too. Esc stops after the current file, keeping what is done (below).
- **Survives a reset or a stop:** each prepared pk4 is written under a temporary name and renamed when complete; the manifest is updated after each one. A new start resumes with the first pk4 not done. A half-written file is deleted and started again.

**What it writes:** a folder `prepared/` in each game folder (`base/prepared/`, `demo/prepared/`, `d3xp/prepared/`), with one pk4 per original, the same names in the same order (`pak000.pk4`–`pak008.pk4`):
- **Every entry the game reads at a load stored** (no compression): DDS images, models, maps, `.proc`/`.cm`/`.aas`, declarations, scripts, GUIs. Written by a small stored-only zip writer (local headers, the central directory; d3es's minizip only reads), the CRCs taken from the original's directory (the inflated data is checked against them as it is written).
- **The sounds below the limit (60 s, `low.cfg`'s) as PCM:** the `.ogg` decoded once, exactly as `idSoundSample::Load` does it (the same decoder and the same cut to 22 or 11 kHz), and written as a standard little-endian RIFF `.wav` under the same name; the `.ogg` left out, since `idWaveFile::Open` prefers the `.ogg` when one exists. The 68k's only step at load is then the byte swap `idWaveFile::Read` already does (one pass over memory; a big-endian form only if the load profile shows the swap). Longer sounds (music, long speech) stay Ogg and keep streaming as now: a WAV is loaded whole, so converting them would put them all in memory. Sound shaders that name an `.ogg` themselves (`snd_shader.cpp` handles them) get a small fallback in `Open`: the `.wav` when the named `.ogg` is missing.
- **The images without a DDS:** every material's images (the declarations are parsed by then) loaded through the CPU half of the image code (`R_LoadImageProgram`, the program, `R_MipMap`; split from `GenerateImage`'s upload) with the settings in force (`image_usePrecompressedTextures 1`, `image_downSize`, ... as in `low.cfg`), and those that do not come from a DDS written as `dds/<the name ImageProgramStringToCompressedFileName gives>.dds`, uncompressed RGBA8 with the whole mip chain (cube maps with six faces). Patch 0002 learns to load uncompressed RGBA8 DDS (today it declines them) with one `glTexImage2D` per level. Few (12 of `mars_city1`'s 1,801 images in the demo), but each a full image program and mip chain on the 68k.
- **What the game does not read at a load kept compressed, copied raw:** a TGA whose plain image has a DDS (`dds/<its name>.dds`), and anything else rarely read (`.map` sources are read: stored). The deflated bytes are copied as they are, with no inflating and no recompressing, so they cost neither time at the preparation nor room. Nothing is left out, so a setting that does read such a file later still finds it (it is inflated then, as now).
- **A manifest** (`prepared/prepared.txt`), written last for each pk4: the original's name, size, date and checksum; the version of the preparation (a number in the code, raised whenever its output changes); the sound limit and the image settings used. **Stale** means: an original changed, appeared or went, or the version differs. Then only the affected pk4s are prepared again (all of them for a new version).
- **Mounting it:** a file-system patch: when a game folder's `prepared/` has a valid manifest, its pk4s are loaded **instead of** the folder's own (the originals stay untouched on the disk and are needed again only for a new preparation). The prepared pk4s' checksums differ from the originals'; only multiplayer's pure servers and addons read them (`FileSystem.cpp`), so single-player is unaffected. Each prepared pk4 stays below **2 GB**, the largest file dos.library's 32-bit `Seek` handles; one that would pass it is split (`pak002.pk4`, `pak002_b.pk4`, kept in the original's place in the order).

**Where the code lives:** a d3es patch, `framework/DataPrepare.cpp` (the steps, the screen, the manifest), with the zip writer, the split of `GenerateImage`, the WAV fallback, the uncompressed DDS in patch 0002's loader, and the mount in `FileSystem.cpp`. Only file system, zlib, stb_vorbis and the renderer's 2D drawing: nothing Amiga-specific, so it runs **on the host first** (the same first start in WSL), where every step is checked before the Amiga sees it.

**Status (2026-11-10): steps 0, 1, 2 and most of 4 done, in `patches/0032-data-prepare`; step 3 not built. Then switched off by the developer** (`mars_city1` 95 to 82 s: not significant; `fs_prepare 0`, `fs_usePrepared 0` by default) (§14, "the game data prepared at the first start"). What was built differs from the text above in three places: the decode is `idSoundSample::DecodeOggToShorts` (Load's code moved into a method, which both call), not a WAV fallback in `Open` (no sound shader names an `.ogg` in the full game's or the demo's data: the host's loads warn of nothing new); the manifest records each original's size and date, not its checksum; `fs_prepareKeep` (default `roq demo`) keeps extensions compressed.

**Steps, each measured against the step before** (`map` package with `fs_prepare 2`, `com_timestampPrints 2`, patch 0018's load line; the full game's `mars_city1`: 97 s now; the demo's `demo_mars_city1` against `map-20261020-194658`):

0. **The inventory** (a host script over the pk4s, no Amiga time): counts and bytes per type (DDS, TGA, models, maps, Ogg); every Ogg's length, rate and channels, its PCM size below 6 s, 60 s and in all; the TGAs a DDS covers. It gives the prepared set's size, and with the measured rates (inflating about 4-8 MB/s of output on the 68k, a DDS's 9.2 ms; an Ogg's 61 ms; writing about 20 MB/s on `Stuff:`) a first estimate of the first start's time for the demo and the full game. `Stuff:` had 3.2 GB free before the full game went on; if the full game's set will not fit, the choice of what stays compressed is revisited before anything is written.
1. **Stored pk4s, the screen and the manifest** (no sound or image conversion): on the host, the prepared set plays the same (the screenshot 150 frames in, `listImages`); on the A1200, the first start's time against the estimate, the load against the original pk4s, a reset during the preparation and the resume.
2. **Sounds as PCM:** load time with sound (`SOUND=1`) against step 1; the host's sound dump (`DUMP=1`, `tools/sndcmp.py`) the same with the prepared set and with the original.
3. **Images without a DDS** (patch 0002's uncompressed DDS): patch 0018's image counts (DDS against image files) and their time; the screenshot the same.
4. **Stale and partial cases:** an original replaced, a pk4 added (`d3xp/` after `base/`), the version raised, too little room, Esc, *not now* and *never*: each on the host, the reset on the A1200.
5. **The load measured again with the PC sampler** (`PCSAMPLE=1` over the load) to see what is left once nothing is inflated or decoded. The candidates already named (§14): the entities' spawn (33 s on the demo), models (14.7 s with their images: `.lwo`/`.ase` parsed as text at each load), the render world (`.proc`, 8 s), collision (`.cm`, 4 s), the game's scripts compiled at start (9.5 s). Each could become another step of the same preparation (binary models, `.proc`/`.cm`/AAS, as Doom 3 BFG does; a compiled-script cache), but each is a larger change to the game's loaders: planned only for the ones step 5 shows large, one at a time, each checked against the original data on the host.

**Not planned, and why:** a PC tool (the developer's decision: the user has only the Amiga); compressing the image-program images to DXT (a visible change the original never had; the uncompressed DDS keeps the picture, and they are few); resampling sounds to the mixer's rate (22,050 Hz) (it changes what is heard, and the mixer's cost does not depend on the source rate; memory halves for 44 kHz sounds: reconsidered if step 0 shows memory short); a faster inflate on the 68k (§14's alternative: nothing left to inflate at a load); converting in the background while the game plays (the 68k has no time to spare while it plays).
