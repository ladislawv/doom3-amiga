/*
 * The Amiga build's allocator (PLAN.md §9): Doug Lea's dlmalloc 2.8.6 (public
 * domain; sources.sh fetches it, checked by SHA-256), configured here and
 * compiled with -I<the download directory>.
 *
 * Why: libnix's free() took 4.4 s for 100,000 small blocks freed in mixed
 * order on the A1200 (the C++ probe, 2026-10-20); Doom 3 allocates and frees
 * all the time.
 *
 * How:
 * - The functions keep dlmalloc's prefixed names (dlmalloc, dlfree, ...): the
 *   game's operator new/delete (d3new.cpp) and idlib's Mem_Alloc call them.
 *   The C library's malloc stays libnix's, for SDL (whose audio runs in a
 *   task of its own) and libnix itself, so no lock is needed: only the game's
 *   main task calls these.
 * - The static objects' destructors (cxa_atexit.c) run before libnix frees
 *   its memory (exit priorities 0 and -50), so they can still free into it.
 * - dlmalloc takes its memory from libnix's malloc in pieces of at least
 *   DEFAULT_GRANULARITY, as separate segments (no sbrk, no mmap). libnix frees
 *   all of them when the program exits, as it does any memory it handed out:
 *   on AmigaOS nothing else would.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static void *d3_morecore(ptrdiff_t size);

#define USE_DL_PREFIX 1
#define USE_LOCKS 0
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define HAVE_MORECORE 1
#define MORECORE d3_morecore
#define MORECORE_CONTIGUOUS 0
#define MORECORE_CANNOT_TRIM 1
#define DEFAULT_GRANULARITY ((size_t)4 * 1024 * 1024)
#define MALLOC_ALIGNMENT ((size_t)8)
#define LACKS_SYS_MMAN_H 1
#define LACKS_SCHED_H 1
#define NO_MALLINFO 0
#define ABORT abort()

/* sbrk-like: MORECORE(n) a new segment of n bytes, or MFAIL; MORECORE(0) the
 * end of the segment handed out last (dlmalloc's sys_alloc asks for it right
 * after each segment, as it would sbrk(0), and takes end - start as the
 * segment's size: it got nothing at all while this returned MFAIL, and the
 * first operator new threw bad_alloc on the A1200). Negative sizes (trimming)
 * are never asked for with MORECORE_CANNOT_TRIM. */
static char *d3_morecore_end = (char *)~(size_t)0;

static void *d3_morecore(ptrdiff_t size)
{
    char *p;
    if (size == 0)
        return d3_morecore_end;
    if (size < 0)
        return (void *)~(size_t)0;
    p = malloc((size_t)size);
    if (!p)
        return (void *)~(size_t)0;
    d3_morecore_end = p + size;
    return p;
}

#include "malloc-2.8.6.c"
