/*
 * AmigaOS 3 port: the operating-system calls of the system layer (amiga_os.h).
 * GPLv3, as the rest of the port.
 */
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>

#include "amiga_os.h"

unsigned long amiga_stack_size(void)
{
    struct Task *t = FindTask(NULL);
    return (unsigned long)t->tc_SPUpper - (unsigned long)t->tc_SPLower;
}

unsigned long amiga_fast_mb(int total)
{
    return AvailMem(MEMF_FAST | (total ? MEMF_TOTAL : 0)) >> 20;
}

int amiga_program_dir(char *buf, int size)
{
    BPTR dir = GetProgramDir();
    if (dir && NameFromLock(dir, (STRPTR)buf, size))
        return 1;
    /* started without a program directory (rare): the current one */
    return GetCurrentDirName((STRPTR)buf, size) ? 1 : 0;
}

int amiga_program_name(char *buf, int size)
{
    if (GetProgramName((STRPTR)buf, size)) {
        const char *base = (const char *)FilePart((STRPTR)buf);
        memmove(buf, base, strlen(base) + 1);
        return 1;
    }
    return 0;
}

int amiga_exists(const char *path, int dir)
{
    BPTR lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    struct FileInfoBlock *fib;
    int ok = 0;
    if (!lock)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(lock, fib))
        ok = dir ? (fib->fib_DirEntryType > 0) : (fib->fib_DirEntryType < 0);
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return ok;
}

int amiga_mkdir(const char *path)
{
    BPTR lock = CreateDir((CONST_STRPTR)path);
    if (lock) {
        UnLock(lock);
        return 1;
    }
    return amiga_exists(path, 1);
}

int amiga_list_dir(const char *dir, void (*found)(const char *, int, void *), void *user)
{
    BPTR lock = Lock((CONST_STRPTR)dir, ACCESS_READ);
    struct FileInfoBlock *fib;
    int n = 0;
    if (!lock)
        return -1;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!fib || !Examine(lock, fib) || fib->fib_DirEntryType <= 0) {
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return -1;
    }
    while (ExNext(lock, fib)) {
        found(fib->fib_FileName, fib->fib_DirEntryType > 0, user);
        n++;
    }
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return n;
}

long amiga_file_time(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    struct FileInfoBlock *fib;
    long t = 0;
    if (!lock)
        return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(lock, fib)) {
        const struct DateStamp *d = &fib->fib_Date;
        /* AmigaDOS counts from 1978-01-01: 2922 days after 1970-01-01 */
        t = ((long)d->ds_Days + 2922) * 86400 + d->ds_Minute * 60 + d->ds_Tick / TICKS_PER_SECOND;
    }
    if (fib)
        FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return t;
}

void amiga_message(const char *title, const char *text)
{
    struct EasyStruct es;
    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)title;
    es.es_TextFormat = (UBYTE *)"%s";
    es.es_GadgetFormat = (UBYTE *)"OK";
    EasyRequest(NULL, &es, NULL, (ULONG)text);
}

/* Emu68's free-running counter (its docs/internals/ControlRegisters.md):
 * MOVEC from control registers 0xe0 (CNTFRQ), 0xe1 (CNTVALLO) and 0xe2
 * (CNTVALHI), the high word read again until it held. MOVEC is privileged,
 * hence exec's Supervisor(). 1.7 us a read on the A1200 against 19.7 for
 * timer.device's ReadEClock, which SDL's performance counter uses
 * (tests/a1200/clock-probe, PLAN.md section 14). */
unsigned long amiga_counter_super_read(unsigned long *hi);
unsigned long amiga_counter_super_hz(void);
__asm__(
    "	.text\n"
    "	.globl	_amiga_counter_super_read\n"
    "_amiga_counter_super_read:\n"
    "	movem.l	a5/a6,-(sp)\n"
    "	lea	counter_read_super(pc),a5\n"
    "	move.l	4.w,a6\n"
    "	jsr	-30(a6)\n"			/* Supervisor */
    "	move.l	12(sp),a0\n"
    "	move.l	d1,(a0)\n"
    "	movem.l	(sp)+,a5/a6\n"
    "	rts\n"
    "counter_read_super:\n"
    "	.word	0x4e7a,0x10e2\n"	/* movec cntvalhi,d1 */
    "	.word	0x4e7a,0x00e1\n"	/* movec cntvallo,d0 */
    "	.word	0x4e7a,0x90e2\n"	/* movec cntvalhi,a1 */
    "	cmp.l	a1,d1\n"
    "	bne.s	counter_read_super\n"
    "	rte\n"
    "	.globl	_amiga_counter_super_hz\n"
    "_amiga_counter_super_hz:\n"
    "	movem.l	a5/a6,-(sp)\n"
    "	lea	counter_hz_super(pc),a5\n"
    "	move.l	4.w,a6\n"
    "	jsr	-30(a6)\n"
    "	movem.l	(sp)+,a5/a6\n"
    "	rts\n"
    "counter_hz_super:\n"
    "	.word	0x4e7a,0x00e0\n"	/* movec cntfrq,d0 */
    "	rte\n");

unsigned long amiga_counter_hz(void)
{
    /* only Emu68 has devicetree.resource; another 68k would take MOVEC
       0xe0 as an illegal instruction */
    if (!OpenResource((CONST_STRPTR)"devicetree.resource"))
        return 0;
    return amiga_counter_super_hz();
}

unsigned long amiga_counter_read(unsigned long *hi)
{
    return amiga_counter_super_read(hi);
}

void amiga_set_env(const char *name, const char *value)
{
    SetVar((CONST_STRPTR)name, (CONST_STRPTR)value, -1, GVF_GLOBAL_ONLY);
}
