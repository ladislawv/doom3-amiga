/*
 * mkicons: the release's icons, made on the Amiga from the system's default
 * ones (release/build_release.sh keeps the results in release/icons/):
 *   DIR/Doom3.info    the launcher's tool icon; its Tool Types are the
 *                     script's main options, in parentheses (off)
 *   DIR/ReadMe.info   a project icon that opens the guide in MultiView
 *   DIR.info          the drawer's icon
 *   mkicons DIR
 * GPLv3, as the rest of the port.
 */
#include <stdio.h>
#include <string.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/exec.h>
#include <proto/icon.h>

struct Library *IconBase;

/* the script's options, in parentheses (ignored): the game's saved settings are
   used (patch 0033); removing the parentheses gives an option at every start,
   which is then saved too */
static const char *tooltypes[] = {
    "(W=1280)",
    "(H=720)",
    "(QUALITY=low)",
    "(LIGHTS=12)",
    "(HZ=20)",
    "AUDIO=paula",
    "(SOUND=0)",
    NULL
};

static int put(const char *path, struct DiskObject *d)
{
    if (!PutDiskObject((CONST_STRPTR)path, d)) {
        printf("mkicons: cannot write %s.info\n", path);
        return 0;
    }
    printf("%s.info\n", path);
    return 1;
}

int main(int argc, char **argv)
{
    char path[256];
    struct DiskObject *d;
    STRPTR old_tt, old_tool;
    LONG old_x, old_y;
    int ok = 1;

    if (argc < 2) {
        printf("usage: mkicons DIR\n");
        return 10;
    }
    if (!(IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 36))) {
        printf("mkicons: no icon.library\n");
        return 20;
    }

    if ((d = GetDefDiskObject(WBTOOL))) {
        old_tt = (STRPTR)d->do_ToolTypes;
        old_x = d->do_CurrentX;
        old_y = d->do_CurrentY;
        d->do_ToolTypes = (STRPTR *)tooltypes;
        d->do_CurrentX = d->do_CurrentY = NO_ICON_POSITION;
        snprintf(path, sizeof(path), "%s/Doom3", argv[1]);
        ok &= put(path, d);
        d->do_ToolTypes = (STRPTR *)old_tt;
        d->do_CurrentX = old_x;
        d->do_CurrentY = old_y;
        FreeDiskObject(d);
    } else
        ok = 0;

    if ((d = GetDefDiskObject(WBPROJECT))) {
        old_tool = d->do_DefaultTool;
        old_x = d->do_CurrentX;
        old_y = d->do_CurrentY;
        d->do_DefaultTool = (STRPTR)"SYS:Utilities/MultiView";
        d->do_CurrentX = d->do_CurrentY = NO_ICON_POSITION;
        snprintf(path, sizeof(path), "%s/ReadMe", argv[1]);
        ok &= put(path, d);
        d->do_DefaultTool = old_tool;
        d->do_CurrentX = old_x;
        d->do_CurrentY = old_y;
        FreeDiskObject(d);
    } else
        ok = 0;

    if ((d = GetDefDiskObject(WBDRAWER))) {
        old_x = d->do_CurrentX;
        old_y = d->do_CurrentY;
        d->do_CurrentX = d->do_CurrentY = NO_ICON_POSITION;
        ok &= put(argv[1], d);
        d->do_CurrentX = old_x;
        d->do_CurrentY = old_y;
        FreeDiskObject(d);
    } else
        ok = 0;

    CloseLibrary(IconBase);
    return ok ? 0 : 10;
}
