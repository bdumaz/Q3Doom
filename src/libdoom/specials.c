/*
===========================================================================
libdoom - line specials, sector movement ranges, texture animations

The special table follows P_CrossSpecialLine, P_UseSpecialLine and
P_ShootSpecialLine of the Doom 1.9 source (p_spec.c, p_switch.c), so
Ultimate Doom and Doom II maps work. The neighbor searches mirror
P_Find*Surrounding (p_spec.c) and take height arrays, so the game can
run them on the current, moving heights.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"

#define W	DOOM_TRIG_WALK
#define S	DOOM_TRIG_SWITCH
#define M	DOOM_TRIG_MANUAL
#define G	DOOM_TRIG_GUN

#define DOOR	DOOM_KIND_DOOR
#define PLAT	DOOM_KIND_PLAT
#define FLOOR	DOOM_KIND_FLOOR
#define CEIL	DOOM_KIND_CEILING
#define STAIRS	DOOM_KIND_STAIRS
#define LIGHT	DOOM_KIND_LIGHT
#define EXIT	DOOM_KIND_EXIT
#define TELE	DOOM_KIND_TELEPORT
#define DONUT	DOOM_KIND_DONUT
#define STOP	DOOM_KIND_STOP

// special, trigger, repeatable, monsters, kind, type, key (amount for plats, level for lights)
static const doom_special_t specials[] = {
	// manual doors (use on the door line itself, act on the back sector)
	{   1, M, 1, 1, DOOR, DOOM_DOOR_NORMAL, 0 },
	{  26, M, 1, 0, DOOR, DOOM_DOOR_NORMAL, DOOM_KEY_BLUE },
	{  27, M, 1, 0, DOOR, DOOM_DOOR_NORMAL, DOOM_KEY_YELLOW },
	{  28, M, 1, 0, DOOR, DOOM_DOOR_NORMAL, DOOM_KEY_RED },
	{  31, M, 0, 0, DOOR, DOOM_DOOR_OPEN, 0 },
	{  32, M, 0, 1, DOOR, DOOM_DOOR_OPEN, DOOM_KEY_BLUE },
	{  33, M, 0, 1, DOOR, DOOM_DOOR_OPEN, DOOM_KEY_RED },
	{  34, M, 0, 1, DOOR, DOOM_DOOR_OPEN, DOOM_KEY_YELLOW },
	{ 117, M, 1, 0, DOOR, DOOM_DOOR_BLAZERAISE, 0 },
	{ 118, M, 0, 0, DOOR, DOOM_DOOR_BLAZEOPEN, 0 },

	// walk over, once
	{   2, W, 0, 0, DOOR, DOOM_DOOR_OPEN, 0 },
	{   3, W, 0, 0, DOOR, DOOM_DOOR_CLOSE, 0 },
	{   4, W, 0, 1, DOOR, DOOM_DOOR_NORMAL, 0 },
	{   5, W, 0, 0, FLOOR, DOOM_FLOOR_RAISE, 0 },
	{   6, W, 0, 0, CEIL, DOOM_CEIL_FASTCRUSHANDRAISE, 0 },
	{   8, W, 0, 0, STAIRS, DOOM_STAIRS_BUILD8, 0 },
	{  10, W, 0, 1, PLAT, DOOM_PLAT_DOWNWAITUPSTAY, 0 },
	{  12, W, 0, 0, LIGHT, DOOM_LIGHT_TURNON, 0 },
	{  13, W, 0, 0, LIGHT, DOOM_LIGHT_TURNON, 255 },
	{  16, W, 0, 0, DOOR, DOOM_DOOR_CLOSE30THENOPEN, 0 },
	{  17, W, 0, 0, LIGHT, DOOM_LIGHT_STROBE, 0 },
	{  19, W, 0, 0, FLOOR, DOOM_FLOOR_LOWER, 0 },
	{  22, W, 0, 0, PLAT, DOOM_PLAT_RAISETONEARESTANDCHANGE, 0 },
	{  25, W, 0, 0, CEIL, DOOM_CEIL_CRUSHANDRAISE, 0 },
	{  30, W, 0, 0, FLOOR, DOOM_FLOOR_RAISETOTEXTURE, 0 },
	{  35, W, 0, 0, LIGHT, DOOM_LIGHT_TURNON, 35 },
	{  36, W, 0, 0, FLOOR, DOOM_FLOOR_TURBOLOWER, 0 },
	{  37, W, 0, 0, FLOOR, DOOM_FLOOR_LOWERANDCHANGE, 0 },
	{  38, W, 0, 0, FLOOR, DOOM_FLOOR_LOWERTOLOWEST, 0 },
	{  39, W, 0, 1, TELE, 0, 0 },
	{  40, W, 0, 0, CEIL, DOOM_CEIL_RAISETOHIGHEST, 0 },		// also lowers the floor to the lowest
	{  44, W, 0, 0, CEIL, DOOM_CEIL_LOWERANDCRUSH, 0 },
	{  52, W, 0, 0, EXIT, 0, 0 },
	{  53, W, 0, 0, PLAT, DOOM_PLAT_PERPETUALRAISE, 0 },
	{  54, W, 0, 0, STOP, DOOM_KIND_PLAT, 0 },
	{  56, W, 0, 0, FLOOR, DOOM_FLOOR_RAISECRUSH, 0 },
	{  57, W, 0, 0, STOP, DOOM_KIND_CEILING, 0 },
	{  58, W, 0, 0, FLOOR, DOOM_FLOOR_RAISE24, 0 },
	{  59, W, 0, 0, FLOOR, DOOM_FLOOR_RAISE24ANDCHANGE, 0 },
	{ 100, W, 0, 0, STAIRS, DOOM_STAIRS_TURBO16, 0 },
	{ 104, W, 0, 0, LIGHT, DOOM_LIGHT_TURNOFF, 0 },
	{ 108, W, 0, 0, DOOR, DOOM_DOOR_BLAZERAISE, 0 },
	{ 109, W, 0, 0, DOOR, DOOM_DOOR_BLAZEOPEN, 0 },
	{ 110, W, 0, 0, DOOR, DOOM_DOOR_BLAZECLOSE, 0 },
	{ 119, W, 0, 0, FLOOR, DOOM_FLOOR_RAISETONEAREST, 0 },
	{ 121, W, 0, 0, PLAT, DOOM_PLAT_BLAZEDWUS, 0 },
	{ 124, W, 0, 0, EXIT, 1, 0 },		// secret exit
	{ 125, W, 0, 2, TELE, 0, 0 },		// monsters only
	{ 130, W, 0, 0, FLOOR, DOOM_FLOOR_RAISETURBO, 0 },
	{ 141, W, 0, 0, CEIL, DOOM_CEIL_SILENTCRUSHANDRAISE, 0 },

	// walk over, repeatable
	{  72, W, 1, 0, CEIL, DOOM_CEIL_LOWERANDCRUSH, 0 },
	{  73, W, 1, 0, CEIL, DOOM_CEIL_CRUSHANDRAISE, 0 },
	{  74, W, 1, 0, STOP, DOOM_KIND_CEILING, 0 },
	{  75, W, 1, 0, DOOR, DOOM_DOOR_CLOSE, 0 },
	{  76, W, 1, 0, DOOR, DOOM_DOOR_CLOSE30THENOPEN, 0 },
	{  77, W, 1, 0, CEIL, DOOM_CEIL_FASTCRUSHANDRAISE, 0 },
	{  79, W, 1, 0, LIGHT, DOOM_LIGHT_TURNON, 35 },
	{  80, W, 1, 0, LIGHT, DOOM_LIGHT_TURNON, 0 },
	{  81, W, 1, 0, LIGHT, DOOM_LIGHT_TURNON, 255 },
	{  82, W, 1, 0, FLOOR, DOOM_FLOOR_LOWERTOLOWEST, 0 },
	{  83, W, 1, 0, FLOOR, DOOM_FLOOR_LOWER, 0 },
	{  84, W, 1, 0, FLOOR, DOOM_FLOOR_LOWERANDCHANGE, 0 },
	{  86, W, 1, 0, DOOR, DOOM_DOOR_OPEN, 0 },
	{  87, W, 1, 0, PLAT, DOOM_PLAT_PERPETUALRAISE, 0 },
	{  88, W, 1, 1, PLAT, DOOM_PLAT_DOWNWAITUPSTAY, 0 },
	{  89, W, 1, 0, STOP, DOOM_KIND_PLAT, 0 },
	{  90, W, 1, 0, DOOR, DOOM_DOOR_NORMAL, 0 },
	{  91, W, 1, 0, FLOOR, DOOM_FLOOR_RAISE, 0 },
	{  92, W, 1, 0, FLOOR, DOOM_FLOOR_RAISE24, 0 },
	{  93, W, 1, 0, FLOOR, DOOM_FLOOR_RAISE24ANDCHANGE, 0 },
	{  94, W, 1, 0, FLOOR, DOOM_FLOOR_RAISECRUSH, 0 },
	{  95, W, 1, 0, PLAT, DOOM_PLAT_RAISETONEARESTANDCHANGE, 0 },
	{  96, W, 1, 0, FLOOR, DOOM_FLOOR_RAISETOTEXTURE, 0 },
	{  97, W, 1, 1, TELE, 0, 0 },
	{  98, W, 1, 0, FLOOR, DOOM_FLOOR_TURBOLOWER, 0 },
	{ 105, W, 1, 0, DOOR, DOOM_DOOR_BLAZERAISE, 0 },
	{ 106, W, 1, 0, DOOR, DOOM_DOOR_BLAZEOPEN, 0 },
	{ 107, W, 1, 0, DOOR, DOOM_DOOR_BLAZECLOSE, 0 },
	{ 120, W, 1, 0, PLAT, DOOM_PLAT_BLAZEDWUS, 0 },
	{ 126, W, 1, 2, TELE, 0, 0 },		// monsters only
	{ 128, W, 1, 0, FLOOR, DOOM_FLOOR_RAISETONEAREST, 0 },
	{ 129, W, 1, 0, FLOOR, DOOM_FLOOR_RAISETURBO, 0 },

	// switches, once
	{   7, S, 0, 0, STAIRS, DOOM_STAIRS_BUILD8, 0 },
	{   9, S, 0, 0, DONUT, 0, 0 },
	{  11, S, 0, 0, EXIT, 0, 0 },
	{  14, S, 0, 0, PLAT, DOOM_PLAT_RAISEANDCHANGE, 32 },
	{  15, S, 0, 0, PLAT, DOOM_PLAT_RAISEANDCHANGE, 24 },
	{  18, S, 0, 0, FLOOR, DOOM_FLOOR_RAISETONEAREST, 0 },
	{  20, S, 0, 0, PLAT, DOOM_PLAT_RAISETONEARESTANDCHANGE, 0 },
	{  21, S, 0, 0, PLAT, DOOM_PLAT_DOWNWAITUPSTAY, 0 },
	{  23, S, 0, 0, FLOOR, DOOM_FLOOR_LOWERTOLOWEST, 0 },
	{  29, S, 0, 0, DOOR, DOOM_DOOR_NORMAL, 0 },
	{  41, S, 0, 0, CEIL, DOOM_CEIL_LOWERTOFLOOR, 0 },
	{  49, S, 0, 0, CEIL, DOOM_CEIL_CRUSHANDRAISE, 0 },
	{  50, S, 0, 0, DOOR, DOOM_DOOR_CLOSE, 0 },
	{  51, S, 0, 0, EXIT, 1, 0 },		// secret exit
	{  55, S, 0, 0, FLOOR, DOOM_FLOOR_RAISECRUSH, 0 },
	{  71, S, 0, 0, FLOOR, DOOM_FLOOR_TURBOLOWER, 0 },
	{ 101, S, 0, 0, FLOOR, DOOM_FLOOR_RAISE, 0 },
	{ 102, S, 0, 0, FLOOR, DOOM_FLOOR_LOWER, 0 },
	{ 103, S, 0, 0, DOOR, DOOM_DOOR_OPEN, 0 },
	{ 111, S, 0, 0, DOOR, DOOM_DOOR_BLAZERAISE, 0 },
	{ 112, S, 0, 0, DOOR, DOOM_DOOR_BLAZEOPEN, 0 },
	{ 113, S, 0, 0, DOOR, DOOM_DOOR_BLAZECLOSE, 0 },
	{ 122, S, 0, 0, PLAT, DOOM_PLAT_BLAZEDWUS, 0 },
	{ 127, S, 0, 0, STAIRS, DOOM_STAIRS_TURBO16, 0 },
	{ 131, S, 0, 0, FLOOR, DOOM_FLOOR_RAISETURBO, 0 },
	{ 133, S, 0, 0, DOOR, DOOM_DOOR_BLAZEOPEN, DOOM_KEY_BLUE },
	{ 135, S, 0, 0, DOOR, DOOM_DOOR_BLAZEOPEN, DOOM_KEY_RED },
	{ 137, S, 0, 0, DOOR, DOOM_DOOR_BLAZEOPEN, DOOM_KEY_YELLOW },
	{ 140, S, 0, 0, FLOOR, DOOM_FLOOR_RAISE512, 0 },

	// switches, repeatable
	{  42, S, 1, 0, DOOR, DOOM_DOOR_CLOSE, 0 },
	{  43, S, 1, 0, CEIL, DOOM_CEIL_LOWERTOFLOOR, 0 },
	{  45, S, 1, 0, FLOOR, DOOM_FLOOR_LOWER, 0 },
	{  60, S, 1, 0, FLOOR, DOOM_FLOOR_LOWERTOLOWEST, 0 },
	{  61, S, 1, 0, DOOR, DOOM_DOOR_OPEN, 0 },
	{  62, S, 1, 0, PLAT, DOOM_PLAT_DOWNWAITUPSTAY, 0 },
	{  63, S, 1, 0, DOOR, DOOM_DOOR_NORMAL, 0 },
	{  64, S, 1, 0, FLOOR, DOOM_FLOOR_RAISE, 0 },
	{  65, S, 1, 0, FLOOR, DOOM_FLOOR_RAISECRUSH, 0 },
	{  66, S, 1, 0, PLAT, DOOM_PLAT_RAISEANDCHANGE, 24 },
	{  67, S, 1, 0, PLAT, DOOM_PLAT_RAISEANDCHANGE, 32 },
	{  68, S, 1, 0, PLAT, DOOM_PLAT_RAISETONEARESTANDCHANGE, 0 },
	{  69, S, 1, 0, FLOOR, DOOM_FLOOR_RAISETONEAREST, 0 },
	{  70, S, 1, 0, FLOOR, DOOM_FLOOR_TURBOLOWER, 0 },
	{  99, S, 1, 0, DOOR, DOOM_DOOR_BLAZEOPEN, DOOM_KEY_BLUE },
	{ 114, S, 1, 0, DOOR, DOOM_DOOR_BLAZERAISE, 0 },
	{ 115, S, 1, 0, DOOR, DOOM_DOOR_BLAZEOPEN, 0 },
	{ 116, S, 1, 0, DOOR, DOOM_DOOR_BLAZECLOSE, 0 },
	{ 123, S, 1, 0, PLAT, DOOM_PLAT_BLAZEDWUS, 0 },
	{ 132, S, 1, 0, FLOOR, DOOM_FLOOR_RAISETURBO, 0 },
	{ 134, S, 1, 0, DOOR, DOOM_DOOR_BLAZEOPEN, DOOM_KEY_RED },
	{ 136, S, 1, 0, DOOR, DOOM_DOOR_BLAZEOPEN, DOOM_KEY_YELLOW },
	{ 138, S, 1, 0, LIGHT, DOOM_LIGHT_TURNON, 255 },
	{ 139, S, 1, 0, LIGHT, DOOM_LIGHT_TURNON, 35 },

	// shot
	{  24, G, 0, 0, FLOOR, DOOM_FLOOR_RAISE, 0 },
	{  46, G, 1, 1, DOOR, DOOM_DOOR_OPEN, 0 },
	{  47, G, 0, 0, PLAT, DOOM_PLAT_RAISETONEARESTANDCHANGE, 0 },

	// effects without a trigger
	{  48, DOOM_TRIG_NONE, 0, 0, DOOM_KIND_SCROLL, 0, 0 },
};

#undef W
#undef S
#undef M
#undef G

const doom_special_t *doom_line_special( int special ) {
	int		i;

	if ( special <= 0 ) {
		return NULL;
	}
	for ( i = 0 ; i < (int)( sizeof( specials ) / sizeof( specials[0] ) ) ; i++ ) {
		if ( specials[i].special == special ) {
			return &specials[i];
		}
	}
	return NULL;
}

/*
=====================================================================

NEIGHBOR SEARCHES (P_Find*Surrounding)

'floors' and 'ceilings' are the current heights of every sector;
NULL uses the heights stored in the map.

=====================================================================
*/

#define FLOOR_OF( s )	( floors ? floors[s] : map->sectors[s].floor_height )
#define CEIL_OF( s )	( ceilings ? ceilings[s] : map->sectors[s].ceiling_height )

// the sector on the other side of a two-sided line, or -1
static int OtherSector( const doom_map_t *map, const doom_linedef_t *ld, int sector ) {
	int s0, s1;

	if ( ld->side[1] == DOOM_NO_SIDE ) {
		return -1;
	}
	s0 = map->sidedefs[ld->side[0]].sector;
	s1 = map->sidedefs[ld->side[1]].sector;
	if ( s0 == sector ) {
		return s1;
	}
	if ( s1 == sector ) {
		return s0;
	}
	return -1;
}

int doom_find_lowest_floor_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings ) {
	int		i, other, h = FLOOR_OF( sector );

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		if ( ( other = OtherSector( map, &map->linedefs[i], sector ) ) >= 0 && FLOOR_OF( other ) < h ) {
			h = FLOOR_OF( other );
		}
	}
	return h;
}

int doom_find_highest_floor_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings ) {
	int		i, other, h = -500;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		if ( ( other = OtherSector( map, &map->linedefs[i], sector ) ) >= 0 && FLOOR_OF( other ) > h ) {
			h = FLOOR_OF( other );
		}
	}
	return h;
}

int doom_find_next_highest_floor( const doom_map_t *map, int sector, int current, const int *floors, const int *ceilings ) {
	int		i, other, h = 0x7fffffff;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		if ( ( other = OtherSector( map, &map->linedefs[i], sector ) ) >= 0 && FLOOR_OF( other ) > current && FLOOR_OF( other ) < h ) {
			h = FLOOR_OF( other );
		}
	}
	return h == 0x7fffffff ? current : h;
}

int doom_find_lowest_ceiling_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings ) {
	int		i, other, h = 0x7fffffff;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		if ( ( other = OtherSector( map, &map->linedefs[i], sector ) ) >= 0 && CEIL_OF( other ) < h ) {
			h = CEIL_OF( other );
		}
	}
	return h;
}

int doom_find_highest_ceiling_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings ) {
	int		i, other, h = 0;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		if ( ( other = OtherSector( map, &map->linedefs[i], sector ) ) >= 0 && CEIL_OF( other ) > h ) {
			h = CEIL_OF( other );
		}
	}
	return h;
}

int doom_find_min_surrounding_light( const doom_map_t *map, int sector, int max, const int *lights ) {
	int		i, other, min = max;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		if ( ( other = OtherSector( map, &map->linedefs[i], sector ) ) >= 0 ) {
			int light = lights ? lights[other] : map->sectors[other].light;
			if ( light < min ) {
				min = light;
			}
		}
	}
	return min;
}

/*
=================
doom_shortest_lower_texture

Height of the shortest lower texture on the sector's two-sided lines
(raiseToTexture), 64 without a WAD.
=================
*/
int doom_shortest_lower_texture( const doom_wad_t *wad, const doom_map_t *map, int sector ) {
	int		i, s, h = 0x7fffffff;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t *ld = &map->linedefs[i];

		if ( OtherSector( map, ld, sector ) < 0 ) {
			continue;
		}
		for ( s = 0 ; s < 2 ; s++ ) {
			const doom_texture_t *tex = wad ? doom_texture( wad, doom_find_texture( wad, map->sidedefs[ld->side[s]].bottom ) ) : NULL;
			int th = tex ? tex->height : 64;
			if ( th < h ) {
				h = th;
			}
		}
	}
	return h == 0x7fffffff ? 64 : h;
}

/*
=================
doom_stair_chain

The sectors a stair builder raises, starting at 'sector' (P_BuildStairs):
following two-sided lines whose front is the current step and whose back
has the same floor flat. Returns the count; 'visited' must be cleared.
=================
*/
int doom_stair_chain( const doom_map_t *map, int sector, int *out, int max, uint8_t *visited ) {
	int		count = 0, i, found;

	if ( visited[sector] ) {
		return 0;
	}
	visited[sector] = 1;
	out[count++] = sector;

	do {
		found = 0;
		for ( i = 0 ; i < map->num_linedefs && count < max ; i++ ) {
			const doom_linedef_t	*ld = &map->linedefs[i];
			int						back;

			if ( ld->side[1] == DOOM_NO_SIDE || map->sidedefs[ld->side[0]].sector != sector ) {
				continue;
			}
			back = map->sidedefs[ld->side[1]].sector;
			if ( strcmp( map->sectors[back].floor_flat, map->sectors[sector].floor_flat ) || visited[back] ) {
				continue;
			}
			visited[back] = 1;
			out[count++] = back;
			sector = back;
			found = 1;
			break;
		}
	} while ( found && count < max );

	return count;
}

/*
=====================================================================

MOVEMENT RANGES

=====================================================================
*/

static void Extend( int *min, int *max, int value ) {
	if ( value < *min ) *min = value;
	if ( value > *max ) *max = value;
}

/*
=================
TargetSectors

The sectors a line special acts on: the back sector for manual doors,
otherwise every sector carrying the line's tag.
=================
*/
static int TargetSectors( const doom_map_t *map, const doom_linedef_t *ld, const doom_special_t *sp, int *out ) {
	int		i, count = 0;

	if ( sp->trigger == DOOM_TRIG_MANUAL ) {
		if ( ld->side[1] != DOOM_NO_SIDE ) {
			out[count++] = map->sidedefs[ld->side[1]].sector;
		}
		return count;
	}
	if ( !ld->tag ) {
		return 0;
	}
	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		if ( map->sectors[i].tag == ld->tag ) {
			out[count++] = i;
		}
	}
	return count;
}

void doom_sector_ranges( const doom_wad_t *wad, const doom_map_t *map, doom_plane_range_t *ranges ) {
	int		i, t, n, *targets;
	uint8_t	*visited;

	targets = malloc( sizeof( int ) * ( map->num_sectors + 1 ) );
	visited = calloc( map->num_sectors, 1 );

	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		ranges[i].floor_min = ranges[i].floor_max = map->sectors[i].floor_height;
		ranges[i].ceiling_min = ranges[i].ceiling_max = map->sectors[i].ceiling_height;
	}

	// sector specials that move doors on a timer
	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		doom_plane_range_t *r = &ranges[i];

		if ( map->sectors[i].special == 10 ) {		// close after 30 seconds
			Extend( &r->ceiling_min, &r->ceiling_max, map->sectors[i].floor_height );
		} else if ( map->sectors[i].special == 14 ) {	// open after 5 minutes
			Extend( &r->ceiling_min, &r->ceiling_max, doom_find_lowest_ceiling_surrounding( map, i, NULL, NULL ) - 4 );
		}
	}

	// A_BossDeath: the tag 666 sectors of E1M8 and E4M8 lower to the lowest
	// floor around, E4M6's open like a door
	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		doom_plane_range_t *r = &ranges[i];

		if ( map->sectors[i].tag != 666 ) {
			continue;
		}
		if ( !strcmp( map->name, "E1M8" ) || !strcmp( map->name, "E4M8" ) ) {
			Extend( &r->floor_min, &r->floor_max, doom_find_lowest_floor_surrounding( map, i, NULL, NULL ) );
		} else if ( !strcmp( map->name, "E4M6" ) ) {
			Extend( &r->ceiling_min, &r->ceiling_max, doom_find_lowest_ceiling_surrounding( map, i, NULL, NULL ) - 4 );
			Extend( &r->ceiling_min, &r->ceiling_max, map->sectors[i].floor_height );
		}
	}

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_special_t	*sp = doom_line_special( ld->special );

		if ( !sp ) {
			continue;
		}
		n = TargetSectors( map, ld, sp, targets );

		for ( t = 0 ; t < n ; t++ ) {
			int					s = targets[t];
			const doom_sector_t	*sec = &map->sectors[s];
			doom_plane_range_t	*r = &ranges[s];

			switch ( sp->kind ) {
			case DOOM_KIND_DOOR:
				if ( sp->type == DOOM_DOOR_CLOSE || sp->type == DOOM_DOOR_BLAZECLOSE || sp->type == DOOM_DOOR_CLOSE30THENOPEN ) {
					Extend( &r->ceiling_min, &r->ceiling_max, sec->floor_height );
				} else {
					Extend( &r->ceiling_min, &r->ceiling_max, doom_find_lowest_ceiling_surrounding( map, s, NULL, NULL ) - 4 );
					Extend( &r->ceiling_min, &r->ceiling_max, sec->floor_height );		// they close again
				}
				break;

			case DOOM_KIND_PLAT:
				switch ( sp->type ) {
				case DOOM_PLAT_PERPETUALRAISE:
					Extend( &r->floor_min, &r->floor_max, doom_find_lowest_floor_surrounding( map, s, NULL, NULL ) );
					Extend( &r->floor_min, &r->floor_max, doom_find_highest_floor_surrounding( map, s, NULL, NULL ) );
					break;
				case DOOM_PLAT_DOWNWAITUPSTAY:
				case DOOM_PLAT_BLAZEDWUS:
					Extend( &r->floor_min, &r->floor_max, doom_find_lowest_floor_surrounding( map, s, NULL, NULL ) );
					break;
				case DOOM_PLAT_RAISEANDCHANGE:
					Extend( &r->floor_min, &r->floor_max, sec->floor_height + sp->amount );
					break;
				case DOOM_PLAT_RAISETONEARESTANDCHANGE:
					Extend( &r->floor_min, &r->floor_max, doom_find_next_highest_floor( map, s, sec->floor_height, NULL, NULL ) );
					break;
				}
				break;

			case DOOM_KIND_FLOOR:
				switch ( sp->type ) {
				case DOOM_FLOOR_LOWER:
					Extend( &r->floor_min, &r->floor_max, doom_find_highest_floor_surrounding( map, s, NULL, NULL ) );
					break;
				case DOOM_FLOOR_LOWERTOLOWEST:
				case DOOM_FLOOR_LOWERANDCHANGE:
					Extend( &r->floor_min, &r->floor_max, doom_find_lowest_floor_surrounding( map, s, NULL, NULL ) );
					break;
				case DOOM_FLOOR_TURBOLOWER: {
					int dest = doom_find_highest_floor_surrounding( map, s, NULL, NULL );
					Extend( &r->floor_min, &r->floor_max, dest != sec->floor_height ? dest + 8 : dest );
					break;
				}
				case DOOM_FLOOR_RAISE:
				case DOOM_FLOOR_RAISECRUSH: {
					int dest = doom_find_lowest_ceiling_surrounding( map, s, NULL, NULL );
					if ( dest > sec->ceiling_height ) {
						dest = sec->ceiling_height;
					}
					Extend( &r->floor_min, &r->floor_max, sp->type == DOOM_FLOOR_RAISECRUSH ? dest - 8 : dest );
					break;
				}
				case DOOM_FLOOR_RAISETONEAREST:
				case DOOM_FLOOR_RAISETURBO:
					Extend( &r->floor_min, &r->floor_max, doom_find_next_highest_floor( map, s, sec->floor_height, NULL, NULL ) );
					break;
				case DOOM_FLOOR_RAISE24:
				case DOOM_FLOOR_RAISE24ANDCHANGE:
					Extend( &r->floor_min, &r->floor_max, sec->floor_height + 24 );
					break;
				case DOOM_FLOOR_RAISE512:
					Extend( &r->floor_min, &r->floor_max, sec->floor_height + 512 );
					break;
				case DOOM_FLOOR_RAISETOTEXTURE:
					Extend( &r->floor_min, &r->floor_max, sec->floor_height + doom_shortest_lower_texture( wad, map, s ) );
					break;
				}
				break;

			case DOOM_KIND_CEILING:
				switch ( sp->type ) {
				case DOOM_CEIL_LOWERTOFLOOR:
					Extend( &r->ceiling_min, &r->ceiling_max, sec->floor_height );
					break;
				case DOOM_CEIL_RAISETOHIGHEST:
					Extend( &r->ceiling_min, &r->ceiling_max, doom_find_highest_ceiling_surrounding( map, s, NULL, NULL ) );
					Extend( &r->floor_min, &r->floor_max, doom_find_lowest_floor_surrounding( map, s, NULL, NULL ) );
					break;
				default:	// crushers
					Extend( &r->ceiling_min, &r->ceiling_max, sec->floor_height + 8 );
					break;
				}
				break;

			case DOOM_KIND_STAIRS: {
				int		chain[1024], k, count, size = sp->type == DOOM_STAIRS_TURBO16 ? 16 : 8;

				count = doom_stair_chain( map, s, chain, 1024, visited );
				for ( k = 0 ; k < count ; k++ ) {
					Extend( &ranges[chain[k]].floor_min, &ranges[chain[k]].floor_max, sec->floor_height + ( k + 1 ) * size );
				}
				break;
			}

			case DOOM_KIND_DONUT: {
				// the tagged sector lowers and the ring around it rises, both to
				// the floor of the sector beyond the ring (EV_DoDonut)
				int		k, ring = -1;

				for ( k = 0 ; k < map->num_linedefs && ring < 0 ; k++ ) {
					ring = OtherSector( map, &map->linedefs[k], s );
				}
				if ( ring < 0 ) {
					break;
				}
				for ( k = 0 ; k < map->num_linedefs ; k++ ) {
					int beyond = OtherSector( map, &map->linedefs[k], ring );

					if ( beyond >= 0 && beyond != s ) {
						Extend( &ranges[ring].floor_min, &ranges[ring].floor_max, map->sectors[beyond].floor_height );
						Extend( &r->floor_min, &r->floor_max, map->sectors[beyond].floor_height );
						break;
					}
				}
				break;
			}

			default:
				break;
			}
		}
	}

	free( targets );
	free( visited );
}

/*
=====================================================================

TEXTURE ANIMATIONS AND SWITCHES

=====================================================================
*/

typedef struct {
	int			texture;		// 1 wall texture, 0 flat
	const char	*end;
	const char	*start;
} animdef_t;

// p_spec.c animdefs, all at 8 tics per frame
static const animdef_t animdefs[] = {
	{ 0, "NUKAGE3", "NUKAGE1" },
	{ 0, "FWATER4", "FWATER1" },
	{ 0, "SWATER4", "SWATER1" },
	{ 0, "LAVA4", "LAVA1" },
	{ 0, "BLOOD3", "BLOOD1" },
	{ 0, "RROCK08", "RROCK05" },
	{ 0, "SLIME04", "SLIME01" },
	{ 0, "SLIME08", "SLIME05" },
	{ 0, "SLIME12", "SLIME09" },
	{ 1, "BLODGR4", "BLODGR1" },
	{ 1, "SLADRIP3", "SLADRIP1" },
	{ 1, "BLODRIP4", "BLODRIP1" },
	{ 1, "FIREWALL", "FIREWALA" },
	{ 1, "GSTFONT3", "GSTFONT1" },
	{ 1, "FIRELAVA", "FIRELAV3" },
	{ 1, "FIREMAG3", "FIREMAG1" },
	{ 1, "FIREBLU2", "FIREBLU1" },
	{ 1, "ROCKRED3", "ROCKRED1" },
	{ 1, "BFALL4", "BFALL1" },
	{ 1, "SFALL4", "SFALL1" },
	{ 1, "WFALL4", "WFALL1" },
	{ 1, "DBRAIN4", "DBRAIN1" },
};

/*
=================
doom_anim_frames

If 'name' is part of an animation cycle, fills 'frames' with the cycle as
seen from that texture (Doom offsets every member: a texture shows the
frame 'its position + time' in the cycle) and returns the count; returns
0 otherwise. Cycles follow the order of the WAD (flats) or of
TEXTURE1/TEXTURE2 (walls), as in P_InitPicAnims.
=================
*/
int doom_anim_frames( const doom_wad_t *wad, int is_texture, const char *name, char frames[][DOOM_LUMP_NAME_LEN + 1], int max_frames ) {
	int		i;

	for ( i = 0 ; i < (int)( sizeof( animdefs ) / sizeof( animdefs[0] ) ) ; i++ ) {
		const animdef_t	*a = &animdefs[i];
		int				start, end, pos, count, k, index;

		if ( a->texture != is_texture ) {
			continue;
		}
		if ( is_texture ) {
			start = doom_find_texture( wad, a->start );
			end = doom_find_texture( wad, a->end );
			index = doom_find_texture( wad, name );
		} else {
			start = doom_wad_find_ns( wad, DOOM_NS_FLATS, a->start );
			end = doom_wad_find_ns( wad, DOOM_NS_FLATS, a->end );
			index = doom_wad_find_ns( wad, DOOM_NS_FLATS, name );
		}
		if ( start < 0 || end < 0 || index < start || index > end ) {
			continue;
		}
		count = end - start + 1;
		pos = index - start;
		if ( count < 2 ) {
			return 0;
		}
		for ( k = 0 ; k < count && k < max_frames ; k++ ) {
			int frame = start + ( pos + k ) % count;
			const char *n = is_texture ? doom_texture( wad, frame )->name : doom_wad_lump( wad, frame )->name;

			snprintf( frames[k], DOOM_LUMP_NAME_LEN + 1, "%s", n );
		}
		return k;
	}
	return 0;
}

/*
=================
doom_switch_texture

SW1xxx <-> SW2xxx when both textures exist (p_switch.c pairs them by name).
Returns 1 and the other texture in 'out', 0 if 'name' is not a switch.
=================
*/
int doom_switch_texture( const doom_wad_t *wad, const char *name, char out[DOOM_LUMP_NAME_LEN + 1] ) {
	if ( strlen( name ) < 4 || ( strncmp( name, "SW1", 3 ) && strncmp( name, "SW2", 3 ) ) ) {
		return 0;
	}
	snprintf( out, DOOM_LUMP_NAME_LEN + 1, "%s", name );
	out[2] = name[2] == '1' ? '2' : '1';
	return doom_find_texture( wad, out ) >= 0;
}
