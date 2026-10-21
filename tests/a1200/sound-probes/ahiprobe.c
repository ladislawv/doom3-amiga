/* AHI probe: lists the audio modes AHI has in memory, then plays a 441 Hz tone at
   amplitude AMP (16-bit stereo, 22050 Hz) for 3 s through AHI's low-level API with
   each audio ID given on the command line (hex), independent of ahi.prefs.
     ahiprobe [AMP] [ID ...]   e.g. ahiprobe 8000 0x2000d 0x3b0001 */
#include <exec/memory.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct Library *AHIBase;

static void ListModes(void) {
    ULONG id = AHI_INVALID_ID;
    while ((id = AHI_NextAudioID(id)) != AHI_INVALID_ID) {
        char name[80] = "", driver[40] = "";
        LONG hifi = 0, bits = 0, stereo = 0, maxch = 0;
        AHI_GetAudioAttrs(id, NULL,
            AHIDB_BufferLen, 80, AHIDB_Name, (ULONG)name,
            AHIDB_HiFi, (ULONG)&hifi, AHIDB_Bits, (ULONG)&bits,
            AHIDB_Stereo, (ULONG)&stereo, AHIDB_MaxChannels, (ULONG)&maxch, TAG_DONE);
        AHI_GetAudioAttrs(id, NULL, AHIDB_BufferLen, 40, AHIDB_Driver, (ULONG)driver, TAG_DONE);
        printf("  0x%08lx  %-40s driver %-10s hifi %ld bits %ld stereo %ld maxch %ld\n",
               (unsigned long)id, name, driver, (long)hifi, (long)bits, (long)stereo, (long)maxch);
    }
}

static void Play(ULONG id, int amp) {
    enum { FRAMES = 22050 };    /* 1 s, 441 Hz: 50 frames a period, so it loops seamlessly */
    struct AHIAudioCtrl *ctrl;
    struct AHISampleInfo si;
    WORD *buf;
    ULONG mixfreq = 0;
    int i;

    buf = (WORD *)AllocVec(FRAMES * 4, MEMF_PUBLIC);
    if (!buf) { printf("0x%08lx: no memory\n", (unsigned long)id); return; }
    for (i = 0; i < FRAMES; i++)
        buf[i * 2] = buf[i * 2 + 1] = (WORD)(amp * sin(2.0 * M_PI * i / 50.0));
    ctrl = AHI_AllocAudio(AHIA_AudioID, id, AHIA_MixFreq, 22050, AHIA_Channels, 1, AHIA_Sounds, 1, TAG_DONE);
    if (!ctrl) { printf("0x%08lx: AHI_AllocAudio failed\n", (unsigned long)id); FreeVec(buf); return; }
    AHI_ControlAudio(ctrl, AHIC_MixFreq_Query, (ULONG)&mixfreq, TAG_DONE);
    si.ahisi_Type = AHIST_S16S;
    si.ahisi_Address = buf;
    si.ahisi_Length = FRAMES;
    if (AHI_LoadSound(0, AHIST_SAMPLE, &si, ctrl) != AHIE_OK) {
        printf("0x%08lx: AHI_LoadSound failed\n", (unsigned long)id);
    } else if (AHI_ControlAudio(ctrl, AHIC_Play, TRUE, TAG_DONE) != AHIE_OK) {
        printf("0x%08lx: AHIC_Play failed\n", (unsigned long)id);
    } else {
        printf("playing through 0x%08lx (mixing at %lu Hz), amplitude %d, 3 s\n",
               (unsigned long)id, (unsigned long)mixfreq, amp);
        fflush(stdout);
        AHI_Play(ctrl, AHIP_BeginChannel, 0, AHIP_Freq, 22050, AHIP_Vol, 0x10000, AHIP_Pan, 0x8000,
                 AHIP_Sound, 0, AHIP_Offset, 0, AHIP_Length, 0, AHIP_EndChannel, 0, TAG_DONE);
        Delay(150);
        AHI_ControlAudio(ctrl, AHIC_Play, FALSE, TAG_DONE);
        Delay(50);  /* a second of silence between modes */
    }
    AHI_FreeAudio(ctrl);
    FreeVec(buf);
}

int main(int argc, char **argv) {
    struct MsgPort *port = CreateMsgPort();
    struct AHIRequest *req = port ? (struct AHIRequest *)CreateIORequest(port, sizeof(*req)) : NULL;
    int amp = argc > 1 ? atoi(argv[1]) : 8000, i;
    if (!req) return 20;
    req->ahir_Version = 4;
    if (OpenDevice((CONST_STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)req, 0) != 0) {
        printf("cannot open ahi.device\n");
        return 20;
    }
    AHIBase = (struct Library *)req->ahir_Std.io_Device;
    printf("ahi.device %d.%d; audio modes in memory:\n", AHIBase->lib_Version, AHIBase->lib_Revision);
    ListModes();
    for (i = 2; i < argc; i++)
        Play(strtoul(argv[i], NULL, 16), amp);
    CloseDevice((struct IORequest *)req);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    return 0;
}
