/*
 * io_probe: what reading the game's pk4 costs on the A1200 (PLAN.md section
 * 14, map load time): opening it, a seek to a random place and a read there,
 * and reading straight on, through libnix's stdio and through dos.library.
 * Times from Emu68's counter (as clock_probe).
 *   io_probe FILE [N]
 */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

ULONG cnt_read(ULONG *hi);
ULONG cnt_freq(void);
__asm__(
	"	.text\n"
	"	.globl	_cnt_read\n"
	"_cnt_read:\n"
	"	movem.l	a5/a6,-(sp)\n"
	"	lea	cnt_super(pc),a5\n"
	"	move.l	4.w,a6\n"
	"	jsr	-30(a6)\n"
	"	move.l	12(sp),a0\n"
	"	move.l	d1,(a0)\n"
	"	movem.l	(sp)+,a5/a6\n"
	"	rts\n"
	"cnt_super:\n"
	"	.word	0x4e7a,0x10e2\n"
	"	.word	0x4e7a,0x00e1\n"
	"	.word	0x4e7a,0x90e2\n"
	"	cmp.l	a1,d1\n"
	"	bne.s	cnt_super\n"
	"	rte\n"
	"	.globl	_cnt_freq\n"
	"_cnt_freq:\n"
	"	movem.l	a5/a6,-(sp)\n"
	"	lea	freq_super(pc),a5\n"
	"	move.l	4.w,a6\n"
	"	jsr	-30(a6)\n"
	"	movem.l	(sp)+,a5/a6\n"
	"	rts\n"
	"freq_super:\n"
	"	.word	0x4e7a,0x00e0\n"
	"	rte\n");

static double freq;

static double now_us(void)
{
	ULONG hi, lo = cnt_read(&hi);
	return ((double)hi * 4294967296.0 + (double)lo) / freq * 1e6;
}

static unsigned long rnd = 12345;
static long rand_pos(long size, long len)
{
	rnd = rnd * 1103515245UL + 12345UL;
	return (long)((rnd >> 4) % (unsigned long)(size - len));
}

/* integers only: libnix's printf has no %f */
static void report(const char *what, double us, int n, long bytes)
{
	printf("io_probe: %-44s %6lu us each", what, (unsigned long)(us / n));
	if (bytes)
		printf(", %lu KB/s", (unsigned long)((double)bytes * n / 1024.0 / (us / 1e6)));
	printf("\n");
}

int main(int argc, char **argv)
{
	const char *name = argc > 1 ? argv[1] : "Stuff:doom3/demo/demo00.pk4";
	int n = argc > 2 ? atoi(argv[2]) : 100, i;
	long size, len = 65536;
	unsigned char *buf = malloc(1048576);
	double t0, t1;
	FILE *f;
	BPTR fh;

	if (!OpenResource((CONST_STRPTR) "devicetree.resource")) {
		printf("io_probe: not Emu68\n");
		return 10;
	}
	freq = (double)cnt_freq();
	f = fopen(name, "rb");
	if (!f || !buf) {
		printf("io_probe: cannot open %s\n", name);
		return 10;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	printf("io_probe: %s, %ld bytes, %d tries each\n", name, size, n);

	t0 = now_us();
	for (i = 0; i < n; i++) {
		f = fopen(name, "rb");
		fclose(f);
	}
	report("fopen + fclose", now_us() - t0, n, 0);

	t0 = now_us();
	for (i = 0; i < n; i++) {
		fh = Open((CONST_STRPTR)name, MODE_OLDFILE);
		Close(fh);
	}
	report("dos Open + Close", now_us() - t0, n, 0);

	f = fopen(name, "rb");
	t0 = now_us();
	for (i = 0; i < n; i++) {
		fseek(f, rand_pos(size, 64), SEEK_SET);
		fread(buf, 1, 64, f);
	}
	report("stdio: random seek + 64-byte read", now_us() - t0, n, 0);
	t0 = now_us();
	for (i = 0; i < n; i++) {
		fseek(f, rand_pos(size, len), SEEK_SET);
		fread(buf, 1, len, f);
	}
	report("stdio: random seek + 64 KB read", now_us() - t0, n, len);
	fseek(f, rand_pos(size, n * len), SEEK_SET);
	t0 = now_us();
	for (i = 0; i < n; i++)
		fread(buf, 1, len, f);
	report("stdio: 64 KB read straight on", now_us() - t0, n, len);
	fclose(f);

	fh = Open((CONST_STRPTR)name, MODE_OLDFILE);
	t0 = now_us();
	for (i = 0; i < n; i++) {
		Seek(fh, rand_pos(size, 64), OFFSET_BEGINNING);
		Read(fh, buf, 64);
	}
	report("dos: random Seek + 64-byte Read", now_us() - t0, n, 0);
	t0 = now_us();
	for (i = 0; i < n; i++) {
		Seek(fh, rand_pos(size, len), OFFSET_BEGINNING);
		Read(fh, buf, len);
	}
	report("dos: random Seek + 64 KB Read", now_us() - t0, n, len);
	Seek(fh, rand_pos(size, n * len), OFFSET_BEGINNING);
	t0 = now_us();
	for (i = 0; i < n; i++)
		Read(fh, buf, len);
	report("dos: 64 KB Read straight on", now_us() - t0, n, len);
	Seek(fh, rand_pos(size, n * 1048576L / 4), OFFSET_BEGINNING);
	t0 = now_us();
	for (i = 0; i < n / 4; i++)
		Read(fh, buf, 1048576);
	report("dos: 1 MB Read straight on", now_us() - t0, n / 4, 1048576);
	t0 = now_us();
	for (i = 0; i < n; i++)
		Seek(fh, rand_pos(size, 64), OFFSET_BEGINNING);
	report("dos: random Seek alone", now_us() - t0, n, 0);
	Close(fh);
	t1 = now_us();
	(void)t1;
	free(buf);
	return 0;
}
