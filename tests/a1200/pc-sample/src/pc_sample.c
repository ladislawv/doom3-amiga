/*
 * pc_sample: a sampling profiler for a program on the A1200 (PLAN.md section
 * 14). Every millisecond it wakes (timer.device, above the program's
 * priority) and reads the PC at which exec stopped the program's process:
 * the program is then ready to run (preempted by this wake-up), and exec has
 * saved its registers on its stack at tc_SPReg (frame_probe.c measured the
 * layout under Emu68: a 0xFFFF word, the FPU's control registers, fp0-fp7 and
 * its fsave frame, then PC, SR, d0-d7/a0-a6). A process waiting in Wait() is
 * counted apart, with the first return address of its own code found on its
 * stack above the frame.
 *
 * It samples only while the global variable VAR is "counting" (the
 * benchmark sets ENV:d3bench, patch 0019), or, with VAR "", from the moment
 * the process is found. Output (text, for tools/pcsample_report.py):
 *   segment N START SIZE      the program's segments (START: the first byte
 *                             of code or data; offsets into the hunk file's
 *                             sections, which nm prints, start there)
 *   lib NAME LOW HIGH         the range of most of each library's vectors
 *   samples ... ready ... wait ... other ... bad ...
 *   pc ADDRESS COUNT          ready samples, by PC
 *   wait ADDRESS COUNT        waiting samples, by return address (0: none found)
 *   stack PC A1 A2 ...        each ready sample: the PC and the first values
 *                             on its stack that point into the program's code (return addresses,
 *                             mostly: with no frame pointers some are stale
 *                             values; the report sums them per function, once
 *                             per sample, as an approximate inclusive profile)
 *
 *   pc_sample NAME OUT [VAR] [MAXSECS]
 *     NAME: the program's command name as its CLI shows it (e.g. dhewm3-a)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>

extern struct ExecBase *SysBase;

#define MAX_SAMPLES 400000
#define MAX_SEGS 16
#define STACK_DEPTH 24		/* addresses kept per sample */
#define STACK_SCAN 2048		/* longs of stack looked at */
#define MAX_STACKS 60000

static ULONG *pcs, *waits, *stacks;
static long numPcs, numWaits, numStacks;
static ULONG segStart[MAX_SEGS], segSize[MAX_SEGS];
static int numSegs;
static ULONG ogLow, ogHigh;		/* ogles2.library's code, for waits inside it */

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
		const char *file;
		if (!p || !p->pr_CLI)
			continue;
		cli = (struct CommandLineInterface *)BADDR(p->pr_CLI);
		b = (UBYTE *)BADDR(cli->cli_CommandName);
		if (!b || !b[0])
			continue;
		memcpy(cmd, b + 1, b[0]);
		cmd[b[0]] = 0;
		file = (const char *)FilePart((CONST_STRPTR)cmd);
		if (!stricmp(file, name))
			return p;
	}
	return NULL;
}

static int process_alive(struct Process *proc, const char *name)
{
	struct Process *p;
	Forbid();
	p = find_process(name);
	Permit();
	return p == proc;
}

static int var_is(const char *var, const char *value)
{
	char buf[32];
	if (GetVar((CONST_STRPTR)var, (STRPTR)buf, sizeof(buf), GVF_GLOBAL_ONLY) < 0)
		return 0;
	return !strcmp(buf, value);
}

static int in_segments(ULONG a)
{
	int i;
	for (i = 0; i < numSegs; i++)
		if (a >= segStart[i] && a < segStart[i] + segSize[i])
			return 1;
	return 0;
}

/* the offset of the saved PC above tc_SPReg; -1 if the frame is not one known */
static int pc_offset(const UBYTE *sp)
{
	static const int offs[] = { 0x74, 0x04, 0x02, 0x06, 0x00 };
	int i;
	if (*(const UWORD *)sp == 0xFFFF) {
		/* the FPU's registers saved: SR's upper byte is 0 in user mode */
		return sp[0x78] == 0 ? 0x74 : -1;
	}
	for (i = 1; i < 5; i++) {
		ULONG pc = *(const ULONG *)(sp + offs[i]);
		if (sp[offs[i] + 4] == 0 && !(pc & 1) && pc > 0x1000)
			return offs[i];
	}
	return -1;
}

/* the range holding most of a library's vector targets (SetFunction moves a few elsewhere) */
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

static void write_counts(FILE *f, const char *what, ULONG *a, long n)
{
	long i, j;
	qsort(a, n, sizeof(a[0]), cmp_ulong);
	for (i = 0; i < n; i = j) {
		for (j = i; j < n && a[j] == a[i]; j++)
			;
		fprintf(f, "%s %08lx %ld\n", what, a[i], j - i);
	}
}

int main(int argc, char **argv)
{
	const char *name, *out, *var;
	long maxSecs, i, samples = 0, ready = 0, wait = 0, other = 0, bad = 0;
	struct Process *proc = NULL;
	struct MsgPort *port;
	struct timerequest *tr;
	FILE *f;
	BPTR seg;

	if (argc < 3) {
		printf("usage: pc_sample NAME OUT [VAR] [MAXSECS]\n");
		return 10;
	}
	name = argv[1];
	out = argv[2];
	var = argc > 3 ? argv[3] : "d3bench";
	maxSecs = argc > 4 ? atol(argv[4]) : 120;
	pcs = (ULONG *)malloc(MAX_SAMPLES * sizeof(ULONG));
	waits = (ULONG *)malloc(MAX_SAMPLES * sizeof(ULONG));
	stacks = (ULONG *)malloc(MAX_STACKS * (STACK_DEPTH + 1) * sizeof(ULONG));
	port = CreateMsgPort();
	tr = port ? (struct timerequest *)CreateIORequest(port, sizeof(*tr)) : NULL;
	if (!pcs || !waits || !stacks || !tr || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0)) {
		printf("pc_sample: no memory or no timer.device\n");
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
				segSize[numSegs] = s[-1] - 8;	/* the allocation's size, with the size and link longs */
				numSegs++;
			}
		}
		Permit();
		if (!proc)
			Delay(5);
	}
	if (!proc) {
		printf("pc_sample: no process %s\n", name);
		return 10;
	}
	printf("pc_sample: %s found, %d segments; waiting for %s\n", name, numSegs, var[0] ? var : "nothing");
	while (var[0] && !var_is(var, "counting")) {
		if (!process_alive(proc, name)) {
			printf("pc_sample: %s ended before %s was set\n", name, var);
			return 5;
		}
		Delay(5);
	}

	/* ogles2's code, for the waits inside it (the game has it open by now) */
	Forbid();
	{
		struct Library *og = (struct Library *)FindName(&SysBase->LibList, (CONST_STRPTR)"ogles2.library");
		if (og) {
			lib_range(og, &ogLow, &ogHigh);
			ogHigh += 0x10000;	/* its last function's body */
		}
	}
	Permit();

	SetTaskPri(FindTask(NULL), 10);
	for (;;) {
		struct Task *t = &proc->pr_Task;
		tr->tr_node.io_Command = TR_ADDREQUEST;
		tr->tr_time.tv_secs = 0;
		tr->tr_time.tv_micro = 1000;
		DoIO((struct IORequest *)tr);
		Disable();
		{
			UBYTE state = t->tc_State;
			const UBYTE *sp = (const UBYTE *)t->tc_SPReg;
			int off = pc_offset(sp);
			if (off < 0) {
				bad++;
			} else if (state == TS_READY) {
				ULONG pc = *(const ULONG *)(sp + off);
				if (numPcs < MAX_SAMPLES)
					pcs[numPcs++] = pc;
				ready++;
				if (numStacks < MAX_STACKS) {
					/* the stack as the program left it: above the 66-byte frame */
					const ULONG *s = (const ULONG *)(sp + off + 66);
					ULONG *st = stacks + numStacks * (STACK_DEPTH + 1);
					long k, n = 0, scan = ((const ULONG *)t->tc_SPUpper - s);
					if (scan > STACK_SCAN)
						scan = STACK_SCAN;
					st[0] = pc;
					for (k = 0; k < scan && n < STACK_DEPTH; k++)
						if (s[k] - segStart[0] < segSize[0] && !(s[k] & 1))
							st[1 + n++] = s[k];
					while (n < STACK_DEPTH)
						st[1 + n++] = 0;
					numStacks++;
				}
			} else if (state == TS_WAIT) {
				/* the caller: the first return address into the program (or ogles2) above the frame */
				const ULONG *s = (const ULONG *)(sp + off + 66);
				ULONG found = 0;
				int k;
				for (k = 0; k < 96; k++)
					if (in_segments(s[k]) || (s[k] >= ogLow && s[k] < ogHigh)) {
						found = s[k];
						break;
					}
				if (numWaits < MAX_SAMPLES)
					waits[numWaits++] = found;
				wait++;
			} else {
				other++;
			}
		}
		Enable();
		samples++;
		if ((samples & 127) == 0) {
			if (!process_alive(proc, name))
				break;
			if (var[0] && !var_is(var, "counting"))
				break;
			if (samples >= maxSecs * 1000)
				break;
		}
	}
	SetTaskPri(FindTask(NULL), 0);

	f = fopen(out, "w");
	if (!f) {
		printf("pc_sample: cannot write %s\n", out);
		return 10;
	}
	fprintf(f, "pc_sample 1 %s\n", name);
	for (i = 0; i < numSegs; i++)
		fprintf(f, "segment %ld %08lx %08lx\n", i, segStart[i], segSize[i]);
	write_libs(f, &SysBase->LibList);
	write_libs(f, &SysBase->DeviceList);
	fprintf(f, "samples %ld ready %ld wait %ld other %ld bad %ld\n", samples, ready, wait, other, bad);
	write_counts(f, "pc", pcs, numPcs);
	write_counts(f, "wait", waits, numWaits);
	for (i = 0; i < numStacks; i++) {
		const ULONG *st = stacks + i * (STACK_DEPTH + 1);
		int k;
		fprintf(f, "stack");
		for (k = 0; k <= STACK_DEPTH && (k == 0 || st[k]); k++)
			fprintf(f, " %lx", (unsigned long)st[k]);
		fprintf(f, "\n");
	}
	fclose(f);
	printf("pc_sample: %ld samples (%ld ready, %ld waiting, %ld other, %ld unreadable) in %s\n",
		samples, ready, wait, other, bad, out);
	CloseDevice((struct IORequest *)tr);
	DeleteIORequest((struct IORequest *)tr);
	DeleteMsgPort(port);
	return 0;
}
