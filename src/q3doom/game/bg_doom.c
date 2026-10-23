/*
===========================================================================
Q3Doom - Doom rules shared by the game and the cgame

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "q_shared.h"
#include "bg_doom.h"

int BG_DoomStaticThing( const doom_thing_t *t, int skill ) {
	int		type, flags;

	// skill levels (P_SpawnMapThing): baby and easy, medium, hard and nightmare
	if ( skill <= 1 ) {
		flags = DOOM_MTF_EASY;
	} else if ( skill == 2 ) {
		flags = DOOM_MTF_NORMAL;
	} else {
		flags = DOOM_MTF_HARD;
	}
	if ( !( t->flags & flags ) || ( t->flags & DOOM_MTF_NOTSINGLE ) ) {
		return -1;
	}

	type = doom_mobjtype_for_ednum( t->type );
	if ( type < 0 || type == MT_PLAYER ) {
		return -1;
	}
	// monsters come with M6; invisible things (teleport destinations) are markers
	if ( ( doom_mobjinfo[type].flags & MF_COUNTKILL ) || ( doom_mobjinfo[type].flags & MF_NOSECTOR ) ) {
		return -1;
	}
	return type;
}

qboolean BG_DoomThingBlocks( const doom_map_t *map, const doom_thing_t *t, int type, vec3_t mins, vec3_t maxs ) {
	const doom_mobjinfo_t	*info = &doom_mobjinfo[type];
	int						sector;
	float					radius, height, z;

	// items are picked up, not walked around
	if ( !( info->flags & MF_SOLID ) || ( info->flags & MF_SPECIAL ) ) {
		return qfalse;
	}
	radius = (float)info->radius / DOOM_FRACUNIT;
	height = (float)info->height / DOOM_FRACUNIT;
	sector = doom_map_sector_at( map, t->x + 0.25, t->y + 0.25 );
	if ( sector < 0 ) {
		return qfalse;
	}
	z = ( info->flags & MF_SPAWNCEILING ) ? map->sectors[sector].ceiling_height - height : map->sectors[sector].floor_height;

	VectorSet( mins, t->x - radius, t->y - radius, z );
	VectorSet( maxs, t->x + radius, t->y + radius, z + height );
	return qtrue;
}
