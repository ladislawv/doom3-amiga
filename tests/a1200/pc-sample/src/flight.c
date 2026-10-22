/*
 * flight: a flight recorder for a program that freezes the whole A1200
 * (PLAN.md section 14, the main menu's freeze). Every PERIOD ms it wakes at
 * priority 21 (above input.device) and appends one line to OUT, closing the
 * file each time so that the lines before a freeze survive the reboot:
 *   n TICKS STATE PC SIGWAIT : RETURN ADDRESSES | READY TASKS
 *     n         the record's number; TICKS the DOS clock (1/50 s since the
 *               day's start)
 *     STATE     R (ready: preempted by this wake-up) with its PC, W (in
 *               Wait()) with the first return address into the program or
 *               ogles2 found above its frame, ? for a frame not understood
 *     SIGWAIT   the signals the process waits for (W only)
 *     RETURN    up to 8 values on its stack that point into the program's
 *               code or ogles2's (return addresses, mostly)
 *     READY     every task ready to run, name/priority: a task spinning
 *               below priority 21 shows here in every record
 * A header gives the program's segments and the libraries' ranges, as
 * pc_sample's (pc_sample.c measured exec's saved frame under Emu68).
 * If records go on during a freeze, something spins below 21 (READY names
 * it); if they stop, the last ones show where the program was.
 *
 *   flight NAME OUT [PERIOD] [COUNT]
 *     NAME: the program's command name as its CLI shows it (e.g. dhewm3)
 *     COUNT: records to write (default: until the process is gone)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>

extern struct ExecBase *SysBase;

#define MAX_SEGS 16
#define RETURNS 8		/* addresses kept per record */
#define STACK_SCAN 512		/* longs of stack looked at */
#define MAX_READY 12

static ULONG segStart[MAX_SEGS], segSize[MAX_SEGS];
static int numSegs;
static ULONG ogLow, ogHigh;		/* ogles2.library's code */

static int cmp_ulong(const void *a, const void *b)
{
	ULONG x = *(const ULONG *)a, y = *(const ULONG *)b;
	return x < y ? -1 : x > y;
}

/* the process whose CLI runs the command NAME (its file part), or NULL */
static struct Process *find_process(const char *name)
{
	long n, max = MaxCli();
	for (n = 1; n <= max; n++) {
		struct Process *p = FindCliProc(n);
		struct CommandLineInterface *cli;
		UBYTE *b;
		char cmd[256];
		if (!p || !p->pr_CLI)
			continue;
		cli = (struct CommandLineInterface *)BADDR(p->pr_CLI);
		b = (UBYTE *)BADDR(cli->cli_CommandName);
		if (!b || !b[0])
			continue;
		memcpy(cmd, b + 1, b[0]);
		cmd[b[0]] = 0;
		if (!stricmp((const char *)FilePart((CONST_STRPTR)cmd), name))
			return p;
	}
	return NULL;
}

static int in_code(ULONG a)
{
	int i;
	if (a & 1)
		return 0;
	if (a >= ogLow && a < ogHigh)
		return 1;
	for (i = 0; i < numSegs; i++)
		if (a >= segStart[i] && a < segStart[i] + segSize[i])
			return 1;
	return 0;
}

/* the offset of the saved PC above tc_SPReg; -1 if the frame is not one known (pc_sample.c) */
static int pc_offset(const UBYTE *sp)
{
	static const int offs[] = { 0x74, 0x04, 0x02, 0x06, 0x00 };
	int i;
	if (*(const UWORD *)sp == 0xFFFF)
		return sp[0x78] == 0 ? 0x74 : -1;
	for (i = 1; i < 5; i++) {
		ULONG pc = *(const ULONG *)(sp + offs[i]);
		if (sp[offs[i] + 4] == 0 && !(pc & 1) && pc > 0x1000)
			return offs[i];
	}
	return -1;
}

/* the range holding most of a library's vector targets (pc_sample.c) */
static void lib_range(struct Library *lib, ULONG *low, ULONG *high)
{
	static ULONG t[2048];
	int n = 0, i;
	ULONG mid;
	long off;
	*low = *high = 0;
	for (off = 6; off <= lib->lib_NegSize && n < 2048; off += 6) {
		const UWORD *v = (const UWORD *)((const UBYTE *)lib - off);
		if (v[0] == 0x4EF9)
			t[n++] = *(const ULONG *)(v + 1);
	}
	if (!n)
		return;
	qsort(t, n, sizeof(t[0]), cmp_ulong);
	mid = t[n / 2];
	*low = *high = mid;
	for (i = 0; i < n; i++)
		if (t[i] + 0x200000 > mid && t[i] < mid + 0x200000) {
			if (t[i] < *low)
				*low = t[i];
			if (t[i] > *high)
				*high = t[i];
		}
}

static void write_libs(FILE *f, struct List *list)
{
	struct Node *nd;
	static struct { char name[40]; ULONG low, high; } l[128];
	int n = 0, i;
	Forbid();
	for (nd = list->lh_Head; nd->ln_Succ && n < 128; nd = nd->ln_Succ) {
		strncpy(l[n].name, nd->ln_Name ? nd->ln_Name : "?", 39);
		l[n].name[39] = 0;
		lib_range((struct Library *)nd, &l[n].low, &l[n].high);
		n++;
	}
	Permit();
	for (i = 0; i < n; i++)
		fprintf(f, "lib %s %08lx %08lx\n", l[i].name, l[i].low, l[i].high);
}

/* one line appended, the file closed again: on disk before the next record */
static void append(const char *out, const char *line, long len)
{
	BPTR fh = Open((CONST_STRPTR)out, MODE_READWRITE);
	if (!fh)
		return;
	Seek(fh, 0, OFFSET_END);
	Write(fh, (APTR)line, len);
	Close(fh);
}

int main(int argc, char **argv)
{
	const char *name, *out;
	long period, count, i, n;
	struct Process *proc = NULL;
	struct MsgPort *port;
	struct timerequest *tr;
	FILE *f;
	BPTR seg;
	static char line[1024];

	if (argc < 3) {
		printf("usage: flight NAME OUT [PERIOD] [COUNT]\n");
		return 10;
	}
	name = argv[1];
	out = argv[2];
	period = argc > 3 ? atol(argv[3]) : 250;
	count = argc > 4 ? atol(argv[4]) : 0;
	if (period < 20)
		period = 20;
	port = CreateMsgPort();
	tr = port ? (struct timerequest *)CreateIORequest(port, sizeof(*tr)) : NULL;
	if (!tr || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0)) {
		printf("flight: no timer.device\n");
		return 10;
	}

	/* the process: up to ten minutes for it to start */
	for (i = 0; i < 6000 && !proc; i++) {
		Forbid();
		proc = find_process(name);
		if (proc) {
			struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(proc->pr_CLI);
			for (seg = cli->cli_Module; seg && numSegs < MAX_SEGS; seg = *(BPTR *)BADDR(seg)) {
				ULONG *s = (ULONG *)BADDR(seg);
				segStart[numSegs] = (ULONG)(s + 1);
				segSize[numSegs] = s[-1] - 8;
				numSegs++;
			}
		}
		Permit();
		if (!proc)
			Delay(5);
	}
	if (!proc) {
		printf("flight: no process %s\n", name);
		return 10;
	}
	/* ogles2 is opened during the game's start: wait for it (up to a minute) */
	for (i = 0; i < 600 && !ogLow; i++) {
		struct Library *og;
		Forbid();
		og = (struct Library *)FindName(&SysBase->LibList, (CONST_STRPTR)"ogles2.library");
		if (og) {
			lib_range(og, &ogLow, &ogHigh);
			ogHigh += 0x10000;	/* its last function's body */
		}
		Permit();
		if (!ogLow)
			Delay(5);
	}

	f = fopen(out, "w");
	if (!f) {
		printf("flight: cannot write %s\n", out);
		return 10;
	}
	fprintf(f, "flight 1 %s period %ld\n", name, period);
	for (i = 0; i < numSegs; i++)
		fprintf(f, "segment %ld %08lx %08lx\n", i, segStart[i], segSize[i]);
	fprintf(f, "ogles2 %08lx %08lx\n", ogLow, ogHigh);
	/* its vectors' targets: the report finds the library's load address by
	 * matching them with the symbols of its file */
	{
		static ULONG vec[1024], voff[1024];
		long off, nv = 0;
		struct Library *og;
		Forbid();	/* copied first: no DOS I/O under Forbid() */
		og = (struct Library *)FindName(&SysBase->LibList, (CONST_STRPTR)"ogles2.library");
		for (off = 6; og && off <= og->lib_NegSize && nv < 1024; off += 6) {
			const UWORD *v = (const UWORD *)((const UBYTE *)og - off);
			if (v[0] == 0x4EF9) {
				voff[nv] = off;
				vec[nv++] = *(const ULONG *)(v + 1);
			}
		}
		Permit();
		for (i = 0; i < nv; i++)
			fprintf(f, "ogvec %ld %08lx\n", voff[i], vec[i]);
	}
	write_libs(f, &SysBase->LibList);
	write_libs(f, &SysBase->DeviceList);
	fclose(f);

	SetTaskPri(FindTask(NULL), 21);
	for (n = 0; !count || n < count; n++) {
		struct Task *t = &proc->pr_Task;
		struct DateStamp ds;
		char state = '?';
		ULONG at = 0, sigwait = 0, ret[RETURNS];
		int nret = 0, nready = 0;
		struct { char name[17]; BYTE pri; } ready[MAX_READY];
		long len;

		tr->tr_node.io_Command = TR_ADDREQUEST;
		tr->tr_time.tv_secs = period / 1000;
		tr->tr_time.tv_micro = (period % 1000) * 1000;
		DoIO((struct IORequest *)tr);
		DateStamp(&ds);

		Forbid();
		if (find_process(name) != proc) {
			Permit();
			break;
		}
		Disable();
		{
			const UBYTE *sp = (const UBYTE *)t->tc_SPReg;
			int off = pc_offset(sp);
			struct Node *nd;
			if (off >= 0 && (t->tc_State == TS_READY || t->tc_State == TS_WAIT)) {
				const ULONG *s = (const ULONG *)(sp + off + 66);
				long k, scan = (const ULONG *)t->tc_SPUpper - s;
				if (scan > STACK_SCAN)
					scan = STACK_SCAN;
				for (k = 0; k < scan && nret < RETURNS; k++)
					if (in_code(s[k]))
						ret[nret++] = s[k];
				if (t->tc_State == TS_READY) {
					state = 'R';
					at = *(const ULONG *)(sp + off);
				} else {
					state = 'W';
					at = nret ? ret[0] : 0;
					sigwait = t->tc_SigWait;
				}
			}
			for (nd = SysBase->TaskReady.lh_Head; nd->ln_Succ && nready < MAX_READY; nd = nd->ln_Succ) {
				strncpy(ready[nready].name, nd->ln_Name ? nd->ln_Name : "?", 16);
				ready[nready].name[16] = 0;
				ready[nready].pri = nd->ln_Pri;
				nready++;
			}
		}
		Enable();
		Permit();

		len = sprintf(line, "%ld %ld %c %08lx %08lx :", n, ds.ds_Minute * 3000 + ds.ds_Tick, state, at, sigwait);
		for (i = 0; i < nret; i++)
			len += sprintf(line + len, " %lx", ret[i]);
		len += sprintf(line + len, " |");
		for (i = 0; i < nready; i++)
			len += sprintf(line + len, " %s/%d", ready[i].name, (int)ready[i].pri);
		line[len++] = '\n';
		append(out, line, len);
	}
	SetTaskPri(FindTask(NULL), 0);
	if (!count || n < count)
		append(out, "end: the process is gone\n", 25);
	CloseDevice((struct IORequest *)tr);
	DeleteIORequest((struct IORequest *)tr);
	DeleteMsgPort(port);
	return 0;
}
