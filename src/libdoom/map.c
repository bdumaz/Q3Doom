/*
===========================================================================
libdoom - map model loading and validation (Doom format maps)

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"

#include <ctype.h>

// map lumps follow the marker in this order (doomdata.h ML_*)
enum {
	ML_LABEL,
	ML_THINGS,
	ML_LINEDEFS,
	ML_SIDEDEFS,
	ML_VERTEXES,
	ML_SEGS,
	ML_SSECTORS,
	ML_NODES,
	ML_SECTORS,
	ML_REJECT,
	ML_BLOCKMAP
};

#define THING_SIZE		10
#define LINEDEF_SIZE	14
#define SIDEDEF_SIZE	30
#define VERTEX_SIZE		4
#define SECTOR_SIZE		26

static const char *mapLumpNames[] = {
	NULL, "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS"
};

static int IsMapMarker( const doom_wad_t *wad, int lump ) {
	return lump + ML_SECTORS < wad->num_lumps
		&& !strcmp( wad->lumps[lump + ML_THINGS].name, "THINGS" )
		&& !strcmp( wad->lumps[lump + ML_LINEDEFS].name, "LINEDEFS" );
}

int doom_wad_list_maps( const doom_wad_t *wad, char names[][DOOM_LUMP_NAME_LEN + 1], int max_names ) {
	int		i, count = 0;

	for ( i = 0 ; i < wad->num_lumps ; i++ ) {
		if ( IsMapMarker( wad, i ) ) {
			if ( count < max_names ) {
				strcpy( names[count], wad->lumps[i].name );
			}
			count++;
		}
	}
	return count;
}

/*
=================
MapLump

Returns the data of one lump of the map starting at 'marker' and its
element count, or NULL if the lump is missing or has the wrong name.
=================
*/
static const uint8_t *MapLump( const doom_wad_t *wad, int marker, int which, int elemsize, int *count ) {
	const doom_lump_t	*l = &wad->lumps[marker + which];

	if ( strcmp( l->name, mapLumpNames[which] ) ) {
		return NULL;
	}
	*count = (int)( l->size / elemsize );
	return doom_wad_lump_data( wad, marker + which );
}

// texture names: Doom treats a name starting with '-' as "no texture"
static void CopyTextureName( char dst[DOOM_LUMP_NAME_LEN + 1], const uint8_t *src ) {
	doom_copy_name( dst, src );
	if ( !dst[0] ) {
		strcpy( dst, "-" );
	}
}

static int SideIndex( uint16_t raw ) {
	return raw == 0xFFFF ? DOOM_NO_SIDE : raw;
}

int doom_map_load( const doom_wad_t *wad, const char *name, doom_map_t *out, char *err, size_t errlen ) {
	int				marker = -1, i, n;
	const uint8_t	*d;

	memset( out, 0, sizeof( *out ) );

	for ( i = wad->num_lumps - 1 ; i >= 0 ; i-- ) {
		if ( doom_name_equal( wad->lumps[i].name, name ) && IsMapMarker( wad, i ) ) {
			marker = i;
			break;
		}
	}
	if ( marker < 0 ) {
		doom_set_error( err, errlen, "map %s not found", name );
		return -1;
	}
	strcpy( out->name, wad->lumps[marker].name );

	if ( !( d = MapLump( wad, marker, ML_VERTEXES, VERTEX_SIZE, &n ) ) ) {
		goto missing;
	}
	out->num_vertexes = n;
	out->vertexes = calloc( n ? n : 1, sizeof( doom_vertex_t ) );
	for ( i = 0 ; i < n ; i++, d += VERTEX_SIZE ) {
		out->vertexes[i].x = doom_s16( d );
		out->vertexes[i].y = doom_s16( d + 2 );
	}

	if ( !( d = MapLump( wad, marker, ML_SECTORS, SECTOR_SIZE, &n ) ) ) {
		goto missing;
	}
	out->num_sectors = n;
	out->sectors = calloc( n ? n : 1, sizeof( doom_sector_t ) );
	for ( i = 0 ; i < n ; i++, d += SECTOR_SIZE ) {
		doom_sector_t *s = &out->sectors[i];

		s->floor_height = doom_s16( d );
		s->ceiling_height = doom_s16( d + 2 );
		doom_copy_name( s->floor_flat, d + 4 );
		doom_copy_name( s->ceiling_flat, d + 12 );
		s->light = doom_s16( d + 20 );
		s->special = doom_s16( d + 22 );
		s->tag = doom_s16( d + 24 );
	}

	if ( !( d = MapLump( wad, marker, ML_SIDEDEFS, SIDEDEF_SIZE, &n ) ) ) {
		goto missing;
	}
	out->num_sidedefs = n;
	out->sidedefs = calloc( n ? n : 1, sizeof( doom_sidedef_t ) );
	for ( i = 0 ; i < n ; i++, d += SIDEDEF_SIZE ) {
		doom_sidedef_t *s = &out->sidedefs[i];

		s->x_offset = doom_s16( d );
		s->y_offset = doom_s16( d + 2 );
		CopyTextureName( s->top, d + 4 );
		CopyTextureName( s->bottom, d + 12 );
		CopyTextureName( s->mid, d + 20 );
		s->sector = doom_s16( d + 28 );
	}

	if ( !( d = MapLump( wad, marker, ML_LINEDEFS, LINEDEF_SIZE, &n ) ) ) {
		goto missing;
	}
	out->num_linedefs = n;
	out->linedefs = calloc( n ? n : 1, sizeof( doom_linedef_t ) );
	for ( i = 0 ; i < n ; i++, d += LINEDEF_SIZE ) {
		doom_linedef_t *l = &out->linedefs[i];

		l->v1 = doom_u16( d );
		l->v2 = doom_u16( d + 2 );
		l->flags = doom_u16( d + 4 );
		l->special = doom_s16( d + 6 );
		l->tag = doom_s16( d + 8 );
		l->side[0] = SideIndex( doom_u16( d + 10 ) );
		l->side[1] = SideIndex( doom_u16( d + 12 ) );
	}

	if ( !( d = MapLump( wad, marker, ML_THINGS, THING_SIZE, &n ) ) ) {
		goto missing;
	}
	out->num_things = n;
	out->things = calloc( n ? n : 1, sizeof( doom_thing_t ) );
	for ( i = 0 ; i < n ; i++, d += THING_SIZE ) {
		doom_thing_t *t = &out->things[i];

		t->x = doom_s16( d );
		t->y = doom_s16( d + 2 );
		t->angle = doom_s16( d + 4 );
		t->type = doom_s16( d + 6 );
		t->flags = doom_s16( d + 8 );
	}

	return 0;

missing:
	doom_set_error( err, errlen, "map %s is missing lumps", name );
	doom_map_free( out );
	return -1;
}

void doom_map_free( doom_map_t *map ) {
	free( map->vertexes );
	free( map->linedefs );
	free( map->sidedefs );
	free( map->sectors );
	free( map->things );
	memset( map, 0, sizeof( *map ) );
}

/*
=================
SectorIsEnclosed

True if a Doom player can never walk into the sector (a window sill between
two rooms, or above a lower yard): each of its two-sided lines is flagged
impassable, or leads to a sector too low to step up from (more than a Doom
step) or through an opening too low for a player
=================
*/
#define DOOM_PLAYER_HEIGHT	56

static int SectorIsEnclosed( const doom_map_t *map, int sector ) {
	const doom_sector_t	*sec = &map->sectors[sector];
	int					i;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_sector_t		*other;
		int						s0, s1, top, bottom;

		if ( ld->side[1] == DOOM_NO_SIDE || ( ld->flags & DOOM_ML_BLOCKING ) ) {
			continue;
		}
		s0 = map->sidedefs[ld->side[0]].sector;
		s1 = map->sidedefs[ld->side[1]].sector;
		if ( ( s0 != sector && s1 != sector ) || s0 == s1 ) {
			continue;
		}
		other = &map->sectors[s0 == sector ? s1 : s0];
		top = sec->ceiling_height < other->ceiling_height ? sec->ceiling_height : other->ceiling_height;
		bottom = sec->floor_height > other->floor_height ? sec->floor_height : other->floor_height;
		if ( sec->floor_height - other->floor_height <= DOOM_MAX_STEP && top - bottom >= DOOM_PLAYER_HEIGHT ) {
			return 0;		// a way in
		}
	}
	return 1;
}

/*
=================
doom_linedef_is_jumpable_window

Doom mappers often flag window openings impassable even though Doom's 24
unit step already made them uncrossable; with Quake 3 jumping that flag
only adds an invisible wall. A flagged two-sided line is a window players
may jump through when no masked middle texture (bars, grates) fills the
opening and either its floors differ by more than a Doom step, or one side
is a sill sector no Doom player could enter (enclosed by impassable lines).
Flagged lines between walkable sectors are deliberate barriers. Whether the
opening is high or large enough is left to the collision.
=================
*/
int doom_linedef_is_jumpable_window( const doom_map_t *map, const doom_linedef_t *ld ) {
	const doom_sidedef_t	*s0, *s1;
	int						f0, f1;

	if ( !( ld->flags & DOOM_ML_BLOCKING ) || ld->side[0] == DOOM_NO_SIDE || ld->side[1] == DOOM_NO_SIDE ) {
		return 0;
	}
	s0 = &map->sidedefs[ld->side[0]];
	s1 = &map->sidedefs[ld->side[1]];
	if ( strcmp( s0->mid, "-" ) || strcmp( s1->mid, "-" ) ) {
		return 0;
	}
	f0 = map->sectors[s0->sector].floor_height;
	f1 = map->sectors[s1->sector].floor_height;
	if ( abs( f0 - f1 ) > DOOM_MAX_STEP ) {
		return 1;
	}
	// a sill: a step up from the room, but a sector only reachable by jumping
	return SectorIsEnclosed( map, s0->sector ) || SectorIsEnclosed( map, s1->sector );
}

int doom_map_validate( const doom_map_t *map, char *err, size_t errlen ) {
	int		i, s;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t *l = &map->linedefs[i];

		if ( l->v1 < 0 || l->v1 >= map->num_vertexes || l->v2 < 0 || l->v2 >= map->num_vertexes ) {
			doom_set_error( err, errlen, "linedef %d: bad vertex", i );
			return -1;
		}
		if ( l->side[0] == DOOM_NO_SIDE ) {
			doom_set_error( err, errlen, "linedef %d: no front side", i );
			return -1;
		}
		for ( s = 0 ; s < 2 ; s++ ) {
			if ( l->side[s] != DOOM_NO_SIDE && ( l->side[s] < 0 || l->side[s] >= map->num_sidedefs ) ) {
				doom_set_error( err, errlen, "linedef %d: bad sidedef %d", i, l->side[s] );
				return -1;
			}
		}
	}
	for ( i = 0 ; i < map->num_sidedefs ; i++ ) {
		if ( map->sidedefs[i].sector < 0 || map->sidedefs[i].sector >= map->num_sectors ) {
			doom_set_error( err, errlen, "sidedef %d: bad sector %d", i, map->sidedefs[i].sector );
			return -1;
		}
	}
	return 0;
}
