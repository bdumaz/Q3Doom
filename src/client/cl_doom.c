/*
===========================================================================
Q3Doom - client side Doom asset test commands

	doom_showimage [<path> [scale]]	draw a doom/... image over the screen
									(no argument clears it)
	doom_playsound <LUMP>			play a DS* sound effect

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "client.h"

static qhandle_t	doomOverlayShader;
static int			doomOverlayWidth, doomOverlayHeight;	// source image size
static float		doomOverlayScale;

/*
=================
CL_DoomShowImage_f
=================
*/
static void CL_DoomShowImage_f( void ) {
	byte	*pic;

	doomOverlayShader = 0;
	if ( Cmd_Argc() < 2 ) {
		return;
	}

	// the renderer resamples to a power of two, so keep the real size for drawing
	pic = Doom_LoadImage( Cmd_Argv( 1 ), &doomOverlayWidth, &doomOverlayHeight );
	if ( !pic ) {
		Com_Printf( "doom_showimage: could not load %s\n", Cmd_Argv( 1 ) );
		return;
	}
	Z_Free( pic );

	doomOverlayScale = Cmd_Argc() > 2 ? atof( Cmd_Argv( 2 ) ) : 4.0f;
	doomOverlayShader = re.RegisterShaderNoMip( Cmd_Argv( 1 ) );
}

/*
=================
CL_DoomPlaySound_f
=================
*/
static void CL_DoomPlaySound_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: doom_playsound <DS lump name>\n" );
		return;
	}
	S_StartLocalSound( S_RegisterSound( va( DOOM_PATH_PREFIX "sound/%s", Cmd_Argv( 1 ) ), qfalse ), CHAN_LOCAL_SOUND );
}

/*
=================
CL_DoomDrawOverlay

Draws the image centered at doomOverlayScale screen pixels per image pixel
=================
*/
void CL_DoomDrawOverlay( void ) {
	float	w, h;

	if ( !doomOverlayShader ) {
		return;
	}

	w = doomOverlayWidth * doomOverlayScale;
	h = doomOverlayHeight * doomOverlayScale;
	re.SetColor( NULL );
	re.DrawStretchPic( ( cls.glconfig.vidWidth - w ) * 0.5f, ( cls.glconfig.vidHeight - h ) * 0.5f, w, h,
		0, 0, 1, 1, doomOverlayShader );
}

void CL_InitDoomCommands( void ) {
	Cmd_AddCommand( "doom_showimage", CL_DoomShowImage_f );
	Cmd_AddCommand( "doom_playsound", CL_DoomPlaySound_f );
}
