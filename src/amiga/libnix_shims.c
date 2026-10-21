/*
 * What GCC 16.2's libstdc++ needs from the C library and libnix lacks.
 * Linked into every C++ program of this port (the probe, the game).
 */
#include <errno.h>
#include <math.h>
#include <string.h>

/* The XSI strerror_r (libstdc++'s system_error.cc calls it): the message
 * copied into buf, 0 or ERANGE when it did not fit. */
int __xpg_strerror_r(int errnum, char *buf, size_t buflen)
{
    const char *msg = strerror(errnum);
    size_t n = strlen(msg);
    if (buflen == 0)
        return ERANGE;
    if (n >= buflen) {
        memcpy(buf, msg, buflen - 1);
        buf[buflen - 1] = '\0';
        return ERANGE;
    }
    memcpy(buf, msg, n + 1);
    return 0;
}

/* C99's exp2f: GCC turns pow(2, x) into it under -ffast-math. This file is
 * compiled without -ffast-math, so pow stays pow. */
float exp2f(float x)
{
    return (float)pow(2.0, (double)x);
}
