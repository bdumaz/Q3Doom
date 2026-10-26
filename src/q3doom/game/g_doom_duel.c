/*
===========================================================================
Q3Doom - Doom Duel

A race through a Doom level, in rounds (Team Deathmatch with g_doomDuel 1):
one team starts at the level's start, the other at its exit, and each must
reach the other's starting point, marked by a Quake 3 CTF flag in its
colour. In a team game, g_duelGoalPlayers players of a team must get there;
those who have arrived watch the rest of the round.

- a round starts with a 3, 2, 1 countdown, the players frozen
- the dead come back after 3 seconds where they started, which is where the
  other side is going; dead monsters come back with them (nightmare's own
  timed return is off)
- the first team to g_duelRounds round wins takes the match
- every round reloads the level (map_restart) with the sides swapped; the
  round and the scores go through g_duelState
- everyone has all the keys, and the level's own exits do nothing

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

typedef enum {
	DUEL_WAITING,		// for a player on each team
	DUEL_COUNTDOWN,		// 3, 2, 1: frozen
	DUEL_PLAYING,
	DUEL_ROUNDOVER		// a team got there: the next round or the end soon
} duelState_t;

#define DUEL_COUNTDOWN_TIME		3000
#define DUEL_ROUNDOVER_TIME		3000
#define DUEL_RESPAWN_TIME		3000

static struct {
	qboolean	active;
	duelState_t	state;
	int			stateTime;		// when the countdown or the round's end runs out
	int			round;			// from 1
	int			scores[TEAM_NUM_TEAMS];
	vec3_t		sides[2];		// 0: the level's start, 1: its exit, on the floor
	float		sideYaw[2];
	float		spawns[2][2];	// where each side's players appear: a step ahead of its flag
	gentity_t	*goals[2];		// the flag on each side
	qboolean	finished[MAX_CLIENTS];	// reached the goal this round
} duel;

qboolean Doom_DuelActive( void ) {
	return g_doomDuel.integer && g_gametype.integer == GT_TEAM && DOOM_LEVEL();
}

/*
=====================================================================

SIDES

=====================================================================
*/

// the side a team starts on: red at the start in odd rounds, then swapped
static int TeamSide( int team ) {
	int side = team == TEAM_RED ? 0 : 1;

	return ( duel.round & 1 ) ? side : side ^ 1;
}

static const char *DuelTeamName( int team ) {
	return team == TEAM_RED ? S_COLOR_RED "Red" S_COLOR_WHITE : S_COLOR_BLUE "Blue" S_COLOR_WHITE;
}

/*
=================
FindStart

Doom's player 1 start (thing 1)
=================
*/
static qboolean FindStart( vec3_t point, float *yaw ) {
	gentity_t	*spot = NULL;

	while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL ) {
		if ( spot->count == 1 ) {
			VectorCopy( spot->s.origin, point );
			*yaw = spot->s.angles[YAW];
			return qtrue;
		}
	}
	return qfalse;
}

/*
=================
FindExit

The level's exit: in front of its exit switch, or on its walk-over exit
line, facing back into the level; a secret exit if there is no other
=================
*/
static qboolean ExitSpot( int line, vec3_t point, float *yaw ) {
	const doom_map_t		*map = doomLevel.map;
	const doom_linedef_t	*ld = &map->linedefs[line];
	const doom_vertex_t		*a = &map->vertexes[ld->v1], *b = &map->vertexes[ld->v2];
	float					dx = b->x - a->x, dy = b->y - a->y, len = sqrt( dx * dx + dy * dy ), dist;
	int						sector;

	if ( len < 1 || ld->side[0] == DOOM_NO_SIDE ) {
		return qfalse;
	}
	// the front side is on the right of v1 -> v2
	for ( dist = 40 ; dist >= 8 ; dist -= 16 ) {
		point[0] = ( a->x + b->x ) * 0.5f + dy / len * dist;
		point[1] = ( a->y + b->y ) * 0.5f - dx / len * dist;
		sector = doom_map_sector_at( map, point[0], point[1] );
		if ( sector == map->sidedefs[ld->side[0]].sector ) {
			point[2] = doomLevel.floor[sector];
			*yaw = RAD2DEG( atan2( -dx, dy ) );
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean FindExit( const vec3_t start, vec3_t point, float *yaw ) {
	static const int	exits[][2] = { { 11, 52 }, { 51, 124 } };		// normal, then secret
	const doom_map_t	*map = doomLevel.map;
	gentity_t			*spot = NULL, *far = NULL;
	float				best = -1;
	int					pass, i;

	for ( pass = 0 ; pass < 2 ; pass++ ) {
		for ( i = 0 ; i < map->num_linedefs ; i++ ) {
			int special = map->linedefs[i].special;

			if ( ( special == exits[pass][0] || special == exits[pass][1] ) && ExitSpot( i, point, yaw ) ) {
				return qtrue;
			}
		}
	}
	// boss levels end with the boss: the deathmatch start farthest away
	while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL ) {
		float d = Distance( spot->s.origin, start );

		if ( spot->count == 11 && d > best ) {
			best = d;
			far = spot;
		}
	}
	if ( !far ) {
		return qfalse;
	}
	VectorCopy( far->s.origin, point );
	*yaw = far->s.angles[YAW];
	return qtrue;
}

/*
=================
Doom_DuelSpawnSpot

ClientSpawn: a free spot on the player's side, around its point; the
side's goal entity stands for the spawn point
=================
*/
gentity_t *Doom_DuelSpawnSpot( gentity_t *ent, vec3_t origin, vec3_t angles ) {
	static const float	offsets[][2] = {
		{ 0, 0 }, { 40, 0 }, { -40, 0 }, { 0, 40 }, { 0, -40 }, { 40, 40 }, { -40, 40 }, { 40, -40 }, { -40, -40 },
		{ 80, 0 }, { -80, 0 }, { 0, 80 }, { 0, -80 }
	};
	int		team = ent->client->sess.sessionTeam, side, i, n = sizeof( offsets ) / sizeof( offsets[0] );
	trace_t	tr;
	vec3_t	p, mins = { -15, -15, MINS_Z }, maxs = { 15, 15, 32 };		// g_client.c's player box

	if ( !duel.active || ( team != TEAM_RED && team != TEAM_BLUE ) ) {
		return NULL;
	}
	side = TeamSide( team );
	maxs[2] = g_playerHeight.value > 0 ? g_playerHeight.value + MINS_Z : maxs[2];
	for ( i = 0 ; i < n ; i++ ) {
		int sector;

		VectorSet( p, duel.spawns[side][0] + offsets[i][0], duel.spawns[side][1] + offsets[i][1], 0 );
		sector = doom_map_sector_at( doomLevel.map, p[0], p[1] );
		if ( sector < 0 ) {
			continue;
		}
		// the player's box standing on the floor, clear of walls and players
		p[2] = doomLevel.floor[sector] - MINS_Z + 1;
		trap_Trace( &tr, p, mins, maxs, p, ent->s.number, MASK_PLAYERSOLID );
		if ( !tr.startsolid && !tr.allsolid ) {
			break;
		}
	}
	if ( i == n ) {
		VectorCopy( duel.sides[side], p );		// telefrags whoever is there
	} else {
		p[2] = doomLevel.floor[doom_map_sector_at( doomLevel.map, p[0], p[1] )];
	}
	p[2] += 9;		// as Doom's starts
	VectorCopy( p, origin );
	VectorSet( angles, 0, duel.sideYaw[side], 0 );
	return duel.goals[side];
}

/*
=====================================================================

GOALS

=====================================================================
*/

static void RoundWon( int team );

static int TeamPlayers( int team ) {
	return TeamCount( -1, team );
}

static int FinishedPlayers( int team ) {
	int		i, n = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( duel.finished[i] && level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam == team ) {
			n++;
		}
	}
	return n;
}

// the players of a team who must get there: g_duelGoalPlayers, or all of them
static int NeededPlayers( int team ) {
	int n = g_duelGoalPlayers.integer < 1 ? 1 : g_duelGoalPlayers.integer;

	return n < TeamPlayers( team ) ? n : TeamPlayers( team );
}

static void TouchGoal( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	int		num, team;

	if ( duel.state != DUEL_PLAYING || !other->client || other->health <= 0 ) {
		return;
	}
	team = other->client->sess.sessionTeam;
	num = other - g_entities;
	if ( team != ent->count || duel.finished[num] ) {
		return;
	}
	duel.finished[num] = qtrue;
	trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " reached the goal.\n\"", other->client->pers.netname ) );
	if ( FinishedPlayers( team ) >= NeededPlayers( team ) ) {
		RoundWon( team );
		return;
	}
	// in a team game, those who got there watch the rest of the round
	trap_SendServerCommand( num, "cp \"You made it: watch the rest of the round.\"" );
}

/*
=================
SpawnGoal

A team's goal: its CTF flag, standing on the other team's starting point
=================
*/
static gentity_t *SpawnGoal( int side, int team ) {
	gitem_t		*item = BG_FindItem( team == TEAM_RED ? "Red Flag" : "Blue Flag" );
	gentity_t	*ent;
	vec3_t		origin;

	if ( !item ) {
		return NULL;
	}
	RegisterItem( item );
	ent = G_Spawn();
	ent->classname = "doom_duel_goal";
	ent->item = item;
	ent->s.eType = ET_ITEM;
	ent->s.modelindex = item - bg_itemlist;
	ent->s.modelindex2 = 0;
	ent->count = team;
	VectorSet( ent->r.mins, -24, -24, -16 );
	VectorSet( ent->r.maxs, 24, 24, 48 );
	ent->r.contents = CONTENTS_TRIGGER;
	ent->touch = TouchGoal;
	VectorCopy( duel.sides[side], origin );
	origin[2] += ITEM_RADIUS;
	G_SetOrigin( ent, origin );
	trap_LinkEntity( ent );
	return ent;
}

/*
=====================================================================

ROUNDS

=====================================================================
*/

static void SaveState( int round ) {
	trap_Cvar_Set( "g_duelState", va( "%d %d %d", round, duel.scores[TEAM_RED], duel.scores[TEAM_BLUE] ) );
}

static void SetScores( void ) {
	level.teamScores[TEAM_RED] = duel.scores[TEAM_RED];
	level.teamScores[TEAM_BLUE] = duel.scores[TEAM_BLUE];
	CalculateRanks();
}

static void Countdown( void ) {
	int		i;

	memset( duel.finished, 0, sizeof( duel.finished ) );
	duel.state = DUEL_COUNTDOWN;
	duel.stateTime = level.time + DUEL_COUNTDOWN_TIME;
	trap_SetConfigstring( CS_WARMUP, va( "%i", duel.stateTime ) );

	// everyone back to their side
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t *cl = &level.clients[i];

		if ( cl->pers.connected == CON_CONNECTED && ( cl->sess.sessionTeam == TEAM_RED || cl->sess.sessionTeam == TEAM_BLUE ) ) {
			// out of the world first, or the kill box finds the player itself
			if ( cl->ps.stats[STAT_HEALTH] <= 0 ) {
				CopyToBodyQue( &g_entities[i] );
			} else {
				trap_UnlinkEntity( &g_entities[i] );
			}
			ClientSpawn( &g_entities[i] );
		}
	}
}

static void RoundWon( int team ) {
	int		winner = 0, i;
	gentity_t	*te;

	duel.scores[team]++;
	SetScores();
	duel.state = DUEL_ROUNDOVER;
	duel.stateTime = level.time + DUEL_ROUNDOVER_TIME;

	// a duel names the player, a team game the team
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( duel.finished[i] && level.clients[i].sess.sessionTeam == team ) {
			winner = i;
		}
	}
	if ( TeamPlayers( team ) == 1 ) {
		trap_SendServerCommand( -1, va( "cp \"%s" S_COLOR_WHITE " wins round %d!\n\"", level.clients[winner].pers.netname, duel.round ) );
	} else {
		trap_SendServerCommand( -1, va( "cp \"%s wins round %d!\n\"", DuelTeamName( team ), duel.round ) );
	}
	te = G_TempEntity( vec3_origin, EV_GLOBAL_TEAM_SOUND );
	te->s.eventParm = team == TEAM_RED ? GTS_RED_CAPTURE : GTS_BLUE_CAPTURE;
	te->r.svFlags |= SVF_BROADCAST;
}

static void RoundOver( void ) {
	int		team;

	for ( team = TEAM_RED ; team <= TEAM_BLUE ; team++ ) {
		if ( duel.scores[team] >= ( g_duelRounds.integer > 0 ? g_duelRounds.integer : 1 ) ) {
			trap_SendServerCommand( -1, va( "print \"%s wins the duel %d to %d.\n\"", DuelTeamName( team ),
				duel.scores[team], duel.scores[team ^ ( TEAM_RED ^ TEAM_BLUE )] ) );
			trap_Cvar_Set( "g_duelState", "" );
			duel.state = DUEL_WAITING;
			duel.stateTime = 0;
			duel.active = qfalse;		// the match is over: Quake 3's intermission
			LogExit( "Duel won." );
			return;
		}
	}
	// the next round: the level again, the sides swapped
	SaveState( duel.round + 1 );
	trap_Cvar_Set( "g_duelContinue", "1" );
	duel.state = DUEL_WAITING;
	duel.stateTime = 0;
	trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
}

/*
=================
Doom_DuelGoal

Where a player's team must get this round (the bots' goal), qfalse when
there is no race on
=================
*/
qboolean Doom_DuelGoal( gentity_t *ent, vec3_t point ) {
	int team;

	if ( !duel.active || !ent->client || duel.finished[ent - g_entities] ) {
		return qfalse;
	}
	team = ent->client->sess.sessionTeam;
	if ( team != TEAM_RED && team != TEAM_BLUE ) {
		return qfalse;
	}
	VectorCopy( duel.sides[TeamSide( team ) ^ 1], point );
	point[2] += 24;		// a player's middle above the floor
	return qtrue;
}

/*
=================
Doom_DuelNearSpawn

No monster comes to life within 128 units of a side's start or spawn spot
(on the map, ignoring height)
=================
*/
#define DUEL_MONSTER_CLEARANCE	128

qboolean Doom_DuelNearSpawn( float x, float y ) {
	int		side;

	if ( !duel.active ) {
		return qfalse;
	}
	for ( side = 0 ; side < 2 ; side++ ) {
		float	dx = x - duel.sides[side][0], dy = y - duel.sides[side][1];
		float	sx = x - duel.spawns[side][0], sy = y - duel.spawns[side][1];

		if ( dx * dx + dy * dy < DUEL_MONSTER_CLEARANCE * DUEL_MONSTER_CLEARANCE
			|| sx * sx + sy * sy < DUEL_MONSTER_CLEARANCE * DUEL_MONSTER_CLEARANCE ) {
			return qtrue;
		}
	}
	return qfalse;
}

// the level's monsters placed near a side are taken out
static void ClearSpawns( void ) {
	int		i, n = 0;

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t	*ent = &g_entities[i];

		if ( ent->inuse && ent->doomMobj && ( ent->doomMobj->flags & MF_COUNTKILL )
			&& Doom_DuelNearSpawn( ent->doomMobj->spawnX, ent->doomMobj->spawnY ) ) {
			G_FreeEntity( ent );
			doomLevel.totalKills--;
			n++;
		}
	}
	if ( n ) {
		G_Printf( "Doom Duel: %d monsters left out near the spawn points\n", n );
	}
}

/*
=================
Doom_InitDuel

After the entities spawned: the sides, the goals, and the round (the next
one of the match after a round's restart, else the first)
=================
*/
void Doom_InitDuel( void ) {
	char	buf[64];
	vec3_t	start;
	float	yaw;
	int		team;

	memset( &duel, 0, sizeof( duel ) );
	if ( !Doom_DuelActive() ) {
		return;
	}
	if ( !FindStart( start, &yaw ) ) {
		G_Printf( S_COLOR_YELLOW "Doom Duel: the level has no player start\n" );
		return;
	}
	VectorCopy( start, duel.sides[0] );
	duel.sideYaw[0] = yaw;
	if ( !FindExit( start, duel.sides[1], &duel.sideYaw[1] ) ) {
		G_Printf( S_COLOR_YELLOW "Doom Duel: the level has no exit\n" );
		return;
	}
	for ( team = 0 ; team < 2 ; team++ ) {
		int		sector = doom_map_sector_at( doomLevel.map, duel.sides[team][0], duel.sides[team][1] );
		float	*spawn = duel.spawns[team];

		if ( sector >= 0 ) {
			duel.sides[team][2] = doomLevel.floor[sector];		// both on the floor
		}
		// a step ahead of the flag, which stands where the other side must get
		spawn[0] = duel.sides[team][0] + cos( DEG2RAD( duel.sideYaw[team] ) ) * 64;
		spawn[1] = duel.sides[team][1] + sin( DEG2RAD( duel.sideYaw[team] ) ) * 64;
		if ( doom_map_sector_at( doomLevel.map, spawn[0], spawn[1] ) < 0 ) {
			spawn[0] = duel.sides[team][0];
			spawn[1] = duel.sides[team][1];
		}
	}
	duel.active = qtrue;
	ClearSpawns();

	duel.round = 1;
	if ( trap_Cvar_VariableIntegerValue( "g_duelContinue" ) ) {
		trap_Cvar_VariableStringBuffer( "g_duelState", buf, sizeof( buf ) );
		sscanf( buf, "%d %d %d", &duel.round, &duel.scores[TEAM_RED], &duel.scores[TEAM_BLUE] );
	}
	trap_Cvar_Set( "g_duelContinue", "0" );
	SaveState( duel.round );
	trap_Cvar_Set( "g_duelRound", va( "%d", duel.round ) );
	SetScores();

	for ( team = TEAM_RED ; team <= TEAM_BLUE ; team++ ) {
		int side = TeamSide( team ) ^ 1;

		duel.goals[side] = SpawnGoal( side, team );
	}
	duel.state = DUEL_WAITING;
	trap_SetConfigstring( CS_WARMUP, "-1" );
	G_Printf( "Doom Duel: round %d, red %d blue %d; start %s, exit %s\n", duel.round,
		duel.scores[TEAM_RED], duel.scores[TEAM_BLUE], vtos( duel.sides[0] ), vtos( duel.sides[1] ) );
}

void Doom_DuelFrame( void ) {
	if ( !duel.active || level.intermissionQueued || level.intermissiontime ) {
		return;
	}
	switch ( duel.state ) {
	case DUEL_WAITING:
		if ( TeamPlayers( TEAM_RED ) && TeamPlayers( TEAM_BLUE ) ) {
			Countdown();
		}
		break;
	case DUEL_COUNTDOWN:
		if ( level.time >= duel.stateTime ) {
			duel.state = DUEL_PLAYING;
			trap_SetConfigstring( CS_WARMUP, "" );
		}
		break;
	case DUEL_PLAYING:
		// a side left: wait for a new opponent
		if ( !TeamPlayers( TEAM_RED ) || !TeamPlayers( TEAM_BLUE ) ) {
			duel.state = DUEL_WAITING;
			trap_SetConfigstring( CS_WARMUP, "-1" );
		}
		break;
	case DUEL_ROUNDOVER:
		if ( level.time >= duel.stateTime ) {
			RoundOver();
		}
		break;
	}
}

/*
=================
Doom_DuelThink

ClientThink, after the movement type is set: frozen in the countdown;
watching once at the goal
=================
*/
void Doom_DuelThink( gentity_t *ent ) {
	gclient_t	*client = ent->client;

	if ( !duel.active ) {
		return;
	}
	if ( duel.finished[ent - g_entities] && duel.state == DUEL_PLAYING ) {
		client->ps.pm_type = PM_SPECTATOR;
		ent->r.contents = 0;
		ent->takedamage = qfalse;
		return;
	}
	if ( duel.state == DUEL_COUNTDOWN && client->ps.stats[STAT_HEALTH] > 0 ) {
		client->ps.pm_type = PM_FREEZE;
	}
}

/*
=================
Doom_DuelPlayerDied

player_die: back in 3 seconds; the dead monsters come back too
=================
*/
void Doom_DuelPlayerDied( gentity_t *ent ) {
	if ( !duel.active ) {
		return;
	}
	ent->client->respawnTime = level.time + DUEL_RESPAWN_TIME;
	Doom_RespawnDeadMonsters();
}
