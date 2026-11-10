/*
 * mkicons: the release's icons, made on the Amiga (release/build_release.sh
 * keeps the results in release/icons/):
 *   DIR/Doom3.info    the launcher's tool icon; its Tool Types are the
 *                     script's main options, in parentheses (off)
 *   DIR/ReadMe.info   a project icon that opens the guide in MultiView
 *   DIR.info          the drawer's icon
 *   mkicons DIR [ART]
 * Each starts from the system's default icon of its kind. With ART, a directory
 * holding game.d3ic, readme.d3ic and drawer.d3ic (release/icons/art/
 * make_icons.py), their pictures replace the default ones as colour icons
 * (GlowIcons: icon.library 44, OS 3.5 and later; the default planar image
 * stays for older systems), frameless.
 * Doom 3 for AmigaOS 3, by R-Typer and Ladislawv. GPLv3, as the rest of the port.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <graphics/view.h>
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
    "AUDIO=paula",
    "(SOUND=0)",
    NULL
};

/* a .d3ic file: "D3IC", width, height (u16 BE), then two images, each:
   colours (u16 BE), transparent colour (u8), palette (RGB), pixels */
struct art {
    int w, h;
    int colours[2], transparent[2];
    UBYTE *palette[2], *pixels[2];
    UBYTE *buf;
};

static int load_art(const char *dir, const char *name, struct art *a)
{
    char path[256];
    FILE *f;
    long size, pos;
    int i;
    snprintf(path, sizeof(path), "%s/%s.d3ic", dir, name);
    if (!(f = fopen(path, "rb"))) {
        printf("mkicons: cannot read %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    a->buf = (UBYTE *)malloc(size);
    if (!a->buf || fread(a->buf, 1, size, f) != (size_t)size || size < 8 || memcmp(a->buf, "D3IC", 4)) {
        printf("mkicons: %s is not a .d3ic file\n", path);
        fclose(f);
        return 0;
    }
    fclose(f);
    a->w = (a->buf[4] << 8) | a->buf[5];
    a->h = (a->buf[6] << 8) | a->buf[7];
    pos = 8;
    for (i = 0; i < 2; i++) {
        if (pos + 3 > size)
            return 0;
        a->colours[i] = (a->buf[pos] << 8) | a->buf[pos + 1];
        a->transparent[i] = a->buf[pos + 2];
        pos += 3;
        a->palette[i] = a->buf + pos;
        pos += 3 * a->colours[i];
        a->pixels[i] = a->buf + pos;
        pos += (long)a->w * a->h;
        if (pos > size || a->colours[i] < 1 || a->colours[i] > 256)
            return 0;
    }
    return 1;
}

/* the art's two pictures into the icon (it is the caller's copy) */
static int set_art(struct DiskObject *d, struct art *a)
{
    LONG err = 0;
    struct TagItem tags[] = {
        { ICONCTRLA_SetWidth, 0 },
        { ICONCTRLA_SetHeight, 0 },
        { ICONCTRLA_SetPaletteSize1, 0 },
        { ICONCTRLA_SetPalette1, 0 },
        { ICONCTRLA_SetTransparentColor1, 0 },
        { ICONCTRLA_SetImageData1, 0 },
        { ICONCTRLA_SetPaletteSize2, 0 },
        { ICONCTRLA_SetPalette2, 0 },
        { ICONCTRLA_SetTransparentColor2, 0 },
        { ICONCTRLA_SetImageData2, 0 },
        { ICONCTRLA_SetFrameless, TRUE },
        { ICONA_ErrorCode, 0 },
        { TAG_DONE, 0 }
    };
    tags[0].ti_Data = a->w;
    tags[1].ti_Data = a->h;
    tags[2].ti_Data = a->colours[0];
    tags[3].ti_Data = (ULONG)a->palette[0];
    tags[4].ti_Data = a->transparent[0];
    tags[5].ti_Data = (ULONG)a->pixels[0];
    tags[6].ti_Data = a->colours[1];
    tags[7].ti_Data = (ULONG)a->palette[1];
    tags[8].ti_Data = a->transparent[1];
    tags[9].ti_Data = (ULONG)a->pixels[1];
    tags[11].ti_Data = (ULONG)&err;
    IconControlA(d, tags);
    if (err) {
        printf("mkicons: IconControl failed (%ld)\n", (long)err);
        return 0;
    }
    /* the hit box follows the picture */
    d->do_Gadget.Width = a->w;
    d->do_Gadget.Height = a->h;
    return 1;
}

static int put(const char *path, struct DiskObject *d)
{
    if (!PutDiskObject((CONST_STRPTR)path, d)) {
        printf("mkicons: cannot write %s.info\n", path);
        return 0;
    }
    printf("%s.info\n", path);
    return 1;
}

/* one icon: the default of its kind, the art (if any), then the changes */
static int make(LONG kind, const char *path, const char *artdir, const char *artname,
                STRPTR *tt, STRPTR tool)
{
    struct DiskObject *d = GetDefDiskObject(kind);
    struct art a;
    STRPTR *old_tt;
    STRPTR old_tool;
    LONG old_x, old_y;
    int ok = 1;
    if (!d)
        return 0;
    memset(&a, 0, sizeof(a));
    if (artdir && (!load_art(artdir, artname, &a) || !set_art(d, &a)))
        ok = 0;
    old_tt = d->do_ToolTypes;
    old_tool = d->do_DefaultTool;
    old_x = d->do_CurrentX;
    old_y = d->do_CurrentY;
    if (tt)
        d->do_ToolTypes = tt;
    if (tool)
        d->do_DefaultTool = tool;
    d->do_CurrentX = d->do_CurrentY = NO_ICON_POSITION;
    if (ok)
        ok = put(path, d);
    d->do_ToolTypes = old_tt;
    d->do_DefaultTool = old_tool;
    d->do_CurrentX = old_x;
    d->do_CurrentY = old_y;
    FreeDiskObject(d);
    free(a.buf);
    return ok;
}

int main(int argc, char **argv)
{
    char path[256];
    const char *art = argc > 2 ? argv[2] : NULL;
    int ok = 1;

    if (argc < 2) {
        printf("usage: mkicons DIR [ART]\n");
        return 10;
    }
    /* colour icons need icon.library 44 (OS 3.5) */
    if (!(IconBase = OpenLibrary((CONST_STRPTR)"icon.library", art ? 44 : 36))) {
        printf("mkicons: no icon.library%s\n", art ? " 44 or newer" : "");
        return 20;
    }
    snprintf(path, sizeof(path), "%s/Doom3", argv[1]);
    ok &= make(WBTOOL, path, art, "game", (STRPTR *)tooltypes, NULL);
    snprintf(path, sizeof(path), "%s/ReadMe", argv[1]);
    ok &= make(WBPROJECT, path, art, "readme", NULL, (STRPTR)"SYS:Utilities/MultiView");
    ok &= make(WBDRAWER, argv[1], art, "drawer", NULL, NULL);
    CloseLibrary(IconBase);
    return ok ? 0 : 10;
}
