/*
===========================================================================
Q3Doom - Doom rules shared by the game and the cgame

Both sides must agree exactly (prediction), so these only depend on the
map, the skill and the Doom tables.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#ifndef BG_DOOM_H
#define BG_DOOM_H

#include "../../libdoom/doom.h"
#include "../../libdoom/doom_info.h"

#define DOOM_DEFAULT_SKILL	"2"		// "Hurt me plenty"

// a map thing that is a static decoration or item in this milestone:
// its mobj type, or -1 if it is not spawned (wrong skill, multiplayer only,
// player starts, monsters, invisible markers)
int		BG_DoomStaticThing( const doom_thing_t *t, int skill );

// a static thing that blocks movement: fills its box and returns qtrue
qboolean BG_DoomThingBlocks( const doom_map_t *map, const doom_thing_t *t, int type, vec3_t mins, vec3_t maxs );

#endif
