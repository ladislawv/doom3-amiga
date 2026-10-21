/*
 * __cxa_atexit for the game (compiled with -fuse-cxa-atexit), which libnix
 * lacks: static objects' destructors then run in the reverse order of their
 * construction, as on Linux and Windows.
 *
 * Without it GCC puts them in the DTOR list, which libnix runs in link order;
 * Doom 3's static objects depend on each other (idlib's string allocator must
 * outlive the static idStr objects), and the first A1200 runs crashed in the
 * exit code after the game had shut down cleanly.
 *
 * The list runs from one atexit() handler, registered with the first
 * destructor (during the static constructors, so before anything main
 * registers, and run after it: atexit is last in, first out).
 */
#include <stdlib.h>

struct cxa_node {
    struct cxa_node *next;
    void (*fn)(void *);
    void *arg;
};

static struct cxa_node *cxa_list = 0;
static int cxa_registered = 0;

/* the identity of "this module" that GCC passes along; one module here */
void *__dso_handle = &__dso_handle;

static void cxa_run(void)
{
    while (cxa_list) {
        struct cxa_node *n = cxa_list;
        cxa_list = n->next;
        n->fn(n->arg);
        free(n);
    }
}

int __cxa_atexit(void (*fn)(void *), void *arg, void *dso)
{
    struct cxa_node *n = (struct cxa_node *)malloc(sizeof(*n));
    (void)dso;
    if (!n)
        return -1;
    n->fn = fn;
    n->arg = arg;
    n->next = cxa_list;
    cxa_list = n;
    if (!cxa_registered) {
        cxa_registered = 1;
        atexit(cxa_run);
    }
    return 0;
}
