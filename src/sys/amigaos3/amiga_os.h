/*
 * AmigaOS 3 port: the few operating-system calls the system layer needs,
 * behind a plain C interface (amiga_os.c), so that the AmigaOS headers (whose
 * macros such as Read, Write and Printf collide with Doom 3's names) are
 * included in that one file only. GPLv3, as the rest of the port.
 */
#ifndef AMIGA_OS_H
#define AMIGA_OS_H

#ifdef __cplusplus
extern "C" {
#endif

/* bytes of stack the running task has */
unsigned long amiga_stack_size(void);
/* free and total fast memory, in MB */
unsigned long amiga_fast_mb(int total);
/* the program's directory as an absolute path ("Work:Games/Doom3"); 0 on failure */
int amiga_program_dir(char *buf, int size);
/* the program's file name; 0 on failure */
int amiga_program_name(char *buf, int size);
/* 1 if `path` is an existing directory (dir 1) or file (dir 0) */
int amiga_exists(const char *path, int dir);
/* creates a directory; 1 when it exists afterwards */
int amiga_mkdir(const char *path);
/* calls `found` for each entry of `dir`; -1 if it cannot be read, else the count */
int amiga_list_dir(const char *dir, void (*found)(const char *name, int isdir, void *user), void *user);
/* 1 when a 32-bit (4 bytes a pixel) RTG screen mode of exactly w x h exists (as
   SDL's OS3_Find32BitMode picks one), 0 when not, -1 without cybergraphics.library */
int amiga_rtg_mode_exists(int w, int h);
/* opens the program's own libs/ogles2.library by its path and keeps it open to
   the end, so that every later open by name (SDL, OGLES2Open, LIBS:) gets this
   one; *idstring gets its version string. 1 when the drawer's copy is the one
   in memory, 0 when another ogles2.library already was (another program uses
   it) or the drawer has none, -1 when none could be opened */
int amiga_open_own_ogles2(char *idstring, int size);
/* free room on the volume holding `path`, in MB; -1 if unknown */
long amiga_free_mb(const char *path);
/* the modification date of a file in seconds since 1970; 0 if unknown */
long amiga_file_time(const char *path);
/* a requester with the message (for fatal errors when no console may be visible) */
void amiga_message(const char *title, const char *text);
/* Emu68's free-running counter: its rate in Hz, 0 when not under Emu68 */
unsigned long amiga_counter_hz(void);
/* the counter's value (low 32 bits; *hi the high ones); only after amiga_counter_hz() gave non-zero */
unsigned long amiga_counter_read(unsigned long *hi);
/* sets a global environment variable (ENV:name), e.g. for a program watching this one */
void amiga_set_env(const char *name, const char *value);

#ifdef __cplusplus
}
#endif

#endif
