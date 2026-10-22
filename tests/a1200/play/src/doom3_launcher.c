/*
 * Doom3: the Workbench icon's program. Workbench cannot start a script from
 * a tool icon, and the game needs Doom3.script's settings and its 9 MB stack:
 * this runs "Execute Doom3.script" from its own drawer, without a Shell
 * window. Started from a Shell it does the same.
 * GPLv3, as the rest of the port.
 */
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <string.h>

static void Message(const char *text)
{
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"Doom 3", (UBYTE *)text, (UBYTE *)"OK" };
    EasyRequestArgs(NULL, &es, NULL, NULL);
}

int main(void)
{
    char dir[256], script[300], cmd[340];
    BPTR in, out;
    LONG rc;

    if (!NameFromLock(GetProgramDir(), (STRPTR)dir, sizeof(dir))) {
        Message("Doom3 could not find its own drawer.");
        return 20;
    }
    strcpy(script, dir);
    if (!AddPart((STRPTR)script, (CONST_STRPTR)"Doom3.script", sizeof(script))) {
        return 20;
    }
    {
        BPTR lock = Lock((CONST_STRPTR)script, ACCESS_READ);
        if (!lock) {
            Message("Doom3.script is missing from Doom 3's drawer.");
            return 20;
        }
        UnLock(lock);
    }
    strcpy(cmd, "Execute \"");
    strcat(cmd, script);
    strcat(cmd, "\"");

    in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    rc = SystemTags((CONST_STRPTR)cmd, SYS_Input, in, SYS_Output, out, NP_StackSize, 65536, TAG_DONE);
    if (rc == -1) {
        Close(in);
        Close(out);
        Message("Doom3 could not start its script.");
        return 20;
    }
    return 0;
}
