/*
 * Doom3: the Workbench icon's program. Workbench cannot start a script from
 * a tool icon, and the game needs Doom3.script's settings and its 9 MB stack:
 * this runs "Execute Doom3.script" in its own drawer (the script's current
 * directory, so the drawer can be anywhere), without a Shell window.
 * Options go on to the script as KEY=VALUE: from Workbench the icon's Tool
 * Types (one in parentheses is off), from a Shell the arguments
 * (Doom3 QUALITY=high W=1280 H=720).
 * GPLv3, as the rest of the port.
 */
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <string.h>

/* the script's keywords (Doom3.script's .key) */
static const char *const keys[] = { "W", "H", "QUALITY", "LIGHTS", "AUDIO", "VOICES", "RATE", "SOUND", "HZ", "MAP", NULL };

static void Message(const char *text)
{
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"Doom 3", (UBYTE *)text, (UBYTE *)"OK" };
    EasyRequestArgs(NULL, &es, NULL, NULL);
}

/* "KEY=VALUE" with one of the script's keywords and a value without spaces or quotes */
static int option_ok(const char *t)
{
    int i;
    const char *eq = strchr(t, '=');
    if (!eq || !eq[1] || strpbrk(eq + 1, " \t\"*;"))
        return 0;
    for (i = 0; keys[i]; i++)
        if ((size_t)(eq - t) == strlen(keys[i]) && !strnicmp(t, keys[i], eq - t))
            return 1;
    return 0;
}

/* the icon's Tool Types that are options, appended to cmd */
static void add_tooltypes(struct WBStartup *wb, char *cmd, size_t size)
{
    struct Library *IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 36);
    struct DiskObject *dobj;
    BPTR old;
    char **tt;
    if (!IconBase)
        return;
    old = CurrentDir(wb->sm_ArgList[0].wa_Lock);
    dobj = GetDiskObject(wb->sm_ArgList[0].wa_Name);
    CurrentDir(old);
    if (dobj) {
        for (tt = (char **)dobj->do_ToolTypes; tt && *tt; tt++)
            if (option_ok(*tt) && strlen(cmd) + strlen(*tt) + 2 < size) {
                strcat(cmd, " ");
                strcat(cmd, *tt);
            }
        FreeDiskObject(dobj);
    }
    CloseLibrary(IconBase);
}

/* the Shell's arguments, appended to cmd (the line ends with a newline) */
static void add_arguments(char *cmd, size_t size)
{
    const char *a = (const char *)GetArgStr();
    size_t n;
    if (!a)
        return;
    while (*a == ' ' || *a == '\t')
        a++;
    n = strcspn(a, "\n");
    if (n && strlen(cmd) + n + 2 < size) {
        strcat(cmd, " ");
        strncat(cmd, a, n);
    }
}

int main(int argc, char **argv)
{
    char cmd[600];
    BPTR in, out, dir, old;
    LONG rc;

    dir = GetProgramDir();
    if (!dir) {
        Message("Doom3 could not find its own drawer.");
        return 20;
    }
    old = CurrentDir(dir);	/* the script's current directory: its new process gets a copy */
    {
        BPTR lock = Lock((CONST_STRPTR)"Doom3.script", ACCESS_READ);
        if (!lock) {
            CurrentDir(old);
            Message("Doom3.script is missing from Doom 3's drawer.");
            return 20;
        }
        UnLock(lock);
    }
    strcpy(cmd, "Execute Doom3.script");
    if (argc == 0)
        add_tooltypes((struct WBStartup *)argv, cmd, sizeof(cmd));
    else
        add_arguments(cmd, sizeof(cmd));

    in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    rc = SystemTags((CONST_STRPTR)cmd, SYS_Input, in, SYS_Output, out, NP_StackSize, 65536, TAG_DONE);
    CurrentDir(old);
    if (rc == -1) {
        Close(in);
        Close(out);
        Message("Doom3 could not start its script.");
        return 20;
    }
    return 0;
}
