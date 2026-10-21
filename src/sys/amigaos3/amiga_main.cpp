/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
 * AmigaOS 3 (68k, libnix) system layer of the port (PLAN.md §8, phase 3), in
 * place of sys/posix and sys/linux. Window, input, timers and threads stay
 * SDL2's (sys/glimp.cpp, sys/events.cpp, sys/threads.cpp). The operating-system
 * calls are in amiga_os.c.
 *
 * Paths: the game's data, configuration and saves are all next to the
 * executable (base/, demo/, d3xp/; saves in save/), as on a typical Amiga.
 */

#include <SDL.h>

#include "sys/platform.h"
#include "idlib/containers/StrList.h"
#include "framework/Common.h"
#include "framework/CmdSystem.h"
#include "framework/CVarSystem.h"
#include "framework/FileSystem.h"
#include "framework/Licensee.h"
#include "sys/sys_local.h"

#include "sys/amigaos3/amiga_os.h"

#include <sys/stat.h>

// what Doom 3 needs: large alloca() calls in model and image code (dhewm3 uses
// 8 MB on Windows). Given by the Shell's Stack command or the icon.
static const unsigned long MIN_STACK = 4 * 1024 * 1024;
static const unsigned long WANT_STACK = 8 * 1024 * 1024;

static char programDir[MAX_OSPATH];
static char exePath[MAX_OSPATH];
static char savePath[MAX_OSPATH];
static FILE *consoleLog = NULL;

idCVar com_pid( "com_pid", "0", CVAR_INTEGER | CVAR_INIT | CVAR_SYSTEM, "process id" );

// joins an AmigaDOS directory and a name: "Work:" + "x" is "Work:x", "Work:a" + "x" is "Work:a/x"
static void JoinPath( char *out, int size, const char *dir, const char *name ) {
	size_t n = strlen( dir );
	if ( n == 0 ) {
		idStr::Copynz( out, name, size );
	} else if ( dir[n - 1] == ':' || dir[n - 1] == '/' ) {
		idStr::snPrintf( out, size, "%s%s", dir, name );
	} else {
		idStr::snPrintf( out, size, "%s/%s", dir, name );
	}
}

// ---------------------------------------------------------------------------
// output

void Sys_VPrintf( const char *msg, va_list arg ) {
	va_list arg2;
	va_copy( arg2, arg );
	vprintf( msg, arg2 );
	va_end( arg2 );
	if ( consoleLog ) {
		vfprintf( consoleLog, msg, arg );
		fflush( consoleLog );	// a crash should not lose the last lines
	}
}

void Sys_Printf( const char *msg, ... ) {
	va_list argptr;
	va_start( argptr, msg );
	Sys_VPrintf( msg, argptr );
	va_end( argptr );
}

void Sys_DebugPrintf( const char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	Sys_VPrintf( fmt, argptr );
	va_end( argptr );
}

void Sys_DebugVPrintf( const char *fmt, va_list arg ) {
	Sys_VPrintf( fmt, arg );
}

static void OpenLog( void ) {
	char logPath[MAX_OSPATH], oldPath[MAX_OSPATH];
	JoinPath( logPath, sizeof( logPath ), savePath, "dhewm3log.txt" );
	JoinPath( oldPath, sizeof( oldPath ), savePath, "dhewm3log-old.txt" );
	remove( oldPath );
	rename( logPath, oldPath );
	consoleLog = fopen( logPath, "w" );
}

/*
================
Sys_Error

The message goes to the console, the log and a requester (the game may be on
a screen of its own, with no Shell in sight).
================
*/
void Sys_Error( const char *error, ... ) {
	char text[1024];
	va_list argptr;
	va_start( argptr, error );
	idStr::vsnPrintf( text, sizeof( text ), error, argptr );
	va_end( argptr );
	Sys_Printf( "Sys_Error: %s\n", text );
	if ( consoleLog ) {
		fclose( consoleLog );
		consoleLog = NULL;
	}
	SDL_Quit();	// closes the game's screen first, so the requester can be seen
	amiga_message( GAME_NAME, text );
	exit( EXIT_FAILURE );
}

void Sys_Quit( void ) {
	// the static objects' destructors run next, in reverse construction order
	// (src/amiga/cxa_atexit.c); libnix closes the log with the other files
	exit( EXIT_SUCCESS );
}

// ---------------------------------------------------------------------------
// paths and files

bool Sys_GetPath( sysPath_t type, idStr &path ) {
	path.Clear();
	switch ( type ) {
	case PATH_BASE:
		path = programDir;
		return true;
	case PATH_CONFIG:
	case PATH_SAVE:
		path = savePath;
		return true;
	case PATH_EXE:
		path = exePath;
		return exePath[0] != '\0';
	}
	return false;
}

void Sys_Mkdir( const char *path ) {
	amiga_mkdir( path );
}

struct listState_t {
	idStrList *	list;
	const char *extension;
	bool		dirOnly;
};

static void ListOne( const char *name, int isDir, void *user ) {
	listState_t *s = (listState_t *)user;
	if ( s->dirOnly ) {
		if ( isDir ) {
			s->list->Append( name );
		}
		return;
	}
	if ( isDir ) {
		return;
	}
	if ( s->extension[0] != '\0' ) {
		idStr ext;
		idStr( name ).ExtractFileExtension( ext );
		if ( ext.Icmp( &s->extension[1] ) != 0 ) {	// extension is ".pk4" and the like
			return;
		}
	}
	s->list->Append( name );
}

int Sys_ListFiles( const char *directory, const char *extension, idStrList &list ) {
	listState_t s;
	list.Clear();
	if ( !extension ) {
		extension = "";
	}
	s.list = &list;
	s.dirOnly = ( extension[0] == '/' && extension[1] == 0 );	// a slash asks for directories
	s.extension = s.dirOnly ? "" : extension;
	if ( amiga_list_dir( directory, ListOne, &s ) < 0 ) {
		if ( cvarSystem->GetCVarBool( "fs_debug" ) ) {
			common->Printf( "Sys_ListFiles: cannot read %s\n", directory );
		}
		return -1;
	}
	if ( cvarSystem->GetCVarBool( "fs_debug" ) ) {
		common->Printf( "Sys_ListFiles: %d entries in %s\n", list.Num(), directory );
	}
	return list.Num();
}

ID_TIME_T Sys_FileTimeStamp( FILE *fp ) {
	struct stat st;
	if ( fp && fstat( fileno( fp ), &st ) == 0 ) {
		return st.st_mtime;
	}
	return 0;
}

int Sys_GetDriveFreeSpace( const char *path ) {
	return 1000 * 1024;	// MB; only the CD-key and download code asks
}

// ---------------------------------------------------------------------------
// time (the game frequency changes of patch 0010 made Sys_Milliseconds inline
// on top of this)

/*
=======================
Sys_MillisecondsPrecise

From Emu68's free-running counter (amiga_os.c: 1.7 us a read on the A1200),
else from SDL's performance counter (timer.device's ReadEClock, 19.7 us), counted
from the first call (set back 1.5 ms so neither this nor Sys_Milliseconds()
ever returns 0). sys_clockReads counts the calls (the benchmark reports them).
=======================
*/
unsigned int sys_clockReads;

double Sys_MillisecondsPrecise( void ) {
	static int useCounter = -1;
	static Uint64 first = 0;
	static double msPerCount = 0.0;
	Uint64 now;
	sys_clockReads++;
	if ( useCounter < 0 ) {
		unsigned long hz = amiga_counter_hz();
		useCounter = hz != 0;
		msPerCount = 1000.0 / ( useCounter ? (double)hz : (double)SDL_GetPerformanceFrequency() );
	}
	if ( useCounter ) {
		unsigned long hi, lo = amiga_counter_read( &hi );
		now = ( (Uint64)hi << 32 ) | lo;
	} else {
		now = SDL_GetPerformanceCounter();
	}
	if ( first == 0 ) {
		first = now - (Uint64)( 1.5 / msPerCount );
	}
	return (double)( now - first ) * msPerCount;
}

// only the async thread uses it, which this port does not have (single task)
void Sys_SleepUntilPrecise( double targetTimeMS ) {
	double msToWait = targetTimeMS - Sys_MillisecondsPrecise();
	if ( msToWait >= 1.0 ) {
		SDL_Delay( (Uint32)msToWait );
	}
}

// ---------------------------------------------------------------------------
// system

void Sys_Init( void ) {
	com_pid.SetInteger( 0 );
	common->Printf( "%d MB System Memory (%d MB free)\n", Sys_GetSystemRam(), (int)amiga_fast_mb( 0 ) );
	common->Printf( "stack: %lu bytes\n", amiga_stack_size() );
}

void Sys_Shutdown( void ) {
}

int Sys_GetSystemRam( void ) {
	return (int)amiga_fast_mb( 1 );
}

bool Sys_LockMemory( void *ptr, int bytes ) {
	return true;
}

bool Sys_UnlockMemory( void *ptr, int bytes ) {
	return true;
}

void Sys_SetPhysicalWorkMemory( int minBytes, int maxBytes ) {
}

void Sys_ShowConsole( int visLevel, bool quitOnClose ) {
}

char *Sys_ConsoleInput( void ) {
	return NULL;	// no terminal input: the game's own console (~) works
}

char *Sys_GetClipboardData( void ) {
	return SDL_GetClipboardText();
}

void Sys_FreeClipboardData( char *data ) {
	SDL_free( data );
}

void Sys_SetClipboardData( const char *string ) {
	SDL_SetClipboardText( string );
}

// the game code is linked into the executable (HARDLINK_GAME): no libraries to load
uintptr_t Sys_DLL_Load( const char *path ) {
	return 0;
}

void *Sys_DLL_GetProcAddress( uintptr_t handle, const char *sym ) {
	return NULL;
}

void Sys_DLL_Unload( uintptr_t handle ) {
}

void Sys_DoStartProcess( const char *exeName, bool dofork ) {
	common->Printf( "Sys_DoStartProcess: not on this platform (%s)\n", exeName );
}

void idSysLocal::StartProcess( const char *exeName, bool quit ) {
	common->Printf( "StartProcess: not on this platform (%s)\n", exeName );
	if ( quit ) {
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
	}
}

void idSysLocal::OpenURL( const char *url, bool quit ) {
	common->Printf( "Open URL: %s (not on this platform)\n", url );
	if ( quit ) {
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
	}
}

// ---------------------------------------------------------------------------

int main( int argc, char **argv ) {
	setvbuf( stdout, NULL, _IOLBF, 0 );

	if ( !amiga_program_dir( programDir, sizeof( programDir ) ) ) {
		programDir[0] = '\0';
	}
	char name[256];
	if ( amiga_program_name( name, sizeof( name ) ) ) {
		JoinPath( exePath, sizeof( exePath ), programDir, name );
	}
	JoinPath( savePath, sizeof( savePath ), programDir, "save" );
	amiga_mkdir( savePath );
	OpenLog();

	unsigned long stack = amiga_stack_size();
	if ( stack == 0 ) {
		// the task's stack bounds are not filled in (the vamos emulator): unknown, go on
		Sys_Printf( "stack size unknown\n" );
	} else if ( stack < MIN_STACK ) {
		char text[256];
		idStr::snPrintf( text, sizeof( text ),
			"The stack is %lu bytes; Doom 3 needs at least %lu (%lu recommended).\n"
			"Start it from a Shell after \"Stack %lu\", or set the stack in its icon.",
			stack, MIN_STACK, WANT_STACK, WANT_STACK + 1024 * 1024 );
		Sys_Printf( "%s\n", text );
		amiga_message( GAME_NAME, text );
		return 20;
	}

	// common->Init calls Sys_Init
	if ( argc > 1 ) {
		common->Init( argc - 1, &argv[1] );
	} else {
		common->Init( 0, NULL );
	}

	while ( 1 ) {
		common->Frame();
	}
	return 0;
}
