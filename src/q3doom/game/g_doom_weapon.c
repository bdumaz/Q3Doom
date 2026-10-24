/*
===========================================================================
Q3Doom - Doom weapon attacks in the game (p_pspr.c, p_map.c, p_mobj.c)

The weapon sprites run in Pmove (bg_doom_weapon.c); when a state fires,
the player gets an EV_FIRE_WEAPON with the Doom action, handled here:
hitscan and melee with Quake 3 traces, Doom damage and spread, puffs and
blood; rockets, plasma and the BFG as Quake 3 missiles with Doom values;
the BFG spray. Shots aim along the view (free mouselook); g_autoaim 1
adds Doom's vertical autoaim.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

#define AIMRANGE		1024		// 16*64, P_BulletSlope and the BFG spray
#define AIMSLOPE		( 100.0f / 160 )
#define SPREAD_DEG		( 360.0f / 16384 )		// (P_Random()-P_Random())<<18, per unit
#define	MISSILE_PRESTEP_TIME	50					// as g_missile.c

void G_ExplodeMissile( gentity_t *ent );

int Doom_Random( void ) {
	return rand() & 255;
}

static float Spread( void ) {
	return ( Doom_Random() - Doom_Random() ) * SPREAD_DEG;
}

/*
=================
Doom_IsTarget

Things Doom shots aim at (MF_SHOOTABLE): living players, monsters, barrels
=================
*/
qboolean Doom_IsTarget( gentity_t *ent ) {
	if ( !ent->inuse || !ent->takedamage || ent->health <= 0 ) {
		return qfalse;
	}
	if ( ent->client ) {
		return ent->client->sess.sessionTeam != TEAM_SPECTATOR;
	}
	if ( ent->doomMobj ) {
		return ( ent->doomMobj->flags & MF_SHOOTABLE ) != 0;
	}
	return qfalse;
}

// shots draw blood from it rather than a puff
qboolean Doom_Bleeds( gentity_t *ent ) {
	return Doom_IsTarget( ent ) && !( ent->doomMobj && ( ent->doomMobj->flags & MF_NOBLOOD ) );
}

/*
=================
Doom_Sound

A Doom sound from an entity; | DOOM_SOUND_FULL plays it at full volume
everywhere (the bosses)
=================
*/
void Doom_Sound( gentity_t *ent, int sfx ) {
	if ( sfx > sfx_None ) {
		G_AddEvent( ent, EV_DOOM_SOUND, sfx );
	}
}

static void EyePoint( gentity_t *ent, vec3_t eye ) {
	VectorCopy( ent->client->ps.origin, eye );
	eye[2] += ent->client->ps.viewheight;
}

/*
=================
Doom_AimLineAttack

P_AimLineAttack: the nearest target along 'yaw' within 'range' and Doom's
vertical aim range (+-100/160), in sight. Returns it and the pitch to its
middle, or NULL.
=================
*/
gentity_t *Doom_AimLineAttack( gentity_t *shooter, const vec3_t eye, float yaw, float range, float *pitch ) {
	gentity_t	*best = NULL;
	float		bestDist = range, dir[2];
	int			i;

	dir[0] = cos( DEG2RAD( yaw ) );
	dir[1] = sin( DEG2RAD( yaw ) );

	for ( i = 0 ; i < level.num_entities ; i++ ) {
		gentity_t	*e = &g_entities[i];
		vec3_t		center;
		float		along, side, radius, top, bottom;
		trace_t		tr;

		if ( e == shooter || !Doom_IsTarget( e ) ) {
			continue;
		}
		VectorAdd( e->r.absmin, e->r.absmax, center );
		VectorScale( center, 0.5f, center );
		along = ( center[0] - eye[0] ) * dir[0] + ( center[1] - eye[1] ) * dir[1];
		if ( along <= 0 || along > bestDist ) {
			continue;
		}
		side = ( center[0] - eye[0] ) * dir[1] - ( center[1] - eye[1] ) * dir[0];
		radius = ( e->r.absmax[0] - e->r.absmin[0] ) * 0.5f;
		if ( fabs( side ) > radius ) {
			continue;
		}
		top = ( e->r.absmax[2] - eye[2] ) / along;
		bottom = ( e->r.absmin[2] - eye[2] ) / along;
		if ( bottom > AIMSLOPE || top < -AIMSLOPE ) {
			continue;
		}
		if ( top > AIMSLOPE ) {
			top = AIMSLOPE;
		}
		if ( bottom < -AIMSLOPE ) {
			bottom = -AIMSLOPE;
		}
		trap_Trace( &tr, eye, NULL, NULL, center, shooter->s.number, MASK_SOLID );
		if ( tr.fraction < 1 && tr.entityNum != e->s.number ) {
			continue;
		}
		best = e;
		bestDist = along;
		*pitch = -RAD2DEG( atan( ( top + bottom ) * 0.5f ) );
	}
	return best;
}

/*
=================
ShotPitch

P_BulletSlope with free aim: the view pitch, or with g_autoaim the pitch
to a target straight ahead or 5.6 degrees to either side
=================
*/
static float ShotPitch( gentity_t *ent, const vec3_t eye, float yaw, float range ) {
	float	pitch;

	if ( g_autoaim.integer ) {
		if ( Doom_AimLineAttack( ent, eye, yaw, range, &pitch )
			|| Doom_AimLineAttack( ent, eye, yaw + 5.625f, range, &pitch )
			|| Doom_AimLineAttack( ent, eye, yaw - 5.625f, range, &pitch ) ) {
			return pitch;
		}
	}
	return ent->client->ps.viewangles[PITCH];
}

/*
=================
Doom_SpawnFx

A Doom sprite animation on the clients, starting in 'state'
=================
*/
void Doom_SpawnFx( const vec3_t origin, int state, int flags ) {
	gentity_t	*te = G_TempEntity( (float *)origin, EV_DOOM_FX );

	te->s.frame = state;
	te->s.eventParm = flags;
}

// P_SpawnPuff
static void SpawnPuff( vec3_t pos, qboolean melee ) {
	pos[2] += ( Doom_Random() - Doom_Random() ) / 64.0f;
	Doom_SpawnFx( pos, melee ? S_PUFF3 : S_PUFF1, DOOMFX_RISE | DOOMFX_RANDOMTICS );
}

// P_SpawnBlood
static void SpawnBlood( vec3_t pos, int damage ) {
	int		state = S_BLOOD1;

	pos[2] += ( Doom_Random() - Doom_Random() ) / 64.0f;
	if ( damage <= 12 && damage >= 9 ) {
		state = S_BLOOD2;
	} else if ( damage < 9 ) {
		state = S_BLOOD3;
	}
	Doom_SpawnFx( pos, state, DOOMFX_GRAVITY | DOOMFX_RANDOMTICS );
}

/*
=================
LineAttack

P_LineAttack: a hitscan shot. Shoots gun lines, leaves a puff on walls
and blood on things that bleed, a few units short of the hit like Doom.
Returns the entity damaged, if any.
=================
*/
gentity_t *Doom_LineAttack( gentity_t *ent, const vec3_t eye, float yaw, float pitch, float range, int damage, int mod ) {
	vec3_t		angles, forward, end, pos;
	trace_t		tr;
	gentity_t	*hit;

	VectorSet( angles, pitch, yaw, 0 );
	AngleVectors( angles, forward, NULL, NULL );
	VectorMA( eye, range, forward, end );
	trap_Trace( &tr, eye, NULL, NULL, end, ent->s.number, MASK_SHOT );

	// only players shoot gun lines (P_ShootSpecialLine)
	if ( ent->client ) {
		Doom_ShotTrace( ent, eye, tr.endpos );
	}

	// nothing hit, or the sky: no puff
	if ( tr.fraction == 1 || ( tr.surfaceFlags & SURF_NOIMPACT ) ) {
		return NULL;
	}
	VectorMA( tr.endpos, -4, forward, pos );
	hit = &g_entities[tr.entityNum];

	// a switch on the wall is pressed by the shot
	if ( !hit->takedamage ) {
		Doom_ShootSwitch( ent, eye, tr.endpos );
	}
	if ( Doom_Bleeds( hit ) ) {
		SpawnBlood( pos, damage );
	} else {
		SpawnPuff( pos, range <= DOOM_MELEERANGE + 1 );
	}
	if ( !hit->takedamage ) {
		return NULL;
	}
	G_Damage( hit, ent, ent, forward, tr.endpos, damage, 0, mod );
	return hit;
}

static void TurnTo( gentity_t *ent, float yaw ) {
	vec3_t	angles;

	VectorCopy( ent->client->ps.viewangles, angles );
	angles[YAW] = yaw;
	SetClientViewAngle( ent, angles );
}

static float YawTo( gentity_t *ent, gentity_t *target ) {
	vec3_t	center;

	VectorAdd( target->r.absmin, target->r.absmax, center );
	VectorScale( center, 0.5f, center );
	return RAD2DEG( atan2( center[1] - ent->client->ps.origin[1], center[0] - ent->client->ps.origin[0] ) );
}

// A_Punch
static void Punch( gentity_t *ent, const vec3_t eye ) {
	int			damage = ( Doom_Random() % 10 + 1 ) << 1;
	float		yaw = ent->client->ps.viewangles[YAW] + Spread(), pitch = ent->client->ps.viewangles[PITCH];
	gentity_t	*target;

	if ( ent->client->ps.powerups[DPW_STRENGTH] ) {
		damage *= 10;
	}
	target = Doom_AimLineAttack( ent, eye, yaw, DOOM_MELEERANGE, &pitch );
	Doom_LineAttack( ent, eye, yaw, pitch, DOOM_MELEERANGE, damage, MOD_GAUNTLET );
	// turn to face the target
	if ( target ) {
		G_AddEvent( ent, EV_DOOM_SOUND, sfx_punch );
		TurnTo( ent, YawTo( ent, target ) );
	}
}

// A_Saw
static void Saw( gentity_t *ent, const vec3_t eye ) {
	int			damage = 2 * ( Doom_Random() % 10 + 1 );
	float		yaw = ent->client->ps.viewangles[YAW], pitch = ent->client->ps.viewangles[PITCH], diff;
	gentity_t	*target;

	yaw += Spread();
	// meleerange + 1 so the puff doesn't skip the flash
	target = Doom_AimLineAttack( ent, eye, yaw, DOOM_MELEERANGE + 1, &pitch );
	Doom_LineAttack( ent, eye, yaw, pitch, DOOM_MELEERANGE + 1, damage, MOD_CHAINSAW );
	if ( !target ) {
		G_AddEvent( ent, EV_DOOM_SOUND, sfx_sawful );
		return;
	}
	G_AddEvent( ent, EV_DOOM_SOUND, sfx_sawhit );

	// pulled toward the target, a little at a time
	yaw = ent->client->ps.viewangles[YAW];
	diff = AngleNormalize180( YawTo( ent, target ) - yaw );
	if ( diff < 0 ) {
		TurnTo( ent, diff < -4.5f ? yaw + diff + 90.0f / 21 : yaw - 4.5f );
	} else {
		TurnTo( ent, diff > 4.5f ? yaw + diff - 90.0f / 21 : yaw + 4.5f );
	}
}

// P_GunShot
static void GunShot( gentity_t *ent, const vec3_t eye, float pitch, qboolean accurate, int mod ) {
	int		damage = 5 * ( Doom_Random() % 3 + 1 );
	float	yaw = ent->client->ps.viewangles[YAW];

	if ( !accurate ) {
		yaw += Spread();
	}
	Doom_LineAttack( ent, eye, yaw, pitch, DOOM_MISSILERANGE, damage, mod );
}

/*
=====================================================================

MISSILES (P_SpawnPlayerMissile)

=====================================================================
*/

/*
=================
Doom_LaunchMissile

A Doom missile (rocket, plasma, BFG ball, monster fireballs) flying from
'start' at 'velocity' (units a second): a Quake 3 missile with the Doom
damage of its type
=================
*/
gentity_t *Doom_LaunchMissile( gentity_t *owner, const vec3_t start, const vec3_t velocity, int type ) {
	const doom_mobjinfo_t	*info = &doom_mobjinfo[type];
	gentity_t				*bolt;

	bolt = G_Spawn();
	bolt->classname = "doom_missile";
	bolt->nextthink = level.time + 15000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weapon = owner->s.weapon;
	bolt->s.generic1 = type;
	bolt->doomMissile = type + 1;
	bolt->r.ownerNum = owner->s.number;
	bolt->parent = owner;
	bolt->damage = ( Doom_Random() % 8 + 1 ) * info->damage;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;
	switch ( type ) {
	case MT_ROCKET:
		bolt->splashDamage = 128;		// A_Explode
		bolt->splashRadius = 128;
		bolt->methodOfDeath = MOD_ROCKET;
		bolt->splashMethodOfDeath = MOD_ROCKET_SPLASH;
		break;
	case MT_PLASMA:
		bolt->methodOfDeath = MOD_PLASMA;
		break;
	case MT_BFG:
		bolt->methodOfDeath = MOD_BFG;
		break;
	default:
		bolt->methodOfDeath = MOD_UNKNOWN;
		break;
	}

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorCopy( velocity, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );
	VectorCopy( start, bolt->r.currentOrigin );
	return bolt;
}

static void SpawnPlayerMissile( gentity_t *ent, const vec3_t eye, int type ) {
	float	yaw = ent->client->ps.viewangles[YAW];
	float	pitch = ShotPitch( ent, eye, yaw, AIMRANGE );
	vec3_t	angles, dir;

	VectorSet( angles, pitch, yaw, 0 );
	AngleVectors( angles, dir, NULL, NULL );
	VectorScale( dir, (float)doom_mobjinfo[type].speed / DOOM_FRACUNIT * DOOM_TICRATE, dir );
	Doom_LaunchMissile( ent, eye, dir, type );
}

/*
=================
BFGSpray

A_BFGSpray, from the shooter, some tics after the ball hit: 40 tracers
over 90 degrees around the ball's direction, 15d8 damage each
=================
*/
static void BFGSpray( gentity_t *self ) {
	gentity_t	*shooter = self->parent;
	vec3_t		eye;
	int			i, j;

	if ( shooter && shooter->inuse && shooter->client ) {
		EyePoint( shooter, eye );
		for ( i = 0 ; i < 40 ; i++ ) {
			float		pitch, yaw = self->s.angles[YAW] - 45 + 90.0f / 40 * i;
			gentity_t	*target = Doom_AimLineAttack( shooter, eye, yaw, AIMRANGE, &pitch );
			vec3_t		pos;
			int			damage = 0;

			if ( !target ) {
				continue;
			}
			VectorSet( pos, ( target->r.absmin[0] + target->r.absmax[0] ) * 0.5f, ( target->r.absmin[1] + target->r.absmax[1] ) * 0.5f,
				target->r.absmin[2] + ( target->r.absmax[2] - target->r.absmin[2] ) * 0.25f );
			Doom_SpawnFx( pos, doom_mobjinfo[MT_EXTRABFG].spawnstate, 0 );
			for ( j = 0 ; j < 15 ; j++ ) {
				damage += ( Doom_Random() & 7 ) + 1;
			}
			G_Damage( target, shooter, shooter, NULL, NULL, damage, DAMAGE_NO_KNOCKBACK, MOD_BFG_SPLASH );
		}
	}
	G_FreeEntity( self );
}

// tics from the start of the death animation to the action
static int TicsToAction( int state, int action ) {
	int		tics = 0, guard = 0;

	while ( state != S_NULL && guard++ < 64 ) {
		if ( doom_states[state].action == action ) {
			return tics;
		}
		tics += doom_states[state].tics > 0 ? doom_states[state].tics : 0;
		state = doom_states[state].nextstate;
	}
	return tics;
}

/*
=================
Doom_MissileImpact

A Doom missile hit something (P_ExplodeMissile): direct damage to what it
hit, then the explosion. The rocket's blast also hurts what it hit, like
Doom's A_Explode.
=================
*/
void Doom_MissileImpact( gentity_t *ent, trace_t *trace ) {
	gentity_t	*other = &g_entities[trace->entityNum];
	gentity_t	*owner = &g_entities[ent->r.ownerNum];
	int			type = ent->doomMissile - 1;
	vec3_t		velocity;

	BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
	// a monster's missiles explode on its own kind without hurting it (PIT_CheckThing)
	if ( owner->doomMobj && other->doomMobj && ( owner->doomMobj->type == other->doomMobj->type
		|| ( owner->doomMobj->type == MT_KNIGHT && other->doomMobj->type == MT_BRUISER )
		|| ( owner->doomMobj->type == MT_BRUISER && other->doomMobj->type == MT_KNIGHT ) ) ) {
		other = &g_entities[ENTITYNUM_WORLD];
	}
	if ( !other->takedamage ) {
		vec3_t start;

		// a switch hit by the missile is pressed
		VectorMA( trace->endpos, -64.0f / ( VectorLength( velocity ) + 1 ), velocity, start );
		Doom_ShootSwitch( owner, start, trace->endpos );
	}
	if ( other->takedamage && ent->damage ) {
		G_Damage( other, ent, owner, velocity, ent->s.origin, ent->damage, 0, ent->methodOfDeath );
	}

	G_AddEvent( ent, Doom_IsTarget( other ) ? EV_MISSILE_HIT : EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
	ent->s.otherEntityNum = other->s.number;
	ent->freeAfterEvent = qtrue;
	ent->s.eType = ET_GENERAL;
	vectoangles( velocity, ent->s.angles );

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );
	G_SetOrigin( ent, trace->endpos );

	if ( ent->splashDamage ) {
		G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, NULL, ent->splashMethodOfDeath );
	}

	if ( type == MT_BFG ) {
		gentity_t *spray = G_Spawn();

		spray->classname = "doom_bfgspray";
		spray->parent = ent->parent;
		spray->s.angles[YAW] = ent->s.angles[YAW];
		spray->think = BFGSpray;
		spray->nextthink = level.time + DOOM_TICS_MSEC( TicsToAction( doom_mobjinfo[MT_BFG].deathstate, ACT_BFGSpray ) );
		spray->r.svFlags = SVF_NOCLIENT;
	}

	trap_LinkEntity( ent );
}

/*
=================
Doom_FireWeapon

EV_FIRE_WEAPON: eventParm is the Doom action, | DOOM_FIRE_ACCURATE for a
first pistol or chaingun shot
=================
*/
void Doom_FireWeapon( gentity_t *ent, int parm ) {
	int		action = parm & ~DOOM_FIRE_ACCURATE, i;
	float	pitch;
	vec3_t	eye;

	if ( !ent->client ) {
		return;
	}
	// firing ends the spawn protection
	ent->client->spawnProtectedUntil = 0;
	EyePoint( ent, eye );

	// the shot wakes up the monsters that hear it (P_FireWeapon)
	Doom_NoiseAlert( ent );

	switch ( action ) {
	case ACT_Punch:
		Punch( ent, eye );
		break;
	case ACT_Saw:
		Saw( ent, eye );
		break;
	case ACT_FirePistol:
	case ACT_FireCGun:
		pitch = ShotPitch( ent, eye, ent->client->ps.viewangles[YAW], AIMRANGE );
		GunShot( ent, eye, pitch, ( parm & DOOM_FIRE_ACCURATE ) != 0, action == ACT_FirePistol ? MOD_PISTOL : MOD_DOOM_CHAINGUN );
		break;
	case ACT_FireShotgun:
		pitch = ShotPitch( ent, eye, ent->client->ps.viewangles[YAW], AIMRANGE );
		for ( i = 0 ; i < 7 ; i++ ) {
			GunShot( ent, eye, pitch, qfalse, MOD_SHOTGUN );
		}
		break;
	case ACT_FireMissile:
		SpawnPlayerMissile( ent, eye, MT_ROCKET );
		break;
	case ACT_FirePlasma:
		SpawnPlayerMissile( ent, eye, MT_PLASMA );
		break;
	case ACT_FireBFG:
		SpawnPlayerMissile( ent, eye, MT_BFG );
		break;
	default:
		break;
	}
}
