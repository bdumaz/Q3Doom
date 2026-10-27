/*
===========================================================================
Q3Doom - moving floors and ceilings

Each moving plane of a Doom sector is a func_doom_plane: an inline brush
model built by libdoom at a canonical height, positioned by its origin.
A move is a Quake 3 linear trajectory from the current height to the Doom
target at Doom's speed, so Quake 3 does the pushing, blocking and client
interpolation. When a move ends (reached) or is blocked, the rules of
Doom's thinkers decide what happens next: p_doors.c, p_plats.c,
p_floor.c and p_ceilng.c.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

#define CRUSH_DAMAGE		10
#define CRUSH_INTERVAL		DOOM_TICS_MSEC( 4 )

/*
=====================================================================

HEIGHTS AND MOVES

=====================================================================
*/

// the height the model is built at: the entity origin is height - base
static float PlaneBase( const gentity_t *ent ) {
	const doom_plane_range_t *r = &doomLevel.ranges[ent->doomSector];

	return ent->doomCeiling ? r->ceiling_min : r->floor_max;
}

static float PlaneHeight( const gentity_t *ent ) {
	vec3_t	origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	return origin[2] + PlaneBase( ent );
}

static void PlaneStop( gentity_t *ent, float height ) {
	VectorSet( ent->s.pos.trBase, 0, 0, height - PlaneBase( ent ) );
	VectorClear( ent->s.pos.trDelta );
	ent->s.pos.trType = TR_STATIONARY;
	ent->s.pos.trTime = level.time;
	ent->s.loopSound = 0;
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
	trap_LinkEntity( ent );
}

/*
=================
PlaneMove

Starts a linear move to 'target' at 'speed' units per tic. The mover's
'reached' callback runs when it gets there.
=================
*/
static void PlaneMove( gentity_t *ent, float target, float speed ) {
	float	height = PlaneHeight( ent );
	float	velocity = DOOM_TICSPEED( speed );

	VectorSet( ent->s.pos.trBase, 0, 0, height - PlaneBase( ent ) );
	if ( fabs( target - height ) < 0.01f || velocity <= 0 ) {
		// already there: finish on the next think
		PlaneStop( ent, target );
		ent->doomPlane->reachPending = qtrue;
		return;
	}
	VectorSet( ent->s.pos.trDelta, 0, 0, target > height ? velocity : -velocity );
	ent->s.pos.trType = TR_LINEAR_STOP;
	ent->s.pos.trTime = level.time;
	ent->s.pos.trDuration = (int)( fabs( target - height ) / velocity * 1000 );
	if ( ent->s.pos.trDuration < 1 ) {
		ent->s.pos.trDuration = 1;
	}
}

static void PlaneSound( gentity_t *ent, const char *sound ) {
	G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( va( "doom/sound/%s", sound ) ) );
}

// floors and ceilings grind while they move (Doom plays sfx_stnmov every 8 tics)
static void PlaneGrind( gentity_t *ent ) {
	ent->s.loopSound = G_SoundIndex( "doom/sound/DSSTNMOV" );
}

static gentity_t *SectorPlane( int sector, int ceiling ) {
	if ( sector < 0 || sector >= doomLevel.numSectors ) {
		return NULL;
	}
	return ceiling ? doomLevel.ceilingMover[sector] : doomLevel.floorMover[sector];
}

// a plane that can start a new action: it moves and nothing acts on it
static gentity_t *FreePlane( int sector, int ceiling ) {
	gentity_t *ent = SectorPlane( sector, ceiling );

	return ( ent && ent->doomPlane->kind == DOOM_KIND_NONE ) ? ent : NULL;
}

static void PlaneDone( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	if ( p->texture[0] && !ent->doomCeiling ) {
		Doom_SetFlat( ent->doomSector, p->texture );
	}
	if ( p->newSpecial >= 0 ) {
		doomLevel.special[ent->doomSector] = p->newSpecial;
	}
	memset( p, 0, sizeof( *p ) );
	p->newSpecial = -1;
}

/*
=================
Doom_UpdateHeights

Current heights of every sector, for the Doom neighbor searches
=================
*/
void Doom_UpdateHeights( void ) {
	int		i;

	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		if ( doomLevel.floorMover[i] ) {
			doomLevel.floor[i] = PlaneHeight( doomLevel.floorMover[i] );
		}
		if ( doomLevel.ceilingMover[i] ) {
			doomLevel.ceiling[i] = PlaneHeight( doomLevel.ceilingMover[i] );
		}
		doomLevel.floorInt[i] = (int)floor( doomLevel.floor[i] + 0.5f );
		doomLevel.ceilingInt[i] = (int)floor( doomLevel.ceiling[i] + 0.5f );
	}
}

#define LOWEST_FLOOR( s )		doom_find_lowest_floor_surrounding( doomLevel.map, s, doomLevel.floorInt, doomLevel.ceilingInt )
#define HIGHEST_FLOOR( s )		doom_find_highest_floor_surrounding( doomLevel.map, s, doomLevel.floorInt, doomLevel.ceilingInt )
#define NEXT_FLOOR( s )			doom_find_next_highest_floor( doomLevel.map, s, doomLevel.floorInt[s], doomLevel.floorInt, doomLevel.ceilingInt )
#define LOWEST_CEILING( s )		doom_find_lowest_ceiling_surrounding( doomLevel.map, s, doomLevel.floorInt, doomLevel.ceilingInt )
#define HIGHEST_CEILING( s )	doom_find_highest_ceiling_surrounding( doomLevel.map, s, doomLevel.floorInt, doomLevel.ceilingInt )

/*
=====================================================================

DOORS (p_doors.c)

=====================================================================
*/

static qboolean IsBlazeDoor( int type ) {
	return type == DOOM_DOOR_BLAZERAISE || type == DOOM_DOOR_BLAZEOPEN || type == DOOM_DOOR_BLAZECLOSE;
}

static void DoorOpen( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	p->direction = 1;
	PlaneMove( ent, p->high, p->speed );
	PlaneSound( ent, IsBlazeDoor( p->type ) ? "DSBDOPN" : "DSDOROPN" );
}

static void DoorClose( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	p->direction = -1;
	PlaneMove( ent, doomLevel.floor[ent->doomSector], p->speed );
	PlaneSound( ent, IsBlazeDoor( p->type ) ? "DSBDCLS" : "DSDORCLS" );
}

static void DoorReached( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	if ( p->direction == 1 ) {
		switch ( p->type ) {
		case DOOM_DOOR_NORMAL:
		case DOOM_DOOR_BLAZERAISE:
			p->direction = 0;		// wait at the top, then close
			p->resumeTime = level.time + DOOM_TICS_MSEC( VDOORWAIT );
			break;
		default:					// open, blazeOpen, close30ThenOpen: stay open
			PlaneDone( ent );
			break;
		}
	} else if ( p->direction == -1 ) {
		if ( p->type == DOOM_DOOR_CLOSE30THENOPEN ) {
			p->direction = 0;
			p->resumeTime = level.time + DOOM_TICS_MSEC( 30 * DOOM_TICRATE );
		} else {
			PlaneDone( ent );
		}
	}
}

static void DoorResume( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	if ( p->direction == 2 ) {						// raiseIn5Mins
		p->type = DOOM_DOOR_NORMAL;
		p->high = LOWEST_CEILING( ent->doomSector ) - 4;
		DoorOpen( ent );
	} else if ( p->type == DOOM_DOOR_CLOSE30THENOPEN ) {
		DoorOpen( ent );
	} else {
		DoorClose( ent );
	}
}

static void DoorBlocked( gentity_t *ent, gentity_t *other ) {
	doomPlane_t *p = ent->doomPlane;

	// close-only doors keep pushing; the others go back up (T_VerticalDoor)
	if ( p->direction == -1 && p->type != DOOM_DOOR_CLOSE && p->type != DOOM_DOOR_BLAZECLOSE ) {
		DoorOpen( ent );
	}
}

/*
=================
Doom_DoDoor

A door action on a tagged sector (EV_DoDoor)
=================
*/
qboolean Doom_DoDoor( int sector, int type, int key ) {
	gentity_t	*ent = FreePlane( sector, 1 );
	doomPlane_t	*p;

	if ( !ent ) {
		return qfalse;
	}
	p = ent->doomPlane;
	p->kind = DOOM_KIND_DOOR;
	p->type = type;
	p->speed = IsBlazeDoor( type ) ? VDOORSPEED * 4 : VDOORSPEED;

	switch ( type ) {
	case DOOM_DOOR_CLOSE:
	case DOOM_DOOR_BLAZECLOSE:
		DoorClose( ent );
		break;
	case DOOM_DOOR_CLOSE30THENOPEN:
		p->high = doomLevel.ceiling[sector];
		DoorClose( ent );
		break;
	default:
		p->high = LOWEST_CEILING( sector ) - 4;
		if ( p->high != doomLevel.ceiling[sector] ) {
			DoorOpen( ent );
		} else {
			p->direction = 1;
			PlaneMove( ent, p->high, p->speed );
		}
		break;
	}
	return qtrue;
}

/*
=================
Doom_ManualDoor

A door opened from its own line (EV_VerticalDoor): by "use", or Quake 3
style by coming close or shooting it ('proximity'). Using a waiting door
closes it as in Doom; coming close only keeps it open.
=================
*/
void Doom_ManualDoor( gentity_t *activator, int line, qboolean proximity ) {
	const doom_linedef_t	*ld = &doomLevel.map->linedefs[line];
	const doom_special_t	*sp = doom_line_special( doomLevel.lineSpecial[line] );
	gentity_t				*ent;
	doomPlane_t				*p;
	int						sector;

	if ( !sp || sp->kind != DOOM_KIND_DOOR || ld->side[1] == DOOM_NO_SIDE ) {
		return;
	}
	// a bot uses what is in front of it twice a second (Doom_BotUse): that
	// must not close the door it is going through
	if ( activator && ( activator->r.svFlags & SVF_BOT ) ) {
		proximity = qtrue;
	}
	sector = doomLevel.map->sidedefs[ld->side[1]].sector;
	ent = SectorPlane( sector, 1 );
	if ( !ent ) {
		return;
	}
	if ( sp->amount && !Doom_PlayerHasKey( activator, sp->amount ) ) {
		Doom_KeyMessage( activator, sp->amount );
		return;
	}

	p = ent->doomPlane;
	if ( p->kind == DOOM_KIND_DOOR ) {
		if ( !sp->repeatable ) {
			return;				// D1 doors already moving: nothing to do
		}
		if ( p->direction == -1 ) {
			DoorOpen( ent );	// closing: go back up
		} else if ( proximity ) {
			if ( p->direction == 0 ) {
				p->resumeTime = level.time + DOOM_TICS_MSEC( VDOORWAIT );	// stay open
			}
		} else if ( activator && activator->client ) {
			DoorClose( ent );	// players close an open door by using it
		}
		return;
	}
	if ( p->kind != DOOM_KIND_NONE ) {
		return;
	}

	Doom_DoDoor( sector, sp->type, sp->amount );
	if ( !sp->repeatable ) {
		doomLevel.lineSpecial[line] = 0;
	}
}

/*
=====================================================================

PLATS (p_plats.c)

=====================================================================
*/

static void PlatReached( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	PlaneSound( ent, "DSPSTOP" );
	if ( p->direction == 1 ) {
		if ( p->type == DOOM_PLAT_PERPETUALRAISE ) {
			p->direction = 0;
			p->oldDirection = 1;
			p->resumeTime = level.time + DOOM_TICS_MSEC( p->waitTics );
		} else {
			PlaneDone( ent );
		}
	} else {
		p->direction = 0;
		p->oldDirection = -1;
		p->resumeTime = level.time + DOOM_TICS_MSEC( p->waitTics );
	}
}

static void PlatResume( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	p->direction = p->oldDirection == 1 ? -1 : 1;
	PlaneMove( ent, p->direction == 1 ? p->high : p->low, p->speed );
	PlaneSound( ent, "DSPSTART" );
}

static void PlatBlocked( gentity_t *ent, gentity_t *other ) {
	doomPlane_t *p = ent->doomPlane;

	// a rising plat that would crush something goes back down
	if ( p->direction == 1 && !p->crush ) {
		p->direction = -1;
		PlaneMove( ent, p->low, p->speed );
		PlaneSound( ent, "DSPSTART" );
	}
}

/*
=================
Doom_DoPlat

EV_DoPlat on one sector; 'line' gives the texture of the "AndChange" types
=================
*/
qboolean Doom_DoPlat( int sector, int type, int amount, int line ) {
	gentity_t	*ent = FreePlane( sector, 0 );
	doomPlane_t	*p;
	int			front = doomLevel.map->sidedefs[doomLevel.map->linedefs[line].side[0]].sector;
	float		floorh;

	if ( !ent ) {
		return qfalse;
	}
	p = ent->doomPlane;
	p->kind = DOOM_KIND_PLAT;
	p->type = type;
	floorh = doomLevel.floor[sector];

	switch ( type ) {
	case DOOM_PLAT_RAISETONEARESTANDCHANGE:
		p->speed = PLATSPEED / 2.0f;
		Doom_SetFlat( sector, doomLevel.floorFlat[front] );
		doomLevel.special[sector] = 0;
		p->high = NEXT_FLOOR( sector );
		p->direction = 1;
		PlaneMove( ent, p->high, p->speed );
		PlaneGrind( ent );
		break;
	case DOOM_PLAT_RAISEANDCHANGE:
		p->speed = PLATSPEED / 2.0f;
		Doom_SetFlat( sector, doomLevel.floorFlat[front] );
		p->high = floorh + amount;
		p->direction = 1;
		PlaneMove( ent, p->high, p->speed );
		PlaneGrind( ent );
		break;
	case DOOM_PLAT_DOWNWAITUPSTAY:
	case DOOM_PLAT_BLAZEDWUS:
		p->speed = PLATSPEED * ( type == DOOM_PLAT_BLAZEDWUS ? 8 : 4 );
		p->low = LOWEST_FLOOR( sector );
		if ( p->low > floorh ) {
			p->low = floorh;
		}
		p->high = floorh;
		p->waitTics = DOOM_TICRATE * PLATWAIT;
		p->direction = -1;
		PlaneMove( ent, p->low, p->speed );
		PlaneSound( ent, "DSPSTART" );
		break;
	case DOOM_PLAT_PERPETUALRAISE:
		p->speed = PLATSPEED;
		p->low = LOWEST_FLOOR( sector );
		if ( p->low > floorh ) {
			p->low = floorh;
		}
		p->high = HIGHEST_FLOOR( sector );
		if ( p->high < floorh ) {
			p->high = floorh;
		}
		p->waitTics = DOOM_TICRATE * PLATWAIT;
		p->direction = ( rand() & 1 ) ? 1 : -1;
		PlaneMove( ent, p->direction == 1 ? p->high : p->low, p->speed );
		PlaneSound( ent, "DSPSTART" );
		break;
	default:
		p->kind = DOOM_KIND_NONE;
		return qfalse;
	}
	return qtrue;
}

/*
=====================================================================

FLOORS (p_floor.c)

=====================================================================
*/

static void FloorReached( gentity_t *ent ) {
	PlaneSound( ent, "DSPSTOP" );
	PlaneDone( ent );
}

static void CrushBlocker( gentity_t *ent, gentity_t *other ) {
	doomPlane_t *p = ent->doomPlane;

	if ( !other->takedamage ) {
		if ( !other->client ) {
			G_FreeEntity( other );		// items and the like get crushed away
		}
		return;
	}
	if ( level.time - p->lastCrushTime >= CRUSH_INTERVAL ) {
		p->lastCrushTime = level.time;
		G_Damage( other, ent, ent, NULL, NULL, CRUSH_DAMAGE, DAMAGE_NO_KNOCKBACK, MOD_CRUSH );
	}
}

static void FloorBlocked( gentity_t *ent, gentity_t *other ) {
	// rising floors that crush hurt what is in the way; the others wait for it to move
	if ( ent->doomPlane->crush ) {
		CrushBlocker( ent, other );
	}
}

// the floor of the neighbor at 'height', for "lowerAndChange" (EV_DoFloor)
static int ModelFloorSector( int sector, float height ) {
	const doom_map_t	*map = doomLevel.map;
	int					i;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t *ld = &map->linedefs[i];
		int s0, s1, other;

		if ( ld->side[1] == DOOM_NO_SIDE ) {
			continue;
		}
		s0 = map->sidedefs[ld->side[0]].sector;
		s1 = map->sidedefs[ld->side[1]].sector;
		if ( s0 != sector && s1 != sector ) {
			continue;
		}
		other = s0 == sector ? s1 : s0;
		if ( doomLevel.floorInt[other] == (int)height ) {
			return other;
		}
	}
	return -1;
}

qboolean Doom_DoFloor( int sector, int type, int line ) {
	gentity_t	*ent = FreePlane( sector, 0 );
	doomPlane_t	*p;
	float		floorh, target;
	int			front = doomLevel.map->sidedefs[doomLevel.map->linedefs[line].side[0]].sector;

	if ( !ent ) {
		return qfalse;
	}
	p = ent->doomPlane;
	p->kind = DOOM_KIND_FLOOR;
	p->type = type;
	p->speed = FLOORSPEED;
	floorh = doomLevel.floor[sector];

	switch ( type ) {
	case DOOM_FLOOR_LOWER:
		target = HIGHEST_FLOOR( sector );
		break;
	case DOOM_FLOOR_LOWERTOLOWEST:
		target = LOWEST_FLOOR( sector );
		break;
	case DOOM_FLOOR_TURBOLOWER:
		p->speed = FLOORSPEED * 4;
		target = HIGHEST_FLOOR( sector );
		if ( target != floorh ) {
			target += 8;
		}
		break;
	case DOOM_FLOOR_RAISECRUSH:
	case DOOM_FLOOR_RAISE:
		target = LOWEST_CEILING( sector );
		if ( target > doomLevel.ceiling[sector] ) {
			target = doomLevel.ceiling[sector];
		}
		if ( type == DOOM_FLOOR_RAISECRUSH ) {
			target -= 8;
			p->crush = qtrue;
		}
		break;
	case DOOM_FLOOR_RAISETURBO:
		p->speed = FLOORSPEED * 4;
		target = NEXT_FLOOR( sector );
		break;
	case DOOM_FLOOR_RAISETONEAREST:
		target = NEXT_FLOOR( sector );
		break;
	case DOOM_FLOOR_RAISE24:
		target = floorh + 24;
		break;
	case DOOM_FLOOR_RAISE512:
		target = floorh + 512;
		break;
	case DOOM_FLOOR_RAISE24ANDCHANGE:
		target = floorh + 24;
		Doom_SetFlat( sector, doomLevel.floorFlat[front] );
		doomLevel.special[sector] = doomLevel.special[front];
		break;
	case DOOM_FLOOR_RAISETOTEXTURE:
		target = floorh + doom_shortest_lower_texture( doomLevel.wad, doomLevel.map, sector );
		break;
	case DOOM_FLOOR_LOWERANDCHANGE: {
		int model;

		target = LOWEST_FLOOR( sector );
		model = ModelFloorSector( sector, target );
		if ( model >= 0 ) {
			Q_strncpyz( p->texture, doomLevel.floorFlat[model], sizeof( p->texture ) );
			p->newSpecial = doomLevel.special[model];
		}
		break;
	}
	default:
		p->kind = DOOM_KIND_NONE;
		return qfalse;
	}

	p->direction = target > floorh ? 1 : -1;
	PlaneMove( ent, target, p->speed );
	PlaneGrind( ent );
	return qtrue;
}

/*
=================
Doom_BuildStairs

EV_BuildStairs: each step of the chain rises one step higher than the last
=================
*/
qboolean Doom_BuildStairs( int sector, int type ) {
	int			chain[256], count, k, size = type == DOOM_STAIRS_TURBO16 ? 16 : 8;
	uint8_t		*visited;
	float		base;
	qboolean	started = qfalse;

	if ( !FreePlane( sector, 0 ) ) {
		return qfalse;
	}
	visited = G_Alloc( doomLevel.numSectors );
	memset( visited, 0, doomLevel.numSectors );
	count = doom_stair_chain( doomLevel.map, sector, chain, 256, visited );
	base = doomLevel.floor[sector];

	for ( k = 0 ; k < count ; k++ ) {
		gentity_t	*ent = FreePlane( chain[k], 0 );
		doomPlane_t	*p;

		if ( !ent ) {
			continue;		// busy steps are skipped but still count (P_BuildStairs)
		}
		p = ent->doomPlane;
		p->kind = DOOM_KIND_STAIRS;
		p->type = type;
		p->speed = type == DOOM_STAIRS_TURBO16 ? FLOORSPEED * 4 : FLOORSPEED / 4.0f;
		p->crush = type == DOOM_STAIRS_TURBO16;
		p->direction = 1;
		PlaneMove( ent, base + ( k + 1 ) * size, p->speed );
		PlaneGrind( ent );
		started = qtrue;
	}
	return started;
}

/*
=================
Doom_DoDonut

EV_DoDonut: the tagged sector lowers and the ring around it rises, both to
the floor of the sector beyond the ring
=================
*/
qboolean Doom_DoDonut( int s1 ) {
	const doom_map_t	*map = doomLevel.map;
	int					i, s2 = -1, s3;

	if ( !FreePlane( s1, 0 ) ) {
		return qfalse;
	}
	for ( i = 0 ; i < map->num_linedefs && s2 < 0 ; i++ ) {
		const doom_linedef_t *ld = &map->linedefs[i];

		if ( ld->side[1] == DOOM_NO_SIDE ) {
			continue;
		}
		if ( map->sidedefs[ld->side[0]].sector == s1 ) {
			s2 = map->sidedefs[ld->side[1]].sector;
		} else if ( map->sidedefs[ld->side[1]].sector == s1 ) {
			s2 = map->sidedefs[ld->side[0]].sector;
		}
	}
	if ( s2 < 0 ) {
		return qfalse;
	}
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		gentity_t				*ring, *hole;

		if ( ld->side[1] == DOOM_NO_SIDE ) {
			continue;
		}
		if ( map->sidedefs[ld->side[0]].sector == s2 ) {
			s3 = map->sidedefs[ld->side[1]].sector;
		} else if ( map->sidedefs[ld->side[1]].sector == s2 ) {
			s3 = map->sidedefs[ld->side[0]].sector;
		} else {
			continue;
		}
		if ( s3 == s1 ) {
			continue;
		}
		ring = FreePlane( s2, 0 );
		hole = FreePlane( s1, 0 );
		if ( ring ) {
			ring->doomPlane->kind = DOOM_KIND_DONUT;
			ring->doomPlane->speed = FLOORSPEED / 2.0f;
			Q_strncpyz( ring->doomPlane->texture, doomLevel.floorFlat[s3], sizeof( ring->doomPlane->texture ) );
			ring->doomPlane->newSpecial = 0;
			PlaneMove( ring, doomLevel.floor[s3], FLOORSPEED / 2.0f );
			PlaneGrind( ring );
		}
		if ( hole ) {
			hole->doomPlane->kind = DOOM_KIND_DONUT;
			hole->doomPlane->speed = FLOORSPEED / 2.0f;
			PlaneMove( hole, doomLevel.floor[s3], FLOORSPEED / 2.0f );
			PlaneGrind( hole );
		}
		break;
	}
	return qtrue;
}

/*
=====================================================================

CEILINGS (p_ceilng.c)

=====================================================================
*/

static qboolean IsCrusher( int type ) {
	return type == DOOM_CEIL_CRUSHANDRAISE || type == DOOM_CEIL_FASTCRUSHANDRAISE || type == DOOM_CEIL_SILENTCRUSHANDRAISE;
}

static void CeilingReached( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	if ( IsCrusher( p->type ) ) {
		// crushers go up and down until stopped
		if ( p->type == DOOM_CEIL_SILENTCRUSHANDRAISE ) {
			PlaneSound( ent, "DSPSTOP" );
		}
		p->direction = -p->direction;
		p->speed = p->type == DOOM_CEIL_FASTCRUSHANDRAISE ? CEILSPEED * 2 : CEILSPEED;
		PlaneMove( ent, p->direction == 1 ? p->high : p->low, p->speed );
		if ( p->type != DOOM_CEIL_SILENTCRUSHANDRAISE ) {
			PlaneGrind( ent );
		}
		return;
	}
	PlaneDone( ent );
}

static void CeilingBlocked( gentity_t *ent, gentity_t *other ) {
	doomPlane_t *p = ent->doomPlane;

	if ( p->direction != -1 ) {
		return;
	}
	if ( p->crush ) {
		CrushBlocker( ent, other );
		// slow crushers slow down further while crushing (T_MoveCeiling)
		if ( p->type != DOOM_CEIL_FASTCRUSHANDRAISE && p->speed == CEILSPEED ) {
			p->speed = CEILSPEED / 8.0f;
			PlaneMove( ent, p->low, p->speed );
			if ( p->type != DOOM_CEIL_SILENTCRUSHANDRAISE ) {
				PlaneGrind( ent );
			}
		}
	}
}

qboolean Doom_DoCeiling( int sector, int type, int line ) {
	gentity_t	*ent = FreePlane( sector, 1 );
	doomPlane_t	*p;
	float		floorh;

	if ( !ent ) {
		return qfalse;
	}
	p = ent->doomPlane;
	p->kind = DOOM_KIND_CEILING;
	p->type = type;
	p->speed = CEILSPEED;
	p->tag = doomLevel.map->linedefs[line].tag;
	floorh = doomLevel.floor[sector];

	switch ( type ) {
	case DOOM_CEIL_LOWERTOFLOOR:
		p->low = floorh;
		p->direction = -1;
		break;
	case DOOM_CEIL_RAISETOHIGHEST:
		p->high = HIGHEST_CEILING( sector );
		p->direction = 1;
		break;
	case DOOM_CEIL_LOWERANDCRUSH:
		p->crush = qtrue;
		p->low = floorh + 8;
		p->direction = -1;
		break;
	case DOOM_CEIL_FASTCRUSHANDRAISE:
		p->speed = CEILSPEED * 2;
		// fall through
	case DOOM_CEIL_CRUSHANDRAISE:
	case DOOM_CEIL_SILENTCRUSHANDRAISE:
		p->crush = qtrue;
		p->high = doomLevel.ceiling[sector];
		p->low = floorh + 8;
		p->direction = -1;
		break;
	default:
		p->kind = DOOM_KIND_NONE;
		return qfalse;
	}
	PlaneMove( ent, p->direction == 1 ? p->high : p->low, p->speed );
	if ( type != DOOM_CEIL_SILENTCRUSHANDRAISE ) {
		PlaneGrind( ent );
	}
	return qtrue;
}

/*
=====================================================================

STASIS: stopping perpetual plats and crushers, and starting them again

=====================================================================
*/

qboolean Doom_StopMovers( int tag, int kind ) {
	int			i;
	qboolean	stopped = qfalse;

	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		gentity_t	*ent = SectorPlane( i, kind == DOOM_KIND_CEILING );
		doomPlane_t	*p;

		if ( !ent || doomLevel.map->sectors[i].tag != tag ) {
			continue;
		}
		p = ent->doomPlane;
		if ( p->kind != kind || p->inStasis ) {
			continue;
		}
		if ( kind == DOOM_KIND_PLAT && p->type != DOOM_PLAT_PERPETUALRAISE ) {
			continue;
		}
		if ( kind == DOOM_KIND_CEILING && !IsCrusher( p->type ) ) {
			continue;
		}
		p->inStasis = qtrue;
		p->oldDirection = p->direction;
		PlaneStop( ent, PlaneHeight( ent ) );
		stopped = qtrue;
	}
	return stopped;
}

qboolean Doom_ActivateInStasis( int tag, int kind ) {
	int			i;
	qboolean	started = qfalse;

	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		gentity_t	*ent = SectorPlane( i, kind == DOOM_KIND_CEILING );
		doomPlane_t	*p;

		if ( !ent || doomLevel.map->sectors[i].tag != tag ) {
			continue;
		}
		p = ent->doomPlane;
		if ( p->kind != kind || !p->inStasis ) {
			continue;
		}
		p->inStasis = qfalse;
		p->direction = p->oldDirection;
		if ( p->direction == 0 ) {
			p->resumeTime = level.time;
		} else {
			PlaneMove( ent, p->direction == 1 ? p->high : p->low, p->speed );
		}
		started = qtrue;
	}
	return started;
}

/*
=====================================================================

THE MOVER ENTITY

=====================================================================
*/

static void Plane_Reached( gentity_t *ent ) {
	doomPlane_t	*p = ent->doomPlane;
	float		height = PlaneHeight( ent );

	PlaneStop( ent, height );
	if ( ent->doomCeiling ) {
		doomLevel.ceiling[ent->doomSector] = height;
	} else {
		doomLevel.floor[ent->doomSector] = height;
	}

	switch ( p->kind ) {
	case DOOM_KIND_DOOR:
		DoorReached( ent );
		break;
	case DOOM_KIND_PLAT:
		PlatReached( ent );
		break;
	case DOOM_KIND_CEILING:
		CeilingReached( ent );
		break;
	case DOOM_KIND_FLOOR:
	case DOOM_KIND_STAIRS:
	case DOOM_KIND_DONUT:
		FloorReached( ent );
		break;
	default:
		break;
	}
}

static void Plane_Blocked( gentity_t *ent, gentity_t *other ) {
	switch ( ent->doomPlane->kind ) {
	case DOOM_KIND_DOOR:
		DoorBlocked( ent, other );
		break;
	case DOOM_KIND_PLAT:
		PlatBlocked( ent, other );
		break;
	case DOOM_KIND_CEILING:
		CeilingBlocked( ent, other );
		break;
	default:
		FloorBlocked( ent, other );
		break;
	}
}

// runs every frame: zero distance moves finish here, and waits at the end
// of a move run out here (doors at the top, plats)
static void Plane_Think( gentity_t *ent ) {
	doomPlane_t *p = ent->doomPlane;

	ent->nextthink = level.time + 1;
	if ( p->reachPending ) {
		p->reachPending = qfalse;
		Plane_Reached( ent );
		return;
	}
	if ( p->kind == DOOM_KIND_NONE || p->inStasis || ( p->direction != 0 && p->direction != 2 ) || p->resumeTime > level.time ) {
		return;
	}
	if ( p->kind == DOOM_KIND_DOOR ) {
		DoorResume( ent );
	} else if ( p->kind == DOOM_KIND_PLAT ) {
		PlatResume( ent );
	}
}

// shootable manual doors open like Quake 3 doors (see Doom_SpawnDoorTriggers)
static void Plane_Pain( gentity_t *ent, gentity_t *attacker, int damage ) {
	ent->health = 100000;
	if ( attacker && attacker->client && ent->doomLine >= 0 ) {
		Doom_ManualDoor( attacker, ent->doomLine, qtrue );
	}
}

/*
=================
SP_func_doom_plane

"model"			inline model built by libdoom
"doom_sector"	sector it belongs to
"doom_plane"	"floor" or "ceiling"
=================
*/
void SP_func_doom_plane( gentity_t *ent ) {
	int		sector;
	char	*plane;
	float	height;

	G_SpawnInt( "doom_sector", "-1", &sector );
	G_SpawnString( "doom_plane", "floor", &plane );
	if ( !DOOM_LEVEL() || sector < 0 || sector >= doomLevel.numSectors ) {
		G_FreeEntity( ent );
		return;
	}

	trap_SetBrushModel( ent, ent->model );
	ent->s.eType = ET_MOVER;
	ent->doomSector = sector;
	ent->doomCeiling = !Q_stricmp( plane, "ceiling" );
	ent->doomLine = -1;
	ent->doomPlane = G_Alloc( sizeof( doomPlane_t ) );
	memset( ent->doomPlane, 0, sizeof( doomPlane_t ) );
	ent->doomPlane->newSpecial = -1;
	ent->reached = Plane_Reached;
	ent->blocked = Plane_Blocked;
	ent->think = Plane_Think;
	ent->nextthink = level.time + 1;
	ent->pain = Plane_Pain;

	if ( ent->doomCeiling ) {
		doomLevel.ceilingMover[sector] = ent;
		height = doomLevel.map->sectors[sector].ceiling_height;
	} else {
		doomLevel.floorMover[sector] = ent;
		height = doomLevel.map->sectors[sector].floor_height;
	}
	PlaneStop( ent, height );
	VectorCopy( ent->s.pos.trBase, ent->s.origin );
}

/*
=================
Doom_SpawnSectorSpecials

Sector specials that start a door on a timer (P_SpawnSpecials)
=================
*/
void Doom_SpawnSectorSpecials( void ) {
	int		i;

	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		gentity_t	*ent = FreePlane( i, 1 );
		doomPlane_t	*p;

		if ( !ent ) {
			continue;
		}
		p = ent->doomPlane;
		if ( doomLevel.special[i] == 10 ) {			// close after 30 seconds
			p->kind = DOOM_KIND_DOOR;
			p->type = DOOM_DOOR_NORMAL;
			p->speed = VDOORSPEED;
			p->direction = 0;
			p->resumeTime = level.time + 30000;
		} else if ( doomLevel.special[i] == 14 ) {	// open after 5 minutes
			p->kind = DOOM_KIND_DOOR;
			p->type = DOOM_DOOR_RAISEIN5MINS;
			p->speed = VDOORSPEED;
			p->direction = 2;
			p->resumeTime = level.time + 5 * 60000;
		}
	}
}

/*
=====================================================================

QUAKE 3 STYLE DOORS

Manual doors (opened with "use" in Doom) also open when a player comes
close, and when shot. Doors flagged secret (they look like walls) keep
needing "use" from the flagged side, so walking past a wall never reveals
a secret; their other side (the way out of a secret area) opens as usual.

=====================================================================
*/

#define DOOR_TRIGGER_SECRET		1	// spawnflags: open only from a side not flagged secret

// a player in the sector in front of a door line that is not flagged secret
static qboolean OnOpenSide( const gentity_t *self, const gentity_t *other ) {
	const doom_map_t	*map = doomLevel.map;
	int					sector = doom_map_sector_at( map, other->r.currentOrigin[0], other->r.currentOrigin[1] );
	int					i;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_special_t	*sp = doom_line_special( ld->special );

		if ( sp && sp->trigger == DOOM_TRIG_MANUAL && ld->side[1] != DOOM_NO_SIDE && !( ld->flags & DOOM_ML_SECRET )
			&& map->sidedefs[ld->side[1]].sector == self->doomSector && map->sidedefs[ld->side[0]].sector == sector ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void DoorTrigger_Touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	const doom_special_t *sp = doom_line_special( doomLevel.lineSpecial[self->doomLine] );

	if ( !other->client || other->client->sess.sessionTeam == TEAM_SPECTATOR || other->health <= 0 ) {
		return;
	}
	if ( ( self->spawnflags & DOOR_TRIGGER_SECRET ) && !OnOpenSide( self, other ) ) {
		return;
	}
	// locked: tell the player now and then, not every frame
	if ( sp && sp->amount && !Doom_PlayerHasKey( other, sp->amount ) ) {
		if ( level.time - self->doomMessageTime > 2000 ) {
			self->doomMessageTime = level.time;
			Doom_KeyMessage( other, sp->amount );
		}
		return;
	}
	Doom_ManualDoor( other, self->doomLine, qtrue );
}

void Doom_SpawnDoorTriggers( void ) {
	const doom_map_t	*map = doomLevel.map;
	qboolean			*done, *secret;
	int					i, j;

	done = G_Alloc( sizeof( qboolean ) * doomLevel.numSectors );
	memset( done, 0, sizeof( qboolean ) * doomLevel.numSectors );
	secret = G_Alloc( sizeof( qboolean ) * doomLevel.numSectors );
	memset( secret, 0, sizeof( qboolean ) * doomLevel.numSectors );

	// a door is secret if any of its door lines is flagged secret (often only one is)
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_special_t	*sp = doom_line_special( ld->special );

		if ( sp && sp->trigger == DOOM_TRIG_MANUAL && ld->side[1] != DOOM_NO_SIDE && ( ld->flags & DOOM_ML_SECRET ) ) {
			secret[map->sidedefs[ld->side[1]].sector] = qtrue;
		}
	}

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_special_t	*sp = doom_line_special( ld->special );
		gentity_t				*trigger, *door;
		vec3_t					mins, maxs;
		int						sector;

		if ( !sp || sp->trigger != DOOM_TRIG_MANUAL || ld->side[1] == DOOM_NO_SIDE ) {
			continue;
		}
		sector = map->sidedefs[ld->side[1]].sector;
		door = doomLevel.ceilingMover[sector];
		if ( !door || done[sector] ) {
			continue;
		}
		done[sector] = qtrue;

		// the door sector's bounds, grown so players are caught on both sides
		ClearBounds( mins, maxs );
		for ( j = 0 ; j < map->num_linedefs ; j++ ) {
			const doom_linedef_t *l = &map->linedefs[j];
			vec3_t p;

			if ( map->sidedefs[l->side[0]].sector != sector
				&& ( l->side[1] == DOOM_NO_SIDE || map->sidedefs[l->side[1]].sector != sector ) ) {
				continue;
			}
			VectorSet( p, map->vertexes[l->v1].x, map->vertexes[l->v1].y, 0 );
			AddPointToBounds( p, mins, maxs );
			VectorSet( p, map->vertexes[l->v2].x, map->vertexes[l->v2].y, 0 );
			AddPointToBounds( p, mins, maxs );
		}
		mins[0] -= 48; mins[1] -= 48;
		maxs[0] += 48; maxs[1] += 48;
		mins[2] = map->sectors[sector].floor_height - 128;
		maxs[2] = doomLevel.ranges[sector].ceiling_max + 64;

		trigger = G_Spawn();
		trigger->classname = "doom_door_trigger";
		trigger->doomLine = i;
		trigger->doomSector = sector;
		trigger->spawnflags = secret[sector] ? DOOR_TRIGGER_SECRET : 0;
		trigger->touch = DoorTrigger_Touch;
		trigger->r.contents = CONTENTS_TRIGGER;
		trigger->r.svFlags = SVF_NOCLIENT;
		VectorCopy( mins, trigger->r.mins );
		VectorCopy( maxs, trigger->r.maxs );
		trap_LinkEntity( trigger );

		// and shootable, unless shooting a wall would find the secret
		if ( secret[sector] ) {
			continue;
		}
		door->doomLine = i;
		door->takedamage = qtrue;
		door->health = 100000;
	}
}
