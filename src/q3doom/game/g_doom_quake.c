/*
===========================================================================
Q3Doom - QuakeDoom Duel: Doom Duel on Quake 3 CTF maps

Team Deathmatch with g_doomDuel 1 on a Quake 3 map with both CTF flags:
the race goes from one flag base to the other (g_doom_duel.c), and Doom
monsters guard the map's weapon, armour and powerup spots, by the prize
(libdoom's doom_quake_spot_monsters). The spots and the flag bases are
noted as the entities spawn, before Doom's items replace Quake 3's; the
flags themselves are not spawned, the duel puts its goal flags there.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

#define MAX_QUAKE_SPOTS		256
#define SPOT_SEARCH			64		// the points around a spot tried when the monster doesn't fit

static struct {
	int							numSpots;
	const doom_spot_monsters_t	*rules[MAX_QUAKE_SPOTS];
	vec3_t						origins[MAX_QUAKE_SPOTS];
	qboolean					haveFlag[2];		// red, blue
	vec3_t						flags[2];
} quake;

// every level load: a round's map_restart notes the spots again
void Doom_ClearQuakeSpots( void ) {
	memset( &quake, 0, sizeof( quake ) );
}

/*
=================
Doom_QuakeSpot

G_CallSpawn, before the swap to Doom's items: notes the flag bases (not
spawned: qtrue) and the spots monsters guard
=================
*/
qboolean Doom_QuakeSpot( gentity_t *ent ) {
	const doom_spot_monsters_t	*rule;

	if ( !doomLevel.quakeDuel ) {
		return qfalse;
	}
	if ( !strcmp( ent->classname, "team_CTF_redflag" ) || !strcmp( ent->classname, "team_CTF_blueflag" ) ) {
		int side = ent->classname[9] == 'r' ? 0 : 1;

		quake.haveFlag[side] = qtrue;
		VectorCopy( ent->s.origin, quake.flags[side] );
		return qtrue;
	}
	rule = doom_quake_spot_monsters( ent->classname );
	if ( rule && quake.numSpots < MAX_QUAKE_SPOTS ) {
		quake.rules[quake.numSpots] = rule;
		VectorCopy( ent->s.origin, quake.origins[quake.numSpots] );
		quake.numSpots++;
	}
	return qfalse;
}

qboolean Doom_QuakeFlag( int team, vec3_t origin ) {
	int side = team == TEAM_RED ? 0 : 1;

	if ( !quake.haveFlag[side] ) {
		return qfalse;
	}
	VectorCopy( quake.flags[side], origin );
	return qtrue;
}

/*
=================
Doom_SpawnQuakeMonsters

The monsters of every spot: one type of the spot's choices, on the spot or
the first point around it (N, NE, E ... NW) where it fits, away from the
sides' starts
=================
*/
void Doom_SpawnQuakeMonsters( void ) {
	static const float	around[9][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 } };
	int					i, k, p, monsters = 0, noRoom = 0;

	if ( !Doom_DuelActive() || !g_doomMonsters.integer || !g_duelMonsters.integer ) {
		return;
	}
	for ( i = 0 ; i < quake.numSpots ; i++ ) {
		const doom_spot_monsters_t	*rule = quake.rules[i];
		const float					*o = quake.origins[i];
		int							type = rule->types[Doom_Random() % rule->numTypes];

		for ( k = 0 ; k < rule->count ; k++ ) {
			for ( p = 0 ; p < 9 ; p++ ) {
				float x = o[0] + around[p][0] * SPOT_SEARCH, y = o[1] + around[p][1] * SPOT_SEARCH;

				if ( !Doom_DuelNearSpawn( x, y ) && Doom_MobjFits( type, x, y, o[2] ) ) {
					Doom_SpawnMobjAt( type, x, y, o[2], ( Doom_Random() & 7 ) * 45 );
					doomLevel.totalKills++;
					monsters++;
					break;
				}
			}
			if ( p == 9 ) {
				noRoom++;
			}
		}
	}
	G_Printf( "QuakeDoom Duel: %d monsters at %d spots (%d without room)\n", monsters, quake.numSpots, noRoom );
}
