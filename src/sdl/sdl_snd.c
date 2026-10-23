/*
===========================================================================
Q3Doom - SDL2 sound output (replaces win32/win_snd.c DirectSound code)

The Quake 3 mixer paints into a ring buffer (dma.buffer) ahead of the
play position; the SDL audio callback consumes it.

This file is part of Q3Doom, based on Quake III Arena source code
Copyright (C) 1999-2005 Id Software, Inc.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../client/snd_local.h"
#include "sdl_local.h"

static SDL_AudioDeviceID	sdlDevice = 0;
static int					dmaPos;		// play position in bytes
static int					dmaSize;	// ring buffer size in bytes

/*
===============
SNDDMA_AudioCallback
===============
*/
static void SDLCALL SNDDMA_AudioCallback( void *userdata, Uint8 *stream, int len )
{
	int		len1, len2;

	if ( !dma.buffer ) {
		memset( stream, 0, len );
		return;
	}

	len1 = dmaSize - dmaPos;
	if ( len1 > len ) {
		len1 = len;
	}
	len2 = len - len1;

	memcpy( stream, dma.buffer + dmaPos, len1 );
	if ( len2 ) {
		memcpy( stream + len1, dma.buffer, len2 );
		dmaPos = len2;
	} else {
		dmaPos += len1;
		if ( dmaPos >= dmaSize ) {
			dmaPos = 0;
		}
	}
}

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init( void )
{
	SDL_AudioSpec	desired, obtained;
	cvar_t			*s_sdlSpeed;
	int				samples;

	if ( sdlDevice ) {
		return qtrue;
	}

	Com_Printf( "Initializing SDL audio\n" );

	if ( !SDL_WasInit( SDL_INIT_AUDIO ) ) {
		if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) != 0 ) {
			Com_Printf( "SDL_InitSubSystem( AUDIO ) failed: %s\n", SDL_GetError() );
			return qfalse;
		}
	}

	s_sdlSpeed = Cvar_Get( "s_sdlSpeed", "44100", CVAR_ARCHIVE | CVAR_LATCH );

	memset( &desired, 0, sizeof( desired ) );
	desired.freq = s_sdlSpeed->integer;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 512;
	desired.callback = SNDDMA_AudioCallback;

	sdlDevice = SDL_OpenAudioDevice( NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE );
	if ( !sdlDevice ) {
		Com_Printf( "SDL_OpenAudioDevice failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	// a power of two ring buffer of about a third of a second
	samples = 1;
	while ( samples < obtained.freq * obtained.channels / 3 ) {
		samples <<= 1;
	}

	dma.channels = obtained.channels;
	dma.samplebits = 16;
	dma.speed = obtained.freq;
	dma.samples = samples;
	dma.submission_chunk = 1;
	dmaSize = samples * ( dma.samplebits / 8 );
	dma.buffer = calloc( 1, dmaSize );
	dmaPos = 0;

	Com_Printf( "SDL audio: %d Hz, %d channels, %d sample ring buffer (%s)\n",
		dma.speed, dma.channels, dma.samples, SDL_GetCurrentAudioDriver() );

	SDL_PauseAudioDevice( sdlDevice, 0 );
	return qtrue;
}

/*
===============
SNDDMA_GetDMAPos

Play position in mono samples
===============
*/
int SNDDMA_GetDMAPos( void )
{
	return dmaPos / ( dma.samplebits / 8 );
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown( void )
{
	Com_Printf( "Closing SDL audio\n" );

	if ( sdlDevice ) {
		SDL_CloseAudioDevice( sdlDevice );
		sdlDevice = 0;
	}
	SDL_QuitSubSystem( SDL_INIT_AUDIO );

	free( dma.buffer );
	dma.buffer = NULL;
	dmaPos = dmaSize = 0;
}

/*
===============
SNDDMA_BeginPainting / SNDDMA_Submit

The mixer writes into dma.buffer between these calls
===============
*/
void SNDDMA_BeginPainting( void )
{
	SDL_LockAudioDevice( sdlDevice );
}

void SNDDMA_Submit( void )
{
	SDL_UnlockAudioDevice( sdlDevice );
}
