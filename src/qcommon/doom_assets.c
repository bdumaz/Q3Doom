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
	// the name asked for: a DoomFrag copy (DF_E1M1) is its level (E1M1) inside
	Q_strncpyz( c->name, name, sizeof( c->name ) );
	Q_strupr( c->name );
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

The sides look further up and down than Doom ever did, beyond the top
and bottom rows of the texture: its last SKY_FADE_ROWS rows fade into
the flat colors, so nothing is stretched there and no edge of the box
shows.
=================
*/
#define SKY_FOCAL		160.0
#define SKY_HORIZON_ROW	100.0
#define SKY_FADE_ROWS	32.0

static byte *Doom_SkyboxImage( const char *name, int *width, int *height ) {
	char			base[DOOM_LUMP_NAME_LEN + 1], *face;
	doom_image_t	sky;
	byte			*pic, *src, *dst;
	int				x, y, w, h, row, edge[2][3], k;
	double			dx, dy, u, t, f;
	int				tu, tv;

	Q_strncpyz( base, name, sizeof( base ) );
	face = strrchr( base, '_' );
	if ( !face ) {
		return NULL;
	}
	*face++ = 0;
	if ( doom_load_texture( doom_wad, base, &sky ) ) {
		return NULL;
	}

	// average colors of the top and bottom rows of the sky
	for ( row = 0 ; row < 2 ; row++ ) {
		y = row ? sky.height - 1 : 0;
		edge[row][0] = edge[row][1] = edge[row][2] = 0;
		for ( x = 0 ; x < sky.width ; x++ ) {
			for ( k = 0 ; k < 3 ; k++ ) {
				edge[row][k] += sky.rgba[( y * sky.width + x ) * 4 + k];
			}
		}
		for ( k = 0 ; k < 3 ; k++ ) {
			edge[row][k] /= sky.width;
		}
	}

	if ( !Q_stricmp( face, "UP" ) || !Q_stricmp( face, "DN" ) ) {
		row = !Q_stricmp( face, "UP" ) ? 0 : 1;
		w = h = 8;
		pic = Z_Malloc( w * h * 4 );
		for ( x = 0 ; x < w * h ; x++ ) {
			for ( k = 0 ; k < 3 ; k++ ) {
				pic[x * 4 + k] = edge[row][k];
			}
			pic[x * 4 + 3] = 255;
		}
	} else {
		w = h = sky.width;
		pic = Z_Malloc( w * h * 4 );
		for ( y = 0 ; y < h ; y++ ) {
			for ( x = 0 ; x < w ; x++ ) {
				// direction through this pixel of a 90 degree face at distance 1
				dx = ( x + 0.5 ) / ( w * 0.5 ) - 1.0;
				dy = 1.0 - ( y + 0.5 ) / ( h * 0.5 );
				u = ( atan( dx ) / ( M_PI * 0.5 ) + 0.5 ) * sky.width;
				t = SKY_HORIZON_ROW - SKY_FOCAL * dy / sqrt( 1.0 + dx * dx );
				tu = (int)u;
				tv = (int)t;

				if ( tu < 0 ) tu = 0;
				if ( tu >= sky.width ) tu = sky.width - 1;
				if ( tv < 0 ) tv = 0;
				if ( tv >= sky.height ) tv = sky.height - 1;
				src = sky.rgba + ( tv * sky.width + tu ) * 4;
				dst = pic + ( y * w + x ) * 4;

				// fade toward the top or the bottom color near the edge rows
				row = t < sky.height * 0.5 ? 0 : 1;
				f = row ? ( t - ( sky.height - SKY_FADE_ROWS ) ) / SKY_FADE_ROWS : ( SKY_FADE_ROWS - t ) / SKY_FADE_ROWS;
				if ( f < 0 ) f = 0;
				if ( f > 1 ) f = 1;
				for ( k = 0 ; k < 3 ; k++ ) {
					dst[k] = (byte)( src[k] + ( edge[row][k] - src[k] ) * f + 0.5 );
				}
				dst[3] = 255;
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

// sprites: as pictures (sprite) or as world billboards (thing); a digit
// after the kind is a marine colour (thing3: red)
static int Doom_SpriteKindTranslation( const char *kind ) {
	const char *digit;

	if ( !Q_stricmpn( kind, "sprite", 6 ) ) {
		digit = kind + 6;
	} else if ( !Q_stricmpn( kind, "thing", 5 ) ) {
		digit = kind + 5;
	} else {
		return -1;
	}
	if ( !digit[0] ) {
		return 0;
	}
	if ( digit[0] >= '1' && digit[0] < '0' + DOOM_NUM_TRANSLATIONS && !digit[1] ) {
		return digit[0] - '0';
	}
	return -1;
}

static qboolean Doom_IsSpriteKind( const char *kind ) {
	return Doom_SpriteKindTranslation( kind ) >= 0;
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
Doom_GeneratedImage

doom/gen/<NAME>: pictures the campaign menu needs that Doom does not have,
64x64: LOCK (a padlock), MEDAL1-4 (bronze, silver, gold, and the dark red
disc the skull medal is drawn on)
=================
*/
static byte *Doom_GeneratedImage( const char *name, int *width, int *height ) {
	static const byte	medals[4][3] = { { 205, 127, 50 }, { 192, 192, 200 }, { 255, 205, 40 }, { 120, 10, 10 } };
	byte				*pic;
	int					x, y, medal = -1;

	if ( !Q_stricmpn( name, "MEDAL", 5 ) && name[5] >= '1' && name[5] <= '4' ) {
		medal = name[5] - '1';
	} else if ( Q_stricmp( name, "LOCK" ) ) {
		return NULL;
	}
	*width = *height = 64;
	pic = Z_Malloc( 64 * 64 * 4 );
	Com_Memset( pic, 0, 64 * 64 * 4 );

	for ( y = 0 ; y < 64 ; y++ ) {
		for ( x = 0 ; x < 64 ; x++ ) {
			byte	*p = pic + ( y * 64 + x ) * 4;
			float	dx = x + 0.5f - 32, dy = y + 0.5f - 32, r = sqrt( dx * dx + dy * dy ), shade;
			int		c, color[3];

			if ( medal >= 0 ) {
				// a disc with a darker rim, lit from the top left
				if ( r > 29 ) {
					continue;
				}
				shade = 0.85f - ( dx + dy ) / 29 * 0.25f;
				if ( r > 25 ) {
					shade *= 0.6f;
				}
				for ( c = 0 ; c < 3 ; c++ ) {
					color[c] = (int)( medals[medal][c] * shade );
					p[c] = color[c] > 255 ? 255 : color[c];
				}
				p[3] = r > 28 ? (byte)( 255 * ( 29 - r ) ) : 255;
				continue;
			}
			// the padlock: a grey shackle over a brass body with a keyhole
			dx = x + 0.5f - 32;
			dy = y + 0.5f - 28;
			r = sqrt( dx * dx + dy * dy );
			if ( y < 30 && r >= 9 && r <= 14 && dy <= 0 ) {
				p[0] = p[1] = p[2] = (byte)( 170 - dy * 2 );
				p[3] = 255;
			} else if ( y >= 22 && y < 30 && ( ( x >= 18 && x < 23 ) || ( x >= 41 && x < 46 ) ) ) {
				p[0] = p[1] = p[2] = 150;
				p[3] = 255;
			} else if ( x >= 12 && x < 52 && y >= 28 && y < 60 ) {
				float kx = x + 0.5f - 32, ky = y + 0.5f - 40;

				if ( kx * kx + ky * ky < 16 || ( fabs( kx ) < 2 && ky > 0 && ky < 10 ) ) {
					p[0] = p[1] = p[2] = 20;
				} else {
					shade = 1.0f - ( y - 28 ) / 64.0f - ( x - 12 ) / 160.0f;
					p[0] = (byte)( 210 * shade );
					p[1] = (byte)( 160 * shade );
					p[2] = (byte)( 60 * shade );
				}
				p[3] = 255;
			}
		}
	}
	return pic;
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
		result = doom_load_sprite_translated( doom_wad, name, Doom_SpriteKindTranslation( kind ), &img );
	} else if ( !Q_stricmp( kind, "patch" ) ) {
		result = doom_load_patch( doom_wad, name, &img );
	} else if ( !Q_stricmp( kind, "skybox" ) ) {
		return Doom_SkyboxImage( name, width, height );
	} else if ( !Q_stricmp( kind, "gen" ) ) {
		return Doom_GeneratedImage( name, width, height );
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
=====================================================================

BOT NAVIGATION

=====================================================================
*/

#ifdef _WIN32
#define EXE_SUFFIX	".exe"
#else
#define EXE_SUFFIX	""
#endif

/*
=================
Doom_BuildBotNavigation

The bots' navigation of a Doom level (maps/<NAME>.aas in the home game
folder), made by bspc (next to the executable) from the level's BSP as
the engine builds it, so its checksum is the one the bots check. Made only
when missing, unless 'force'. Returns qtrue if the level has it now.
=================
*/
qboolean Doom_BuildBotNavigation( const char *name, qboolean force ) {
	char		bsp[MAX_QPATH], aas[MAX_QPATH], bspPath[MAX_OSPATH], dir[MAX_OSPATH], exe[MAX_OSPATH], *slash;
	const byte	*data;
	int			size, start, code;

	Com_sprintf( bsp, sizeof( bsp ), "maps/%s.bsp", name );
	Com_sprintf( aas, sizeof( aas ), "maps/%s.aas", name );
	data = Doom_GetMapBSP( bsp, &size );
	if ( !data ) {
		return qfalse;		// not a Doom level
	}
	// kept while it was made from this very BSP (the bots check its checksum:
	// a builder change makes it out of date)
	if ( !force ) {
		void	*header;
		int		len = FS_ReadFile( aas, &header );

		if ( len >= 12 ) {
			int stored = LittleLong( ( (int *)header )[2] );

			FS_FreeFile( header );
			if ( stored == LittleLong( Com_BlockChecksum( data, size ) ) ) {
				return qtrue;
			}
			Com_Printf( "The bots' navigation of %s is out of date\n", name );
		} else if ( len > 0 ) {
			FS_FreeFile( header );
		}
	}
	Com_sprintf( exe, sizeof( exe ), "%s%cbspc%s", Sys_DefaultInstallPath(), PATH_SEP, EXE_SUFFIX );

	Com_Printf( "Making the bots' navigation of %s...\n", name );
	start = Sys_Milliseconds();
	FS_WriteFile( bsp, data, size );
	Q_strncpyz( bspPath, FS_HomeOSPath( bsp ), sizeof( bspPath ) );
	Q_strncpyz( dir, bspPath, sizeof( dir ) );
	slash = strrchr( dir, PATH_SEP );
	if ( slash ) {
		*slash = 0;
	}
	code = Sys_RunProcess( exe, va( "-bsp2aas \"%s\" -output \"%s\"", bspPath, dir ), dir );
	// kept with developer 1, to look into
	if ( !com_developer || !com_developer->integer ) {
		remove( bspPath );
		remove( va( "%s%cbspc.log", dir, PATH_SEP ) );
	}

	if ( code < 0 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s did not start: no bot navigation for %s\n", exe, name );
		return qfalse;
	}
	if ( FS_ReadFile( aas, NULL ) <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: bspc could not make the bot navigation of %s (exit code %d)\n", name, code );
		return qfalse;
	}
	Com_Printf( "...done in %d msec\n", Sys_Milliseconds() - start );
	return qtrue;
}

/*
=================
Doom_BotNavigation_f

doom_botnav <map>: makes (again) the bots' navigation of a Doom level;
doom_botnav all: of every level that has none yet
=================
*/
static void Doom_BotNavigation_f( void ) {
	char	maps[64][DOOM_LUMP_NAME_LEN + 1];
	int		i, n, made = 0;

	if ( !doom_wad ) {
		Com_Printf( "No Doom IWAD loaded\n" );
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: doom_botnav <map|all>\n" );
		return;
	}
	if ( Q_stricmp( Cmd_Argv( 1 ), "all" ) ) {
		if ( !Doom_BuildBotNavigation( Cmd_Argv( 1 ), qtrue ) ) {
			Com_Printf( "doom_botnav: no navigation made for %s\n", Cmd_Argv( 1 ) );
		}
		return;
	}
	n = doom_wad_list_maps( doom_wad, maps, 64 );
	for ( i = 0 ; i < n && i < 64 ; i++ ) {
		made += Doom_BuildBotNavigation( maps[i], qfalse );
	}
	Com_Printf( "doom_botnav: %d of %d levels have their bot navigation\n", made, n < 64 ? n : 64 );
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
	Cmd_AddCommand( "doom_botnav", Doom_BotNavigation_f );

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
