/*
===========================================================================
Q3Doom - Doom IWAD access for the engine

The IWAD is read once at startup through the Quake 3 filesystem (before
any pure server can restrict loose files) and kept in memory. Renderer and
sound system resolve virtual paths through here:

	doom/wall/<NAME>	composite wall texture (TEXTURE1/2)
	doom/flat/<NAME>	floor/ceiling flat
	doom/sprite/<NAME>	sprite frame (S_START..S_END)
	doom/patch/<NAME>	any other picture lump (status bar, menus...)
	doom/sound/<NAME>	DMX sound effect (DS* lumps)

An extension on the name (".tga" added by the shader system) is ignored.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../game/q_shared.h"
#include "qcommon.h"
#include "../libdoom/doom.h"

static doom_wad_t	*doom_wad;
static cvar_t		*doom_iwad;

/*
=================
Doom_ParsePath

Splits "doom/<kind>/<NAME>[.ext]" into kind and an uppercase lump name.
=================
*/
static qboolean Doom_ParsePath( const char *path, char *kind, int kindSize, char name[DOOM_LUMP_NAME_LEN + 1] ) {
	const char	*s, *slash;
	int			i;

	if ( Q_stricmpn( path, DOOM_PATH_PREFIX, strlen( DOOM_PATH_PREFIX ) ) ) {
		return qfalse;
	}
	s = path + strlen( DOOM_PATH_PREFIX );
	slash = strchr( s, '/' );
	if ( !slash || slash - s >= kindSize ) {
		return qfalse;
	}
	Q_strncpyz( kind, s, slash - s + 1 );

	s = slash + 1;
	for ( i = 0 ; i < DOOM_LUMP_NAME_LEN && s[i] && s[i] != '.' ; i++ ) {
		name[i] = toupper( s[i] );
	}
	name[i] = 0;
	return i > 0;
}

/*
=================
Doom_LoadImage

Returns Z_Malloc'ed RGBA pixels (freed by the caller with Z_Free), or NULL.
=================
*/
byte *Doom_LoadImage( const char *path, int *width, int *height ) {
	char			kind[16], name[DOOM_LUMP_NAME_LEN + 1];
	doom_image_t	img;
	int				result;
	byte			*pic;

	*width = *height = 0;
	if ( !doom_wad || !Doom_ParsePath( path, kind, sizeof( kind ), name ) ) {
		return NULL;
	}

	if ( !Q_stricmp( kind, "wall" ) ) {
		result = doom_load_texture( doom_wad, name, &img );
	} else if ( !Q_stricmp( kind, "flat" ) ) {
		result = doom_load_flat( doom_wad, name, &img );
	} else if ( !Q_stricmp( kind, "sprite" ) ) {
		result = doom_load_sprite( doom_wad, name, &img );
	} else if ( !Q_stricmp( kind, "patch" ) ) {
		result = doom_load_patch( doom_wad, name, &img );
	} else {
		return NULL;
	}
	if ( result ) {
		return NULL;
	}

	pic = Z_Malloc( img.width * img.height * 4 );
	Com_Memcpy( pic, img.rgba, img.width * img.height * 4 );
	*width = img.width;
	*height = img.height;
	doom_image_free( &img );
	return pic;
}

/*
=================
Doom_LoadSound

Unsigned 8-bit mono samples pointing into the WAD (do not free).
=================
*/
qboolean Doom_LoadSound( const char *path, const byte **samples, int *numSamples, int *rate ) {
	char			kind[16], name[DOOM_LUMP_NAME_LEN + 1];
	doom_sound_t	snd;

	if ( !doom_wad || !Doom_ParsePath( path, kind, sizeof( kind ), name ) || Q_stricmp( kind, "sound" ) ) {
		return qfalse;
	}
	if ( doom_load_sound( doom_wad, name, &snd ) ) {
		return qfalse;
	}
	*samples = snd.samples;
	*numSamples = snd.num_samples;
	*rate = snd.rate;
	return qtrue;
}

const doom_wad_t *Doom_GetWad( void ) {
	return doom_wad;
}

/*
=================
Doom_Info_f
=================
*/
static void Doom_Info_f( void ) {
	char	maps[64][DOOM_LUMP_NAME_LEN + 1];
	int		i, n;

	if ( !doom_wad ) {
		Com_Printf( "No Doom IWAD loaded (doom_iwad \"%s\")\n", doom_iwad->string );
		return;
	}
	Com_Printf( "%s: %s, %d lumps, %d textures, %d flats, %d sprite frames\n", doom_iwad->string,
		doom_wad_is_iwad( doom_wad ) ? "IWAD" : "PWAD", doom_wad_num_lumps( doom_wad ),
		doom_num_textures( doom_wad ), doom_wad_ns_count( doom_wad, DOOM_NS_FLATS ),
		doom_wad_ns_count( doom_wad, DOOM_NS_SPRITES ) );

	n = doom_wad_list_maps( doom_wad, maps, 64 );
	Com_Printf( "%d maps:", n );
	for ( i = 0 ; i < n && i < 64 ; i++ ) {
		Com_Printf( " %s", maps[i] );
	}
	Com_Printf( "\n" );
}

/*
=================
Doom_Init

Called once after the filesystem is up
=================
*/
void Doom_Init( void ) {
	fileHandle_t	f;
	int				len;
	void			*data;
	char			err[256];

	doom_iwad = Cvar_Get( "doom_iwad", "DOOM.WAD", CVAR_INIT );
	Cmd_AddCommand( "doom_info", Doom_Info_f );

	len = FS_FOpenFileRead( doom_iwad->string, &f, qfalse );
	if ( len <= 0 || !f ) {
		Com_Printf( "WARNING: Doom IWAD %s not found in the game directories\n", doom_iwad->string );
		return;
	}

	data = malloc( len );
	if ( !data || FS_Read( data, len, f ) != len ) {
		Com_Printf( "WARNING: could not read %s\n", doom_iwad->string );
		FS_FCloseFile( f );
		free( data );
		return;
	}
	FS_FCloseFile( f );

	doom_wad = doom_wad_open( data, len, err, sizeof( err ) );	// takes ownership of data
	if ( !doom_wad ) {
		Com_Printf( "WARNING: %s: %s\n", doom_iwad->string, err );
		return;
	}

	Com_Printf( "Doom IWAD %s: %d lumps, %d textures\n", doom_iwad->string,
		doom_wad_num_lumps( doom_wad ), doom_num_textures( doom_wad ) );
}

void Doom_Shutdown( void ) {
	doom_wad_close( doom_wad );
	doom_wad = NULL;
}
