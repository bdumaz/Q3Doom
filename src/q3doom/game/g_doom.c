/*
===========================================================================
Q3Doom - Doom level logic: line specials, switches, exits, teleporters,
damaging floors, secrets, light changes and blocking decorations
(p_spec.c, p_switch.c, p_telept.c, p_lights.c)

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"
#include "bg_doom.h"

doomLevel_t		doomLevel;

extern vmCvar_t	g_doomSkill;
extern vmCvar_t	g_doomAllKeys;

/*
=====================================================================

2D GEOMETRY

=====================================================================
*/

// 0 front (right of v1->v2), 1 back, like P_PointOnLineSide
static int PointOnLineSide( const doom_linedef_t *ld, float x, float y ) {
	const doom_vertex_t	*a = &doomLevel.map->vertexes[ld->v1];
	const doom_vertex_t	*b = &doomLevel.map->vertexes[ld->v2];
	float				cross = ( b->x - a->x ) * ( y - a->y ) - ( b->y - a->y ) * ( x - a->x );

	return cross < 0 ? 0 : 1;
}

// where segment p0->p1 crosses the linedef, as a fraction of p0->p1; -1 if it doesn't
static float SegmentCrossesLine( const doom_linedef_t *ld, float x0, float y0, float x1, float y1 ) {
	const doom_vertex_t	*a = &doomLevel.map->vertexes[ld->v1];
	const doom_vertex_t	*b = &doomLevel.map->vertexes[ld->v2];
	float				dx = x1 - x0, dy = y1 - y0;
	float				lx = b->x - a->x, ly = b->y - a->y;
	float				denom = dx * ly - dy * lx;
	float				t, u;

	if ( fabs( denom ) < 1e-6f ) {
		return -1;
	}
	t = ( ( a->x - x0 ) * ly - ( a->y - y0 ) * lx ) / denom;
	u = ( ( a->x - x0 ) * dy - ( a->y - y0 ) * dx ) / denom;
	if ( t < 0 || t > 1 || u < 0 || u > 1 ) {
		return -1;
	}
	return t;
}

static int SideSector( const doom_linedef_t *ld, int side ) {
	if ( ld->side[side] == DOOM_NO_SIDE ) {
		return -1;
	}
	return doomLevel.map->sidedefs[ld->side[side]].sector;
}

static void LineMidpoint( int line, vec3_t out ) {
	const doom_linedef_t *ld = &doomLevel.map->linedefs[line];
	const doom_vertex_t *a = &doomLevel.map->vertexes[ld->v1], *b = &doomLevel.map->vertexes[ld->v2];
	int front = SideSector( ld, 0 );

	VectorSet( out, ( a->x + b->x ) * 0.5f, ( a->y + b->y ) * 0.5f, doomLevel.floor[front] + 32 );
}

static void SoundAt( const vec3_t origin, const char *sound ) {
	gentity_t *te = G_TempEntity( (float *)origin, EV_GENERAL_SOUND );

	te->s.eventParm = G_SoundIndex( va( "doom/sound/%s", sound ) );
}

/*
=================
Doom_PlaySectorSound

From the sector's mover, or the middle of the sector
=================
*/
void Doom_PlaySectorSound( int sector, const char *sound ) {
	gentity_t	*mover = doomLevel.ceilingMover[sector] ? doomLevel.ceilingMover[sector] : doomLevel.floorMover[sector];
	vec3_t		mins, maxs, mid;
	int			i;

	if ( mover ) {
		G_AddEvent( mover, EV_GENERAL_SOUND, G_SoundIndex( va( "doom/sound/%s", sound ) ) );
		return;
	}
	ClearBounds( mins, maxs );
	for ( i = 0 ; i < doomLevel.map->num_linedefs ; i++ ) {
		const doom_linedef_t *ld = &doomLevel.map->linedefs[i];

		if ( SideSector( ld, 0 ) == sector || SideSector( ld, 1 ) == sector ) {
			vec3_t p;
			VectorSet( p, doomLevel.map->vertexes[ld->v1].x, doomLevel.map->vertexes[ld->v1].y, doomLevel.floor[sector] + 32 );
			AddPointToBounds( p, mins, maxs );
		}
	}
	VectorAdd( mins, maxs, mid );
	VectorScale( mid, 0.5f, mid );
	SoundAt( mid, sound );
}

/*
=====================================================================

TEXTURE CHANGES (shader remapping, sent to the clients)

=====================================================================
*/

/*
=================
Doom_SetFlat

Changes the floor flat of a moving floor (its surfaces use the
per-sector shader doom/fl<sector>/<original flat>)
=================
*/
void Doom_SetFlat( int sector, const char *flat ) {
	const char *original = doomLevel.map->sectors[sector].floor_flat;

	if ( !doomLevel.floorMover[sector] || !strcmp( doomLevel.floorFlat[sector], flat ) ) {
		return;
	}
	Q_strncpyz( doomLevel.floorFlat[sector], flat, sizeof( doomLevel.floorFlat[sector] ) );
	AddRemap( va( "doom/fl%d/%s", sector, original ), va( "doom/fl%d/%s", sector, flat ), level.time * 0.001f );
	trap_SetConfigstring( CS_SHADERSTATE, BuildShaderStateConfig() );
}

/*
=================
Doom_ChangeSwitchTexture

SW1 <-> SW2 on the front side of a line (P_ChangeSwitchTexture). Switch
walls have a shader per line (doom/sw<line>/<texture>). Repeatable switches
turn back after a second.
=================
*/
void Doom_ChangeSwitchTexture( int line, qboolean useAgain ) {
	const doom_linedef_t	*ld = &doomLevel.map->linedefs[line];
	const doom_sidedef_t	*sd = &doomLevel.map->sidedefs[ld->side[0]];
	const char				*slots[3];
	char					other[DOOM_LUMP_NAME_LEN + 1];
	qboolean				changed = qfalse, on;
	vec3_t					origin;
	int						i;

	slots[0] = sd->top;
	slots[1] = sd->mid;
	slots[2] = sd->bottom;

	// the texture is "on" while switchUntil is set (repeatable) or forever (once)
	on = doomLevel.switchUntil[line] == 0;
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( !doom_switch_texture( doomLevel.wad, slots[i], other ) ) {
			continue;
		}
		AddRemap( va( "doom/sw%d/%s", line, slots[i] ), va( "doom/sw%d/%s", line, on ? other : slots[i] ), level.time * 0.001f );
		changed = qtrue;
	}
	if ( changed ) {
		trap_SetConfigstring( CS_SHADERSTATE, BuildShaderStateConfig() );
	}

	LineMidpoint( line, origin );
	SoundAt( origin, doomLevel.lineSpecial[line] == 11 ? "DSSWTCHX" : "DSSWTCHN" );

	if ( on ) {
		doomLevel.switchUntil[line] = useAgain ? level.time + DOOM_TICS_MSEC( BUTTONTIME ) : -1;
	} else {
		doomLevel.switchUntil[line] = 0;
	}
	if ( !useAgain ) {
		doomLevel.lineSpecial[line] = 0;
	}
}

/*
=====================================================================

LIGHTS (p_lights.c): the base levels the cgame animates

=====================================================================
*/

static void SendLights( void ) {
	char	buffer[MAX_STRING_CHARS * 4];
	int		i;

	buffer[0] = 0;
	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		if ( doomLevel.lightChanged[i] ) {
			Q_strcat( buffer, sizeof( buffer ), va( "%d %d %d ", i, doomLevel.light[i], doomLevel.special[i] ) );
		}
	}
	trap_SetConfigstring( CS_DOOM_LIGHTS, buffer );
}

static void SetLight( int sector, int level ) {
	doomLevel.light[sector] = level;
	doomLevel.lightChanged[sector] = qtrue;
}

static qboolean DoLight( int tag, int type, int amount ) {
	int			i;
	qboolean	any = qfalse;

	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		if ( doomLevel.map->sectors[i].tag != tag ) {
			continue;
		}
		switch ( type ) {
		case DOOM_LIGHT_TURNON: {
			int bright = amount;
			// 0: the brightest neighbor (EV_LightTurnOn)
			if ( !bright ) {
				int j;

				for ( j = 0 ; j < doomLevel.map->num_linedefs ; j++ ) {
					const doom_linedef_t *ld = &doomLevel.map->linedefs[j];
					int other = SideSector( ld, 0 ) == i ? SideSector( ld, 1 ) : SideSector( ld, 1 ) == i ? SideSector( ld, 0 ) : -1;

					if ( other >= 0 && doomLevel.light[other] > bright ) {
						bright = doomLevel.light[other];
					}
				}
			}
			SetLight( i, bright );
			break;
		}
		case DOOM_LIGHT_TURNOFF:
			SetLight( i, doom_find_min_surrounding_light( doomLevel.map, i, doomLevel.light[i], doomLevel.light ) );
			break;
		case DOOM_LIGHT_STROBE:
			// a slow strobe, like sector special 3 (EV_StartLightStrobing)
			if ( !doomLevel.special[i] ) {
				doomLevel.special[i] = 3;
				doomLevel.lightChanged[i] = qtrue;
			}
			break;
		}
		any = qtrue;
	}
	if ( any ) {
		SendLights();
	}
	return any;
}

/*
=====================================================================

EXITS AND TELEPORTERS

=====================================================================
*/

/*
=================
Teleport

EV_Teleport: to the teleport destination in a sector with the line's tag.
Doom stops the player; the Quake 3 telefrag (G_KillBox) applies.
=================
*/
static qboolean Teleport( gentity_t *ent, int tag ) {
	const doom_map_t	*map = doomLevel.map;
	int					i, sector;
	vec3_t				origin, angles;

	if ( !ent->client ) {
		return Doom_TeleportMobj( ent, tag );
	}
	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t *t = &map->things[i];

		if ( t->type != 14 ) {
			continue;
		}
		sector = doom_map_sector_at( map, t->x + 0.25, t->y + 0.25 );
		if ( sector < 0 || map->sectors[sector].tag != tag ) {
			continue;
		}

		SoundAt( ent->r.currentOrigin, "DSTELEPT" );
		VectorSet( origin, t->x, t->y, doomLevel.floor[sector] + 25 );
		VectorSet( angles, 0, t->angle, 0 );

		trap_UnlinkEntity( ent );
		VectorCopy( origin, ent->client->ps.origin );
		VectorClear( ent->client->ps.velocity );
		ent->client->ps.eFlags ^= EF_TELEPORT_BIT;
		SetClientViewAngle( ent, angles );
		G_KillBox( ent );
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qtrue );
		VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
		trap_LinkEntity( ent );

		SoundAt( origin, "DSTELEPT" );
		return qtrue;
	}
	return qfalse;
}

/*
=====================================================================

LINE SPECIALS

=====================================================================
*/

/*
=================
Doom_PlayerHasKey

A card or the skull of the color (DOOM_KEY_*) opens the door;
g_doomAllKeys (cheat) opens every door
=================
*/
qboolean Doom_PlayerHasKey( gentity_t *ent, int key ) {
	int		keys;

	if ( g_doomAllKeys.integer ) {
		return qtrue;
	}
	if ( !ent || !ent->client || key < DOOM_KEY_BLUE || key > DOOM_KEY_RED ) {
		return qfalse;
	}
	keys = ent->client->ps.stats[STAT_DOOM_KEYS];
	// DOOM_KEY_BLUE..RED follow the card order, the skulls come 3 later
	return ( keys & ( ( 1 << ( key - 1 ) ) | ( 1 << ( key - 1 + 3 ) ) ) ) != 0;
}

/*
=================
Doom_Message

A message on the player's Doom message line (top left)
=================
*/
void Doom_Message( gentity_t *ent, const char *message ) {
	if ( !ent || !ent->client ) {
		return;
	}
	trap_SendServerCommand( ent - g_entities, va( "dmsg \"%s\"", message ) );
}

void Doom_KeyMessage( gentity_t *ent, int key ) {
	static const char *colors[] = { "", "blue", "yellow", "red" };

	if ( !ent || !ent->client ) {
		return;
	}
	Doom_Message( ent, va( "You need a %s key to open this door", colors[key] ) );
	G_AddEvent( ent, EV_DOOM_SOUND, sfx_oof );
}

/*
=================
Doom_ActivateLine

A player (or, later, a monster) walks over, uses or shoots a line.
'side' is the side the activator is on. Returns qtrue if it did
something (switches change texture then).
=================
*/
qboolean Doom_ActivateLine( gentity_t *activator, int line, int side, doom_trigger_t trigger ) {
	const doom_linedef_t	*ld = &doomLevel.map->linedefs[line];
	const doom_special_t	*sp = doom_line_special( doomLevel.lineSpecial[line] );
	qboolean				success = qfalse, isPlayer = activator && activator->client;
	int						i;

	if ( !sp ) {
		return qfalse;
	}
	// using a line also works the manual doors
	if ( trigger == DOOM_TRIG_SWITCH && sp->trigger == DOOM_TRIG_MANUAL ) {
		trigger = DOOM_TRIG_MANUAL;
	}
	if ( sp->trigger != trigger || ( isPlayer && sp->monsters == 2 ) || ( !isPlayer && !sp->monsters ) ) {
		return qfalse;
	}
	// lines are used from their front side only (P_UseSpecialLine)
	if ( ( trigger == DOOM_TRIG_SWITCH || trigger == DOOM_TRIG_MANUAL ) && side == 1 ) {
		return qfalse;
	}

	if ( sp->kind == DOOM_KIND_DOOR && sp->trigger == DOOM_TRIG_MANUAL ) {
		Doom_ManualDoor( activator, line, qfalse );
		return qtrue;
	}

	// locked switch doors
	if ( sp->kind == DOOM_KIND_DOOR && sp->amount && !Doom_PlayerHasKey( activator, sp->amount ) ) {
		Doom_KeyMessage( activator, sp->amount );
		return qfalse;
	}

	switch ( sp->kind ) {
	case DOOM_KIND_EXIT:
		// a DoomFrag run with monsters left refuses it
		success = Doom_ExitLevel( activator, sp->type == 1 );
		break;
	case DOOM_KIND_TELEPORT:
		// only from the front, so you can step out of a teleporter
		success = side == 0 && Teleport( activator, ld->tag );
		break;
	case DOOM_KIND_LIGHT:
		success = DoLight( ld->tag, sp->type, sp->amount );
		break;
	case DOOM_KIND_STOP:
		success = Doom_StopMovers( ld->tag, sp->type );
		break;
	default:
		if ( sp->kind == DOOM_KIND_PLAT && sp->type == DOOM_PLAT_PERPETUALRAISE ) {
			success |= Doom_ActivateInStasis( ld->tag, DOOM_KIND_PLAT );
		}
		if ( sp->kind == DOOM_KIND_CEILING ) {
			success |= Doom_ActivateInStasis( ld->tag, DOOM_KIND_CEILING );
		}
		for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
			if ( !ld->tag || doomLevel.map->sectors[i].tag != ld->tag ) {
				continue;
			}
			switch ( sp->kind ) {
			case DOOM_KIND_DOOR:
				success |= Doom_DoDoor( i, sp->type, sp->amount );
				break;
			case DOOM_KIND_PLAT:
				success |= Doom_DoPlat( i, sp->type, sp->amount, line );
				break;
			case DOOM_KIND_FLOOR:
				success |= Doom_DoFloor( i, sp->type, line );
				break;
			case DOOM_KIND_CEILING:
				success |= Doom_DoCeiling( i, sp->type, line );
				if ( sp->special == 40 ) {		// raise ceiling, lower floor
					success |= Doom_DoFloor( i, DOOM_FLOOR_LOWERTOLOWEST, line );
				}
				break;
			case DOOM_KIND_STAIRS:
				success |= Doom_BuildStairs( i, sp->type );
				break;
			case DOOM_KIND_DONUT:
				success |= Doom_DoDonut( i );
				break;
			default:
				break;
			}
		}
		break;
	}

	// walk lines work once whatever happened; switches and gun lines
	// change texture and, for the once-only ones, stop working when they acted
	if ( trigger == DOOM_TRIG_WALK ) {
		if ( !sp->repeatable && ( success || sp->kind != DOOM_KIND_EXIT ) ) {
			doomLevel.lineSpecial[line] = 0;
		}
	} else if ( success && ( trigger == DOOM_TRIG_SWITCH || trigger == DOOM_TRIG_GUN ) ) {
		Doom_ChangeSwitchTexture( line, sp->repeatable );
	}
	return success;
}

/*
=================
Doom_PlayerMoved

Walk-over lines: crossing from one side to the other between the old and
the new position (P_CrossSpecialLine), at any height.
=================
*/
void Doom_PlayerMoved( gentity_t *ent, const vec3_t oldOrigin ) {
	const doom_map_t	*map;
	const float			*o = ent->client->ps.origin;
	int					i;

	if ( !DOOM_LEVEL() || ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->health <= 0 ) {
		return;
	}
	if ( Distance( oldOrigin, o ) > 512 || ( oldOrigin[0] == o[0] && oldOrigin[1] == o[1] ) ) {
		return;		// teleported or not moving
	}
	map = doomLevel.map;
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_special_t	*sp;
		int						oldSide;

		if ( !doomLevel.lineSpecial[i] ) {
			continue;
		}
		sp = doom_line_special( doomLevel.lineSpecial[i] );
		if ( !sp || sp->trigger != DOOM_TRIG_WALK ) {
			continue;
		}
		oldSide = PointOnLineSide( ld, oldOrigin[0], oldOrigin[1] );
		if ( oldSide == PointOnLineSide( ld, o[0], o[1] ) ) {
			continue;
		}
		if ( SegmentCrossesLine( ld, oldOrigin[0], oldOrigin[1], o[0], o[1] ) < 0 ) {
			continue;
		}
		Doom_ActivateLine( ent, i, oldSide, DOOM_TRIG_WALK );
	}
}

/*
=================
Doom_PlayerUse

P_UseLines: along the view direction, up to USERANGE. The first line with
a special is used; a wall or a closed opening stops the search with an "oof".
=================
*/
void Doom_PlayerUse( gentity_t *ent ) {
	const doom_map_t	*map = doomLevel.map;
	const float			*o = ent->client->ps.origin;
	float				yaw = DEG2RAD( ent->client->ps.viewangles[YAW] );
	float				x1 = o[0] + cos( yaw ) * USERANGE, y1 = o[1] + sin( yaw ) * USERANGE;
	int					i, best = -1, blocker = -1;
	float				bestT = 2, blockT = 2;

	if ( !DOOM_LEVEL() || ent->health <= 0 ) {
		return;
	}
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		float					t = SegmentCrossesLine( ld, o[0], o[1], x1, y1 );
		int						front, back;

		if ( t < 0 ) {
			continue;
		}
		if ( doomLevel.lineSpecial[i] ) {
			if ( t < bestT ) {
				bestT = t;
				best = i;
			}
			continue;
		}
		front = SideSector( ld, 0 );
		back = SideSector( ld, 1 );
		if ( back < 0 || ( doomLevel.ceiling[front] < doomLevel.ceiling[back] ? doomLevel.ceiling[front] : doomLevel.ceiling[back] )
			- ( doomLevel.floor[front] > doomLevel.floor[back] ? doomLevel.floor[front] : doomLevel.floor[back] ) <= 0 ) {
			if ( t < blockT ) {
				blockT = t;
				blocker = i;
			}
		}
	}

	if ( best >= 0 && bestT < blockT ) {
		int side = PointOnLineSide( &map->linedefs[best], o[0], o[1] );

		if ( Doom_ActivateLine( ent, best, side, DOOM_TRIG_SWITCH ) ) {
			return;
		}
		SoundAt( o, "DSNOWAY" );
		return;
	}
	if ( blocker >= 0 ) {
		SoundAt( o, "DSNOWAY" );
	}
}

/*
=====================================================================

QUAKE-STYLE SWITCHES: bumped into or shot

=====================================================================
*/

// a switch (S1/SR, exits included) that is not pressed right now
static qboolean IsSwitch( int line ) {
	const doom_special_t *sp = doom_line_special( doomLevel.lineSpecial[line] );

	return sp && sp->trigger == DOOM_TRIG_SWITCH && doomLevel.switchUntil[line] == 0;
}

static float DistanceToLine( const doom_linedef_t *ld, float x, float y ) {
	const doom_vertex_t	*a = &doomLevel.map->vertexes[ld->v1], *b = &doomLevel.map->vertexes[ld->v2];
	float				dx = b->x - a->x, dy = b->y - a->y, len2 = dx * dx + dy * dy, t;

	t = len2 > 0 ? ( ( x - a->x ) * dx + ( y - a->y ) * dy ) / len2 : 0;
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	dx = a->x + t * dx - x;
	dy = a->y + t * dy - y;
	return sqrt( dx * dx + dy * dy );
}

/*
=================
Doom_PlayerBump

A player pushing against the front of a switch presses it, as if using it
=================
*/
void Doom_PlayerBump( gentity_t *ent ) {
	const doom_map_t	*map;
	const usercmd_t		*cmd = &ent->client->pers.cmd;
	const float			*o = ent->client->ps.origin;
	float				yaw, wish[2];
	int					i;

	if ( !DOOM_LEVEL() || ent->health <= 0 || doomLevel.intermission || ( !cmd->forwardmove && !cmd->rightmove ) ) {
		return;
	}
	// the direction the player wants to go
	yaw = DEG2RAD( ent->client->ps.viewangles[YAW] );
	wish[0] = cos( yaw ) * cmd->forwardmove + sin( yaw ) * cmd->rightmove;
	wish[1] = sin( yaw ) * cmd->forwardmove - cos( yaw ) * cmd->rightmove;

	map = doomLevel.map;
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_vertex_t		*a, *b;
		float					nx, ny, len;

		if ( !doomLevel.lineSpecial[i] || !IsSwitch( i ) || PointOnLineSide( ld, o[0], o[1] ) != 0 ) {
			continue;
		}
		if ( DistanceToLine( ld, o[0], o[1] ) > 15 + 6 ) {
			continue;
		}
		// moving into the front side
		a = &map->vertexes[ld->v1];
		b = &map->vertexes[ld->v2];
		nx = b->y - a->y;
		ny = -( b->x - a->x );
		len = sqrt( nx * nx + ny * ny );
		if ( len <= 0 || ( wish[0] * nx + wish[1] * ny ) / len >= 0 ) {
			continue;
		}
		Doom_ActivateLine( ent, i, 0, DOOM_TRIG_SWITCH );
		return;
	}
}

/*
=================
Doom_ShootSwitch

A shot or a missile from 'start' hit a wall at 'end': the switch there, if
shot from its front, is pressed
=================
*/
void Doom_ShootSwitch( gentity_t *attacker, const vec3_t start, const vec3_t end ) {
	const doom_map_t	*map;
	float				dx = end[0] - start[0], dy = end[1] - start[1], len = sqrt( dx * dx + dy * dy ), bestT = 2;
	float				x1, y1;
	int					i, best = -1;

	if ( !DOOM_LEVEL() || !attacker || !attacker->client || len < 0.001f ) {
		return;
	}
	// a little past the hit, into the wall
	x1 = end[0] + dx / len * 4;
	y1 = end[1] + dy / len * 4;
	map = doomLevel.map;
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		float t;

		if ( !doomLevel.lineSpecial[i] || !IsSwitch( i ) ) {
			continue;
		}
		t = SegmentCrossesLine( &map->linedefs[i], start[0], start[1], x1, y1 );
		if ( t >= 0 && t < bestT && PointOnLineSide( &map->linedefs[i], start[0], start[1] ) == 0 ) {
			bestT = t;
			best = i;
		}
	}
	if ( best >= 0 && bestT * ( len + 4 ) > len - 8 ) {
		Doom_ActivateLine( attacker, best, 0, DOOM_TRIG_SWITCH );
	}
}

/*
=================
Doom_ShotTrace

Gun lines crossed by a hitscan shot (P_ShootSpecialLine)
=================
*/
void Doom_ShotTrace( gentity_t *attacker, const vec3_t start, const vec3_t end ) {
	const doom_map_t	*map;
	int					i;

	if ( !DOOM_LEVEL() ) {
		return;
	}
	map = doomLevel.map;
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_special_t *sp;

		if ( !doomLevel.lineSpecial[i] ) {
			continue;
		}
		sp = doom_line_special( doomLevel.lineSpecial[i] );
		if ( !sp || sp->trigger != DOOM_TRIG_GUN ) {
			continue;
		}
		if ( SegmentCrossesLine( &map->linedefs[i], start[0], start[1], end[0], end[1] ) < 0 ) {
			continue;
		}
		Doom_ActivateLine( attacker, i, PointOnLineSide( &map->linedefs[i], start[0], start[1] ), DOOM_TRIG_GUN );
	}
}

/*
=====================================================================

PER TIC: damaging floors, secrets, switches turning back, exits

=====================================================================
*/

static void PlayerInSpecialSector( gentity_t *ent, int tic ) {
	const float	*o = ent->client->ps.origin;
	int			sector = doom_map_sector_at( doomLevel.map, o[0], o[1] );
	int			damage = 0;
	qboolean	ironfeet;

	if ( sector < 0 || ent->health <= 0 ) {
		return;
	}
	// only when standing on the floor (P_PlayerInSpecialSector)
	if ( ent->client->ps.groundEntityNum == ENTITYNUM_NONE || fabs( o[2] + MINS_Z - doomLevel.floor[sector] ) > 1 ) {
		return;
	}

	// the radiation suit protects, but not always from the worst floors
	ironfeet = ent->client->ps.powerups[DPW_IRONFEET] != 0;
	switch ( doomLevel.special[sector] ) {
	case 5:		// hellslime
		if ( ironfeet ) {
			return;
		}
		damage = 10;
		break;
	case 7:		// nukage
		if ( ironfeet ) {
			return;
		}
		damage = 5;
		break;
	case 16:	// super hellslime
	case 4:		// strobe hurt
		if ( ironfeet && Doom_Random() >= 5 ) {
			return;
		}
		damage = 20;
		break;
	case 11:	// exit when almost dead (E1M8)
		// the damage never kills (P_DamageMobj's "end of game hell hack")
		damage = MIN( 20, ent->health - 1 );
		if ( ent->health <= 10 ) {
			Doom_ExitLevel( ent, qfalse );
			return;
		}
		if ( damage < 1 ) {
			return;
		}
		break;
	case 9:		// secret
		doomLevel.special[sector] = 0;
		doomLevel.secretsFound++;
		Doom_Message( ent, "You found a secret area!" );
		return;
	default:
		return;
	}
	if ( !( tic & 31 ) ) {
		G_Damage( ent, NULL, NULL, NULL, NULL, damage, DAMAGE_NO_KNOCKBACK, MOD_SLIME );
	}
}

void Doom_RunFrame( void ) {
	int		tic, i;

	if ( !DOOM_LEVEL() ) {
		return;
	}
	Doom_UpdateHeights();
	Doom_UpdateItems();

	// the world stops at the end of the level (Doom's intermission)
	if ( doomLevel.intermission ) {
		Doom_CampaignFrame();
		return;
	}

	tic = ( level.time - level.startTime ) * DOOM_TICRATE / 1000;
	while ( doomLevel.lastTic < tic ) {
		doomLevel.lastTic++;
		// DoomFrag: the monsters wait for the run to start
		if ( !Doom_FragWaiting() ) {
			Doom_RunMobjs();
		}
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			gentity_t *ent = &g_entities[i];

			if ( ent->inuse && ent->client && ent->client->pers.connected == CON_CONNECTED
				&& ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
				PlayerInSpecialSector( ent, doomLevel.lastTic );
			}
		}
	}

	for ( i = 0 ; i < doomLevel.map->num_linedefs ; i++ ) {
		if ( doomLevel.switchUntil[i] > 0 && level.time >= doomLevel.switchUntil[i] ) {
			Doom_ChangeSwitchTexture( i, qtrue );	// turns back off
		}
	}

	Doom_CampaignFrame();

}

/*
=====================================================================

LEVEL SETUP

=====================================================================
*/

/*
=================
SpawnThingBlockers

Solid decorations (columns, lamps, barrels...) block movement; shootable
ones stop shots too. They are drawn by the cgame, which predicts the same
boxes (BG_DoomThingBlocks), so these entities are not sent.
=================
*/
static void SpawnThingBlockers( void ) {
	const doom_map_t	*map = doomLevel.map;
	int					i;

	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t	*t = &map->things[i];
		int					type = BG_DoomStaticThing( t, g_doomSkill.integer );
		vec3_t				mins, maxs;
		gentity_t			*ent;

		if ( type < 0 || !BG_DoomThingBlocks( map, t, type, mins, maxs ) ) {
			continue;
		}
		ent = G_Spawn();
		ent->classname = "doom_thing_blocker";
		ent->r.svFlags = SVF_NOCLIENT;
		ent->r.contents = ( doom_mobjinfo[type].flags & MF_SHOOTABLE ) ? CONTENTS_SOLID : ( CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP );
		VectorCopy( mins, ent->r.mins );
		VectorCopy( maxs, ent->r.maxs );
		trap_LinkEntity( ent );
	}
}

/*
=================
Doom_InitLevel

Before the entities spawn: they need the sector state
=================
*/
void Doom_InitLevel( void ) {
	char	mapname[MAX_QPATH];
	int		i, n;

	memset( &doomLevel, 0, sizeof( doomLevel ) );
	trap_Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
	doomLevel.map = trap_Doom_GetMap( mapname );
	if ( !doomLevel.map ) {
		return;
	}
	doomLevel.wad = trap_Doom_GetWad();
	n = doomLevel.numSectors = doomLevel.map->num_sectors;

	doomLevel.ranges = G_Alloc( sizeof( doom_plane_range_t ) * n );
	doom_sector_ranges( doomLevel.wad, doomLevel.map, doomLevel.ranges );
	doomLevel.floorMover = G_Alloc( sizeof( gentity_t * ) * n );
	doomLevel.ceilingMover = G_Alloc( sizeof( gentity_t * ) * n );
	doomLevel.floor = G_Alloc( sizeof( float ) * n );
	doomLevel.ceiling = G_Alloc( sizeof( float ) * n );
	doomLevel.floorInt = G_Alloc( sizeof( int ) * n );
	doomLevel.ceilingInt = G_Alloc( sizeof( int ) * n );
	doomLevel.special = G_Alloc( sizeof( int ) * n );
	doomLevel.floorFlat = G_Alloc( sizeof( *doomLevel.floorFlat ) * n );
	doomLevel.light = G_Alloc( sizeof( int ) * n );
	doomLevel.lightChanged = G_Alloc( sizeof( qboolean ) * n );
	for ( i = 0 ; i < n ; i++ ) {
		const doom_sector_t *sec = &doomLevel.map->sectors[i];

		doomLevel.floorMover[i] = doomLevel.ceilingMover[i] = NULL;
		doomLevel.floor[i] = sec->floor_height;
		doomLevel.ceiling[i] = sec->ceiling_height;
		doomLevel.floorInt[i] = sec->floor_height;
		doomLevel.ceilingInt[i] = sec->ceiling_height;
		doomLevel.special[i] = sec->special;
		Q_strncpyz( doomLevel.floorFlat[i], sec->floor_flat, sizeof( doomLevel.floorFlat[i] ) );
		doomLevel.light[i] = sec->light;
		doomLevel.lightChanged[i] = qfalse;
	}

	n = doomLevel.map->num_linedefs;
	doomLevel.lineSpecial = G_Alloc( sizeof( int ) * n );
	doomLevel.switchUntil = G_Alloc( sizeof( int ) * n );
	for ( i = 0 ; i < n ; i++ ) {
		doomLevel.lineSpecial[i] = doomLevel.map->linedefs[i].special;
		doomLevel.switchUntil[i] = 0;
	}
	trap_SetConfigstring( CS_DOOM_LIGHTS, "" );

	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		if ( doomLevel.special[i] == 9 ) {
			doomLevel.totalSecrets++;
		}
	}
	Doom_InitMobjs();
	Doom_InitCampaign();

	// the weapons of this level: DoomFrag's option, the campaign's option,
	// else Doom's; the clients read it from the serverinfo
	if ( g_doomFrag.integer ) {
		doomLevel.weaponSet = (int)Com_Clamp( DOOM_WEAPONS_DOOM, DOOM_WEAPONS_NONE, g_doomWeapons.integer );
	} else if ( doomLevel.campaign && g_doomCampaignWeapons.integer ) {
		doomLevel.weaponSet = DOOM_WEAPONS_Q3;
	} else {
		doomLevel.weaponSet = DOOM_WEAPONS_DOOM;
	}
	trap_Cvar_Set( "g_doomWeaponSet", va( "%d", doomLevel.weaponSet ) );
}

/*
=================
Doom_StartLevel

After the entities spawned (the movers exist)
=================
*/
void Doom_StartLevel( void ) {
	if ( !DOOM_LEVEL() ) {
		return;
	}
	Doom_SpawnSectorSpecials();
	Doom_SpawnDoorTriggers();
	SpawnThingBlockers();
	Doom_SpawnItems();
	Doom_SpawnMobjs();
	Doom_InitFrag();
	trap_SetConfigstring( CS_MUSIC, Doom_LevelMusic() );
}

/*
=================
Doom_LevelMusic

S_Start: the music of the level, played by the clients through libdoom's
synthesizer; episode 4 reuses tracks of the first three
=================
*/
const char *Doom_LevelMusic( void ) {
	static const char	*e4[9] = { "E3M4", "E3M2", "E3M3", "E1M5", "E2M7", "E2M4", "E2M6", "E2M5", "E1M9" };
	char				map[MAX_QPATH];

	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
	Q_strupr( map );
	if ( strlen( map ) == 4 && map[0] == 'E' && map[2] == 'M' && map[3] >= '1' && map[3] <= '9' ) {
		if ( map[1] == '4' ) {
			return va( "doom/music/D_%s", e4[map[3] - '1'] );
		}
		return va( "doom/music/D_%s", map );
	}
	return "";		// Doom II's MAPxx tracks have other names
}

void Doom_ShutdownLevel( void ) {
	memset( &doomLevel, 0, sizeof( doomLevel ) );
}

/*
=================
Doom_StatusCommand

doom_status: every moving plane, what acts on it and where it is
=================
*/
void Doom_StatusCommand( void ) {
	static const char	*kinds[] = { "idle", "door", "plat", "floor", "ceiling", "stairs", "light", "exit", "teleport", "donut", "stop", "scroll" };
	int					i, c;

	if ( !DOOM_LEVEL() ) {
		G_Printf( "doom_status: not a Doom map\n" );
		return;
	}
	Doom_UpdateHeights();
	for ( i = 0 ; i < doomLevel.numSectors ; i++ ) {
		for ( c = 0 ; c < 2 ; c++ ) {
			gentity_t	*ent = c ? doomLevel.ceilingMover[i] : doomLevel.floorMover[i];
			doomPlane_t	*p;

			if ( !ent ) {
				continue;
			}
			p = ent->doomPlane;
			G_Printf( "sector %3d %-7s %-7s type %d dir %2d height %7.1f (range %d..%d)%s\n", i, c ? "ceiling" : "floor",
				kinds[p->kind], p->type, p->direction, c ? doomLevel.ceiling[i] : doomLevel.floor[i],
				c ? doomLevel.ranges[i].ceiling_min : doomLevel.ranges[i].floor_min,
				c ? doomLevel.ranges[i].ceiling_max : doomLevel.ranges[i].floor_max,
				ent->doomLine >= 0 ? va( " door line %d", ent->doomLine ) : "" );
		}
	}
	G_Printf( "secrets found: %d of %d, kills %d of %d\n", doomLevel.secretsFound, doomLevel.totalSecrets, doomLevel.kills, doomLevel.totalKills );

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t	*ent = &g_entities[i];

		if ( !ent->inuse || !ent->doomMobj ) {
			continue;
		}
		vec3_t	mid;

		// "pvs 0": the server would not send it to player 0 (its middle is
		// in no visible leaf)
		VectorCopy( ent->r.currentOrigin, mid );
		mid[2] += ent->doomMobj->height * 0.5f;
		G_Printf( "thing %d: type %d at %.0f %.0f %.0f health %d state %d target %d flags 0x%x pvs %d\n", i, ent->doomMobj->type,
			ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2], ent->health, ent->doomMobj->state,
			ent->doomMobj->target ? ent->doomMobj->target->s.number : -1, ent->doomMobj->flags,
			g_entities[0].client ? trap_InPVS( mid, g_entities[0].client->ps.origin ) : -1 );
	}

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t	*cl = &level.clients[i];

		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		G_Printf( "player %d: health %d armor %d (type %d) weapon %d state %d keys %d ammo %d %d %d %d weapons 0x%x\n", i,
			cl->ps.stats[STAT_HEALTH], cl->ps.stats[STAT_ARMOR], cl->ps.stats[STAT_DOOM_ARMORTYPE], DOOM_DWP( cl->ps.weapon ),
			cl->ps.doomWeaponState, cl->ps.stats[STAT_DOOM_KEYS], cl->ps.ammo[DAM_CLIP], cl->ps.ammo[DAM_SHELL],
			cl->ps.ammo[DAM_CELL], cl->ps.ammo[DAM_MISL], cl->ps.stats[STAT_WEAPONS] );
	}
}
