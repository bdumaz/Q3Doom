/*
===========================================================================
Q3Doom - SDL2 window and OpenGL context (replaces win32/win_glimp.c)

This file is part of Q3Doom, based on Quake III Arena source code
Copyright (C) 1999-2005 Id Software, Inc.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../renderer/tr_local.h"
#include "sdl_local.h"

SDL_Window				*SDL_window = NULL;
static SDL_GLContext	SDL_glContext = NULL;

/*
** GLimp_InitExtensions
**
** Same extension set as the original renderer. SDL_GL_ExtensionSupported
** is used instead of searching glConfig.extensions_string because modern
** extension strings are far longer than that buffer.
*/
static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	// GL_S3_s3tc
	glConfig.textureCompression = TC_NONE;
	if ( SDL_GL_ExtensionSupported( "GL_S3_s3tc" ) )
	{
		if ( r_ext_compressed_textures->integer )
		{
			glConfig.textureCompression = TC_S3TC;
			ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
	}

	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( SDL_GL_ExtensionSupported( "GL_EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// swap control is always available through SDL
	r_swapInterval->modified = qtrue;	// force a set next frame

	// non power of two textures: core since OpenGL 2.0
	glNPOTSupported = atoi( glConfig.version_string ) >= 2 || SDL_GL_ExtensionSupported( "GL_ARB_texture_non_power_of_two" );
	ri.Printf( PRINT_ALL, "...non power of two textures: %s\n", glNPOTSupported ? "yes" : "no" );

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( SDL_GL_ExtensionSupported( "GL_ARB_multitexture" ) )
	{
		if ( r_ext_multitexture->integer )
		{
			qglMultiTexCoord2fARB = ( PFNGLMULTITEXCOORD2FARBPROC ) SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( PFNGLACTIVETEXTUREARBPROC ) SDL_GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( PFNGLCLIENTACTIVETEXTUREARBPROC ) SDL_GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB )
			{
				qglGetIntegerv( GL_MAX_ACTIVE_TEXTURES_ARB, &glConfig.maxActiveTextures );

				if ( glConfig.maxActiveTextures > 1 )
				{
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	if ( SDL_GL_ExtensionSupported( "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ( void ( APIENTRY * )( GLint, GLint ) ) SDL_GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void ( APIENTRY * )( void ) ) SDL_GL_GetProcAddress( "glUnlockArraysEXT" );
			if ( !qglLockArraysEXT || !qglUnlockArraysEXT ) {
				ri.Error( ERR_FATAL, "bad getprocaddress" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}
}

/*
** GLimp_SetMode
**
** r_mode -2 uses the desktop resolution, -1 uses r_customwidth/height,
** otherwise the classic mode table from tr_init.c.
*/
static qboolean GLimp_SetMode( void )
{
	SDL_DisplayMode	desktop;
	Uint32			flags;
	int				value;

	if ( SDL_GetDesktopDisplayMode( 0, &desktop ) != 0 ) {
		desktop.w = 640;
		desktop.h = 480;
		desktop.refresh_rate = 60;
	}

	if ( r_mode->integer == -2 ) {
		glConfig.vidWidth = desktop.w;
		glConfig.vidHeight = desktop.h;
		glConfig.windowAspect = (float)desktop.w / desktop.h;
	} else if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, r_mode->integer ) ) {
		ri.Printf( PRINT_ALL, "invalid r_mode %d, using 640x480\n", r_mode->integer );
		R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, 3 );
	}

	ri.Printf( PRINT_ALL, "...setting mode %d: %d x %d %s\n", r_mode->integer,
		glConfig.vidWidth, glConfig.vidHeight, r_fullscreen->integer ? "FS" : "W" );

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, r_depthbits->integer ? r_depthbits->integer : 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, r_stencilbits->integer );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	flags = SDL_WINDOW_OPENGL;
	if ( r_fullscreen->integer ) {
		if ( r_mode->integer == -2 ) {
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		} else {
			flags |= SDL_WINDOW_FULLSCREEN;
		}
	}

	SDL_window = SDL_CreateWindow( "Q3Doom", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		glConfig.vidWidth, glConfig.vidHeight, flags );
	if ( !SDL_window ) {
		ri.Printf( PRINT_ALL, "SDL_CreateWindow failed: %s\n", SDL_GetError() );
		return qfalse;
	}

	SDL_glContext = SDL_GL_CreateContext( SDL_window );
	if ( !SDL_glContext ) {
		ri.Printf( PRINT_ALL, "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
		SDL_DestroyWindow( SDL_window );
		SDL_window = NULL;
		return qfalse;
	}
	SDL_GL_MakeCurrent( SDL_window, SDL_glContext );

	glConfig.colorBits = 24;
	if ( SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &value ) == 0 ) {
		glConfig.depthBits = value;
	}
	if ( SDL_GL_GetAttribute( SDL_GL_STENCIL_SIZE, &value ) == 0 ) {
		glConfig.stencilBits = value;
	}
	glConfig.displayFrequency = desktop.refresh_rate;
	glConfig.isFullscreen = r_fullscreen->integer ? qtrue : qfalse;
	glConfig.stereoEnabled = qfalse;
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;

	return qtrue;
}

// the desktop gamma ramp before we changed it, restored even after a crash
static Uint16	originalGamma[3][256];
static qboolean	originalGammaSaved;

/*
** GLimp_CheckHardwareGamma
*/
static void GLimp_CheckHardwareGamma( void )
{
	glConfig.deviceSupportsGamma = qfalse;
	if ( r_ignorehwgamma->integer ) {
		return;
	}
	if ( SDL_GetWindowGammaRamp( SDL_window, originalGamma[0], originalGamma[1], originalGamma[2] ) == 0 ) {
		glConfig.deviceSupportsGamma = qtrue;
		originalGammaSaved = qtrue;
	}
}

/*
** GLimp_RestoreGamma
**
** Called by the crash handler: SDL restores the ramp when the window is
** destroyed, which never happens if the process dies. The window is also
** minimized so the crash message is not hidden behind a fullscreen window.
*/
void GLimp_RestoreGamma( void )
{
	if ( !SDL_window ) {
		return;
	}
	if ( originalGammaSaved ) {
		SDL_SetWindowGammaRamp( SDL_window, originalGamma[0], originalGamma[1], originalGamma[2] );
	}
	SDL_MinimizeWindow( SDL_window );
}

/*
** GLimp_Init
**
** Creates the window and GL context, loads the GL entry points and
** fills in glConfig.
*/
void GLimp_Init( void )
{
	cvar_t *lastValidRenderer = ri.Cvar_Get( "r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE );

	ri.Printf( PRINT_ALL, "Initializing OpenGL subsystem\n" );

	if ( !SDL_WasInit( SDL_INIT_VIDEO ) ) {
		if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) != 0 ) {
			ri.Error( ERR_FATAL, "GLimp_Init() - SDL video init failed: %s\n", SDL_GetError() );
		}
	}

	if ( !GLimp_SetMode() ) {
		// fall back to a safe windowed mode
		ri.Cvar_Set( "r_fullscreen", "0" );
		ri.Cvar_Set( "r_mode", "3" );
		if ( !GLimp_SetMode() ) {
			ri.Error( ERR_FATAL, "GLimp_Init() - could not create an OpenGL window\n" );
		}
	}

	if ( !QGL_Init() ) {
		ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL entry points\n" );
	}

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (const char *)qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (const char *)qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	Q_strncpyz( glConfig.version_string, (const char *)qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (const char *)qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	ri.Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );

	GLimp_InitExtensions();
	GLimp_CheckHardwareGamma();
}

/*
** GLimp_Shutdown
*/
void GLimp_Shutdown( void )
{
	ri.Printf( PRINT_ALL, "Shutting down OpenGL subsystem\n" );

	if ( SDL_glContext ) {
		SDL_GL_MakeCurrent( NULL, NULL );
		SDL_GL_DeleteContext( SDL_glContext );
		SDL_glContext = NULL;
	}
	if ( SDL_window ) {
		SDL_DestroyWindow( SDL_window );	// also restores the gamma ramp
		SDL_window = NULL;
	}
	originalGammaSaved = qfalse;
	SDL_QuitSubSystem( SDL_INIT_VIDEO );

	QGL_Shutdown();

	memset( &glConfig, 0, sizeof( glConfig ) );
	memset( &glState, 0, sizeof( glState ) );
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame( void )
{
	if ( r_swapInterval->modified ) {
		r_swapInterval->modified = qfalse;
		SDL_GL_SetSwapInterval( r_swapInterval->integer );
	}

	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 ) {
		SDL_GL_SwapWindow( SDL_window );
	}
}

/*
** GLimp_SetGamma
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] )
{
	Uint16	table[3][256];
	int		i;

	if ( !glConfig.deviceSupportsGamma || !SDL_window ) {
		return;
	}

	for ( i = 0 ; i < 256 ; i++ ) {
		table[0][i] = ( ( Uint16 )red[i] << 8 ) | red[i];
		table[1][i] = ( ( Uint16 )green[i] << 8 ) | green[i];
		table[2][i] = ( ( Uint16 )blue[i] << 8 ) | blue[i];
	}
	SDL_SetWindowGammaRamp( SDL_window, table[0], table[1], table[2] );
}

/*
** GLimp_LogComment
**
** r_logFile GL call logging was a feature of the win32 QGL layer; not kept.
*/
void GLimp_LogComment( char *comment )
{
}

/*
===========================================================

SMP acceleration: not supported, r_smp is ignored

===========================================================
*/
qboolean GLimp_SpawnRenderThread( void (*function)( void ) )
{
	return qfalse;
}

void *GLimp_RendererSleep( void )
{
	return NULL;
}

void GLimp_FrontEndSleep( void )
{
}

void GLimp_WakeRenderer( void *data )
{
}
