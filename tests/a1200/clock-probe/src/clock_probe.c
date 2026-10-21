/*
 * clock_probe: what one clock read costs on the A1200 (PLAN.md section 14,
 * the game profile). SDL's performance counter, which Sys_Milliseconds runs
 * on, is timer.device's ReadEClock; Emu68 also offers the ARM's free-running
 * counter to supervisor code (MOVEC from control registers 0xe0-0xe2, its
 * docs/internals/ControlRegisters.md), reached through exec's Supervisor().
 *   clock_probe [CALLS]
 */
#include <stdio.h>
#include <stdlib.h>
#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

struct Device *TimerBase;

/* Emu68's counter: CNTVALHI, CNTVALLO, CNTVALHI again until the high word
 * held (the documented read); CNTFRQ is its rate in Hz. MOVEC is privileged. */
ULONG cnt_read(ULONG *hi);
ULONG cnt_freq(void);
__asm__(
	"	.text\n"
	"	.globl	_cnt_read\n"
	"_cnt_read:\n"
	"	movem.l	a5/a6,-(sp)\n"
	"	lea	cnt_super(pc),a5\n"
	"	move.l	4.w,a6\n"
	"	jsr	-30(a6)\n"			/* Supervisor */
	"	move.l	12(sp),a0\n"
	"	move.l	d1,(a0)\n"
	"	movem.l	(sp)+,a5/a6\n"
	"	rts\n"
	"cnt_super:\n"
	"	.word	0x4e7a,0x10e2\n"	/* movec cntvalhi,d1 */
	"	.word	0x4e7a,0x00e1\n"	/* movec cntvallo,d0 */
	"	.word	0x4e7a,0x90e2\n"	/* movec cntvalhi,a1 */
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
	"	.word	0x4e7a,0x00e0\n"	/* movec cntfrq,d0 */
	"	rte\n");

static double cnt_now(void)
{
	ULONG hi, lo = cnt_read(&hi);
	return (double)hi * 4294967296.0 + (double)lo;
}

int main(int argc, char **argv)
{
	struct MsgPort *port;
	struct timerequest *tr;
	struct EClockVal ev, ev0;
	struct timeval tv;
	double freq, t0, t1, e0, e1;
	ULONG efreq;
	int calls = argc > 1 ? atoi(argv[1]) : 10000, i;

	if (calls < 100)
		calls = 100;
	if (!OpenResource((CONST_STRPTR) "devicetree.resource")) {
		printf("clock_probe: no devicetree.resource: not Emu68, its counter is not there\n");
		return 10;
	}
	port = CreateMsgPort();
	tr = port ? (struct timerequest *)CreateIORequest(port, sizeof(*tr)) : NULL;
	if (!tr || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)tr, 0)) {
		printf("clock_probe: no timer.device\n");
		return 10;
	}
	TimerBase = tr->tr_node.io_Device;

	freq = (double)cnt_freq();
	/* integers only: libnix's printf has no %f */
	printf("clock_probe: Emu68 counter at %lu Hz, %d calls each\n", (unsigned long)freq, calls);

	/* the counter against the E-clock over one second */
	efreq = ReadEClock(&ev0);
	t0 = cnt_now();
	Delay(50);
	t1 = cnt_now();
	ReadEClock(&ev);
	e0 = (double)ev0.ev_hi * 4294967296.0 + ev0.ev_lo;
	e1 = (double)ev.ev_hi * 4294967296.0 + ev.ev_lo;
	printf("clock_probe: Delay(50): %lu ms by the counter, %lu ms by the E-clock (%lu Hz)\n",
	       (unsigned long)((t1 - t0) / freq * 1e3), (unsigned long)((e1 - e0) / efreq * 1e3),
	       (unsigned long)efreq);

	t0 = cnt_now();
	for (i = 0; i < calls; i++)
		cnt_now();
	t1 = cnt_now();
	printf("clock_probe: Emu68 counter through Supervisor(): %lu ns a call\n",
	       (unsigned long)((t1 - t0) / freq * 1e9 / calls));

	t0 = cnt_now();
	for (i = 0; i < calls; i++)
		ReadEClock(&ev);
	t1 = cnt_now();
	printf("clock_probe: ReadEClock (SDL's performance counter): %lu ns a call\n",
	       (unsigned long)((t1 - t0) / freq * 1e9 / calls));

	t0 = cnt_now();
	for (i = 0; i < calls; i++)
		GetSysTime(&tv);
	t1 = cnt_now();
	printf("clock_probe: GetSysTime: %lu ns a call\n", (unsigned long)((t1 - t0) / freq * 1e9 / calls));

	/* monotonic: no read earlier than the one before */
	{
		double prev = cnt_now(), now;
		int back = 0;
		for (i = 0; i < calls; i++) {
			now = cnt_now();
			if (now < prev)
				back++;
			prev = now;
		}
		printf("clock_probe: %d of %d counter reads went backwards\n", back, calls);
	}

	CloseDevice((struct IORequest *)tr);
	DeleteIORequest((struct IORequest *)tr);
	DeleteMsgPort(port);
	return 0;
}
