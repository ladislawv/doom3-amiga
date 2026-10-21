/* Host test of src/amiga/d3malloc.c (the Amiga allocator's configuration,
 * compiled natively): a random load of allocations and frees, every block
 * filled and checked, then dlmalloc's own statistics.
 *   gcc -O2 -I<dlmalloc dir> d3malloc_test.c ../../src/amiga/d3malloc.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *dlmalloc(size_t);
void dlfree(void *);
void *dlrealloc(void *, size_t);
void dlmalloc_stats(void);

int main(void)
{
    enum { SLOTS = 4096, OPS = 2000000 };
    static unsigned char *p[SLOTS];
    static size_t sz[SLOTS];
    unsigned int seed = 12345;
    int i, s, errors = 0;
    for (i = 0; i < OPS; i++) {
        seed = seed * 1103515245 + 12345;
        s = (seed >> 8) % SLOTS;
        if (p[s]) {
            for (size_t j = 0; j < sz[s]; j += 61)
                if (p[s][j] != (unsigned char)(s + j))
                    errors++;
            if (seed & 0x100000) {  /* realloc sometimes */
                size_t n = (seed >> 4) % 100000 + 1, keep = n < sz[s] ? n : sz[s];
                p[s] = dlrealloc(p[s], n);
                for (size_t j = 0; j < keep; j += 61)
                    if (p[s][j] != (unsigned char)(s + j))
                        errors++;
                for (size_t j = 0; j < n; j += 61)
                    p[s][j] = (unsigned char)(s + j);
                sz[s] = n;
            } else {
                dlfree(p[s]);
                p[s] = NULL;
            }
        } else {
            seed = seed * 1103515245 + 12345;
            sz[s] = (seed >> 8) % ((seed & 0x10000) ? 65536 : 256) + 1;
            p[s] = dlmalloc(sz[s]);
            if (!p[s]) { printf("dlmalloc(%zu) failed\n", sz[s]); return 1; }
            for (size_t j = 0; j < sz[s]; j += 61)
                p[s][j] = (unsigned char)(s + j);
        }
    }
    for (s = 0; s < SLOTS; s++)
        dlfree(p[s]);
    dlmalloc_stats();
    printf("%d operations, %d errors\n", OPS, errors);
    return errors != 0;
}
