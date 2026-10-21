/*
 * tasks: lists every exec task and process (name, type, priority, state),
 * which the Shell's Status does not (it shows Shell processes only). Used to
 * find tasks a program left running after it exited.
 *   tasks
 * GPLv3, as the rest of the port.
 */
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#define MAX_TASKS 128

struct entry {
    char name[48];
    BYTE pri;
    UBYTE type, state;
    APTR addr;
};

static struct entry list[MAX_TASKS];
static int n;

static void take(struct Task *t)
{
    if (n >= MAX_TASKS)
        return;
    strncpy(list[n].name, t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "(no name)", sizeof(list[n].name) - 1);
    list[n].name[sizeof(list[n].name) - 1] = 0;
    list[n].pri = t->tc_Node.ln_Pri;
    list[n].type = t->tc_Node.ln_Type;
    list[n].state = t->tc_State;
    list[n].addr = t;
    n++;
}

int main(void)
{
    struct ExecBase *eb = SysBase;
    struct Node *node;
    int i;

    /* copy under Disable (the lists change while we walk them), print after */
    Disable();
    take(eb->ThisTask);
    for (node = eb->TaskReady.lh_Head; node->ln_Succ; node = node->ln_Succ)
        take((struct Task *)node);
    for (node = eb->TaskWait.lh_Head; node->ln_Succ; node = node->ln_Succ)
        take((struct Task *)node);
    Enable();

    for (i = 0; i < n; i++) {
        const char *state = list[i].state == TS_RUN ? "running" : list[i].state == TS_READY ? "READY" :
                            list[i].state == TS_WAIT ? "waiting" : "other";
        printf("%08lx %-8s pri %4d %-8s %s\n", (unsigned long)list[i].addr,
               list[i].type == NT_PROCESS ? "process" : "task", list[i].pri, state, list[i].name);
    }
    return 0;
}
