/*
===========================================================================
Q3Doom - client side Doom asset test commands

	doom_showimage [<path> [scale]]	draw a doom/... image over the screen
									(no argument clears it)
	doom_playsound <LUMP>			play a DS* sound effect
	doom_where						Doom sector under the player
	doom_goto <x> <y> [angle]		teleport to a Doom map position (needs devmap / cheats)
	doom_campaign [map] [skill]		play an unlocked campaign level
	doom_progress					print the saved campaign progress
	doomfrag <map>					a DoomFrag time trial with the g_doom* options

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

/*
=================
CL_DoomLevelShot

The cgame saves this frame as a level's menu picture: the console notify
lines stay off it
=================
*/
static int	doomLevelShotFrame = -1;

void CL_DoomLevelShot( const char *name ) {
	doomLevelShotFrame = cls.framecount;
	re.DoomLevelShot( name );
}

qboolean CL_DoomLevelShotFrame( void ) {
	return doomLevelShotFrame == cls.framecount;
}

/*
=====================================================================

CAMPAIGN

=====================================================================
*/

// campaign.dat, written by the game when a level ends
static void CL_DoomLoadProgress( doom_progress_t *p ) {
	char	*text = NULL;
	int		len = FS_ReadFile( "campaign.dat", (void **)&text );

	doom_progress_parse( len > 0 ? text : NULL, p );
	if ( text ) {
		FS_FreeFile( text );
	}
}

/*
=================
CL_DoomCampaign_f

doom_campaign [map] [skill]: plays a level of the campaign the player has
unlocked, with a pistol start (the menus come in M8)
=================
*/
static void CL_DoomCampaign_f( void ) {
	doom_progress_t	p;
	const char		*map = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "E1M1";
	int				skill = Cmd_Argc() > 2 ? atoi( Cmd_Argv( 2 ) ) : Cvar_VariableIntegerValue( "g_doomSkill" );
	int				i;

	CL_DoomLoadProgress( &p );
	if ( !doom_progress_unlocked( &p, map ) ) {
		Com_Printf( "doom_campaign: %s is locked\n", map );
		return;
	}
	if ( skill < 0 || skill > 4 ) {
		skill = 2;
	}
	Cvar_Set( "g_doomSkill", va( "%d", skill ) );
	Cvar_Set( "g_doomCampaign", "1" );
	Cvar_Set( "g_doomFrag", "0" );
	Cvar_Set( "g_doomMonsters", "1" );
	Cvar_Set( "g_doomDidSecret", "0" );
	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		Cvar_Set( va( "g_doomCarry%d", i ), "" );
	}
	Cbuf_AddText( va( "map %s\n", map ) );
}

/*
=================
CL_DoomFrag_f

doomfrag <map>: a DoomFrag run on any map, with the options in g_doomPhysics,
g_doomWeapons, g_doomLoadout, g_doomSkill, g_doomMonsters and g_doomFinish
=================
*/
static void CL_DoomFrag_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: doomfrag <map>\n" );
		return;
	}
	Cvar_Set( "g_doomFrag", "1" );
	Cvar_Set( "g_doomCampaign", "0" );
	Cbuf_AddText( va( "map %s\n", Cmd_Argv( 1 ) ) );
}

/*
=================
CL_DoomProgress_f

doom_progress: the campaign as saved
=================
*/
static void CL_DoomProgress_f( void ) {
	static const char	*medals[] = { "-", "bronze", "silver", "gold", "skull" };
	doom_progress_t		p;
	int					i;

	CL_DoomLoadProgress( &p );
	for ( i = 0 ; i < p.count ; i++ ) {
		const doom_progress_level_t *l = &p.levels[i];

		Com_Printf( "%-6s %-8s medal %-6s best %s\n", l->map, l->unlocked ? "unlocked" : "locked", medals[l->medal],
			l->best_tics ? va( "%d:%02d", l->best_tics / 35 / 60, l->best_tics / 35 % 60 ) : "-" );
	}
}

void CL_InitDoomCommands( void ) {
	Cmd_AddCommand( "doom_campaign", CL_DoomCampaign_f );
	Cmd_AddCommand( "doom_progress", CL_DoomProgress_f );
	Cmd_AddCommand( "doomfrag", CL_DoomFrag_f );
	Cmd_AddCommand( "doom_showimage", CL_DoomShowImage_f );
	Cmd_AddCommand( "doom_playsound", CL_DoomPlaySound_f );
	Cmd_AddCommand( "doom_where", CL_DoomWhere_f );
	Cmd_AddCommand( "doom_goto", CL_DoomGoto_f );
}
