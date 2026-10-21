/*
 * frame_probe: where exec keeps a preempted task's registers on this machine
 * (Emu68, the ROM's exec), for the PC sampler (pc_sample.c). A busy task in
 * assembly holds recognisable values in every register (d0 = 0xD0D0D0D0 ...
 * a6 = 0xA6A6A6A6, fp1 = 1.5) and loops between busy and busy_end; this task,
 * at a higher priority, wakes every millisecond and dumps the 48 longs at the
 * busy task's tc_SPReg.
 *   frame_probe [SAMPLES]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <devices/timer.h>
#include <proto/exec.h>

volatile ULONG spin;
void busy(void);
void busy_loop(void);
void busy_end(void);
__asm__(
	"	.text\n"
	"	.globl	_busy\n"
	"_busy:\n"
	"	fmove.l	#3,fp1\n"
	"	fdiv.l	#2,fp1\n"
	"	move.l	#0xD0D0D0D0,d0\n"
	"	move.l	#0xD1D1D1D1,d1\n"
	"	move.l	#0xD2D2D2D2,d2\n"
	"	move.l	#0xD3D3D3D3,d3\n"
	"	move.l	#0xD4D4D4D4,d4\n"
	"	move.l	#0xD5D5D5D5,d5\n"
	"	move.l	#0xD6D6D6D6,d6\n"
	"	move.l	#0xD7D7D7D7,d7\n"
	"	move.l	#0xA0A0A0A0,a0\n"
	"	move.l	#0xA1A1A1A1,a1\n"
	"	move.l	#0xA2A2A2A2,a2\n"
	"	move.l	#0xA3A3A3A3,a3\n"
	"	move.l	#0xA4A4A4A4,a4\n"
	"	move.l	#0xA5A5A5A5,a5\n"
	"	move.l	#0xA6A6A6A6,a6\n"
	"	.globl	_busy_loop\n"
	"_busy_loop:\n"
	"	fmove.x	fp1,fp2\n"
	"	addq.l	#1,_spin\n"
	"	bra.s	_busy_loop\n"
	"	.globl	_busy_end\n"
	"_busy_end:\n"
	"	nop\n");

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 6, i, j;
	struct Task *t;
	ULONG stackSize = 8192;
	UBYTE *stack;
	struct MsgPort *port = CreateMsgPort();
	struct timerequest *tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
	ULONG dump[48];

	if (!port || !tr || OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0)) {
		printf("frame_probe: no timer.device\n");
		return 10;
	}
	t = (struct Task *)AllocMem(sizeof(*t), MEMF_PUBLIC | MEMF_CLEAR);
	stack = (UBYTE *)AllocMem(stackSize, MEMF_PUBLIC | MEMF_CLEAR);
	t->tc_Node.ln_Type = NT_TASK;
	t->tc_Node.ln_Name = (char *)"frame_probe busy";
	t->tc_Node.ln_Pri = -1;
	t->tc_SPLower = stack;
	t->tc_SPUpper = stack + stackSize;
	t->tc_SPReg = stack + stackSize;
	printf("frame_probe: busy %08lx, loop %08lx, end %08lx; stack %08lx-%08lx\n",
		(ULONG)busy, (ULONG)busy_loop, (ULONG)busy_end, (ULONG)stack, (ULONG)(stack + stackSize));
	AddTask(t, (APTR)busy, NULL);
	SetTaskPri(FindTask(NULL), 5);

	for (i = 0; i < n; i++) {
		tr->tr_node.io_Command = TR_ADDREQUEST;
		tr->tr_time.tv_secs = 0;
		tr->tr_time.tv_micro = 1000 + 137 * i;
		DoIO((struct IORequest *)tr);
		Disable();
		{
			ULONG *sp = (ULONG *)t->tc_SPReg;
			UBYTE state = t->tc_State;
			for (j = 0; j < 48; j++)
				dump[j] = sp[j];
			Enable();
			printf("sample %d: state %d, tc_SPReg %08lx (top - %ld), spin %lu\n", i, state, (ULONG)sp,
				(long)((stack + stackSize) - (UBYTE *)sp), spin);
		}
		for (j = 0; j < 48; j++) {
			printf("  +%02x %08lx", j * 4, dump[j]);
			if (j % 6 == 5)
				printf("\n");
		}
	}

	Forbid();
	RemTask(t);
	Permit();
	FreeMem(stack, stackSize);
	FreeMem(t, sizeof(*t));
	CloseDevice((struct IORequest *)tr);
	DeleteIORequest((struct IORequest *)tr);
	DeleteMsgPort(port);
	return 0;
}
