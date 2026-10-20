// cxx_probe: what the Doom 3 port needs from GCC 16.2, libnix and SDL2 on the
// A1200 (PLAN.md §8, phase 2). Prints one line per check, "ok" or "FAIL",
// and timings; exit code 0 when every check passed.
//   cxx_probe [--file <big file to read>] [--nogl] [--noaudio]
#include <exec/tasks.h>
#include <proto/exec.h>
#include <SDL.h>
#include <alloca.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <typeinfo>
#include <vector>

// libnix's swapstack.o gives main() a stack of this size (Doom 3 makes large
// alloca() calls; dhewm3 uses 8 MB on Windows).
extern "C" { unsigned long __stack = 8 * 1024 * 1024; }

static int failures = 0;

static void check(const char *what, bool ok, const char *detail = "") {
	printf("%-44s %s %s\n", what, ok ? "ok  " : "FAIL", detail);
	if ( !ok ) {
		failures++;
	}
}

// --- C++ runtime ---------------------------------------------------------

static int constructed = 0;
struct Global {
	Global() { constructed = 42; }
};
static Global global;	// static constructors must run before main

static int destroyed = 0;
struct Guard {
	~Guard() { destroyed++; }
};

struct ProbeError {
	int code;
};

static void throw3( int depth ) {
	Guard g;
	if ( depth == 0 ) {
		throw ProbeError{ 7 };
	}
	throw3( depth - 1 );
}

struct Base { virtual ~Base() {} };
struct Derived : Base { int x = 5; };

static void cxx_checks() {
	check( "static constructor ran", constructed == 42 );

	int code = 0;
	try {
		throw3( 2 );
	} catch ( const ProbeError &e ) {
		code = e.code;
	}
	check( "exception through 3 frames", code == 7 );
	check( "destructors ran while unwinding", destroyed == 3 );

	Base *b = new Derived;
	Derived *d = dynamic_cast<Derived *>( b );
	check( "dynamic_cast and typeid", d && d->x == 5 && typeid( *b ) == typeid( Derived ) );
	delete b;

	std::string s = "doom";
	s += "3";
	std::vector<int> v( 1000, 3 );
	check( "std::string and std::vector", s == "doom3" && v.size() == 1000 && v[999] == 3 );
}

// --- CPU and FPU ---------------------------------------------------------

static void cpu_checks() {
	volatile float f1 = 255.5f, f2 = -1.5f, f3 = 0.999f;
	check( "float to int truncates", (int)f1 == 255 && (int)f2 == -1 && (int)f3 == 0 );
	volatile float x = 2.0f;
	check( "sqrtf, sinf, atan2f", fabsf( sqrtf( x ) - 1.41421356f ) < 1e-6f
		&& fabsf( sinf( 0.5f ) - 0.47942554f ) < 1e-6f && fabsf( atan2f( 1.0f, 1.0f ) - 0.78539816f ) < 1e-6f );
	unsigned int one = 1;
	check( "big-endian", *(unsigned char *)&one == 0 );
	check( "sizes (int 4, long 4, pointer 4)", sizeof( int ) == 4 && sizeof( long ) == 4 && sizeof( void * ) == 4 );
}

// --- stack and memory ------------------------------------------------------

static void stack_checks() {
	struct Task *t = FindTask( NULL );
	unsigned long size = (unsigned long)t->tc_SPUpper - (unsigned long)t->tc_SPLower;
	char detail[64];
	snprintf( detail, sizeof( detail ), "%lu bytes", size );
	check( "stack at least 8 MB (__stack)", size >= 8 * 1024 * 1024 - 1024, detail );
	if ( size >= 2 * 1024 * 1024 ) {
		volatile char *p = (volatile char *)alloca( 1024 * 1024 );
		p[0] = 1;
		p[1024 * 1024 - 1] = 2;
		check( "alloca of 1 MB", p[0] + p[1024 * 1024 - 1] == 3 );
	} else {
		check( "alloca of 1 MB", false, "skipped: stack too small" );
	}
}

static void memory_checks() {
	const int n = 100000;
	std::vector<char *> blocks( n );
	Uint32 t0 = SDL_GetTicks();
	for ( int i = 0; i < n; i++ ) {
		blocks[i] = new char[16 + ( i % 7 ) * 24];
	}
	Uint32 t1 = SDL_GetTicks();
	for ( int i = 0; i < n; i += 2 ) {	// every other first: fragmented frees
		delete[] blocks[i];
	}
	for ( int i = 1; i < n; i += 2 ) {
		delete[] blocks[i];
	}
	Uint32 t2 = SDL_GetTicks();
	char detail[96];
	snprintf( detail, sizeof( detail ), "new %u ms, delete %u ms (100000 blocks)", (unsigned)( t1 - t0 ), (unsigned)( t2 - t1 ) );
	check( "new/delete of small blocks", true, detail );

	void *big = malloc( 256 * 1024 * 1024 );
	check( "malloc of 256 MB", big != NULL );
	free( big );
}

// --- files -------------------------------------------------------------------

static void file_checks( const char *path ) {
	if ( !path ) {
		printf( "%-44s skip (no --file)\n", "read a large file" );
		return;
	}
	FILE *f = fopen( path, "rb" );
	if ( !f ) {
		check( "read a large file", false, path );
		return;
	}
	static char buf[64 * 1024];
	Uint32 t0 = SDL_GetTicks();
	unsigned long total = 0;
	size_t got;
	while ( ( got = fread( buf, 1, sizeof( buf ), f ) ) > 0 ) {
		total += got;
	}
	Uint32 t1 = SDL_GetTicks();
	int seeks = 0;
	for ( unsigned long pos = 0; pos < total; pos += total / 64 + 1 ) {
		if ( fseek( f, (long)pos, SEEK_SET ) == 0 && fread( buf, 1, 4096, f ) > 0 ) {
			seeks++;
		}
	}
	Uint32 t2 = SDL_GetTicks();
	fclose( f );
	char detail[128];
	unsigned ms = ( t1 - t0 ) ? ( t1 - t0 ) : 1;
	snprintf( detail, sizeof( detail ), "%lu bytes in %u ms (%lu KB/s), %d seeks+reads in %u ms",
		total, (unsigned)( t1 - t0 ), total / ms * 1000 / 1024, seeks, (unsigned)( t2 - t1 ) );
	check( "read a large file in 64 KB pieces", total > 0, detail );
}

// --- SDL2: GL ES context and audio ---------------------------------------

typedef void ( *ClearColor_t )( float, float, float, float );
typedef void ( *Clear_t )( unsigned int );
typedef const unsigned char *( *GetString_t )( unsigned int );

static void gl_checks() {
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
	SDL_Window *w = SDL_CreateWindow( "cxx_probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		320, 240, SDL_WINDOW_OPENGL );
	if ( !w ) {
		check( "SDL GL ES window", false, SDL_GetError() );
		return;
	}
	SDL_GLContext ctx = SDL_GL_CreateContext( w );
	if ( !ctx ) {
		check( "SDL GL ES context", false, SDL_GetError() );
		SDL_DestroyWindow( w );
		return;
	}
	ClearColor_t clearColor = (ClearColor_t)SDL_GL_GetProcAddress( "glClearColor" );
	Clear_t clear = (Clear_t)SDL_GL_GetProcAddress( "glClear" );
	GetString_t getString = (GetString_t)SDL_GL_GetProcAddress( "glGetString" );
	int stencil = 0, depth = 0;
	SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &stencil );
	SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &depth );
	char detail[160];
	snprintf( detail, sizeof( detail ), "%s, depth %d, stencil %d",
		getString ? (const char *)getString( 0x1F01 /* GL_RENDERER */ ) : "?", depth, stencil );
	check( "SDL GL ES 2.0 context", clearColor && clear && getString, detail );
	if ( clearColor && clear ) {
		// visible: red, green, blue, a second each
		static const float colours[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
		static const char *names[3] = { "red", "green", "blue" };
		for ( int i = 0; i < 3; i++ ) {
			printf( "  window shows %s for a second\n", names[i] );
			fflush( stdout );
			clearColor( colours[i][0], colours[i][1], colours[i][2], 1 );
			clear( 0x4000 | 0x100 | 0x400 );	// colour, depth, stencil
			SDL_GL_SwapWindow( w );
			SDL_Delay( 1000 );
		}
	}
	SDL_GL_DeleteContext( ctx );
	SDL_DestroyWindow( w );
}

static volatile int audioCallbacks = 0;
static float phase = 0;

static void SDLCALL tone( void *, Uint8 *stream, int len ) {
	Sint16 *s = (Sint16 *)stream;
	for ( int i = 0; i < len / 4; i++ ) {	// stereo, 16 bits
		Sint16 v = (Sint16)( sinf( phase ) * 3000 );	// quiet
		s[2 * i] = s[2 * i + 1] = v;
		phase += 2 * 3.14159265f * 440 / 22050;
		if ( phase > 2 * 3.14159265f ) {
			phase -= 2 * 3.14159265f;
		}
	}
	audioCallbacks++;
}

static void audio_checks() {
	SDL_AudioSpec want, have;
	SDL_zero( want );
	want.freq = 22050;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = tone;
	SDL_AudioDeviceID dev = SDL_OpenAudioDevice( NULL, 0, &want, &have, 0 );
	if ( !dev ) {
		check( "SDL audio (a 440 Hz tone, 1 s)", false, SDL_GetError() );
		return;
	}
	printf( "  a quiet 440 Hz tone plays for a second\n" );
	fflush( stdout );
	SDL_PauseAudioDevice( dev, 0 );
	SDL_Delay( 1000 );
	SDL_PauseAudioDevice( dev, 1 );
	SDL_CloseAudioDevice( dev );
	char detail[96];
	snprintf( detail, sizeof( detail ), "%s, %d Hz, %d callbacks (about 21 expected)",
		SDL_GetCurrentAudioDriver(), have.freq, audioCallbacks );
	check( "SDL audio (a 440 Hz tone, 1 s)", audioCallbacks >= 15, detail );
}

int main( int argc, char **argv ) {
	const char *file = NULL;
	bool gl = true, audio = true;
	for ( int i = 1; i < argc; i++ ) {
		if ( !strcmp( argv[i], "--file" ) && i + 1 < argc ) {
			file = argv[++i];
		} else if ( !strcmp( argv[i], "--nogl" ) ) {
			gl = false;
		} else if ( !strcmp( argv[i], "--noaudio" ) ) {
			audio = false;
		}
	}
	printf( "cxx_probe: GCC %d.%d.%d, libnix\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );

	cxx_checks();
	cpu_checks();
	stack_checks();

	if ( SDL_Init( SDL_INIT_TIMER | ( gl ? SDL_INIT_VIDEO : 0 ) | ( audio ? SDL_INIT_AUDIO : 0 ) ) != 0 ) {
		check( "SDL_Init", false, SDL_GetError() );
		return 20;
	}
	memory_checks();
	file_checks( file );
	if ( gl ) {
		gl_checks();
	}
	if ( audio ) {
		audio_checks();
	}
	SDL_Quit();

	printf( "cxx_probe: %d failure(s)\n", failures );
	return failures ? 10 : 0;
}
