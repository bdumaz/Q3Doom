/*
===========================================================================
Q3Doom - Doom things that act: monsters and barrels (p_mobj.c, p_map.c,
p_maputl.c, p_enemy.c, p_inter.c, p_sight.c)

Each one is a game entity (ET_DOOM_THING) with Doom's thing state: the
state table drives it at 35 tics a second and its actions are Doom's AI.
Movement is Doom's own 2D movement against the map lines and the current
sector heights (P_TryMove: steps of 24, no dropping off ledges, lines
that block monsters), and against the other things and the players.
Monsters are solid bodies for the players, the shots and the movers (a
closing door goes back up on them, crushers hurt them).

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

#define MAXMOVE			30
#define STOPSPEED		( 1.0f / 16 )
#define FRICTION		0.90625f
#define FLOATSPEED		4
#define MAXSTEP			24
#define BASETHRESHOLD	100
#define SKULLSPEED		20
#define MAPBLOCK		128
#define MAXSPECHIT		16
#define TELEFOG_HEIGHT	0

typedef enum { DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_NODIR } dirtype_t;

static const float	xspeed[8] = { 1, 0.70710678f, 0, -0.70710678f, -1, -0.70710678f, 0, 0.70710678f };
static const float	yspeed[8] = { 0, 0.70710678f, 1, 0.70710678f, 0, -0.70710678f, -1, -0.70710678f };
static const int	opposite[9] = { DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR };
static const int	diags[4] = { DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST };

static doomMobj_t	mobjs[MAX_GENTITIES];

// results of a position check (tm* in p_map.c)
static struct {
	float		floorz, ceilingz, dropoffz;
	int			spechit[MAXSPECHIT];
	int			numspechit;
	qboolean	floatok;
} tm;

// lines by 128 unit blocks (the blockmap), and the lines of each sector
static struct {
	float		x, y;
	int			width, height;
	int			*first;			// per block: index in 'lines', count in first[b + 1] - first[b]
	int			*lines;
	int			*checked;		// per line: validcount it was last checked
	int			validcount;

	int			*sectorFirst;	// per sector: index in sectorLines
	int			*sectorLines;

	gentity_t	**soundTarget;	// per sector (P_NoiseAlert)
	int			*soundTraversed;
	int			*soundValid;
	int			soundValidcount;
} mm;

extern vmCvar_t	g_doomSkill;

static qboolean SetMobjState( gentity_t *ent, int state );
static void		Action( gentity_t *ent, int action );

/*
=====================================================================

GEOMETRY

=====================================================================
*/

static float ApproxDistance( float dx, float dy ) {
	dx = fabs( dx );
	dy = fabs( dy );
	return dx + dy - ( dx < dy ? dx : dy ) * 0.5f;
}

static float PointToAngle( float x1, float y1, float x2, float y2 ) {
	return AngleNormalize360( RAD2DEG( atan2( y2 - y1, x2 - x1 ) ) );
}

static int SideSector( const doom_linedef_t *ld, int side ) {
	return ld->side[side] == DOOM_NO_SIDE ? -1 : doomLevel.map->sidedefs[ld->side[side]].sector;
}

// 0 front, 1 back (P_PointOnLineSide)
static int PointOnLineSide( float x, float y, const doom_linedef_t *ld ) {
	const doom_vertex_t	*a = &doomLevel.map->vertexes[ld->v1], *b = &doomLevel.map->vertexes[ld->v2];

	return ( b->x - a->x ) * ( y - a->y ) - ( b->y - a->y ) * ( x - a->x ) < 0 ? 0 : 1;
}

// the side of the line the box is on, -1 if the line crosses it (P_BoxOnLineSide)
static int BoxOnLineSide( const float box[4], const doom_linedef_t *ld ) {
	int		s0 = PointOnLineSide( box[0], box[1], ld );

	if ( PointOnLineSide( box[2], box[1], ld ) != s0 || PointOnLineSide( box[0], box[3], ld ) != s0
		|| PointOnLineSide( box[2], box[3], ld ) != s0 ) {
		return -1;
	}
	return s0;
}

float Doom_ThingZ( gentity_t *ent ) {
	return ent->doomMobj ? ent->r.currentOrigin[2] : ent->r.absmin[2];
}

float Doom_ThingHeight( gentity_t *ent ) {
	return ent->doomMobj ? ent->doomMobj->height : ent->r.absmax[2] - ent->r.absmin[2];
}

static float ThingRadius( gentity_t *ent ) {
	return ent->doomMobj ? ent->doomMobj->radius : ( ent->r.absmax[0] - ent->r.absmin[0] ) * 0.5f;
}

static float ThingX( gentity_t *ent ) {
	return ( ent->r.absmin[0] + ent->r.absmax[0] ) * 0.5f;
}

static float ThingY( gentity_t *ent ) {
	return ( ent->r.absmin[1] + ent->r.absmax[1] ) * 0.5f;
}

/*
=====================================================================

SETUP: blockmap, sector lines

=====================================================================
*/

void Doom_InitMobjs( void ) {
	const doom_map_t	*map = doomLevel.map;
	float				minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
	int					i, b, total, *count;

	memset( &mm, 0, sizeof( mm ) );
	memset( mobjs, 0, sizeof( mobjs ) );
	if ( !map ) {
		return;		// a Quake 3 map: traces stand for the lines
	}
	for ( i = 0 ; i < map->num_vertexes ; i++ ) {
		minx = MIN( minx, map->vertexes[i].x );
		miny = MIN( miny, map->vertexes[i].y );
		maxx = MAX( maxx, map->vertexes[i].x );
		maxy = MAX( maxy, map->vertexes[i].y );
	}
	mm.x = minx - 8;
	mm.y = miny - 8;
	mm.width = (int)( ( maxx - mm.x ) / MAPBLOCK ) + 2;
	mm.height = (int)( ( maxy - mm.y ) / MAPBLOCK ) + 2;

	// blockmap: two passes, count then fill
	count = G_Alloc( sizeof( int ) * mm.width * mm.height );
	mm.first = G_Alloc( sizeof( int ) * ( mm.width * mm.height + 1 ) );
	for ( b = 0 ; b < 2 ; b++ ) {
		memset( count, 0, sizeof( int ) * mm.width * mm.height );
		for ( i = 0 ; i < map->num_linedefs ; i++ ) {
			const doom_vertex_t	*v1 = &map->vertexes[map->linedefs[i].v1], *v2 = &map->vertexes[map->linedefs[i].v2];
			int					x0 = (int)( ( MIN( v1->x, v2->x ) - mm.x ) / MAPBLOCK ), x1 = (int)( ( MAX( v1->x, v2->x ) - mm.x ) / MAPBLOCK );
			int					y0 = (int)( ( MIN( v1->y, v2->y ) - mm.y ) / MAPBLOCK ), y1 = (int)( ( MAX( v1->y, v2->y ) - mm.y ) / MAPBLOCK );
			int					x, y;

			for ( y = y0 ; y <= y1 ; y++ ) {
				for ( x = x0 ; x <= x1 ; x++ ) {
					int cell = y * mm.width + x;

					if ( b ) {
						mm.lines[mm.first[cell] + count[cell]] = i;
					}
					count[cell]++;
				}
			}
		}
		if ( !b ) {
			for ( i = 0, total = 0 ; i < mm.width * mm.height ; i++ ) {
				mm.first[i] = total;
				total += count[i];
			}
			mm.first[i] = total;
			mm.lines = G_Alloc( sizeof( int ) * ( total + 1 ) );
		}
	}
	mm.checked = G_Alloc( sizeof( int ) * map->num_linedefs );

	// the lines of each sector
	mm.sectorFirst = G_Alloc( sizeof( int ) * ( map->num_sectors + 1 ) );
	count = G_Alloc( sizeof( int ) * ( map->num_sectors + 1 ) );
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		for ( b = 0 ; b < 2 ; b++ ) {
			int s = SideSector( &map->linedefs[i], b );

			if ( s >= 0 && ( !b || s != SideSector( &map->linedefs[i], 0 ) ) ) {
				count[s]++;
			}
		}
	}
	for ( i = 0, total = 0 ; i < map->num_sectors ; i++ ) {
		mm.sectorFirst[i] = total;
		total += count[i];
		count[i] = 0;
	}
	mm.sectorFirst[i] = total;
	mm.sectorLines = G_Alloc( sizeof( int ) * ( total + 1 ) );
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		for ( b = 0 ; b < 2 ; b++ ) {
			int s = SideSector( &map->linedefs[i], b );

			if ( s >= 0 && ( !b || s != SideSector( &map->linedefs[i], 0 ) ) ) {
				mm.sectorLines[mm.sectorFirst[s] + count[s]++] = i;
			}
		}
	}

	mm.soundTarget = G_Alloc( sizeof( gentity_t * ) * map->num_sectors );
	mm.soundTraversed = G_Alloc( sizeof( int ) * map->num_sectors );
	mm.soundValid = G_Alloc( sizeof( int ) * map->num_sectors );
}

/*
=====================================================================

POSITION AND MOVEMENT (p_map.c)

=====================================================================
*/

static qboolean IsSolidFor( gentity_t *ent, gentity_t *other ) {
	if ( other->client ) {
		return other->health > 0 && other->client->sess.sessionTeam != TEAM_SPECTATOR;
	}
	if ( other->doomMobj ) {
		return ( other->doomMobj->flags & MF_SOLID ) != 0;
	}
	return other->classname && !strcmp( other->classname, "doom_thing_blocker" );
}

/*
=================
CheckPosition

P_CheckPosition: can 'ent' stand at x, y (at its height)? Sets tm with the
floor, ceiling and drop-off heights there and the special lines touched.
A flying lost soul hits the first thing in the way.
=================
*/
#define CP_THINGS	1
#define CP_LINES	2

#define QUAKE_MASK		( CONTENTS_SOLID | CONTENTS_MONSTERCLIP )
#define QUAKE_REACH		4096

/*
=================
QuakeGeometry

CheckPosition's floor, ceiling and drop-off on a Quake 3 map, from traces:
the box above a step's height must be clear (else a wall, with CP_LINES);
the floor is the highest under the box (none within reach: no room), the
drop-off the lowest under its middle and corners
=================
*/
static qboolean QuakeGeometry( gentity_t *ent, float x, float y, int what ) {
	doomMobj_t	*mo = ent->doomMobj;
	float		r = mo->radius, z = ent->r.currentOrigin[2] + MAXSTEP;
	vec3_t		start, end, mins, maxs;
	trace_t		tr;
	int			i;

	VectorSet( start, x, y, z );
	VectorSet( mins, -r, -r, 0 );
	VectorSet( maxs, r, r, mo->height - MAXSTEP > 1 ? mo->height - MAXSTEP : 1 );
	trap_Trace( &tr, start, mins, maxs, start, ent->s.number, QUAKE_MASK );
	if ( tr.startsolid ) {
		if ( what & CP_LINES ) {
			return qfalse;
		}
		tm.floorz = tm.dropoffz = ent->r.currentOrigin[2];
		tm.ceilingz = ent->r.currentOrigin[2] + mo->height;
		return qtrue;
	}

	VectorSet( end, x, y, z - QUAKE_REACH );
	trap_Trace( &tr, start, mins, maxs, end, ent->s.number, QUAKE_MASK );
	if ( tr.fraction == 1 ) {
		return qfalse;		// no floor: out of the world, or over the void
	}
	tm.floorz = tr.endpos[2];

	maxs[2] = 1;
	VectorSet( end, x, y, z + QUAKE_REACH );
	trap_Trace( &tr, start, mins, maxs, end, ent->s.number, QUAKE_MASK );
	tm.ceilingz = tr.endpos[2] + 1;

	tm.dropoffz = tm.floorz;
	for ( i = 0 ; i < 5 ; i++ ) {
		static const float	corners[5][2] = { { 0, 0 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
		float				in = r > 1 ? r - 1 : 0;

		VectorSet( start, x + corners[i][0] * in, y + corners[i][1] * in, z );
		VectorSet( end, start[0], start[1], z - QUAKE_REACH );
		trap_Trace( &tr, start, NULL, NULL, end, ent->s.number, QUAKE_MASK );
		if ( !tr.startsolid && tr.endpos[2] < tm.dropoffz ) {
			tm.dropoffz = tr.endpos[2];
		}
	}
	return qtrue;
}

static qboolean CheckPosition( gentity_t *ent, float x, float y, int what ) {
	doomMobj_t			*mo = ent->doomMobj;
	const doom_map_t	*map = doomLevel.map;
	float				box[4];
	int					sector, bx0, bx1, by0, by1, bx, by, i;

	tm.numspechit = 0;
	if ( !map ) {
		if ( !QuakeGeometry( ent, x, y, what ) ) {
			return qfalse;
		}
	} else {
		sector = doom_map_sector_at( map, x, y );
		if ( sector < 0 ) {
			return qfalse;
		}
		tm.floorz = tm.dropoffz = doomLevel.floor[sector];
		tm.ceilingz = doomLevel.ceiling[sector];
	}

	if ( mo->flags & MF_NOCLIP ) {
		return qtrue;
	}
	box[0] = x - mo->radius;
	box[1] = y - mo->radius;
	box[2] = x + mo->radius;
	box[3] = y + mo->radius;

	// things in the way (at an overlapping height: things can be stood on in 3D)
	if ( what & CP_THINGS ) {
		vec3_t	mins, maxs;
		int		list[MAX_GENTITIES], n;

		VectorSet( mins, box[0], box[1], ent->r.currentOrigin[2] + 1 );
		VectorSet( maxs, box[2], box[3], ent->r.currentOrigin[2] + mo->height - 1 );
		n = trap_EntitiesInBox( mins, maxs, list, MAX_GENTITIES );
		for ( i = 0 ; i < n ; i++ ) {
			gentity_t *other = &g_entities[list[i]];

			if ( other == ent || !IsSolidFor( ent, other ) ) {
				continue;
			}
			// a flying lost soul slams into it
			if ( mo->flags & MF_SKULLFLY ) {
				G_Damage( other, ent, ent, NULL, NULL, ( Doom_Random() % 8 + 1 ) * mo->info->damage, 0, MOD_UNKNOWN );
				mo->flags &= ~MF_SKULLFLY;
				VectorClear( mo->mom );
				SetMobjState( ent, mo->info->spawnstate );
			}
			return qfalse;
		}
	}

	if ( !( what & CP_LINES ) || !map ) {
		return qtrue;
	}

	// lines crossing the box (PIT_CheckLine)
	mm.validcount++;
	bx0 = (int)( ( box[0] - mm.x ) / MAPBLOCK );
	bx1 = (int)( ( box[2] - mm.x ) / MAPBLOCK );
	by0 = (int)( ( box[1] - mm.y ) / MAPBLOCK );
	by1 = (int)( ( box[3] - mm.y ) / MAPBLOCK );
	for ( by = MAX( by0, 0 ) ; by <= by1 && by < mm.height ; by++ ) {
		for ( bx = MAX( bx0, 0 ) ; bx <= bx1 && bx < mm.width ; bx++ ) {
			int cell = by * mm.width + bx;

			for ( i = mm.first[cell] ; i < mm.first[cell + 1] ; i++ ) {
				int						l = mm.lines[i], front, back;
				const doom_linedef_t	*ld = &map->linedefs[l];
				const doom_vertex_t		*v1 = &map->vertexes[ld->v1], *v2 = &map->vertexes[ld->v2];
				float					opentop, openbottom, lowfloor;

				if ( mm.checked[l] == mm.validcount ) {
					continue;
				}
				mm.checked[l] = mm.validcount;
				if ( box[2] <= MIN( v1->x, v2->x ) || box[0] >= MAX( v1->x, v2->x )
					|| box[3] <= MIN( v1->y, v2->y ) || box[1] >= MAX( v1->y, v2->y ) ) {
					continue;
				}
				if ( BoxOnLineSide( box, ld ) != -1 ) {
					continue;
				}
				front = SideSector( ld, 0 );
				back = SideSector( ld, 1 );
				// one sided lines, impassable lines, lines that block monsters
				if ( back < 0 || front < 0 || ( ld->flags & ( DOOM_ML_BLOCKING | DOOM_ML_BLOCKMONSTERS ) ) ) {
					return qfalse;
				}
				// P_LineOpening
				opentop = MIN( doomLevel.ceiling[front], doomLevel.ceiling[back] );
				openbottom = MAX( doomLevel.floor[front], doomLevel.floor[back] );
				lowfloor = MIN( doomLevel.floor[front], doomLevel.floor[back] );
				if ( opentop < tm.ceilingz ) {
					tm.ceilingz = opentop;
				}
				if ( openbottom > tm.floorz ) {
					tm.floorz = openbottom;
				}
				if ( lowfloor < tm.dropoffz ) {
					tm.dropoffz = lowfloor;
				}
				if ( doomLevel.lineSpecial[l] && tm.numspechit < MAXSPECHIT ) {
					tm.spechit[tm.numspechit++] = l;
				}
			}
		}
	}
	return qtrue;
}

// the box starts a little above the feet: a thing resting on a rising floor
// is not inside it, so the floor lifts it instead of being blocked by it
#define MOBJ_FLOOR_GAP	0.25f

static void LinkMobj( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;

	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trType = TR_INTERPOLATE;
	ent->s.angles[YAW] = mo->angle;
	ent->s.frame = mo->state;
	// shootable: a body; solid only (dying): blocks players, not shots
	ent->r.contents = ( mo->flags & MF_SHOOTABLE ) ? CONTENTS_BODY : ( mo->flags & MF_SOLID ) ? CONTENTS_PLAYERCLIP : 0;
	ent->r.mins[2] = mo->height > MOBJ_FLOOR_GAP ? MOBJ_FLOOR_GAP : 0;
	ent->r.maxs[2] = mo->height;
	trap_LinkEntity( ent );
}

static void SetPosition( gentity_t *ent, float x, float y, float z ) {
	ent->r.currentOrigin[0] = x;
	ent->r.currentOrigin[1] = y;
	ent->r.currentOrigin[2] = z;
	LinkMobj( ent );
}

/*
=================
TryMove

P_TryMove: moves there if it fits, may step up 24, does not drop off
ledges (unless it flies or is a corpse); crosses walk-over lines
=================
*/
static qboolean TryMove( gentity_t *ent, float x, float y ) {
	doomMobj_t	*mo = ent->doomMobj;
	float		oldx = ent->r.currentOrigin[0], oldy = ent->r.currentOrigin[1], z = ent->r.currentOrigin[2];
	int			i;

	tm.floatok = qfalse;
	if ( !CheckPosition( ent, x, y, CP_THINGS | CP_LINES ) ) {
		return qfalse;
	}
	if ( !( mo->flags & MF_NOCLIP ) ) {
		if ( tm.ceilingz - tm.floorz < mo->height ) {
			return qfalse;		// doesn't fit
		}
		tm.floatok = qtrue;
		if ( !( mo->flags & MF_TELEPORT ) && tm.ceilingz - z < mo->height ) {
			return qfalse;		// must lower itself to fit
		}
		if ( !( mo->flags & MF_TELEPORT ) && tm.floorz - z > MAXSTEP ) {
			return qfalse;		// too big a step up
		}
		if ( !( mo->flags & ( MF_DROPOFF | MF_FLOAT ) ) && tm.floorz - tm.dropoffz > MAXSTEP ) {
			return qfalse;		// don't stand over a dropoff
		}
	}

	mo->floorz = tm.floorz;
	mo->ceilingz = tm.ceilingz;
	SetPosition( ent, x, y, z );

	// walk-over lines (the ones monsters may trigger)
	if ( !( mo->flags & ( MF_TELEPORT | MF_NOCLIP ) ) ) {
		int		hits[MAXSPECHIT], n = tm.numspechit;

		memcpy( hits, tm.spechit, sizeof( hits ) );
		for ( i = n - 1 ; i >= 0 ; i-- ) {
			const doom_linedef_t	*ld = &doomLevel.map->linedefs[hits[i]];
			int						oldside = PointOnLineSide( oldx, oldy, ld );
			const doom_special_t	*sp = doom_line_special( doomLevel.lineSpecial[hits[i]] );

			if ( sp && sp->trigger == DOOM_TRIG_WALK && PointOnLineSide( x, y, ld ) != oldside ) {
				Doom_ActivateLine( ent, hits[i], oldside, DOOM_TRIG_WALK );
				if ( !ent->inuse || ent->doomMobj != mo ) {
					return qtrue;
				}
			}
		}
	}
	return qtrue;
}

/*
=================
ThingHeightClip

P_ThingHeightClip: floors and ceilings move under and over things; things
on the floor stay on it
=================
*/
static void ThingHeightClip( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	qboolean	onfloor = ent->r.currentOrigin[2] <= mo->floorz + 0.01f;
	float		z = ent->r.currentOrigin[2];

	if ( !CheckPosition( ent, ent->r.currentOrigin[0], ent->r.currentOrigin[1], CP_LINES ) ) {
		return;
	}
	mo->floorz = tm.floorz;
	mo->ceilingz = tm.ceilingz;
	if ( onfloor ) {
		z = mo->floorz;
	} else if ( z + mo->height > mo->ceilingz ) {
		z = MAX( mo->floorz, mo->ceilingz - mo->height );
	}
	if ( z != ent->r.currentOrigin[2] ) {
		SetPosition( ent, ent->r.currentOrigin[0], ent->r.currentOrigin[1], z );
	}
}

// P_XYMovement
static void XYMovement( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	float		xmove, ymove;

	if ( !mo->mom[0] && !mo->mom[1] ) {
		if ( mo->flags & MF_SKULLFLY ) {
			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			VectorClear( mo->mom );
			SetMobjState( ent, mo->info->spawnstate );
		}
		return;
	}
	mo->mom[0] = Com_Clamp( -MAXMOVE, MAXMOVE, mo->mom[0] );
	mo->mom[1] = Com_Clamp( -MAXMOVE, MAXMOVE, mo->mom[1] );
	xmove = mo->mom[0];
	ymove = mo->mom[1];
	do {
		float px, py;

		if ( fabs( xmove ) > MAXMOVE / 2 || fabs( ymove ) > MAXMOVE / 2 ) {
			px = ent->r.currentOrigin[0] + xmove * 0.5f;
			py = ent->r.currentOrigin[1] + ymove * 0.5f;
			xmove *= 0.5f;
			ymove *= 0.5f;
		} else {
			px = ent->r.currentOrigin[0] + xmove;
			py = ent->r.currentOrigin[1] + ymove;
			xmove = ymove = 0;
		}
		if ( !TryMove( ent, px, py ) ) {
			mo->mom[0] = mo->mom[1] = 0;
			break;
		}
		if ( !ent->inuse || ent->doomMobj != mo ) {
			return;
		}
	} while ( xmove || ymove );

	// friction, except in the air and for flying skulls
	if ( ( mo->flags & MF_SKULLFLY ) || ent->r.currentOrigin[2] > mo->floorz ) {
		return;
	}
	if ( fabs( mo->mom[0] ) < STOPSPEED && fabs( mo->mom[1] ) < STOPSPEED ) {
		mo->mom[0] = mo->mom[1] = 0;
	} else {
		mo->mom[0] *= FRICTION;
		mo->mom[1] *= FRICTION;
	}
}

// P_ZMovement
static void ZMovement( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	float		z = ent->r.currentOrigin[2] + mo->mom[2];

	// floaters move toward the height of their target
	if ( ( mo->flags & MF_FLOAT ) && mo->target && !( mo->flags & ( MF_SKULLFLY | MF_INFLOAT ) ) ) {
		float dist = ApproxDistance( ent->r.currentOrigin[0] - ThingX( mo->target ), ent->r.currentOrigin[1] - ThingY( mo->target ) );
		float delta = Doom_ThingZ( mo->target ) + mo->height * 0.5f - z;

		if ( delta < 0 && dist < -( delta * 3 ) ) {
			z -= FLOATSPEED;
		} else if ( delta > 0 && dist < delta * 3 ) {
			z += FLOATSPEED;
		}
	}

	if ( z <= mo->floorz ) {
		// hit the floor
		if ( mo->flags & MF_SKULLFLY ) {
			mo->mom[2] = -mo->mom[2];
		}
		if ( mo->mom[2] < 0 ) {
			mo->mom[2] = 0;
		}
		z = mo->floorz;
	} else if ( !( mo->flags & MF_NOGRAVITY ) ) {
		mo->mom[2] -= mo->mom[2] == 0 ? 2 : 1;
	}
	if ( z + mo->height > mo->ceilingz ) {
		// hit the ceiling
		if ( mo->mom[2] > 0 ) {
			mo->mom[2] = 0;
		}
		z = mo->ceilingz - mo->height;
		if ( mo->flags & MF_SKULLFLY ) {
			mo->mom[2] = -mo->mom[2];
		}
	}
	if ( z != ent->r.currentOrigin[2] ) {
		SetPosition( ent, ent->r.currentOrigin[0], ent->r.currentOrigin[1], z );
	}
}

/*
=====================================================================

SPAWNING, STATES, REMOVAL (p_mobj.c)

=====================================================================
*/

static qboolean FastMonsters( void ) {
	return g_doomSkill.integer >= DOOM_SK_NIGHTMARE;
}

/*
=================
SetMobjState

P_SetMobjState: enter a state and run its action; zero-tic states pass
through. Returns qfalse if the thing was removed.
=================
*/
static qboolean SetMobjState( gentity_t *ent, int state ) {
	doomMobj_t	*mo = ent->doomMobj;
	int			guard = 0;

	do {
		const doom_state_t *st;

		if ( state <= S_NULL || state >= NUMSTATES ) {
			mo->state = S_NULL;
			G_FreeEntity( ent );
			return qfalse;
		}
		st = &doom_states[state];
		mo->state = state;
		mo->tics = st->tics;
		// nightmare: demons are twice as fast (G_InitNew)
		if ( FastMonsters() && state >= S_SARG_RUN1 && state <= S_SARG_PAIN2 && mo->tics > 0 ) {
			mo->tics >>= 1;
		}
		ent->s.frame = state;
		if ( st->action != ACT_NULL ) {
			Action( ent, st->action );
			if ( !ent->inuse || ent->doomMobj != mo ) {
				return qfalse;
			}
		}
		state = st->nextstate;
	} while ( !mo->tics && ++guard < 64 );
	return qtrue;
}

/*
=================
Doom_SpawnMobj

P_SpawnMobj: on the floor (or the ceiling for hanging things); on a
Quake 3 map, the floor and ceiling around fromZ
=================
*/
gentity_t *Doom_SpawnMobj( int type, float x, float y, float angle ) {
	return Doom_SpawnMobjAt( type, x, y, 0, angle );
}

gentity_t *Doom_SpawnMobjAt( int type, float x, float y, float fromZ, float angle ) {
	const doom_mobjinfo_t	*info = &doom_mobjinfo[type];
	gentity_t				*ent = G_Spawn();
	doomMobj_t				*mo = &mobjs[ent->s.number];
	float					z;

	memset( mo, 0, sizeof( *mo ) );
	ent->doomMobj = mo;
	ent->classname = "doom_thing";
	mo->type = type;
	mo->info = info;
	mo->flags = info->flags;
	mo->radius = (float)info->radius / DOOM_FRACUNIT;
	mo->height = (float)info->height / DOOM_FRACUNIT;
	mo->angle = angle;
	mo->movedir = DI_NODIR;
	mo->reactiontime = g_doomSkill.integer != DOOM_SK_NIGHTMARE ? info->reactiontime : 0;
	mo->lastlook = Doom_Random() % MAX_CLIENTS;
	mo->spawnX = x;
	mo->spawnY = y;
	mo->spawnAngle = angle;
	mo->spawnZ = fromZ;
	mo->state = info->spawnstate;
	mo->tics = doom_states[mo->state].tics;

	ent->health = info->spawnhealth;
	ent->takedamage = ( mo->flags & MF_SHOOTABLE ) != 0;
	ent->s.eType = ET_DOOM_THING;
	ent->s.generic1 = type;
	ent->s.frame = mo->state;
	ent->physicsObject = qtrue;		// movers push it and are blocked by it
	ent->clipmask = MASK_SOLID;
	VectorSet( ent->r.mins, -mo->radius, -mo->radius, MOBJ_FLOOR_GAP );
	VectorSet( ent->r.maxs, mo->radius, mo->radius, mo->height );

	ent->r.currentOrigin[0] = x;
	ent->r.currentOrigin[1] = y;
	ent->r.currentOrigin[2] = fromZ;
	CheckPosition( ent, x, y, CP_LINES );
	mo->floorz = tm.floorz;
	mo->ceilingz = tm.ceilingz;
	z = ( mo->flags & MF_SPAWNCEILING ) ? mo->ceilingz - mo->height : mo->floorz;
	G_SetOrigin( ent, ent->r.currentOrigin );
	SetPosition( ent, x, y, z );
	return ent;
}

/*
=================
Doom_SpawnMobjs

The monsters and barrels of the map on this skill (P_SpawnMapThing)
=================
*/
void Doom_SpawnMobjs( void ) {
	const doom_map_t	*map = doomLevel.map;
	int					i, monsters = 0, others = 0;

	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t	*t = &map->things[i];
		int					type = doom_mobjtype_for_ednum( t->type );
		gentity_t			*ent;

		if ( type < 0 || type == MT_PLAYER || !BG_DoomThingOnSkill( t, g_doomSkill.integer, doomLevel.deathmatch ) ) {
			continue;
		}
		if ( !( doom_mobjinfo[type].flags & ( MF_COUNTKILL | MF_SHOOTABLE ) ) ) {
			continue;
		}
		// no monsters in a deathmatch, unless asked for
		if ( ( doom_mobjinfo[type].flags & MF_COUNTKILL ) && ( !g_doomMonsters.integer || ( doomLevel.deathmatch && !g_doomMatchMonsters.integer )
			|| ( Doom_DuelActive() && !g_duelMonsters.integer ) ) ) {
			continue;
		}
		ent = Doom_SpawnMobj( type, t->x, t->y, t->angle );
		ent->doomMobj->spawnFlags = t->flags;
		if ( ent->doomMobj->tics > 0 ) {
			ent->doomMobj->tics = 1 + Doom_Random() % ent->doomMobj->tics;
		}
		if ( t->flags & DOOM_MTF_AMBUSH ) {
			ent->doomMobj->flags |= MF_AMBUSH;
		}
		if ( doom_mobjinfo[type].flags & MF_COUNTKILL ) {
			doomLevel.totalKills++;
			monsters++;
		} else {
			others++;
		}
	}
	G_Printf( "Doom monsters: %d, other things that act: %d\n", monsters, others );
}

static void SpawnTeleFog( float x, float y, float z ) {
	vec3_t	pos;

	VectorSet( pos, x, y, z );
	Doom_SpawnFx( pos, doom_mobjinfo[MT_TFOG].spawnstate, 0 );
	{
		gentity_t *te = G_TempEntity( pos, EV_DOOM_SOUND );

		te->s.eventParm = sfx_telept;
	}
}

/*
=================
NightmareRespawn

P_NightmareRespawn: a dead monster comes back where it started
=================
*/
static void NightmareRespawn( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	gentity_t	*fresh;
	float		x = mo->spawnX, y = mo->spawnY;

	// somthing is occupying its position?
	if ( !DOOM_LEVEL() ) {
		if ( !Doom_MobjFits( mo->type, x, y, mo->spawnZ ) ) {
			return;
		}
	} else {
		gentity_t	probe;
		doomMobj_t	pm = *mo;

		memset( &probe, 0, sizeof( probe ) );
		probe.doomMobj = &pm;
		pm.flags = mo->info->flags;
		pm.height = (float)mo->info->height / DOOM_FRACUNIT;
		probe.r.currentOrigin[2] = doomLevel.floor[MAX( 0, doom_map_sector_at( doomLevel.map, x, y ) )];
		if ( !CheckPosition( &probe, x, y, CP_THINGS | CP_LINES ) ) {
			return;
		}
	}
	SpawnTeleFog( ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2] );
	fresh = Doom_SpawnMobjAt( mo->type, x, y, mo->spawnZ, mo->spawnAngle );
	fresh->doomMobj->spawnFlags = mo->spawnFlags;
	if ( mo->spawnFlags & DOOM_MTF_AMBUSH ) {
		fresh->doomMobj->flags |= MF_AMBUSH;
	}
	fresh->doomMobj->reactiontime = 18;
	SpawnTeleFog( x, y, fresh->r.currentOrigin[2] );
	G_FreeEntity( ent );
}

// P_MobjThinker
static void MobjThinker( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;

	ThingHeightClip( ent );

	// PIT_ChangeSector: a corpse the floor and ceiling close on is crushed to gibs
	if ( ent->health <= 0 && mo->height > 0 && mo->ceilingz - mo->floorz < mo->height ) {
		SetMobjState( ent, S_GIBS );
		mo->flags &= ~MF_SOLID;
		mo->height = 0;
		mo->radius = 0;
		LinkMobj( ent );
		return;
	}

	if ( mo->mom[0] || mo->mom[1] || ( mo->flags & MF_SKULLFLY ) ) {
		XYMovement( ent );
		if ( !ent->inuse || ent->doomMobj != mo ) {
			return;
		}
	}
	if ( ent->r.currentOrigin[2] != mo->floorz || mo->mom[2] ) {
		ZMovement( ent );
	}

	if ( mo->tics != -1 ) {
		if ( --mo->tics <= 0 ) {
			SetMobjState( ent, doom_states[mo->state].nextstate );
		}
		return;
	}
	// nightmare: dead monsters come back (in Doom Duel, when a player dies)
	if ( g_doomSkill.integer == DOOM_SK_NIGHTMARE && ( mo->flags & MF_COUNTKILL ) && ( mo->flags & MF_CORPSE ) && !Doom_DuelActive() ) {
		mo->movecount++;
		if ( mo->movecount < 12 * DOOM_TICRATE || ( level.time / 1000 * DOOM_TICRATE & 31 ) || Doom_Random() > 4 ) {
			return;
		}
		NightmareRespawn( ent );
	}
}

/*
=================
Doom_RespawnDeadMonsters

Doom Duel: when a player dies, the dead monsters come back where they
started (those whose spot is taken stay dead)
=================
*/
void Doom_RespawnDeadMonsters( void ) {
	int		i, n = 0;

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t	*ent = &g_entities[i];

		if ( ent->inuse && ent->doomMobj && ( ent->doomMobj->flags & MF_COUNTKILL ) && ( ent->doomMobj->flags & MF_CORPSE )
			&& !Doom_DuelNearSpawn( ent->doomMobj->spawnX, ent->doomMobj->spawnY ) ) {
			NightmareRespawn( ent );
			n++;
		}
	}
	if ( n ) {
		G_Printf( "Doom Duel: %d dead monsters come back\n", n );
	}
}

void Doom_RunMobjs( void ) {
	int		i;

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t *ent = &g_entities[i];

		if ( ent->inuse && ent->doomMobj ) {
			MobjThinker( ent );
		}
	}
}

/*
=====================================================================

DAMAGE AND DEATH (p_inter.c)

=====================================================================
*/

static void KillMobj( gentity_t *source, gentity_t *target ) {
	doomMobj_t	*mo = target->doomMobj;
	int			item = -1;

	mo->flags &= ~( MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY );
	if ( mo->type != MT_SKULL ) {
		mo->flags &= ~MF_NOGRAVITY;
	}
	mo->flags |= MF_CORPSE | MF_DROPOFF;
	mo->height /= 4;
	target->takedamage = qfalse;
	if ( mo->flags & MF_COUNTKILL ) {
		doomLevel.kills++;
		Doom_FragUpdate();
	}

	if ( target->health < -mo->info->spawnhealth && mo->info->xdeathstate ) {
		SetMobjState( target, mo->info->xdeathstate );
	} else {
		SetMobjState( target, mo->info->deathstate );
	}
	if ( !target->inuse || target->doomMobj != mo ) {
		return;
	}
	mo->tics -= Doom_Random() & 3;
	if ( mo->tics < 1 ) {
		mo->tics = 1;
	}
	mo->movecount = 0;
	LinkMobj( target );

	// drop stuff
	switch ( mo->type ) {
	case MT_WOLFSS:
	case MT_POSSESSED:
		item = MT_CLIP;
		break;
	case MT_SHOTGUY:
		item = MT_SHOTGUN;
		break;
	case MT_CHAINGUY:
		item = MT_CHAINGUN;
		break;
	default:
		return;
	}
	Doom_SpawnItemAt( BG_DoomItemForThing( doom_mobjinfo[item].doomednum ), target->r.currentOrigin[0], target->r.currentOrigin[1],
		target->r.currentOrigin[2], qtrue );
}

/*
=================
Doom_DamageMobj

P_DamageMobj on a monster or barrel (G_Damage sends them here): the
thrust, pain, waking up and turning on the attacker (infighting)
=================
*/
void Doom_DamageMobj( gentity_t *target, gentity_t *inflictor, gentity_t *source, vec3_t dir, int damage, int dflags ) {
	doomMobj_t	*mo = target->doomMobj;

	if ( !( mo->flags & MF_SHOOTABLE ) || target->health <= 0 ) {
		return;
	}
	// the cyberdemon and the spider mastermind ignore blasts
	if ( ( dflags & DAMAGE_RADIUS ) && ( mo->type == MT_CYBORG || mo->type == MT_SPIDER ) ) {
		return;
	}
	if ( inflictor == &g_entities[ENTITYNUM_WORLD] ) {
		inflictor = NULL;
	}
	if ( source == &g_entities[ENTITYNUM_WORLD] ) {
		source = NULL;
	}
	if ( mo->flags & MF_SKULLFLY ) {
		VectorClear( mo->mom );
	}

	// thrust away from the inflictor (not from a chainsaw)
	if ( !( mo->flags & MF_NOCLIP ) && ( inflictor || dir )
		&& !( source && source->client && source->client->ps.weapon == DOOM_WP( DWP_CHAINSAW ) ) ) {
		float	ang = inflictor ? PointToAngle( ThingX( inflictor ), ThingY( inflictor ), target->r.currentOrigin[0], target->r.currentOrigin[1] )
						: RAD2DEG( atan2( dir[1], dir[0] ) );
		float	thrust = damage * 0.125f * 100 / mo->info->mass;

		// make fall forwards sometimes
		if ( inflictor && damage < 40 && damage > target->health && target->r.currentOrigin[2] - Doom_ThingZ( inflictor ) > 64
			&& ( Doom_Random() & 1 ) ) {
			ang += 180;
			thrust *= 4;
		}
		mo->mom[0] += thrust * cos( DEG2RAD( ang ) );
		mo->mom[1] += thrust * sin( DEG2RAD( ang ) );
	}

	target->health -= damage;
	if ( target->health <= 0 ) {
		mo->target = source;
		KillMobj( source, target );
		return;
	}

	if ( Doom_Random() < mo->info->painchance && !( mo->flags & MF_SKULLFLY ) ) {
		mo->flags |= MF_JUSTHIT;	// fight back!
		if ( !SetMobjState( target, mo->info->painstate ) ) {
			return;
		}
	}
	mo->reactiontime = 0;			// we're awake now

	if ( !mo->threshold && source && source != target && ( source->client || source->doomMobj ) ) {
		// if not intent on another player, chase after this one
		mo->target = source;
		mo->threshold = BASETHRESHOLD;
		if ( mo->state == mo->info->spawnstate && mo->info->seestate != S_NULL ) {
			SetMobjState( target, mo->info->seestate );
		}
	}
}

/*
=====================================================================

SOUND ALERTS (P_NoiseAlert)

=====================================================================
*/

static void RecursiveSound( int sector, int soundblocks, gentity_t *target ) {
	int		i;

	if ( mm.soundValid[sector] == mm.soundValidcount && mm.soundTraversed[sector] <= soundblocks + 1 ) {
		return;		// already flooded
	}
	mm.soundValid[sector] = mm.soundValidcount;
	mm.soundTraversed[sector] = soundblocks + 1;
	mm.soundTarget[sector] = target;

	for ( i = mm.sectorFirst[sector] ; i < mm.sectorFirst[sector + 1] ; i++ ) {
		const doom_linedef_t	*ld = &doomLevel.map->linedefs[mm.sectorLines[i]];
		int						front = SideSector( ld, 0 ), back = SideSector( ld, 1 ), other;

		if ( !( ld->flags & DOOM_ML_TWOSIDED ) || front < 0 || back < 0 ) {
			continue;
		}
		// a closed door
		if ( MIN( doomLevel.ceiling[front], doomLevel.ceiling[back] ) - MAX( doomLevel.floor[front], doomLevel.floor[back] ) <= 0 ) {
			continue;
		}
		other = front == sector ? back : front;
		if ( ld->flags & DOOM_ML_SOUNDBLOCK ) {
			if ( !soundblocks ) {
				RecursiveSound( other, 1, target );
			}
		} else {
			RecursiveSound( other, soundblocks, target );
		}
	}
}

/*
=================
Doom_NoiseAlert

A shot is heard through the open sectors around (two sound blocking
lines stop it); the monsters there wake up and come. On a Quake 3 map, by
the monsters within QUAKE_HEARING in the shooter's potentially visible set.
=================
*/
#define QUAKE_HEARING	1500

void Doom_NoiseAlert( gentity_t *emitter ) {
	int		sector, i;

	if ( emitter->flags & FL_NOTARGET ) {
		return;
	}
	if ( !DOOM_LEVEL() ) {
		for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
			gentity_t *ent = &g_entities[i];

			if ( ent->inuse && ent->doomMobj && ent->health > 0
				&& Distance( ent->r.currentOrigin, emitter->r.currentOrigin ) < QUAKE_HEARING
				&& trap_InPVS( emitter->r.currentOrigin, ent->r.currentOrigin ) ) {
				ent->doomMobj->soundTarget = emitter;
			}
		}
		return;
	}
	sector = doom_map_sector_at( doomLevel.map, emitter->r.currentOrigin[0], emitter->r.currentOrigin[1] );
	if ( sector < 0 ) {
		return;
	}
	mm.soundValidcount++;
	RecursiveSound( sector, 0, emitter );
}

/*
=====================================================================

ENEMY AI (p_enemy.c)

=====================================================================
*/

static qboolean Shootable( gentity_t *ent ) {
	return ent && ent->inuse && Doom_IsTarget( ent ) && !( ent->flags & FL_NOTARGET );
}

/*
=================
CheckSight

P_CheckSight: from the eyes (3/4 of the height) to any of the top, the
middle or the bottom of the other thing, through the solid world
=================
*/
static qboolean CheckSight( gentity_t *ent, gentity_t *other ) {
	vec3_t		eye, end;
	trace_t		tr;
	float		z = Doom_ThingZ( other ), h = Doom_ThingHeight( other );
	int			i;

	VectorCopy( ent->r.currentOrigin, eye );
	eye[2] += Doom_ThingHeight( ent ) * 0.75f;
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorSet( end, ThingX( other ), ThingY( other ), z + h * ( i == 0 ? 0.5f : i == 1 ? 0.95f : 0.05f ) );
		trap_Trace( &tr, eye, NULL, NULL, end, ent->s.number, MASK_SOLID );
		if ( tr.fraction == 1 || tr.entityNum == other->s.number ) {
			return qtrue;
		}
	}
	return qfalse;
}

static float DistanceTo( gentity_t *ent, gentity_t *other ) {
	return ApproxDistance( ThingX( other ) - ent->r.currentOrigin[0], ThingY( other ) - ent->r.currentOrigin[1] );
}

/*
=================
LookForPlayers

P_LookForPlayers: a living player in sight, in front (or close behind)
unless 'allaround'
=================
*/
static qboolean LookForPlayers( gentity_t *ent, qboolean allaround ) {
	doomMobj_t	*mo = ent->doomMobj;
	int			i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gentity_t	*player = &g_entities[( mo->lastlook + i ) % level.maxclients];
		float		an;

		if ( !player->client || player->client->pers.connected != CON_CONNECTED || !Shootable( player ) ) {
			continue;
		}
		if ( !CheckSight( ent, player ) ) {
			continue;
		}
		if ( !allaround ) {
			an = AngleNormalize360( PointToAngle( ent->r.currentOrigin[0], ent->r.currentOrigin[1], ThingX( player ), ThingY( player ) ) - mo->angle );
			if ( an > 90 && an < 270 && DistanceTo( ent, player ) > DOOM_MELEERANGE ) {
				continue;	// behind its back
			}
		}
		mo->lastlook = player->s.number;
		mo->target = player;
		return qtrue;
	}
	return qfalse;
}

/*
=================
MobjSound

A thing's sound; the sight and death sounds of some come in variants,
the bosses' are heard everywhere
=================
*/
static void MobjSound( gentity_t *ent, int sfx ) {
	int		type = ent->doomMobj->type;

	switch ( sfx ) {
	case sfx_posit1: case sfx_posit2: case sfx_posit3:
		sfx = sfx_posit1 + Doom_Random() % 3;
		break;
	case sfx_bgsit1: case sfx_bgsit2:
		sfx = sfx_bgsit1 + Doom_Random() % 2;
		break;
	case sfx_podth1: case sfx_podth2: case sfx_podth3:
		sfx = sfx_podth1 + Doom_Random() % 3;
		break;
	case sfx_bgdth1: case sfx_bgdth2:
		sfx = sfx_bgdth1 + Doom_Random() % 2;
		break;
	default:
		break;
	}
	if ( sfx <= sfx_None ) {
		return;
	}
	if ( ( type == MT_SPIDER || type == MT_CYBORG ) && ( sfx == ent->doomMobj->info->seesound || sfx == ent->doomMobj->info->deathsound ) ) {
		sfx |= DOOM_SOUND_FULL;
	}
	Doom_Sound( ent, sfx );
}

static qboolean CheckMeleeRange( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;

	if ( !mo->target ) {
		return qfalse;
	}
	if ( DistanceTo( ent, mo->target ) >= DOOM_MELEERANGE - 20 + ThingRadius( mo->target ) ) {
		return qfalse;
	}
	return CheckSight( ent, mo->target );
}

static qboolean CheckMissileRange( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	float		dist;

	if ( !CheckSight( ent, mo->target ) ) {
		return qfalse;
	}
	if ( mo->flags & MF_JUSTHIT ) {
		// the target just hit the enemy, so fight back!
		mo->flags &= ~MF_JUSTHIT;
		return qtrue;
	}
	if ( mo->reactiontime ) {
		return qfalse;		// do not attack yet
	}
	dist = DistanceTo( ent, mo->target ) - 64;
	if ( !mo->info->meleestate ) {
		dist -= 128;		// no melee attack, so fire more
	}
	if ( mo->type == MT_CYBORG || mo->type == MT_SPIDER || mo->type == MT_SKULL ) {
		dist *= 0.5f;
	}
	if ( dist > 200 ) {
		dist = 200;
	}
	if ( mo->type == MT_CYBORG && dist > 160 ) {
		dist = 160;
	}
	return Doom_Random() >= dist;
}

/*
=================
Move

P_Move: a step in the move direction; when blocked, floaters change
height and the others try to open the doors they touch
=================
*/
static qboolean Move( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	float		speed = (float)mo->info->speed;
	int			i, n;
	qboolean	good;

	if ( mo->movedir == DI_NODIR ) {
		return qfalse;
	}
	if ( TryMove( ent, ent->r.currentOrigin[0] + speed * xspeed[mo->movedir], ent->r.currentOrigin[1] + speed * yspeed[mo->movedir] ) ) {
		if ( !ent->inuse || ent->doomMobj != mo ) {
			return qfalse;
		}
		mo->flags &= ~MF_INFLOAT;
		if ( !( mo->flags & MF_FLOAT ) ) {
			SetPosition( ent, ent->r.currentOrigin[0], ent->r.currentOrigin[1], mo->floorz );
		}
		return qtrue;
	}
	if ( ( mo->flags & MF_FLOAT ) && tm.floatok ) {
		SetPosition( ent, ent->r.currentOrigin[0], ent->r.currentOrigin[1],
			ent->r.currentOrigin[2] + ( ent->r.currentOrigin[2] < tm.floorz ? FLOATSPEED : -FLOATSPEED ) );
		mo->flags |= MF_INFLOAT;
		return qtrue;
	}
	n = tm.numspechit;
	if ( !n ) {
		return qfalse;
	}
	mo->movedir = DI_NODIR;
	good = qfalse;
	for ( i = n - 1 ; i >= 0 ; i-- ) {
		int line = tm.spechit[i];

		// monsters open doors by using them (never secret ones)
		if ( !( doomLevel.map->linedefs[line].flags & DOOM_ML_SECRET ) && Doom_ActivateLine( ent, line, 0, DOOM_TRIG_SWITCH ) ) {
			good = qtrue;
		}
		if ( !ent->inuse || ent->doomMobj != mo ) {
			return qfalse;
		}
	}
	return good;
}

static qboolean TryWalk( gentity_t *ent ) {
	if ( !Move( ent ) ) {
		return qfalse;
	}
	ent->doomMobj->movecount = Doom_Random() & 15;
	return qtrue;
}

// P_NewChaseDir
static void NewChaseDir( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	int			d[3], tdir, olddir = mo->movedir, turnaround = opposite[mo->movedir];
	float		deltax = ThingX( mo->target ) - ent->r.currentOrigin[0];
	float		deltay = ThingY( mo->target ) - ent->r.currentOrigin[1];

	d[1] = deltax > 10 ? DI_EAST : deltax < -10 ? DI_WEST : DI_NODIR;
	d[2] = deltay < -10 ? DI_SOUTH : deltay > 10 ? DI_NORTH : DI_NODIR;

	// try direct route
	if ( d[1] != DI_NODIR && d[2] != DI_NODIR ) {
		mo->movedir = diags[( ( deltay < 0 ) << 1 ) + ( deltax > 0 )];
		if ( mo->movedir != turnaround && TryWalk( ent ) ) {
			return;
		}
	}
	// try other directions
	if ( Doom_Random() > 200 || fabs( deltay ) > fabs( deltax ) ) {
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}
	if ( d[1] == turnaround ) {
		d[1] = DI_NODIR;
	}
	if ( d[2] == turnaround ) {
		d[2] = DI_NODIR;
	}
	if ( d[1] != DI_NODIR ) {
		mo->movedir = d[1];
		if ( TryWalk( ent ) ) {
			return;
		}
	}
	if ( d[2] != DI_NODIR ) {
		mo->movedir = d[2];
		if ( TryWalk( ent ) ) {
			return;
		}
	}
	// there is no direct path to the player, so pick another direction
	if ( olddir != DI_NODIR ) {
		mo->movedir = olddir;
		if ( TryWalk( ent ) ) {
			return;
		}
	}
	// randomly determine direction of search
	if ( Doom_Random() & 1 ) {
		for ( tdir = DI_EAST ; tdir <= DI_SOUTHEAST ; tdir++ ) {
			if ( tdir != turnaround ) {
				mo->movedir = tdir;
				if ( TryWalk( ent ) ) {
					return;
				}
			}
		}
	} else {
		for ( tdir = DI_SOUTHEAST ; tdir >= DI_EAST ; tdir-- ) {
			if ( tdir != turnaround ) {
				mo->movedir = tdir;
				if ( TryWalk( ent ) ) {
					return;
				}
			}
		}
	}
	if ( turnaround != DI_NODIR ) {
		mo->movedir = turnaround;
		if ( TryWalk( ent ) ) {
			return;
		}
	}
	mo->movedir = DI_NODIR;		// can not move
}

// A_FaceTarget: invisible targets are hard to aim at
static void FaceTarget( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;

	if ( !mo->target ) {
		return;
	}
	mo->flags &= ~MF_AMBUSH;
	mo->angle = PointToAngle( ent->r.currentOrigin[0], ent->r.currentOrigin[1], ThingX( mo->target ), ThingY( mo->target ) );
	if ( mo->target->client && mo->target->client->ps.powerups[DPW_INVISIBILITY] ) {
		mo->angle += ( Doom_Random() - Doom_Random() ) * ( 360.0f / 2048 );
	}
}

// the height monsters shoot from (P_AimLineAttack's shootz)
static void ShootPoint( gentity_t *ent, vec3_t eye ) {
	VectorCopy( ent->r.currentOrigin, eye );
	eye[2] += ent->doomMobj->height * 0.5f + 8;
}

// P_AimLineAttack at the target along the facing: the pitch, 0 if nothing
static float AimPitch( gentity_t *ent, const vec3_t eye, float yaw ) {
	float	pitch;

	return Doom_AimLineAttack( ent, eye, yaw, DOOM_MISSILERANGE, &pitch ) ? pitch : 0;
}

/*
=================
SpawnMissile

P_SpawnMissile: 32 units up, toward the target's feet height at that
distance (with the fuzz of an invisible target)
=================
*/
static void SpawnMissile( gentity_t *ent, gentity_t *dest, int type ) {
	const doom_mobjinfo_t	*info = &doom_mobjinfo[type];
	float					speed = (float)info->speed / DOOM_FRACUNIT, angle, dist;
	vec3_t					start, velocity;

	if ( !dest ) {
		return;
	}
	if ( FastMonsters() && ( type == MT_BRUISERSHOT || type == MT_HEADSHOT || type == MT_TROOPSHOT ) ) {
		speed = 20;
	}
	VectorCopy( ent->r.currentOrigin, start );
	start[2] += 32;
	angle = PointToAngle( start[0], start[1], ThingX( dest ), ThingY( dest ) );
	if ( dest->client && dest->client->ps.powerups[DPW_INVISIBILITY] ) {
		angle += ( Doom_Random() - Doom_Random() ) * ( 360.0f / 4096 );
	}
	dist = ApproxDistance( ThingX( dest ) - start[0], ThingY( dest ) - start[1] ) / speed;
	if ( dist < 1 ) {
		dist = 1;
	}
	velocity[0] = cos( DEG2RAD( angle ) ) * speed * DOOM_TICRATE;
	velocity[1] = sin( DEG2RAD( angle ) ) * speed * DOOM_TICRATE;
	velocity[2] = ( Doom_ThingZ( dest ) - ent->r.currentOrigin[2] ) / dist * DOOM_TICRATE;
	Doom_LaunchMissile( ent, start, velocity, type );
	MobjSound( ent, info->seesound );
}

/*
=================
BossDeath

A_BossDeath (The Ultimate Doom): when the last boss of the map dies, the
tag 666 sectors move or the level ends
=================
*/
static void BossDeath( gentity_t *ent ) {
	const char	*map = Doom_LevelName();
	int			type = ent->doomMobj->type, i;
	qboolean	e1m8, e2m8, e3m8, e4m6, e4m8;

	if ( !DOOM_LEVEL() ) {
		return;
	}
	e1m8 =!Q_stricmp( map, "E1M8" ) && type == MT_BRUISER;
	e2m8 = !Q_stricmp( map, "E2M8" ) && type == MT_CYBORG;
	e3m8 = !Q_stricmp( map, "E3M8" ) && type == MT_SPIDER;
	e4m6 = !Q_stricmp( map, "E4M6" ) && type == MT_CYBORG;
	e4m8 = !Q_stricmp( map, "E4M8" ) && type == MT_SPIDER;
	if ( !e1m8 && !e2m8 && !e3m8 && !e4m6 && !e4m8 ) {
		return;
	}
	// all of them dead?
	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t *other = &g_entities[i];

		if ( other != ent && other->inuse && other->doomMobj && other->doomMobj->type == type && other->health > 0 ) {
			return;
		}
	}
	if ( e2m8 || e3m8 ) {
		Doom_ExitLevel( NULL, qfalse );
		return;
	}
	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		if ( doomLevel.map->sectors[i].tag != 666 ) {
			continue;
		}
		if ( e4m6 ) {
			Doom_DoDoor( i, DOOM_DOOR_BLAZEOPEN, 0 );
		} else {
			Doom_DoFloor( i, DOOM_FLOOR_LOWERTOLOWEST, 0 );
		}
	}
}

// A_Look
static void Look( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;
	int			sector = DOOM_LEVEL() ? doom_map_sector_at( doomLevel.map, ent->r.currentOrigin[0], ent->r.currentOrigin[1] ) : -1;
	gentity_t	*targ = sector >= 0 ? mm.soundTarget[sector] : DOOM_LEVEL() ? NULL : mo->soundTarget;
	qboolean	seen = qfalse;

	mo->threshold = 0;		// any shot will wake up
	if ( Shootable( targ ) ) {
		mo->target = targ;
		seen = !( mo->flags & MF_AMBUSH ) || CheckSight( ent, targ );
	}
	if ( !seen && !LookForPlayers( ent, qfalse ) ) {
		return;
	}
	// go into chase state
	MobjSound( ent, mo->info->seesound );
	SetMobjState( ent, mo->info->seestate );
}

// A_Chase
static void Chase( gentity_t *ent ) {
	doomMobj_t	*mo = ent->doomMobj;

	if ( mo->reactiontime ) {
		mo->reactiontime--;
	}
	// modify target threshold
	if ( mo->threshold ) {
		if ( !Shootable( mo->target ) ) {
			mo->threshold = 0;
		} else {
			mo->threshold--;
		}
	}
	// turn towards movement direction if not there yet
	if ( mo->movedir < 8 ) {
		float a = floor( AngleNormalize360( mo->angle ) / 45 ) * 45;
		float delta = AngleNormalize180( a - mo->movedir * 45 );

		if ( delta > 0 ) {
			a -= 45;
		} else if ( delta < 0 ) {
			a += 45;
		}
		mo->angle = AngleNormalize360( a );
	}
	if ( !Shootable( mo->target ) ) {
		// look for a new target
		if ( LookForPlayers( ent, qtrue ) ) {
			return;
		}
		SetMobjState( ent, mo->info->spawnstate );
		return;
	}
	// do not attack twice in a row
	if ( mo->flags & MF_JUSTATTACKED ) {
		mo->flags &= ~MF_JUSTATTACKED;
		if ( !FastMonsters() ) {
			NewChaseDir( ent );
		}
		return;
	}
	// check for melee attack
	if ( mo->info->meleestate && CheckMeleeRange( ent ) ) {
		MobjSound( ent, mo->info->attacksound );
		SetMobjState( ent, mo->info->meleestate );
		return;
	}
	// check for missile attack
	if ( mo->info->missilestate && ( FastMonsters() || !mo->movecount ) && CheckMissileRange( ent ) ) {
		SetMobjState( ent, mo->info->missilestate );
		mo->flags |= MF_JUSTATTACKED;
		return;
	}
	// possibly choose another target (in multiplayer)
	if ( level.numConnectedClients > 1 && !mo->threshold && !CheckSight( ent, mo->target ) ) {
		if ( LookForPlayers( ent, qtrue ) ) {
			return;
		}
	}
	// chase towards player
	if ( --mo->movecount < 0 || !Move( ent ) ) {
		if ( ent->inuse && ent->doomMobj == mo && mo->target ) {
			NewChaseDir( ent );
		}
	}
	if ( !ent->inuse || ent->doomMobj != mo ) {
		return;
	}
	// make active sound
	if ( mo->info->activesound && Doom_Random() < 3 ) {
		MobjSound( ent, mo->info->activesound );
	}
}

// hitscan attacks: A_PosAttack, A_SPosAttack, A_CPosAttack
static void BulletAttack( gentity_t *ent, int sound, int count ) {
	doomMobj_t	*mo = ent->doomMobj;
	vec3_t		eye;
	float		pitch;
	int			i;

	if ( !mo->target ) {
		return;
	}
	MobjSound( ent, sound );
	FaceTarget( ent );
	ShootPoint( ent, eye );
	pitch = AimPitch( ent, eye, mo->angle );
	for ( i = 0 ; i < count ; i++ ) {
		float	yaw = mo->angle + ( Doom_Random() - Doom_Random() ) * ( 360.0f / 4096 );
		int		damage = ( ( Doom_Random() % 5 ) + 1 ) * 3;

		Doom_LineAttack( ent, eye, yaw, pitch, DOOM_MISSILERANGE, damage, MOD_MACHINEGUN );
	}
}

// melee: a hit on the target in reach
static qboolean MeleeAttack( gentity_t *ent, int damage, int sound ) {
	doomMobj_t	*mo = ent->doomMobj;

	if ( !CheckMeleeRange( ent ) ) {
		return qfalse;
	}
	MobjSound( ent, sound );
	G_Damage( mo->target, ent, ent, NULL, NULL, damage, 0, MOD_UNKNOWN );
	return qtrue;
}

/*
=================
Action

The action functions of the thing states (Doom II ones do nothing)
=================
*/
static void Action( gentity_t *ent, int action ) {
	doomMobj_t	*mo = ent->doomMobj;

	switch ( action ) {
	case ACT_Look:
		Look( ent );
		break;
	case ACT_Chase:
		Chase( ent );
		break;
	case ACT_FaceTarget:
		FaceTarget( ent );
		break;

	case ACT_PosAttack:
		BulletAttack( ent, sfx_pistol, 1 );
		break;
	case ACT_SPosAttack:
		BulletAttack( ent, sfx_shotgn, 3 );
		break;
	case ACT_CPosAttack:
		BulletAttack( ent, sfx_shotgn, 1 );
		break;
	case ACT_CPosRefire:
	case ACT_SpidRefire:
		// keep firing while the target is in sight
		FaceTarget( ent );
		if ( Doom_Random() < ( action == ACT_CPosRefire ? 40 : 10 ) ) {
			break;
		}
		if ( !Shootable( mo->target ) || !CheckSight( ent, mo->target ) ) {
			SetMobjState( ent, mo->info->seestate );
		}
		break;

	case ACT_TroopAttack:
		if ( !mo->target ) {
			break;
		}
		FaceTarget( ent );
		if ( MeleeAttack( ent, ( Doom_Random() % 8 + 1 ) * 3, sfx_claw ) ) {
			break;
		}
		SpawnMissile( ent, mo->target, MT_TROOPSHOT );
		break;
	case ACT_SargAttack:
		if ( !mo->target ) {
			break;
		}
		FaceTarget( ent );
		MeleeAttack( ent, ( Doom_Random() % 10 + 1 ) * 4, sfx_None );
		break;
	case ACT_HeadAttack:
		if ( !mo->target ) {
			break;
		}
		FaceTarget( ent );
		if ( MeleeAttack( ent, ( Doom_Random() % 6 + 1 ) * 10, sfx_None ) ) {
			break;
		}
		SpawnMissile( ent, mo->target, MT_HEADSHOT );
		break;
	case ACT_BruisAttack:
		if ( !mo->target ) {
			break;
		}
		if ( MeleeAttack( ent, ( Doom_Random() % 8 + 1 ) * 10, sfx_claw ) ) {
			break;
		}
		SpawnMissile( ent, mo->target, MT_BRUISERSHOT );
		break;
	case ACT_CyberAttack:
		if ( !mo->target ) {
			break;
		}
		FaceTarget( ent );
		SpawnMissile( ent, mo->target, MT_ROCKET );
		break;
	case ACT_SkullAttack: {
		// the lost soul flies at its target
		float dist;

		if ( !mo->target ) {
			break;
		}
		mo->flags |= MF_SKULLFLY;
		MobjSound( ent, mo->info->attacksound );
		FaceTarget( ent );
		mo->mom[0] = cos( DEG2RAD( mo->angle ) ) * SKULLSPEED;
		mo->mom[1] = sin( DEG2RAD( mo->angle ) ) * SKULLSPEED;
		dist = DistanceTo( ent, mo->target ) / SKULLSPEED;
		if ( dist < 1 ) {
			dist = 1;
		}
		mo->mom[2] = ( Doom_ThingZ( mo->target ) + Doom_ThingHeight( mo->target ) * 0.5f - ent->r.currentOrigin[2] ) / dist;
		break;
	}

	case ACT_Metal:
		MobjSound( ent, sfx_metal );
		Chase( ent );
		break;
	case ACT_BabyMetal:
		MobjSound( ent, sfx_bspwlk );
		Chase( ent );
		break;
	case ACT_Hoof:
		MobjSound( ent, sfx_hoof );
		Chase( ent );
		break;

	case ACT_Pain:
		MobjSound( ent, mo->info->painsound );
		break;
	case ACT_Scream:
		MobjSound( ent, mo->info->deathsound );
		break;
	case ACT_XScream:
		MobjSound( ent, sfx_slop );
		break;
	case ACT_PlayerScream:
		MobjSound( ent, sfx_pldeth );
		break;
	case ACT_Fall:
		// it is a corpse now: walk over it
		mo->flags &= ~MF_SOLID;
		LinkMobj( ent );
		break;
	case ACT_Explode: {
		vec3_t center;

		VectorCopy( ent->r.currentOrigin, center );
		center[2] += 8;
		G_RadiusDamage( center, mo->target ? mo->target : ent, 128, 128, ent, MOD_ROCKET_SPLASH );
		break;
	}
	case ACT_BossDeath:
		BossDeath( ent );
		break;
	default:
		break;
	}
}

/*
=================
Doom_TeleportMobj

EV_Teleport for a monster: to the destination in a sector with the tag,
unless something stands there (monsters don't telefrag)
=================
*/
qboolean Doom_TeleportMobj( gentity_t *ent, int tag ) {
	const doom_map_t	*map = doomLevel.map;
	doomMobj_t			*mo = ent->doomMobj;
	int					i;

	if ( !mo || !map ) {
		return qfalse;
	}
	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t	*t = &map->things[i];
		int					sector;
		float				oldx = ent->r.currentOrigin[0], oldy = ent->r.currentOrigin[1], oldz = ent->r.currentOrigin[2];

		if ( t->type != 14 ) {
			continue;
		}
		sector = doom_map_sector_at( map, t->x + 0.25, t->y + 0.25 );
		if ( sector < 0 || map->sectors[sector].tag != tag ) {
			continue;
		}
		ent->r.currentOrigin[2] = doomLevel.floor[sector];
		if ( !CheckPosition( ent, t->x, t->y, CP_THINGS ) ) {
			ent->r.currentOrigin[2] = oldz;
			return qfalse;
		}
		CheckPosition( ent, t->x, t->y, CP_LINES );
		mo->floorz = tm.floorz;
		mo->ceilingz = tm.ceilingz;
		SpawnTeleFog( oldx, oldy, oldz );
		mo->angle = t->angle;
		VectorClear( mo->mom );
		SetPosition( ent, t->x, t->y, doomLevel.floor[sector] );
		SpawnTeleFog( t->x, t->y, doomLevel.floor[sector] );
		return qtrue;
	}
	return qfalse;
}

/*
=================
Doom_KillAllCommand

doom_killall: every living monster dies, killed by the player (cheat, like
Boom's TNTEM), so the boss deaths and the 100% kills rule can be tested
=================
*/
void Doom_KillAllCommand( gentity_t *ent ) {
	int		i, count = 0;

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t *target = &g_entities[i];

		if ( target->inuse && target->doomMobj && ( target->doomMobj->flags & MF_COUNTKILL ) && target->health > 0 ) {
			Doom_DamageMobj( target, ent, ent, NULL, 10000, 0 );
			count++;
		}
	}
	trap_SendServerCommand( ent - g_entities, va( "print \"%d monsters killed\n\"", count ) );
}

/*
=================
Doom_MobjFits

Room for a thing of that type standing on the floor at x, y (searched from
z on a Quake 3 map), no wall and no thing in the way
=================
*/
qboolean Doom_MobjFits( int type, float x, float y, float z ) {
	gentity_t	probe;
	doomMobj_t	pm;

	memset( &probe, 0, sizeof( probe ) );
	memset( &pm, 0, sizeof( pm ) );
	probe.doomMobj = &pm;
	pm.radius = (float)doom_mobjinfo[type].radius / DOOM_FRACUNIT;
	pm.height = (float)doom_mobjinfo[type].height / DOOM_FRACUNIT;
	probe.s.number = ENTITYNUM_NONE;
	probe.r.currentOrigin[2] = DOOM_LEVEL() ? 0 : z;
	if ( ( DOOM_LEVEL() && doom_map_sector_at( doomLevel.map, x, y ) < 0 ) || !CheckPosition( &probe, x, y, CP_LINES )
		|| tm.ceilingz - tm.floorz < pm.height ) {
		return qfalse;
	}
	probe.r.currentOrigin[2] = tm.floorz;
	return CheckPosition( &probe, x, y, CP_THINGS );
}

/*
=================
Doom_SpawnCommand

doom_spawn <thing number or mobj name>: a thing in front of the player
(cheat), to try out every monster
=================
*/
void Doom_SpawnCommand( gentity_t *ent ) {
	char		arg[MAX_TOKEN_CHARS];
	int			type = -1, i;
	vec3_t		forward;
	gentity_t	*spawned;

	if ( !ent->client ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	if ( arg[0] >= '0' && arg[0] <= '9' ) {
		type = doom_mobjtype_for_ednum( atoi( arg ) );
	} else {
		static const struct { const char *name; int type; } names[] = {
			{ "zombieman", MT_POSSESSED }, { "shotgunguy", MT_SHOTGUY }, { "imp", MT_TROOP }, { "demon", MT_SERGEANT },
			{ "spectre", MT_SHADOWS }, { "lostsoul", MT_SKULL }, { "cacodemon", MT_HEAD }, { "baron", MT_BRUISER },
			{ "cyberdemon", MT_CYBORG }, { "spider", MT_SPIDER }, { "barrel", MT_BARREL }
		};
		for ( i = 0 ; i < (int)( sizeof( names ) / sizeof( names[0] ) ) ; i++ ) {
			if ( !Q_stricmp( arg, names[i].name ) ) {
				type = names[i].type;
			}
		}
	}
	if ( type < 0 || type == MT_PLAYER ) {
		trap_SendServerCommand( ent - g_entities, "print \"doom_spawn <zombieman|shotgunguy|imp|demon|spectre|lostsoul|cacodemon|baron|cyberdemon|spider|barrel|thing number>\n\"" );
		return;
	}
	// in front of the player, as far as 128 units, where it fits
	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );
	forward[2] = 0;
	VectorNormalize( forward );
	for ( i = 128 ; i >= 32 ; i -= 16 ) {
		if ( Doom_MobjFits( type, ent->client->ps.origin[0] + forward[0] * i, ent->client->ps.origin[1] + forward[1] * i,
			ent->client->ps.origin[2] ) ) {
			break;
		}
	}
	if ( i < 32 ) {
		trap_SendServerCommand( ent - g_entities, "print \"doom_spawn: no room in front\n\"" );
		return;
	}
	spawned = Doom_SpawnMobjAt( type, ent->client->ps.origin[0] + forward[0] * i, ent->client->ps.origin[1] + forward[1] * i,
		ent->client->ps.origin[2], AngleNormalize360( ent->client->ps.viewangles[YAW] + 180 ) );
	if ( doom_mobjinfo[type].flags & MF_COUNTKILL ) {
		doomLevel.totalKills++;
	}
	G_Printf( "doom_spawn: %d (thing %d) entity %d at %s\n", type, doom_mobjinfo[type].doomednum, spawned->s.number, vtos( spawned->r.currentOrigin ) );
}
