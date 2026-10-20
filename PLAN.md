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
  - `PLAN.md`;
  - `sources.sh`, `build.sh`, `build_host.sh`;
  - `patches/` (changes to d3es, made against the WSL clone, LF line ends);
  - `src/` (new files: the AmigaOS 3 system layer, the sound library, the build glue);
  - `sdl2/` (SDL2 patches);
  - `tests/a1200/<package>/` and `tests/a1200/results/`;
  - `config/` (the default `autoexec.cfg`).
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
  - The 2019 re-release and the BFG Edition use different data and are not supported.

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
| Vertex upload | Per-frame temporary buffers (2 MB each), filled on the CPU and sent with `glBufferData(..., GL_STATIC_DRAW)` (`renderer/VertexCache.cpp`) | `GL_STREAM_DRAW` for the temporary buffers (§10.1) |
| Alignment | `ALIGN16` on about 270 declarations | 8-byte alignment (§5) |
| Debug drawing | `tr_rendertools.cpp` uses desktop GL calls (`qglBegin` and others) | Stubbed out |
| Depth copy | `CopyDepthbuffer` calls `glCopyTexImage2D(GL_DEPTH_COMPONENT)`, which ES 2.0 does not allow | Check whether any shipped material uses it; disable if not |
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
| D7 | Sound | **A small OpenAL 1.1 subset written for the port** (`src/al/`): exactly the calls d3es uses, mixing in SDL2's audio callback (AHI) at 22,050 Hz stereo by default, with distance attenuation, stereo panning, pitch, looping and queued streaming (Ogg music and speech). EFX reports itself unavailable, which d3es supports (`s_useEAXReverb` 0). Why: OpenAL Soft needs `std::thread`/`std::mutex`, which this libstdc++ lacks, and does far more per sample than this CPU can spare. Sound stays off (`s_noSound 1`, which skips OpenAL) until phase 7 |
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

   This is the build the port's patches are written against first.
3. Patches made and tested here:
   - the main-loop async tic;
   - the `GL_STREAM_DRAW` temporary buffers;
   - the DDS loader;
   - `ALIGN16`;
   - the stubs (rendertools, depth copy, download thread).
4. **Exit:**
   - `timedemo demo1` runs to the end on the host;
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

- d3es fills each frame's dynamic geometry (skinned models, deforms, GUI surfaces, shadow volumes) into 2 MB temporary buffers and sends them with `glBufferData(..., GL_STATIC_DRAW)`.
- For a static buffer, ogles2 converts the whole buffer once per attribute layout. An interaction draw reads up to six attributes of `idDrawVert`: position, texture coordinates, normal, two tangents and colour. So one frame can swap the same 2 MB several times.
- **First change:** the temporary buffers use `GL_STREAM_DRAW`, so each draw converts only the vertices it reads. Static model data stays `GL_STATIC_DRAW` and is converted once.
- **Then measure:**
  - client arrays (`r_useVertexBuffers 0`) against both;
  - how often the same vertices are converted twice (the interaction pass reads a surface once per light).

  If that cost is large, the request to ogles2 (§13) is a whole-buffer conversion for layouts that are all 32-bit words. The port would store `idDrawVert`'s colour bytes reversed, so the buffer is uniform.

### 10.2 CPU savings from ldhewm3-rearmed

These are ported without its `__vita__`, vitaGL and NEON parts:
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

None yet. Requests go here only with a measurement behind them; the developer decides whether ogles2 takes them.

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

**Next, in order:**
1. Put this folder under git (a private GitHub repository) with `sources.sh` pinning d3es, dhewm3 1.5.5 and SDL2_68k.
2. The developer copies the 1.3.1 data (`base/`, `d3xp/`) to WSL and to the Amiga.
3. Phase 1 (§8): the host build and the reference set.
4. Phase 2: libnix SDL2, zlib, the C++ probe on the A1200.
