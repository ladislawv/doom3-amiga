/*
 * strtod and atof for the game (linked with --wrap for both, build.sh):
 * libnix's return an undefined value when the text holds no number ("" or
 * blanks), where C says 0 (with *endptr = the text). In the game that value
 * was the previous conversion's result: a GUI script compares its empty
 * state values with atof, and the exit elevator's panel in mars_city1 read
 * its unset malfunction parm (gui_parm5) as the 2 of gui_parm1 before it, so
 * it showed "system malfunction" and its call button only buzzed
 * (PLAN.md section 14, 2026-11-04).
 *
 * A number is there when, after blanks and a sign, a digit follows, or a
 * point and a digit, or "inf"/"nan"; then libnix converts it as before.
 *
 * The linker takes C names here (--wrap=strtod: references to strtod go to
 * __wrap_strtod, __real_strtod to libnix's), adding the target's underscore.
 */
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>

double __real_strtod(const char *nptr, char **endptr);

static int has_number(const char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    if (*s == '+' || *s == '-')
        s++;
    if (isdigit((unsigned char)*s))
        return 1;
    if (*s == '.' && isdigit((unsigned char)s[1]))
        return 1;
    return strncasecmp(s, "inf", 3) == 0 || strncasecmp(s, "nan", 3) == 0;
}

double __wrap_strtod(const char *nptr, char **endptr)
{
    if (!has_number(nptr)) {
        if (endptr)
            *endptr = (char *)nptr;
        return 0.0;
    }
    return __real_strtod(nptr, endptr);
}

double __wrap_atof(const char *nptr)
{
    return __wrap_strtod(nptr, NULL);
}
