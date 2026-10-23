/*
===========================================================================
Q3Doom - client side Doom asset test commands

	doom_showimage [<path> [scale]]	draw a doom/... image over the screen
									(no argument clears it)
	doom_playsound <LUMP>			play a DS* sound effect
	doom_where						Doom sector under the player
	doom_goto <x> <y> [angle]		teleport to a Doom map position (needs devmap / cheats)

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "client.h"
#include "../libdoom/doom.h"

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

// the Doom map being played, or NULL
static const doom_map_t *CL_DoomCurrentMap( const char *command ) {
	const char			*info, *mapname;
	const doom_map_t	*map;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "%s: not in a game\n", command );
		return NULL;
	}
	info = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
	mapname = Info_ValueForKey( info, "mapname" );
	map = Doom_GetMap( mapname );
	if ( !map ) {
		Com_Printf( "%s: %s is not a Doom map\n", command, mapname );
	}
	return map;
}

/*
=================
CL_DoomWhere_f
=================
*/
static void CL_DoomWhere_f( void ) {
	const doom_map_t	*map = CL_DoomCurrentMap( "doom_where" );
	const float			*org;
	int					sector;

	if ( !map ) {
		return;
	}

	org = cl.snap.ps.origin;
	sector = doom_map_sector_at( map, org[0], org[1] );
	Com_Printf( "%s at (%.0f %.0f %.0f): ", map->name, org[0], org[1], org[2] );
	if ( sector < 0 ) {
		Com_Printf( "outside the map\n" );
		return;
	}
	Com_Printf( "sector %d, floor %d (%s), ceiling %d (%s), light %d, special %d, tag %d\n", sector,
		map->sectors[sector].floor_height, map->sectors[sector].floor_flat,
		map->sectors[sector].ceiling_height, map->sectors[sector].ceiling_flat,
		map->sectors[sector].light, map->sectors[sector].special, map->sectors[sector].tag );
}

/*
=================
CL_DoomGoto_f

Standing height: the 56 unit player box has its origin 24 above its feet
=================
*/
static void CL_DoomGoto_f( void ) {
	const doom_map_t	*map;
	float				x, y;
	int					sector;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "usage: doom_goto <x> <y> [angle]\n" );
		return;
	}
	map = CL_DoomCurrentMap( "doom_goto" );
	if ( !map ) {
		return;
	}
	x = atof( Cmd_Argv( 1 ) );
	y = atof( Cmd_Argv( 2 ) );
	sector = doom_map_sector_at( map, x + 0.25, y + 0.25 );
	if ( sector < 0 ) {
		Com_Printf( "doom_goto: (%.0f %.0f) is outside the map\n", x, y );
		return;
	}
	// inserted, not appended: it must run before whatever follows in the buffer
	Cbuf_ExecuteText( EXEC_INSERT, va( "setviewpos %.0f %.0f %d %s\n", x, y, map->sectors[sector].floor_height + 25,
		Cmd_Argc() > 3 ? Cmd_Argv( 3 ) : "0" ) );
}

void CL_InitDoomCommands( void ) {
	Cmd_AddCommand( "doom_showimage", CL_DoomShowImage_f );
	Cmd_AddCommand( "doom_playsound", CL_DoomPlaySound_f );
	Cmd_AddCommand( "doom_where", CL_DoomWhere_f );
	Cmd_AddCommand( "doom_goto", CL_DoomGoto_f );
}
