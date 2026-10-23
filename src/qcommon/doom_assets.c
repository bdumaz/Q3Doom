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
	doom/skybox/<SKY>_<face>	skybox face generated from a sky texture
	maps/<MAP>.bsp		Quake 3 BSP built from the map (see FS_ReadFile)

An extension on the name (".tga" added by the shader system) is ignored.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../game/q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "../game/surfaceflags.h"
#include "../libdoom/doom.h"

static doom_wad_t	*doom_wad;
static cvar_t		*doom_iwad;

// the last built maps (server, client collision and renderer all load the
// same map, sometimes in the same process)
#define BSP_CACHE_SIZE	2

typedef struct {
	char		name[DOOM_LUMP_NAME_LEN + 1];
	doom_map_t	map;
	uint8_t		*bsp;
	size_t		bspSize;
	int			lastUsed;
} doomMapCache_t;

static doomMapCache_t	mapCache[BSP_CACHE_SIZE];
static int				mapCacheTime;

/*
=================
Doom_CachedMap

Loads the map model and builds its BSP once; NULL if the map does not exist.
=================
*/
static doomMapCache_t *Doom_CachedMap( const char *name ) {
	doomMapCache_t		*c, *oldest = &mapCache[0];
	doom_build_stats_t	stats;
	char				err[256];
	int					i, start;

	if ( !doom_wad ) {
		return NULL;
	}
	for ( i = 0 ; i < BSP_CACHE_SIZE ; i++ ) {
		c = &mapCache[i];
		if ( c->bsp && !Q_stricmp( c->name, name ) ) {
			c->lastUsed = ++mapCacheTime;
			return c;
		}
		if ( c->lastUsed < oldest->lastUsed ) {
			oldest = c;
		}
	}

	c = oldest;
	if ( c->bsp ) {
		free( c->bsp );
		doom_map_free( &c->map );
		Com_Memset( c, 0, sizeof( *c ) );
	}
	if ( doom_map_load( doom_wad, name, &c->map, err, sizeof( err ) ) ) {
		return NULL;
	}

	start = Sys_Milliseconds();
	if ( doom_build_bsp( doom_wad, &c->map, &c->bsp, &c->bspSize, &stats, err, sizeof( err ) ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: building %s failed: %s\n", name, err );
		doom_map_free( &c->map );
		return NULL;
	}
	Q_strncpyz( c->name, c->map.name, sizeof( c->name ) );
	c->lastUsed = ++mapCacheTime;
	Com_Printf( "Built %s in %d msec: %d leafs, %d brushes, %d surfaces, %d bytes\n", c->name,
		Sys_Milliseconds() - start, stats.sector_leafs + stats.void_leafs, stats.brushes, stats.surfaces, (int)c->bspSize );
	return c;
}

const byte *Doom_GetMapBSP( const char *qpath, int *size ) {
	char			name[DOOM_LUMP_NAME_LEN + 1];
	const char		*s;
	int				len;
	doomMapCache_t	*c;

	if ( !doom_wad || Q_stricmpn( qpath, "maps/", 5 ) ) {
		return NULL;
	}
	s = qpath + 5;
	len = strlen( s );
	if ( len <= 4 || len - 4 > DOOM_LUMP_NAME_LEN || Q_stricmp( s + len - 4, ".bsp" ) || strchr( s, '/' ) ) {
		return NULL;
	}
	Q_strncpyz( name, s, len - 4 + 1 );

	c = Doom_CachedMap( name );
	if ( !c ) {
		return NULL;
	}
	*size = (int)c->bspSize;
	return c->bsp;
}

const doom_map_t *Doom_GetMap( const char *name ) {
	doomMapCache_t *c = Doom_CachedMap( name );

	return c ? &c->map : NULL;
}

/*
=================
Doom_SkyboxImage

Doom draws the sky as a cylinder: a 256 texel wide sky texture covers 90
degrees, texel row 100 is on the horizon and a texel is one screen row
of the 160 pixel focal length view. Each skybox side covers 90 degrees,
so all four sides are the same image. Top and bottom are flat colors.
=================
*/
#define SKY_FOCAL		160.0
#define SKY_HORIZON_ROW	100.0

static byte *Doom_SkyboxImage( const char *name, int *width, int *height ) {
	char			base[DOOM_LUMP_NAME_LEN + 1], *face;
	doom_image_t	sky;
	byte			*pic;
	int				x, y, w, h, row, sum[3], k;

	Q_strncpyz( base, name, sizeof( base ) );
	face = strrchr( base, '_' );
	if ( !face ) {
		return NULL;
	}
	*face++ = 0;
	if ( doom_load_texture( doom_wad, base, &sky ) ) {
		return NULL;
	}

	if ( !Q_stricmp( face, "UP" ) || !Q_stricmp( face, "DN" ) ) {
		// average of the top or bottom row of the sky
		row = !Q_stricmp( face, "UP" ) ? 0 : sky.height - 1;
		sum[0] = sum[1] = sum[2] = 0;
		for ( x = 0 ; x < sky.width ; x++ ) {
			for ( k = 0 ; k < 3 ; k++ ) {
				sum[k] += sky.rgba[( row * sky.width + x ) * 4 + k];
			}
		}
		w = h = 8;
		pic = Z_Malloc( w * h * 4 );
		for ( x = 0 ; x < w * h ; x++ ) {
			for ( k = 0 ; k < 3 ; k++ ) {
				pic[x * 4 + k] = sum[k] / sky.width;
			}
			pic[x * 4 + 3] = 255;
		}
	} else {
		w = h = sky.width;
		pic = Z_Malloc( w * h * 4 );
		for ( y = 0 ; y < h ; y++ ) {
			for ( x = 0 ; x < w ; x++ ) {
				// direction through this pixel of a 90 degree face at distance 1
				double	dx = ( x + 0.5 ) / ( w * 0.5 ) - 1.0;
				double	dy = 1.0 - ( y + 0.5 ) / ( h * 0.5 );
				double	u = ( atan( dx ) / ( M_PI * 0.5 ) + 0.5 ) * sky.width;
				double	t = SKY_HORIZON_ROW - SKY_FOCAL * dy / sqrt( 1.0 + dx * dx );
				int		tu = (int)u, tv = (int)t;

				if ( tu < 0 ) tu = 0;
				if ( tu >= sky.width ) tu = sky.width - 1;
				if ( tv < 0 ) tv = 0;
				if ( tv >= sky.height ) tv = sky.height - 1;
				Com_Memcpy( pic + ( y * w + x ) * 4, sky.rgba + ( tv * sky.width + tu ) * 4, 4 );
				pic[( y * w + x ) * 4 + 3] = 255;
			}
		}
	}

	doom_image_free( &sky );
	*width = w;
	*height = h;
	return pic;
}

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

// wall textures under their different shader kinds: plain, scrolling, per-line switches
static qboolean Doom_IsWallKind( const char *kind ) {
	return !Q_stricmp( kind, "wall" ) || !Q_stricmp( kind, "scroll" )
		|| ( !Q_stricmpn( kind, "sw", 2 ) && isdigit( (unsigned char)kind[2] ) );
}

// flats: plain, or the per-sector shader of a moving floor (fl<sector>)
static qboolean Doom_IsFlatKind( const char *kind ) {
	return !Q_stricmp( kind, "flat" ) || ( !Q_stricmpn( kind, "fl", 2 ) && isdigit( (unsigned char)kind[2] ) );
}

// sprites: as pictures (sprite) or as world billboards (thing)
static qboolean Doom_IsSpriteKind( const char *kind ) {
	return !Q_stricmp( kind, "sprite" ) || !Q_stricmp( kind, "thing" );
}

/*
=================
Doom_AnimFrames

The animation cycle of a texture or flat path, as lump names of
'frameSize' bytes; 0 if it is not animated.
=================
*/
int Doom_AnimFrames( const char *path, char *frames, int frameSize, int maxFrames ) {
	char	kind[16], name[DOOM_LUMP_NAME_LEN + 1];
	char	cycle[16][DOOM_LUMP_NAME_LEN + 1];
	int		i, n, isTexture;

	if ( !doom_wad || !Doom_ParsePath( path, kind, sizeof( kind ), name ) ) {
		return 0;
	}
	if ( Doom_IsWallKind( kind ) ) {
		isTexture = 1;
	} else if ( Doom_IsFlatKind( kind ) ) {
		isTexture = 0;
	} else {
		return 0;
	}
	n = doom_anim_frames( doom_wad, isTexture, name, cycle, maxFrames < 16 ? maxFrames : 16 );
	for ( i = 0 ; i < n ; i++ ) {
		Q_strncpyz( frames + i * frameSize, cycle[i], frameSize );
	}
	return n;
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

	if ( Doom_IsWallKind( kind ) ) {
		result = doom_load_texture( doom_wad, name, &img );
	} else if ( Doom_IsFlatKind( kind ) ) {
		result = doom_load_flat( doom_wad, name, &img );
	} else if ( Doom_IsSpriteKind( kind ) ) {
		result = doom_load_sprite( doom_wad, name, &img );
	} else if ( !Q_stricmp( kind, "patch" ) ) {
		result = doom_load_patch( doom_wad, name, &img );
	} else if ( !Q_stricmp( kind, "skybox" ) ) {
		return Doom_SkyboxImage( name, width, height );
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
Doom_CheckWorld_f

Validates the collision world of the running Doom map against the map
model: for every sidedef, a point just inside its sector must be open
space, traces down and up must stop exactly at the sector floor and
ceiling, and one-sided walls must stop a horizontal trace.
=================
*/
#define MASK_PLAYERSOLID	( CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY )	// bg_public.h

static void Doom_CheckWorld_f( void ) {
	const doom_map_t	*map;
	const char			*mapname = Cvar_VariableString( "mapname" );
	int					i, s, tested = 0, failed = 0, skipped = 0;
	trace_t				tr;
	vec3_t				p, end;
	doom_plane_range_t	*ranges;

	if ( !com_sv_running || !com_sv_running->integer || !( map = Doom_GetMap( mapname ) ) ) {
		Com_Printf( "doom_checkworld: run a Doom map first\n" );
		return;
	}
	// the world stops where moving planes can go furthest (the movers,
	// separate models, fill the rest)
	ranges = Z_Malloc( sizeof( doom_plane_range_t ) * map->num_sectors );
	doom_sector_ranges( doom_wad, map, ranges );

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t *ld = &map->linedefs[i];

		for ( s = 0 ; s < 2 ; s++ ) {
			const doom_vertex_t	*a, *c;
			const doom_sector_t	*sec;
			double				dx, dy, len, nx, ny;
			int					sector, bad = 0;
			const char			*why = "";

			if ( ld->side[s] == DOOM_NO_SIDE ) {
				continue;
			}
			sector = map->sidedefs[ld->side[s]].sector;
			sec = &map->sectors[sector];
			a = &map->vertexes[s ? ld->v2 : ld->v1];
			c = &map->vertexes[s ? ld->v1 : ld->v2];
			dx = c->x - a->x;
			dy = c->y - a->y;
			len = sqrt( dx * dx + dy * dy );
			if ( len < 8 || ranges[sector].ceiling_max - ranges[sector].floor_min < 8 ) {
				skipped++;
				continue;
			}
			// 4 units into the sector, which is on the right of a->c
			nx = dy / len;
			ny = -dx / len;
			p[0] = ( a->x + c->x ) * 0.5 + nx * 4;
			p[1] = ( a->y + c->y ) * 0.5 + ny * 4;
			p[2] = ( ranges[sector].floor_min + ranges[sector].ceiling_max ) * 0.5;
			if ( doom_map_sector_at( map, p[0], p[1] ) != sector ) {
				skipped++;		// thin or overlapping geometry: the point is elsewhere
				continue;
			}
			tested++;

			if ( CM_PointContents( p, 0 ) & CONTENTS_SOLID ) {
				bad = 1; why = "inside solid";
			}
			if ( !bad ) {
				VectorCopy( p, end );
				end[2] -= 8192;
				CM_BoxTrace( &tr, p, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
				if ( tr.fraction == 1 || fabs( tr.endpos[2] - ranges[sector].floor_min ) > 0.5 ) {
					bad = 1; why = va( "floor at %.1f, expected %d", tr.endpos[2], ranges[sector].floor_min );
				}
			}
			if ( !bad ) {
				VectorCopy( p, end );
				end[2] += 8192;
				CM_BoxTrace( &tr, p, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
				if ( tr.fraction == 1 || fabs( tr.endpos[2] - ranges[sector].ceiling_max ) > 0.5 ) {
					bad = 1; why = va( "ceiling at %.1f, expected %d", tr.endpos[2], ranges[sector].ceiling_max );
				}
			}
			if ( !bad && ld->side[1] == DOOM_NO_SIDE ) {
				VectorCopy( p, end );
				end[0] -= nx * 16;
				end[1] -= ny * 16;
				CM_BoxTrace( &tr, p, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
				if ( tr.fraction == 1 ) {
					bad = 1; why = "one-sided wall does not block";
				}
			}

			if ( bad ) {
				if ( failed < 20 ) {
					Com_Printf( "  linedef %d side %d (sector %d) at (%.0f %.0f %.0f): %s\n", i, s, sector, p[0], p[1], p[2], why );
				}
				failed++;
			}
		}
	}
	Com_Printf( "doom_checkworld %s: %d sides tested, %d failed, %d skipped\n", map->name, tested, failed, skipped );

	// impassable two-sided lines: jumpable windows let players through, the
	// others block; traced across the middle of the line just above the
	// higher floor, where a jumping player passes. The trace only spans
	// the 1 unit clip slab on each side, so walls behind ledges don't count.
	{
		int		windows = 0, barriers = 0, wrong = 0;

		for ( i = 0 ; i < map->num_linedefs ; i++ ) {
			const doom_linedef_t	*ld = &map->linedefs[i];
			const doom_vertex_t		*a = &map->vertexes[ld->v1], *c = &map->vertexes[ld->v2];
			const doom_sector_t		*s0, *s1;
			double					dx = c->x - a->x, dy = c->y - a->y, len = sqrt( dx * dx + dy * dy );
			int						jumpable, lo, hi;
			vec3_t					start;

			if ( !( ld->flags & DOOM_ML_BLOCKING ) || ld->side[1] == DOOM_NO_SIDE || len < 8 ) {
				continue;
			}
			s0 = &map->sectors[map->sidedefs[ld->side[0]].sector];
			s1 = &map->sectors[map->sidedefs[ld->side[1]].sector];
			lo = s0->floor_height > s1->floor_height ? s0->floor_height : s1->floor_height;
			hi = s0->ceiling_height < s1->ceiling_height ? s0->ceiling_height : s1->ceiling_height;
			if ( hi - lo < 8 ) {
				continue;		// closed: nothing to go through
			}
			jumpable = doom_linedef_is_jumpable_window( map, ld );
			jumpable ? windows++ : barriers++;

			start[0] = ( a->x + c->x ) * 0.5 + dy / len * 2;
			start[1] = ( a->y + c->y ) * 0.5 - dx / len * 2;
			start[2] = lo + 4;
			end[0] = start[0] - dy / len * 4;
			end[1] = start[1] + dx / len * 4;
			end[2] = start[2];
			CM_BoxTrace( &tr, start, end, vec3_origin, vec3_origin, 0, MASK_PLAYERSOLID, qfalse );
			if ( jumpable != ( tr.fraction == 1 ) ) {
				if ( wrong < 10 ) {
					Com_Printf( "  linedef %d: %s but %s\n", i, jumpable ? "jumpable window" : "barrier",
						tr.fraction == 1 ? "open" : "blocked" );
				}
				wrong++;
			}
		}
		Com_Printf( "doom_checkworld %s: %d jumpable windows, %d barriers, %d wrong\n", map->name, windows, barriers, wrong );
	}
	Z_Free( ranges );
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
	Cmd_AddCommand( "doom_checkworld", Doom_CheckWorld_f );

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
	int		i;

	for ( i = 0 ; i < BSP_CACHE_SIZE ; i++ ) {
		free( mapCache[i].bsp );
		doom_map_free( &mapCache[i].map );
	}
	Com_Memset( mapCache, 0, sizeof( mapCache ) );
	doom_wad_close( doom_wad );
	doom_wad = NULL;
}
