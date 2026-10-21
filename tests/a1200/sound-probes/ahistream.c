/* AHI device-interface streamer, no SDL: a 441 Hz tone at AMP, 16-bit stereo at
   22050 Hz, written with CMD_WRITE in double-buffered blocks of FRAMES (ahir_Link),
   as AHI's own example does it, to UNIT for 3 s.
     ahistream UNIT AMP FRAMES */
#include <exec/memory.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char **argv) {
    int unit = argc > 1 ? atoi(argv[1]) : 0, amp = argc > 2 ? atoi(argv[2]) : 8000;
    int frames = argc > 3 ? atoi(argv[3]) : 1024, blocks = 3 * 22050 / frames, b, i;
    /* SDL2_68k's driver's ways, one at a time: request priority, task priority,
       and its order of waits (WaitIO on the other request right after SendIO) */
    int lnpri = argc > 4 ? atoi(argv[4]) : 0, taskpri = argc > 5 ? atoi(argv[5]) : 0;
    int sdlwait = argc > 6 ? atoi(argv[6]) : 0;
    struct MsgPort *port = CreateMsgPort();
    struct AHIRequest *req[2] = { NULL, NULL }, *link = NULL;
    WORD *buf[2];
    unsigned long pos = 0;

    req[0] = (struct AHIRequest *)CreateIORequest(port, sizeof(struct AHIRequest));
    req[0]->ahir_Version = 4;
    if (OpenDevice((CONST_STRPTR)AHINAME, unit, (struct IORequest *)req[0], 0) != 0) {
        printf("unit %d: OpenDevice failed\n", unit);
        return 20;
    }
    req[1] = (struct AHIRequest *)AllocVec(sizeof(struct AHIRequest), MEMF_PUBLIC);
    CopyMem(req[0], req[1], sizeof(struct AHIRequest));
    buf[0] = (WORD *)AllocVec(frames * 4, MEMF_PUBLIC);
    buf[1] = (WORD *)AllocVec(frames * 4, MEMF_PUBLIC);
    printf("unit %d, amplitude %d, %d frames a block, %d blocks, ln_Pri %d, task pri %d, SDL's waits %d\n",
           unit, amp, frames, blocks, lnpri, taskpri, sdlwait);
    fflush(stdout);
    SetTaskPri(FindTask(NULL), taskpri);
    for (b = 0; b < blocks; b++) {
        struct AHIRequest *r = req[b & 1];
        WORD *d = buf[b & 1];
        if (b >= 2)
            WaitIO((struct IORequest *)r);  /* this buffer's previous write is done */
        for (i = 0; i < frames; i++, pos++)
            d[i * 2] = d[i * 2 + 1] = (WORD)(amp * sin(2.0 * M_PI * (pos % 50) / 50.0));
        r->ahir_Std.io_Message.mn_Node.ln_Pri = lnpri;
        r->ahir_Std.io_Command = CMD_WRITE;
        r->ahir_Std.io_Data = d;
        r->ahir_Std.io_Length = frames * 4;
        r->ahir_Std.io_Offset = 0;
        r->ahir_Type = AHIST_S16S;
        r->ahir_Frequency = 22050;
        r->ahir_Volume = 0x10000;
        r->ahir_Position = 0x8000;
        r->ahir_Link = link;
        SendIO((struct IORequest *)r);
        link = r;
        if (sdlwait && b >= 1) {
            /* SDL's WaitDevice: CheckIO, then WaitIO, on the other buffer's request */
            struct AHIRequest *o = req[(b + 1) & 1];
            if (!CheckIO((struct IORequest *)o))
                WaitIO((struct IORequest *)o);
        }
    }
    SetTaskPri(FindTask(NULL), 0);
    WaitIO((struct IORequest *)req[(blocks - 2) & 1]);
    WaitIO((struct IORequest *)req[(blocks - 1) & 1]);
    CloseDevice((struct IORequest *)req[0]);
    printf("done\n");
    return 0;
}
