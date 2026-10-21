/*
 * OpenAL 1.1, the subset Doom 3 (d3es) calls, for the AmigaOS 3 port
 * (PLAN.md section 7, D7; phase 7). OpenAL Soft needs threads this C++
 * library lacks and does far more per sample than the 68k can spare; Doom 3's
 * sound system does the rest itself (which sounds play, their distance
 * attenuation: it sets AL_ROLLOFF_FACTOR 0 on every source, decoding, the
 * streaming of long sounds).
 *
 * What this does: buffers of 8 or 16-bit PCM (mono, stereo); sources with
 * gain, pitch, position (relative or not), looping, play/pause/stop/rewind
 * and queued buffers; a listener (gain, position, orientation). The mixer runs
 * in SDL2's audio callback (on AmigaOS AHI through SDL2_68k's driver; SDL's
 * default there is its Paula driver): every playing source is resampled
 * (linear; an average of two for exactly half rate) to the output rate,
 * panned by its direction from the listener (equal power; stereo buffers play
 * as they are), its gain ramped over each block (no clicks), and summed.
 * EFX and every other extension are reported absent (Doom 3 then turns its
 * reverb off). Distance models, rolloff, cones and Doppler are accepted and
 * ignored.
 *
 * Settings (environment variables, e.g. ENV:D3AL_VOICES):
 *   D3AL_VOICES   sources alGenSources gives (default 32; Doom 3 keeps 8 of them for stereo sounds)
 *   D3AL_RATE     output rate in Hz (default 22050)
 *   D3AL_SAMPLES  samples per audio callback (default 1024)
 *   D3AL_NOMIX    1: output silence and mix nothing (a measurement: the game's
 *                 sound work and scripts as with sound, without the mixer's)
 *   SDL_AUDIODRIVER  SDL's audio driver (default "ahi" on AmigaOS)
 *   (on AmigaOS read with GetVar, local then global: libnix's getenv sees only
 *   the shell's local variables (Set), not ENV: (SetEnv), and SDL2_68k's
 *   SDL_getenv only what SDL_setenv set; elsewhere the C library's getenv)
 *   D3AL_DUMP     a diagnostic: PREFIX.raw gets 30 s of the mixed output from
 *                 the first sound on (16-bit stereo, this machine's byte order) and
 *                 PREFIX.txt one line per alBufferData (format, rate, frames,
 *                 peak, mean level), both written when the device closes
 *
 * GPLv3, as the rest of the port.
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "SDL.h"

#ifdef __amigaos3__
#include <proto/dos.h>
#endif

#define AL_MAX_SOURCES	256
#define AL_MAX_QUEUE	16

typedef struct {
	int		used;
	short	*data;		/* frames * channels samples, native byte order */
	int		frames;
	int		channels;
	int		rate;
} alBuf_t;

typedef struct {
	int		used;
	ALenum	state;		/* AL_INITIAL, AL_PLAYING, AL_PAUSED, AL_STOPPED */
	float	gain, pitch;
	float	pos[3];
	int		relative, looping;
	float	refDist, maxDist, rolloff, minGain, maxGain;
	ALuint	queue[AL_MAX_QUEUE];	/* buffer names; queue[0] is AL_BUFFER for a static source */
	int		qlen;
	int		qcur;		/* the buffer playing */
	int		processed;	/* buffers done (AL_BUFFERS_PROCESSED) */
	int		isStatic;	/* attached with AL_BUFFER, not queued */
	unsigned int	frame;	/* position in the current buffer */
	unsigned int	frac;	/* and its fraction, 16 bits */
	int		curL, curR;	/* the gains the mixer used last, 0..32768 */
	int		started;	/* curL/curR valid: ramp from them */
} alSrc_t;

static struct {
	SDL_AudioDeviceID	dev;
	int		rate;
	int		maxSources;
	int		*acc;		/* the callback's sum, an int a sample (channels a frame) */
	int		accFrames;
	Uint64	mixTicks;	/* time in the callback, in performance counter ticks */
	Uint64	openTicks;	/* when the device opened */
	int		mixCalls;
	Uint64	busyTicks;	/* the same, for the callbacks that mixed a source */
	int		busyCalls;
	int		noMix;		/* D3AL_NOMIX: silence, nothing mixed (a measurement) */
	int		channels;	/* the device's: 2, or 1 (SDL2_68k's Paula driver) */
	int		sampleBytes;	/* 2 (AUDIO_S16SYS) or 1 (AUDIO_S8, Paula) */
	ALenum	error;
	ALCenum	alcError;
	float	gain;
	float	pos[3];
	float	at[3], up[3];
	alSrc_t	src[AL_MAX_SOURCES];
	alBuf_t	*buf;
	int		numBufs;	/* buffer names 1..numBufs */
	int		opened;		/* the device */
	int		context;
	/* D3AL_DUMP */
	char	dumpPrefix[256];
	Sint16	*dump;
	int		dumpFrames, dumpMax;
	char	*dumpLog;
	int		dumpLogLen, dumpLogMax;
} al;

static ALCdevice *const theDevice = (ALCdevice *)&al;
static ALCcontext *const theContext = (ALCcontext *)&al.context;

static void LockMixer( void ) {
	if ( al.dev ) {
		SDL_LockAudioDevice( al.dev );
	}
}

static void UnlockMixer( void ) {
	if ( al.dev ) {
		SDL_UnlockAudioDevice( al.dev );
	}
}

static void SetError( ALenum e ) {
	if ( al.error == AL_NO_ERROR ) {
		al.error = e;
	}
}

static alSrc_t *Src( ALuint name ) {
	if ( name == 0 || name > AL_MAX_SOURCES || !al.src[name - 1].used ) {
		SetError( AL_INVALID_NAME );
		return NULL;
	}
	return &al.src[name - 1];
}

static alBuf_t *Buf( ALuint name ) {
	if ( name == 0 || (int)name > al.numBufs || !al.buf[name - 1].used ) {
		return NULL;
	}
	return &al.buf[name - 1];
}

/* a setting from the environment (the header's comment), or NULL; the value
   stays valid until the fourth call after this one */
static const char *Env( const char *name ) {
#ifdef __amigaos3__
	static char vals[4][256];
	static int next;
	char *v = vals[next++ & 3];
	return GetVar( (CONST_STRPTR)name, (STRPTR)v, sizeof( vals[0] ), 0 ) >= 0 ? v : NULL;
#else
	return getenv( name );
#endif
}

static int EnvInt( const char *name, int def, int lo, int hi ) {
	const char *s = Env( name );
	int v;
	if ( !s || !*s ) {
		return def;
	}
	v = atoi( s );
	return v < lo ? lo : v > hi ? hi : v;
}

/*
==============================================================
the mixer (SDL's audio callback; the device is locked while it runs)
==============================================================
*/

/* the gains of a source for both channels, 0..32768 */
static void SourceGains( const alSrc_t *s, int channels, int *gl, int *gr ) {
	float g = s->gain * al.gain;
	float l, r;
	if ( g < s->minGain ) {
		g = s->minGain;
	}
	if ( g > s->maxGain ) {
		g = s->maxGain;
	}
	if ( g > 1.0f ) {
		g = 1.0f;
	}
	if ( g <= 0.0f ) {
		*gl = *gr = 0;
		return;
	}
	if ( al.channels == 1 ) {
		/* a mono device: no panning */
		*gl = *gr = (int)( g * 32768.0f );
		return;
	}
	if ( channels == 2 ) {
		l = r = g;
	} else {
		/* the direction in the listener's space: its right is at x up */
		float d[3], right[3], len, p;
		if ( s->relative ) {
			d[0] = s->pos[0]; d[1] = s->pos[1]; d[2] = s->pos[2];
			/* relative to a listener looking down -z with y up */
			right[0] = 1.0f; right[1] = 0.0f; right[2] = 0.0f;
		} else {
			d[0] = s->pos[0] - al.pos[0]; d[1] = s->pos[1] - al.pos[1]; d[2] = s->pos[2] - al.pos[2];
			right[0] = al.at[1] * al.up[2] - al.at[2] * al.up[1];
			right[1] = al.at[2] * al.up[0] - al.at[0] * al.up[2];
			right[2] = al.at[0] * al.up[1] - al.at[1] * al.up[0];
			len = sqrtf( right[0] * right[0] + right[1] * right[1] + right[2] * right[2] );
			if ( len > 1e-6f ) {
				right[0] /= len; right[1] /= len; right[2] /= len;
			}
		}
		len = sqrtf( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		p = len > 1e-3f ? ( d[0] * right[0] + d[1] * right[1] + d[2] * right[2] ) / len : 0.0f;
		if ( p < -1.0f ) {
			p = -1.0f;
		}
		if ( p > 1.0f ) {
			p = 1.0f;
		}
		/* equal power: a source straight ahead plays at 0.707 on each side, as OpenAL Soft's stereo */
		l = g * sqrtf( 0.5f * ( 1.0f - p ) );
		r = g * sqrtf( 0.5f * ( 1.0f + p ) );
	}
	*gl = (int)( l * 32768.0f );
	*gr = (int)( r * 32768.0f );
}

/* the buffer after the current one ended: 0 if the source stops */
static alBuf_t *NextBuffer( alSrc_t *s ) {
	if ( s->isStatic ) {
		if ( s->looping ) {
			return Buf( s->queue[0] );
		}
		s->processed = 1;
		return NULL;
	}
	s->qcur++;
	if ( s->qcur >= s->qlen ) {
		if ( s->looping && s->qlen > 0 ) {
			s->qcur = 0;
			s->processed = 0;
			return Buf( s->queue[0] );
		}
		s->processed = s->qlen;
		return NULL;
	}
	s->processed = s->qcur;
	return Buf( s->queue[s->qcur] );
}

static void MixSource( alSrc_t *s, int *acc, int frames ) {
	alBuf_t *b = s->qlen > 0 ? Buf( s->queue[s->qcur] ) : NULL;
	int gl, gr, l, r, dl, dr, i, ch;
	unsigned int step, frame = s->frame, frac = s->frac;
	float fstep;

	if ( !b ) {
		s->state = AL_STOPPED;
		return;
	}
	ch = b->channels;
	SourceGains( s, ch, &gl, &gr );
	if ( !s->started ) {
		s->curL = gl;
		s->curR = gr;
		s->started = 1;
	}
	/* ramp from the last block's gains to these over this block (fixed point, 15 bits of fraction) */
	l = s->curL << 15;
	r = s->curR << 15;
	dl = ( ( gl - s->curL ) << 15 ) / frames;
	dr = ( ( gr - s->curR ) << 15 ) / frames;
	s->curL = gl;
	s->curR = gr;

	fstep = (float)b->rate * ( s->pitch > 0.0f ? s->pitch : 1.0f ) / (float)al.rate;
	if ( fstep > 16.0f ) {
		fstep = 16.0f;
	}
	step = (unsigned int)( fstep * 65536.0f + 0.5f );

	for ( i = 0; i < frames; i++ ) {
		int sl, sr;
		while ( frame >= (unsigned int)b->frames ) {
			frame -= b->frames;
			b = b->frames > 0 ? NextBuffer( s ) : NULL;	/* an empty buffer would loop for ever */
			if ( !b ) {
				s->state = AL_STOPPED;
				s->frame = 0;
				s->frac = 0;
				s->started = 0;
				return;
			}
			ch = b->channels;
		}
		{
			const short *d = b->data + frame * ch;
			int last = frame + 1 >= (unsigned int)b->frames;
			if ( step == 0x20000 && frac == 0 ) {
				/* exactly half rate (44 kHz sounds at 22 kHz): the average of two */
				if ( ch == 1 ) {
					sl = last ? d[0] : ( d[0] + d[1] ) >> 1;
					sr = sl;
				} else {
					sl = last ? d[0] : ( d[0] + d[2] ) >> 1;
					sr = last ? d[1] : ( d[1] + d[3] ) >> 1;
				}
			} else if ( frac == 0 && ( step & 0xffff ) == 0 ) {
				sl = d[0];
				sr = ch == 1 ? sl : d[1];
			} else {
				/* linear */
				if ( ch == 1 ) {
					int s1 = last ? d[0] : d[1];
					sl = d[0] + ( ( ( s1 - d[0] ) * (int)( frac >> 2 ) ) >> 14 );
					sr = sl;
				} else {
					int s1 = last ? d[0] : d[2];
					int s2 = last ? d[1] : d[3];
					sl = d[0] + ( ( ( s1 - d[0] ) * (int)( frac >> 2 ) ) >> 14 );
					sr = d[1] + ( ( ( s2 - d[1] ) * (int)( frac >> 2 ) ) >> 14 );
				}
			}
		}
		if ( al.channels == 1 ) {
			acc[0] += ( ( ( sl + sr ) >> 1 ) * ( l >> 15 ) ) >> 15;
			acc += 1;
		} else {
			acc[0] += ( sl * ( l >> 15 ) ) >> 15;
			acc[1] += ( sr * ( r >> 15 ) ) >> 15;
			acc += 2;
		}
		l += dl;
		r += dr;
		frac += step;
		frame += frac >> 16;
		frac &= 0xffff;
	}
	s->frame = frame;
	s->frac = frac;
}

static void SDLCALL MixCallback( void *user, Uint8 *stream, int len ) {
	const int ch = al.channels;
	int frames = len / ( ch * al.sampleBytes ), i, n, k, busy = 0;
	Uint64 t0 = SDL_GetPerformanceCounter(), dt;
	(void)user;

	if ( al.noMix ) {
		memset( stream, 0, len );
		return;
	}
	while ( frames > 0 ) {
		n = frames < al.accFrames ? frames : al.accFrames;
		k = n * ch;
		memset( al.acc, 0, k * sizeof( int ) );
		for ( i = 0; i < al.maxSources; i++ ) {
			if ( al.src[i].used && al.src[i].state == AL_PLAYING ) {
				MixSource( &al.src[i], al.acc, n );
				busy = 1;
			}
		}
		/* the device's format, so that SDL converts nothing (Paula: 8-bit mono) */
		if ( al.sampleBytes == 1 ) {
			Sint8 *out = (Sint8 *)stream;
			for ( i = 0; i < k; i++ ) {
				int v = al.acc[i];
				out[i] = (Sint8)( ( v > 32767 ? 32767 : v < -32768 ? -32768 : v ) >> 8 );
			}
		} else {
			Sint16 *out = (Sint16 *)stream;
			for ( i = 0; i < k; i++ ) {
				int v = al.acc[i];
				out[i] = (Sint16)( v > 32767 ? 32767 : v < -32768 ? -32768 : v );
			}
		}
		if ( al.dump ) {
			/* the window starts with the first sound, not at the device's
			   opening: the A1200 spends minutes loading the map in silence */
			for ( i = 0; al.dumpFrames == 0 && i < k && al.acc[i] == 0; i++ ) {
			}
			if ( ( al.dumpFrames > 0 || i < k ) && al.dumpFrames < al.dumpMax ) {
				int m = al.dumpMax - al.dumpFrames < n ? al.dumpMax - al.dumpFrames : n;
				Sint16 *d = al.dump + al.dumpFrames * ch;
				for ( i = 0; i < m * ch; i++ ) {
					int v = al.acc[i];
					d[i] = (Sint16)( v > 32767 ? 32767 : v < -32768 ? -32768 : v );
				}
				al.dumpFrames += m;
			}
		}
		stream += k * al.sampleBytes;
		frames -= n;
	}
	dt = SDL_GetPerformanceCounter() - t0;
	al.mixTicks += dt;
	al.mixCalls++;
	if ( busy ) {
		al.busyTicks += dt;
		al.busyCalls++;
	}
}

/*
==============================================================
ALC
==============================================================
*/

ALC_API ALCdevice *ALC_APIENTRY alcOpenDevice( const ALCchar *devicename ) {
	SDL_AudioSpec want, have;
	const char *driver;
	(void)devicename;

	if ( al.opened ) {
		al.alcError = ALC_INVALID_VALUE;
		return NULL;
	}
	memset( &al, 0, sizeof( al ) );
	al.gain = 1.0f;
	al.at[2] = -1.0f;
	al.up[1] = 1.0f;
	al.maxSources = EnvInt( "D3AL_VOICES", 32, 1, AL_MAX_SOURCES );
	al.noMix = EnvInt( "D3AL_NOMIX", 0, 0, 1 );

	driver = Env( "SDL_AUDIODRIVER" );
#ifdef __amigaos3__
	if ( !driver || !*driver ) {
		driver = "ahi";	/* SDL2_68k's default is its Paula driver */
	}
#endif
	if ( SDL_WasInit( SDL_INIT_AUDIO ) ) {
		SDL_AudioQuit();
	}
	if ( SDL_AudioInit( driver ) < 0 ) {
		al.alcError = ALC_INVALID_DEVICE;
		return NULL;
	}

	SDL_zero( want );
	want.freq = EnvInt( "D3AL_RATE", 22050, 8000, 48000 );
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = (Uint16)EnvInt( "D3AL_SAMPLES", 1024, 256, 8192 );
	want.callback = MixCallback;
	/* the device's own format and channels if the mixer writes them (16 or
	   8-bit, stereo or mono): SDL's conversion (through float, per sample)
	   costs the 68k more than the mixing; SDL2_68k's Paula driver is 8-bit mono */
	al.dev = SDL_OpenAudioDevice( NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE );
	if ( al.dev != 0 && ( ( have.format != AUDIO_S16SYS && have.format != AUDIO_S8 ) || have.channels < 1 || have.channels > 2 ) ) {
		SDL_CloseAudioDevice( al.dev );
		al.dev = SDL_OpenAudioDevice( NULL, 0, &want, &have, 0 );
	}
	if ( al.dev == 0 ) {
		SDL_AudioQuit();
		al.alcError = ALC_INVALID_DEVICE;
		return NULL;
	}
	al.rate = have.freq;
	al.channels = have.channels;
	al.sampleBytes = have.format == AUDIO_S8 ? 1 : 2;
	al.accFrames = have.samples > 0 ? have.samples : 1024;
	al.acc = (int *)malloc( al.accFrames * 2 * sizeof( int ) );
	if ( !al.acc ) {
		SDL_CloseAudioDevice( al.dev );
		SDL_AudioQuit();
		al.dev = 0;
		al.alcError = ALC_OUT_OF_MEMORY;
		return NULL;
	}
	al.opened = 1;
	al.openTicks = SDL_GetPerformanceCounter();
	{
		const char *dump = Env( "D3AL_DUMP" );
		if ( dump && *dump && strlen( dump ) < sizeof( al.dumpPrefix ) - 8 ) {
			strcpy( al.dumpPrefix, dump );
			al.dumpMax = al.rate * 30;
			al.dump = (Sint16 *)malloc( al.dumpMax * 2 * sizeof( Sint16 ) );
			al.dumpLogMax = 256 * 1024;
			al.dumpLog = (char *)malloc( al.dumpLogMax );
			if ( !al.dump || !al.dumpLog ) {
				free( al.dump );
				free( al.dumpLog );
				al.dump = NULL;
				al.dumpLog = NULL;
			}
		}
	}
	printf( "OpenAL subset: SDL audio driver %s, %d Hz, %d channels, %d frames a block, format 0x%x, %d voices%s%s\n",
		SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?", have.freq, have.channels, have.samples,
		have.format, al.maxSources, al.dump ? ", output dumped to " : "", al.dump ? al.dumpPrefix : "" );
	SDL_PauseAudioDevice( al.dev, 0 );
	return theDevice;
}

/* D3AL_DUMP: the output and the buffer log written out (the device is closed: no callback runs) */
static void WriteDump( void ) {
	char path[300];
	FILE *f;
	if ( !al.dump ) {
		return;
	}
	SDL_snprintf( path, sizeof( path ), "%s.raw", al.dumpPrefix );
	f = fopen( path, "wb" );
	if ( f ) {
		fwrite( al.dump, sizeof( Sint16 ) * al.channels, al.dumpFrames, f );
		fclose( f );
	}
	printf( "OpenAL subset: %d frames of output %s %s\n", al.dumpFrames, f ? "written to" : "NOT written to", path );
	SDL_snprintf( path, sizeof( path ), "%s.txt", al.dumpPrefix );
	f = fopen( path, "wb" );
	if ( f ) {
		fprintf( f, "rate %d, %d channels, %d frames of output, %d sources\n", al.rate, al.channels, al.dumpFrames, al.maxSources );
		fwrite( al.dumpLog, 1, al.dumpLogLen, f );
		fclose( f );
	}
	free( al.dump );
	free( al.dumpLog );
	al.dump = NULL;
	al.dumpLog = NULL;
}

static void LogBuffer( ALuint name, int channels, int rate, const short *d, int frames ) {
	int i, peak = 0, n = frames * channels;
	double sum = 0.0;
	if ( !al.dumpLog || al.dumpLogLen > al.dumpLogMax - 200 ) {
		return;
	}
	for ( i = 0; i < n; i++ ) {
		int v = d[i] < 0 ? -d[i] : d[i];
		if ( v > peak ) {
			peak = v;
		}
		sum += v;
	}
	al.dumpLogLen += SDL_snprintf( al.dumpLog + al.dumpLogLen, al.dumpLogMax - al.dumpLogLen,
		"buffer %u: %d ch, %d Hz, %d frames, peak %d, mean %d, first %d %d %d %d\n", name, channels, rate, frames, peak,
		n ? (int)( sum / n ) : 0, n > 0 ? d[0] : 0, n > 1 ? d[1] : 0, n > 2 ? d[2] : 0, n > 3 ? d[3] : 0 );
}

ALC_API ALCboolean ALC_APIENTRY alcCloseDevice( ALCdevice *device ) {
	int i;
	if ( device != theDevice || !al.opened ) {
		al.alcError = ALC_INVALID_DEVICE;
		return ALC_FALSE;
	}
	SDL_CloseAudioDevice( al.dev );
	SDL_AudioQuit();
	al.dev = 0;
	{
		/* the mixer's cost: its share of the time the device was open */
		double f = (double)SDL_GetPerformanceFrequency();
		double open = ( SDL_GetPerformanceCounter() - al.openTicks ) / f * 1000.0;
		double mix = al.mixTicks / f * 1000.0;
		double busy = al.busyTicks / f * 1000.0, block = 1000.0 * al.accFrames / al.rate;
		printf( "OpenAL subset: mixing took %.0f ms in %d callbacks, %.1f%% of the %.0f ms the device was open; "
			"%d callbacks mixed sound, %.2f ms each, %.1f%% of their %.1f ms of output\n",
			mix, al.mixCalls, open > 0.0 ? 100.0 * mix / open : 0.0, open,
			al.busyCalls, al.busyCalls ? busy / al.busyCalls : 0.0,
			al.busyCalls ? 100.0 * busy / al.busyCalls / block : 0.0, block );
	}
	WriteDump();
	for ( i = 0; i < al.numBufs; i++ ) {
		free( al.buf[i].data );
	}
	free( al.buf );
	free( al.acc );
	memset( &al, 0, sizeof( al ) );
	return ALC_TRUE;
}

ALC_API ALCcontext *ALC_APIENTRY alcCreateContext( ALCdevice *device, const ALCint *attrlist ) {
	(void)attrlist;
	if ( device != theDevice || !al.opened ) {
		al.alcError = ALC_INVALID_DEVICE;
		return NULL;
	}
	al.context = 1;
	return theContext;
}

ALC_API void ALC_APIENTRY alcDestroyContext( ALCcontext *context ) {
	int i;
	if ( context != theContext ) {
		al.alcError = ALC_INVALID_CONTEXT;
		return;
	}
	LockMixer();
	for ( i = 0; i < AL_MAX_SOURCES; i++ ) {
		al.src[i].used = 0;
	}
	UnlockMixer();
	al.context = 0;
}

ALC_API ALCboolean ALC_APIENTRY alcMakeContextCurrent( ALCcontext *context ) {
	return context == NULL || context == theContext ? ALC_TRUE : ALC_FALSE;
}

ALC_API void ALC_APIENTRY alcProcessContext( ALCcontext *context ) {
	(void)context;
}

ALC_API void ALC_APIENTRY alcSuspendContext( ALCcontext *context ) {
	(void)context;
}

ALC_API ALCcontext *ALC_APIENTRY alcGetCurrentContext( void ) {
	return al.context ? theContext : NULL;
}

ALC_API ALCdevice *ALC_APIENTRY alcGetContextsDevice( ALCcontext *context ) {
	return context == theContext ? theDevice : NULL;
}

ALC_API ALCenum ALC_APIENTRY alcGetError( ALCdevice *device ) {
	ALCenum e = al.alcError;
	(void)device;
	al.alcError = ALC_NO_ERROR;
	return e;
}

ALC_API ALCboolean ALC_APIENTRY alcIsExtensionPresent( ALCdevice *device, const ALCchar *extname ) {
	(void)device;
	(void)extname;
	return ALC_FALSE;
}

ALC_API void *ALC_APIENTRY alcGetProcAddress( ALCdevice *device, const ALCchar *funcname ) {
	(void)device;
	(void)funcname;
	return NULL;
}

ALC_API ALCenum ALC_APIENTRY alcGetEnumValue( ALCdevice *device, const ALCchar *enumname ) {
	(void)device;
	(void)enumname;
	return 0;
}

ALC_API const ALCchar *ALC_APIENTRY alcGetString( ALCdevice *device, ALCenum param ) {
	(void)device;
	switch ( param ) {
	case ALC_NO_ERROR:			return "No Error";
	case ALC_INVALID_DEVICE:	return "Invalid Device";
	case ALC_INVALID_CONTEXT:	return "Invalid Context";
	case ALC_INVALID_ENUM:		return "Invalid Enum";
	case ALC_INVALID_VALUE:		return "Invalid Value";
	case ALC_OUT_OF_MEMORY:		return "Out of Memory";
	case ALC_DEVICE_SPECIFIER:
	case ALC_DEFAULT_DEVICE_SPECIFIER:
		return "SDL audio (d3 port's OpenAL subset)\0";
	case ALC_EXTENSIONS:		return "";
	default:
		al.alcError = ALC_INVALID_ENUM;
		return NULL;
	}
}

ALC_API void ALC_APIENTRY alcGetIntegerv( ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values ) {
	(void)device;
	if ( size < 1 || !values ) {
		al.alcError = ALC_INVALID_VALUE;
		return;
	}
	switch ( param ) {
	case ALC_MAJOR_VERSION:		values[0] = 1; break;
	case ALC_MINOR_VERSION:		values[0] = 1; break;
	case ALC_FREQUENCY:			values[0] = al.rate; break;
	case ALC_MONO_SOURCES:		values[0] = al.maxSources; break;
	case ALC_STEREO_SOURCES:	values[0] = al.maxSources; break;
	case ALC_ATTRIBUTES_SIZE:	values[0] = 1; break;
	case ALC_ALL_ATTRIBUTES:	values[0] = 0; break;
	case 0x313:	/* ALC_CONNECTED (ALC_EXT_disconnect) */
		values[0] = al.opened;
		break;
	default:
		al.alcError = ALC_INVALID_ENUM;
		break;
	}
}

/*
==============================================================
AL: state, errors, strings
==============================================================
*/

AL_API ALenum AL_APIENTRY alGetError( void ) {
	ALenum e = al.error;
	al.error = AL_NO_ERROR;
	return e;
}

AL_API const ALchar *AL_APIENTRY alGetString( ALenum param ) {
	switch ( param ) {
	case AL_VENDOR:		return "Doom 3 AmigaOS port";
	case AL_RENDERER:	return "OpenAL subset on SDL audio";
	case AL_VERSION:	return "1.1";
	case AL_EXTENSIONS:	return "";
	case AL_NO_ERROR:	return "No Error";
	case AL_INVALID_NAME:		return "Invalid Name";
	case AL_INVALID_ENUM:		return "Invalid Enum";
	case AL_INVALID_VALUE:		return "Invalid Value";
	case AL_INVALID_OPERATION:	return "Invalid Operation";
	case AL_OUT_OF_MEMORY:		return "Out of Memory";
	default:
		SetError( AL_INVALID_ENUM );
		return NULL;
	}
}

AL_API ALboolean AL_APIENTRY alIsExtensionPresent( const ALchar *extname ) {
	(void)extname;
	return AL_FALSE;
}

AL_API void *AL_APIENTRY alGetProcAddress( const ALchar *fname ) {
	(void)fname;
	return NULL;
}

AL_API ALenum AL_APIENTRY alGetEnumValue( const ALchar *ename ) {
	(void)ename;
	return 0;
}

AL_API void AL_APIENTRY alEnable( ALenum capability ) { (void)capability; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alDisable( ALenum capability ) { (void)capability; SetError( AL_INVALID_ENUM ); }
AL_API ALboolean AL_APIENTRY alIsEnabled( ALenum capability ) { (void)capability; return AL_FALSE; }
AL_API void AL_APIENTRY alDopplerFactor( ALfloat value ) { (void)value; }
AL_API void AL_APIENTRY alDopplerVelocity( ALfloat value ) { (void)value; }
AL_API void AL_APIENTRY alSpeedOfSound( ALfloat value ) { (void)value; }
AL_API void AL_APIENTRY alDistanceModel( ALenum distanceModel ) { (void)distanceModel; }

AL_API ALint AL_APIENTRY alGetInteger( ALenum param ) {
	if ( param == AL_DISTANCE_MODEL ) {
		return AL_INVERSE_DISTANCE_CLAMPED;
	}
	SetError( AL_INVALID_ENUM );
	return 0;
}

AL_API ALfloat AL_APIENTRY alGetFloat( ALenum param ) {
	if ( param == AL_DOPPLER_FACTOR || param == AL_DOPPLER_VELOCITY ) {
		return 1.0f;
	}
	if ( param == AL_SPEED_OF_SOUND ) {
		return 343.3f;
	}
	SetError( AL_INVALID_ENUM );
	return 0.0f;
}

AL_API ALboolean AL_APIENTRY alGetBoolean( ALenum param ) { (void)param; SetError( AL_INVALID_ENUM ); return AL_FALSE; }
AL_API ALdouble AL_APIENTRY alGetDouble( ALenum param ) { return alGetFloat( param ); }
AL_API void AL_APIENTRY alGetIntegerv( ALenum param, ALint *values ) { if ( values ) values[0] = alGetInteger( param ); }
AL_API void AL_APIENTRY alGetFloatv( ALenum param, ALfloat *values ) { if ( values ) values[0] = alGetFloat( param ); }
AL_API void AL_APIENTRY alGetBooleanv( ALenum param, ALboolean *values ) { if ( values ) values[0] = alGetBoolean( param ); }
AL_API void AL_APIENTRY alGetDoublev( ALenum param, ALdouble *values ) { if ( values ) values[0] = alGetDouble( param ); }

/*
==============================================================
AL: listener
==============================================================
*/

AL_API void AL_APIENTRY alListenerf( ALenum param, ALfloat value ) {
	if ( param == AL_GAIN ) {
		if ( value < 0.0f ) {
			SetError( AL_INVALID_VALUE );
			return;
		}
		al.gain = value;
	} else {
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alListener3f( ALenum param, ALfloat v1, ALfloat v2, ALfloat v3 ) {
	if ( param == AL_POSITION ) {
		LockMixer();
		al.pos[0] = v1; al.pos[1] = v2; al.pos[2] = v3;
		UnlockMixer();
	} else if ( param != AL_VELOCITY ) {
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alListenerfv( ALenum param, const ALfloat *values ) {
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	switch ( param ) {
	case AL_GAIN:
		alListenerf( param, values[0] );
		break;
	case AL_POSITION:
	case AL_VELOCITY:
		alListener3f( param, values[0], values[1], values[2] );
		break;
	case AL_ORIENTATION:
		LockMixer();
		al.at[0] = values[0]; al.at[1] = values[1]; al.at[2] = values[2];
		al.up[0] = values[3]; al.up[1] = values[4]; al.up[2] = values[5];
		UnlockMixer();
		break;
	default:
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alListeneri( ALenum param, ALint value ) { (void)value; (void)param; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alListener3i( ALenum param, ALint v1, ALint v2, ALint v3 ) { alListener3f( param, (ALfloat)v1, (ALfloat)v2, (ALfloat)v3 ); }
AL_API void AL_APIENTRY alListeneriv( ALenum param, const ALint *values ) {
	ALfloat f[6];
	int i;
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	for ( i = 0; i < ( param == AL_ORIENTATION ? 6 : 3 ); i++ ) {
		f[i] = (ALfloat)values[i];
	}
	alListenerfv( param, f );
}

AL_API void AL_APIENTRY alGetListenerf( ALenum param, ALfloat *value ) {
	if ( value && param == AL_GAIN ) {
		*value = al.gain;
	} else {
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alGetListener3f( ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3 ) {
	if ( param == AL_POSITION && v1 && v2 && v3 ) {
		*v1 = al.pos[0]; *v2 = al.pos[1]; *v3 = al.pos[2];
	} else if ( param == AL_VELOCITY && v1 && v2 && v3 ) {
		*v1 = *v2 = *v3 = 0.0f;
	} else {
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alGetListenerfv( ALenum param, ALfloat *values ) {
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
	} else if ( param == AL_ORIENTATION ) {
		memcpy( values, al.at, sizeof( al.at ) );
		memcpy( values + 3, al.up, sizeof( al.up ) );
	} else if ( param == AL_GAIN ) {
		alGetListenerf( param, values );
	} else {
		alGetListener3f( param, values, values + 1, values + 2 );
	}
}

AL_API void AL_APIENTRY alGetListeneri( ALenum param, ALint *value ) { (void)param; (void)value; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alGetListener3i( ALenum param, ALint *v1, ALint *v2, ALint *v3 ) { (void)param; (void)v1; (void)v2; (void)v3; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alGetListeneriv( ALenum param, ALint *values ) { (void)param; (void)values; SetError( AL_INVALID_ENUM ); }

/*
==============================================================
AL: buffers
==============================================================
*/

AL_API void AL_APIENTRY alGenBuffers( ALsizei n, ALuint *buffers ) {
	int i, j;
	if ( n < 0 || ( n > 0 && !buffers ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	LockMixer();
	for ( i = 0; i < n; i++ ) {
		for ( j = 0; j < al.numBufs && al.buf[j].used; j++ ) {
		}
		if ( j == al.numBufs ) {
			int grow = al.numBufs ? al.numBufs * 2 : 1024;
			alBuf_t *nb = (alBuf_t *)realloc( al.buf, grow * sizeof( alBuf_t ) );
			if ( !nb ) {
				UnlockMixer();
				alDeleteBuffers( i, buffers );
				SetError( AL_OUT_OF_MEMORY );
				return;
			}
			memset( nb + al.numBufs, 0, ( grow - al.numBufs ) * sizeof( alBuf_t ) );
			al.buf = nb;
			al.numBufs = grow;
		}
		al.buf[j].used = 1;
		buffers[i] = j + 1;
	}
	UnlockMixer();
}

static int BufferInUse( ALuint name ) {
	int i, j;
	for ( i = 0; i < AL_MAX_SOURCES; i++ ) {
		if ( !al.src[i].used ) {
			continue;
		}
		for ( j = 0; j < al.src[i].qlen; j++ ) {
			if ( al.src[i].queue[j] == name ) {
				return 1;
			}
		}
	}
	return 0;
}

AL_API void AL_APIENTRY alDeleteBuffers( ALsizei n, const ALuint *buffers ) {
	int i;
	if ( n < 0 || ( n > 0 && !buffers ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	LockMixer();
	for ( i = 0; i < n; i++ ) {
		if ( buffers[i] != 0 && !Buf( buffers[i] ) ) {
			UnlockMixer();
			SetError( AL_INVALID_NAME );
			return;
		}
		if ( buffers[i] != 0 && BufferInUse( buffers[i] ) ) {
			UnlockMixer();
			SetError( AL_INVALID_OPERATION );
			return;
		}
	}
	for ( i = 0; i < n; i++ ) {
		alBuf_t *b = Buf( buffers[i] );
		if ( b ) {
			free( b->data );
			memset( b, 0, sizeof( *b ) );
		}
	}
	UnlockMixer();
}

AL_API ALboolean AL_APIENTRY alIsBuffer( ALuint buffer ) {
	return buffer == 0 || Buf( buffer ) ? AL_TRUE : AL_FALSE;
}

AL_API void AL_APIENTRY alBufferData( ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq ) {
	alBuf_t *b = Buf( buffer );
	int channels, bits, frames, i;
	short *d;

	if ( !b ) {
		SetError( AL_INVALID_NAME );
		return;
	}
	switch ( format ) {
	case AL_FORMAT_MONO8:		channels = 1; bits = 8; break;
	case AL_FORMAT_MONO16:		channels = 1; bits = 16; break;
	case AL_FORMAT_STEREO8:		channels = 2; bits = 8; break;
	case AL_FORMAT_STEREO16:	channels = 2; bits = 16; break;
	default:
		SetError( AL_INVALID_ENUM );
		return;
	}
	if ( size < 0 || freq <= 0 || ( size > 0 && !data ) || size % ( channels * bits / 8 ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	frames = size / ( channels * bits / 8 );
	d = (short *)malloc( frames * channels * sizeof( short ) + sizeof( short ) * 2 );
	if ( !d ) {
		SetError( AL_OUT_OF_MEMORY );
		return;
	}
	if ( bits == 16 ) {
		memcpy( d, data, frames * channels * sizeof( short ) );
	} else {
		const unsigned char *u = (const unsigned char *)data;
		for ( i = 0; i < frames * channels; i++ ) {
			d[i] = (short)( ( u[i] - 128 ) << 8 );
		}
	}
	LockMixer();
	if ( BufferInUse( buffer ) ) {
		UnlockMixer();
		free( d );
		SetError( AL_INVALID_OPERATION );
		return;
	}
	free( b->data );
	b->data = d;
	b->frames = frames;
	b->channels = channels;
	b->rate = freq;
	UnlockMixer();
	LogBuffer( buffer, channels, freq, d, frames );
}

AL_API void AL_APIENTRY alGetBufferi( ALuint buffer, ALenum param, ALint *value ) {
	alBuf_t *b = Buf( buffer );
	if ( !b ) {
		SetError( AL_INVALID_NAME );
		return;
	}
	if ( !value ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	switch ( param ) {
	case AL_FREQUENCY:	*value = b->rate; break;
	case AL_BITS:		*value = 16; break;
	case AL_CHANNELS:	*value = b->channels; break;
	case AL_SIZE:		*value = b->frames * b->channels * 2; break;
	default:			SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alGetBufferiv( ALuint buffer, ALenum param, ALint *values ) { alGetBufferi( buffer, param, values ); }
AL_API void AL_APIENTRY alGetBufferf( ALuint buffer, ALenum param, ALfloat *value ) { (void)buffer; (void)param; (void)value; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alGetBufferfv( ALuint buffer, ALenum param, ALfloat *values ) { (void)buffer; (void)param; (void)values; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alGetBuffer3f( ALuint buffer, ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3 ) { (void)buffer; (void)param; (void)v1; (void)v2; (void)v3; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alGetBuffer3i( ALuint buffer, ALenum param, ALint *v1, ALint *v2, ALint *v3 ) { (void)buffer; (void)param; (void)v1; (void)v2; (void)v3; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alBufferf( ALuint buffer, ALenum param, ALfloat value ) { (void)buffer; (void)param; (void)value; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alBuffer3f( ALuint buffer, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3 ) { (void)buffer; (void)param; (void)v1; (void)v2; (void)v3; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alBufferfv( ALuint buffer, ALenum param, const ALfloat *values ) { (void)buffer; (void)param; (void)values; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alBufferi( ALuint buffer, ALenum param, ALint value ) { (void)buffer; (void)param; (void)value; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alBuffer3i( ALuint buffer, ALenum param, ALint v1, ALint v2, ALint v3 ) { (void)buffer; (void)param; (void)v1; (void)v2; (void)v3; SetError( AL_INVALID_ENUM ); }
AL_API void AL_APIENTRY alBufferiv( ALuint buffer, ALenum param, const ALint *values ) { (void)buffer; (void)param; (void)values; SetError( AL_INVALID_ENUM ); }

/*
==============================================================
AL: sources
==============================================================
*/

static void ResetSource( alSrc_t *s ) {
	memset( s, 0, sizeof( *s ) );
	s->used = 1;
	s->state = AL_INITIAL;
	s->gain = 1.0f;
	s->pitch = 1.0f;
	s->refDist = 1.0f;
	s->maxDist = 3.4e38f;
	s->rolloff = 1.0f;
	s->minGain = 0.0f;
	s->maxGain = 1.0f;
}

AL_API void AL_APIENTRY alGenSources( ALsizei n, ALuint *sources ) {
	int i, j, got = 0;
	if ( n < 0 || ( n > 0 && !sources ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	LockMixer();
	for ( i = 0; i < n; i++ ) {
		for ( j = 0; j < al.maxSources && al.src[j].used; j++ ) {
		}
		if ( j == al.maxSources ) {
			break;
		}
		ResetSource( &al.src[j] );
		sources[i] = j + 1;
		got++;
	}
	if ( got < n ) {
		/* all or none */
		for ( i = 0; i < got; i++ ) {
			al.src[sources[i] - 1].used = 0;
		}
		UnlockMixer();
		SetError( AL_OUT_OF_MEMORY );
		return;
	}
	UnlockMixer();
}

AL_API void AL_APIENTRY alDeleteSources( ALsizei n, const ALuint *sources ) {
	int i;
	if ( n < 0 || ( n > 0 && !sources ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	for ( i = 0; i < n; i++ ) {
		if ( !Src( sources[i] ) ) {
			return;
		}
	}
	LockMixer();
	for ( i = 0; i < n; i++ ) {
		al.src[sources[i] - 1].used = 0;
	}
	UnlockMixer();
}

AL_API ALboolean AL_APIENTRY alIsSource( ALuint source ) {
	return source != 0 && source <= AL_MAX_SOURCES && al.src[source - 1].used ? AL_TRUE : AL_FALSE;
}

AL_API void AL_APIENTRY alSourcef( ALuint source, ALenum param, ALfloat value ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	switch ( param ) {
	case AL_GAIN:
		if ( value < 0.0f ) { SetError( AL_INVALID_VALUE ); return; }
		s->gain = value;
		break;
	case AL_PITCH:
		if ( value <= 0.0f ) { SetError( AL_INVALID_VALUE ); return; }
		s->pitch = value;
		break;
	case AL_REFERENCE_DISTANCE:	s->refDist = value; break;
	case AL_MAX_DISTANCE:		s->maxDist = value; break;
	case AL_ROLLOFF_FACTOR:		s->rolloff = value; break;
	case AL_MIN_GAIN:			s->minGain = value; break;
	case AL_MAX_GAIN:			s->maxGain = value; break;
	case AL_CONE_INNER_ANGLE:
	case AL_CONE_OUTER_ANGLE:
	case AL_CONE_OUTER_GAIN:
	case AL_SEC_OFFSET:
	case AL_SAMPLE_OFFSET:
	case AL_BYTE_OFFSET:
		break;
	default:
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alSource3f( ALuint source, ALenum param, ALfloat v1, ALfloat v2, ALfloat v3 ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	if ( param == AL_POSITION ) {
		LockMixer();
		s->pos[0] = v1; s->pos[1] = v2; s->pos[2] = v3;
		UnlockMixer();
	} else if ( param != AL_VELOCITY && param != AL_DIRECTION ) {
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alSourcefv( ALuint source, ALenum param, const ALfloat *values ) {
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	if ( param == AL_POSITION || param == AL_VELOCITY || param == AL_DIRECTION ) {
		alSource3f( source, param, values[0], values[1], values[2] );
	} else {
		alSourcef( source, param, values[0] );
	}
}

static void StopSource( alSrc_t *s ) {
	s->state = AL_STOPPED;
	s->processed = s->qlen;
	s->qcur = 0;
	s->frame = 0;
	s->frac = 0;
	s->started = 0;
}

AL_API void AL_APIENTRY alSourcei( ALuint source, ALenum param, ALint value ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	switch ( param ) {
	case AL_BUFFER:
		if ( value != 0 && !Buf( (ALuint)value ) ) {
			SetError( AL_INVALID_VALUE );
			return;
		}
		if ( s->state == AL_PLAYING || s->state == AL_PAUSED ) {
			SetError( AL_INVALID_OPERATION );
			return;
		}
		LockMixer();
		s->qlen = value ? 1 : 0;
		s->queue[0] = (ALuint)value;
		s->isStatic = value != 0;
		s->qcur = 0;
		s->processed = 0;
		s->frame = 0;
		s->frac = 0;
		UnlockMixer();
		break;
	case AL_LOOPING:
		s->looping = value != 0;
		break;
	case AL_SOURCE_RELATIVE:
		s->relative = value != 0;
		break;
	case AL_SAMPLE_OFFSET:
	case AL_SEC_OFFSET:
	case AL_BYTE_OFFSET:
	case AL_CONE_INNER_ANGLE:
	case AL_CONE_OUTER_ANGLE:
		break;
	case AL_REFERENCE_DISTANCE:
	case AL_MAX_DISTANCE:
	case AL_ROLLOFF_FACTOR:
		alSourcef( source, param, (ALfloat)value );
		break;
	default:
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alSource3i( ALuint source, ALenum param, ALint v1, ALint v2, ALint v3 ) {
	if ( param == AL_POSITION || param == AL_VELOCITY || param == AL_DIRECTION ) {
		alSource3f( source, param, (ALfloat)v1, (ALfloat)v2, (ALfloat)v3 );
	} else {
		SetError( AL_INVALID_ENUM );	/* EFX's AL_AUXILIARY_SEND_FILTER: no EFX */
	}
}

AL_API void AL_APIENTRY alSourceiv( ALuint source, ALenum param, const ALint *values ) {
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	if ( param == AL_POSITION || param == AL_VELOCITY || param == AL_DIRECTION ) {
		alSource3i( source, param, values[0], values[1], values[2] );
	} else {
		alSourcei( source, param, values[0] );
	}
}

AL_API void AL_APIENTRY alGetSourcef( ALuint source, ALenum param, ALfloat *value ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	if ( !value ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	switch ( param ) {
	case AL_GAIN:				*value = s->gain; break;
	case AL_PITCH:				*value = s->pitch; break;
	case AL_REFERENCE_DISTANCE:	*value = s->refDist; break;
	case AL_MAX_DISTANCE:		*value = s->maxDist; break;
	case AL_ROLLOFF_FACTOR:		*value = s->rolloff; break;
	case AL_MIN_GAIN:			*value = s->minGain; break;
	case AL_MAX_GAIN:			*value = s->maxGain; break;
	case AL_SEC_OFFSET: {
		alBuf_t *b = s->qlen ? Buf( s->queue[s->qcur] ) : NULL;
		*value = b ? (float)s->frame / (float)b->rate : 0.0f;
		break;
	}
	default:					SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alGetSource3f( ALuint source, ALenum param, ALfloat *v1, ALfloat *v2, ALfloat *v3 ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	if ( !v1 || !v2 || !v3 ) {
		SetError( AL_INVALID_VALUE );
	} else if ( param == AL_POSITION ) {
		*v1 = s->pos[0]; *v2 = s->pos[1]; *v3 = s->pos[2];
	} else if ( param == AL_VELOCITY || param == AL_DIRECTION ) {
		*v1 = *v2 = *v3 = 0.0f;
	} else {
		SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alGetSourcefv( ALuint source, ALenum param, ALfloat *values ) {
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
	} else if ( param == AL_POSITION || param == AL_VELOCITY || param == AL_DIRECTION ) {
		alGetSource3f( source, param, values, values + 1, values + 2 );
	} else {
		alGetSourcef( source, param, values );
	}
}

AL_API void AL_APIENTRY alGetSourcei( ALuint source, ALenum param, ALint *value ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	if ( !value ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	switch ( param ) {
	case AL_SOURCE_STATE:		*value = s->state; break;
	case AL_BUFFERS_PROCESSED:	*value = s->isStatic ? 0 : s->processed; break;
	case AL_BUFFERS_QUEUED:		*value = s->qlen; break;
	case AL_BUFFER:				*value = s->qlen ? (ALint)s->queue[s->isStatic ? 0 : s->qcur] : 0; break;
	case AL_LOOPING:			*value = s->looping; break;
	case AL_SOURCE_RELATIVE:	*value = s->relative; break;
	case AL_SOURCE_TYPE:		*value = s->qlen == 0 ? AL_UNDETERMINED : s->isStatic ? AL_STATIC : AL_STREAMING; break;
	case AL_SAMPLE_OFFSET:		*value = (ALint)s->frame; break;
	default:					SetError( AL_INVALID_ENUM );
	}
}

AL_API void AL_APIENTRY alGetSource3i( ALuint source, ALenum param, ALint *v1, ALint *v2, ALint *v3 ) {
	ALfloat f[3] = { 0.0f, 0.0f, 0.0f };
	alGetSource3f( source, param, &f[0], &f[1], &f[2] );
	if ( v1 && v2 && v3 ) {
		*v1 = (ALint)f[0]; *v2 = (ALint)f[1]; *v3 = (ALint)f[2];
	}
}

AL_API void AL_APIENTRY alGetSourceiv( ALuint source, ALenum param, ALint *values ) {
	if ( !values ) {
		SetError( AL_INVALID_VALUE );
	} else if ( param == AL_POSITION || param == AL_VELOCITY || param == AL_DIRECTION ) {
		alGetSource3i( source, param, values, values + 1, values + 2 );
	} else {
		alGetSourcei( source, param, values );
	}
}

AL_API void AL_APIENTRY alSourcePlay( ALuint source ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	LockMixer();
	if ( s->state == AL_PAUSED ) {
		s->state = AL_PLAYING;
	} else {
		/* from the start (a playing source restarts; a stopped queue plays all its buffers again) */
		s->qcur = 0;
		s->processed = 0;
		s->frame = 0;
		s->frac = 0;
		s->started = 0;
		s->state = s->qlen > 0 ? AL_PLAYING : AL_STOPPED;
	}
	UnlockMixer();
}

AL_API void AL_APIENTRY alSourceStop( ALuint source ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	LockMixer();
	if ( s->state != AL_INITIAL ) {
		StopSource( s );
	}
	UnlockMixer();
}

AL_API void AL_APIENTRY alSourceRewind( ALuint source ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	LockMixer();
	StopSource( s );
	s->state = AL_INITIAL;
	s->processed = 0;
	UnlockMixer();
}

AL_API void AL_APIENTRY alSourcePause( ALuint source ) {
	alSrc_t *s = Src( source );
	if ( !s ) {
		return;
	}
	LockMixer();
	if ( s->state == AL_PLAYING ) {
		s->state = AL_PAUSED;
	}
	UnlockMixer();
}

AL_API void AL_APIENTRY alSourcePlayv( ALsizei n, const ALuint *sources ) { int i; for ( i = 0; i < n; i++ ) alSourcePlay( sources[i] ); }
AL_API void AL_APIENTRY alSourceStopv( ALsizei n, const ALuint *sources ) { int i; for ( i = 0; i < n; i++ ) alSourceStop( sources[i] ); }
AL_API void AL_APIENTRY alSourceRewindv( ALsizei n, const ALuint *sources ) { int i; for ( i = 0; i < n; i++ ) alSourceRewind( sources[i] ); }
AL_API void AL_APIENTRY alSourcePausev( ALsizei n, const ALuint *sources ) { int i; for ( i = 0; i < n; i++ ) alSourcePause( sources[i] ); }

AL_API void AL_APIENTRY alSourceQueueBuffers( ALuint source, ALsizei nb, const ALuint *buffers ) {
	alSrc_t *s = Src( source );
	int i;
	if ( !s ) {
		return;
	}
	if ( nb < 0 || ( nb > 0 && !buffers ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	for ( i = 0; i < nb; i++ ) {
		if ( buffers[i] != 0 && !Buf( buffers[i] ) ) {
			SetError( AL_INVALID_NAME );
			return;
		}
	}
	LockMixer();
	if ( s->isStatic ) {
		UnlockMixer();
		SetError( AL_INVALID_OPERATION );
		return;
	}
	if ( s->qlen + nb > AL_MAX_QUEUE ) {
		UnlockMixer();
		SetError( AL_INVALID_OPERATION );
		return;
	}
	for ( i = 0; i < nb; i++ ) {
		s->queue[s->qlen++] = buffers[i];
	}
	if ( s->state == AL_STOPPED ) {
		/* a stopped queue: the new buffers wait to be played (as OpenAL Soft) */
		s->qcur = s->processed;
	}
	UnlockMixer();
}

AL_API void AL_APIENTRY alSourceUnqueueBuffers( ALuint source, ALsizei nb, ALuint *buffers ) {
	alSrc_t *s = Src( source );
	int i;
	if ( !s ) {
		return;
	}
	if ( nb < 0 || ( nb > 0 && !buffers ) ) {
		SetError( AL_INVALID_VALUE );
		return;
	}
	LockMixer();
	if ( s->isStatic || nb > s->processed ) {
		UnlockMixer();
		SetError( AL_INVALID_VALUE );
		return;
	}
	for ( i = 0; i < nb; i++ ) {
		buffers[i] = s->queue[i];
	}
	memmove( s->queue, s->queue + nb, ( s->qlen - nb ) * sizeof( ALuint ) );
	s->qlen -= nb;
	s->processed -= nb;
	s->qcur -= nb;
	if ( s->qcur < 0 ) {
		s->qcur = 0;
	}
	UnlockMixer();
}
