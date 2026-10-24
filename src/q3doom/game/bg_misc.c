/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// bg_misc.c -- both games misc functions, all completely stateless

#include "q_shared.h"
#include "bg_public.h"
#include "bg_doom.h"

/*QUAKED item_***** ( 0 0 0 ) (-16 -16 -16) (16 16 16) suspended
DO NOT USE THIS CLASS, IT JUST HOLDS GENERAL INFORMATION.
The suspended flag will allow items to hang in the air, otherwise they are dropped to the next surface.

If an item is the target of another entity, it will not spawn in until fired.

An item fires all of its targets when it is picked up.  If the toucher can't carry it, the targets won't be fired.

"notfree" if set to 1, don't spawn in free for all games
"notteam" if set to 1, don't spawn in team games
"notsingle" if set to 1, don't spawn in single player games
"wait"	override the default wait before respawning.  -1 = never respawn automatically, which can be used with targeted spawning.
"random" random number of plus or minus seconds varied from the respawn time
"count" override quantity or duration on most items.
*/

/*
Q3Doom: the items of The Ultimate Doom (P_TouchSpecialThing), spawned from
the map things with their doomednum and drawn with their Doom sprites.

giTag and quantity:
	IT_WEAPON	the Doom weapon (DWP_*)
	IT_AMMO		the ammo type (DAM_*, DAM_NUMAMMO: backpack) and the number of clips
	IT_HEALTH	0 up to 100, 1 up to 200 (bonus, soulsphere); the amount
	IT_ARMOR	the armor type it gives (0: armor bonus); the amount
	IT_KEY		DOOM_IT_*
	IT_POWERUP	DPW_*
*/
#define DOOM_SOUND( s )		"doom/sound/" s

gitem_t	bg_itemlist[] = 
{
	{ NULL },	// leave index 0 alone

	// weapons
	{ "doom_shotgun",		DOOM_SOUND( "DSWPNUP" ), { NULL }, NULL, "You got the shotgun!", 0, IT_WEAPON, DWP_SHOTGUN, "", "", 2001 },
	{ "doom_chaingun",		DOOM_SOUND( "DSWPNUP" ), { NULL }, NULL, "You got the chaingun!", 0, IT_WEAPON, DWP_CHAINGUN, "", "", 2002 },
	{ "doom_launcher",		DOOM_SOUND( "DSWPNUP" ), { NULL }, NULL, "You got the rocket launcher!", 0, IT_WEAPON, DWP_MISSILE, "", "", 2003 },
	{ "doom_plasmagun",		DOOM_SOUND( "DSWPNUP" ), { NULL }, NULL, "You got the plasma gun!", 0, IT_WEAPON, DWP_PLASMA, "", "", 2004 },
	{ "doom_chainsaw",		DOOM_SOUND( "DSWPNUP" ), { NULL }, NULL, "A chainsaw!  Find some meat!", 0, IT_WEAPON, DWP_CHAINSAW, "", "", 2005 },
	{ "doom_bfg9000",		DOOM_SOUND( "DSWPNUP" ), { NULL }, NULL, "You got the BFG9000!  Oh, yes.", 0, IT_WEAPON, DWP_BFG, "", "", 2006 },

	// ammo
	{ "doom_clip",			DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a clip.", 1, IT_AMMO, DAM_CLIP, "", "", 2007 },
	{ "doom_ammobox",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a box of bullets.", 5, IT_AMMO, DAM_CLIP, "", "", 2048 },
	{ "doom_shells",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up 4 shotgun shells.", 1, IT_AMMO, DAM_SHELL, "", "", 2008 },
	{ "doom_shellbox",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a box of shotgun shells.", 5, IT_AMMO, DAM_SHELL, "", "", 2049 },
	{ "doom_rocket",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a rocket.", 1, IT_AMMO, DAM_MISL, "", "", 2010 },
	{ "doom_rocketbox",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a box of rockets.", 5, IT_AMMO, DAM_MISL, "", "", 2046 },
	{ "doom_cell",			DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up an energy cell.", 1, IT_AMMO, DAM_CELL, "", "", 2047 },
	{ "doom_cellpack",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up an energy cell pack.", 5, IT_AMMO, DAM_CELL, "", "", 17 },
	{ "doom_backpack",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a backpack full of ammo!", 1, IT_AMMO, DAM_NUMAMMO, "", "", 8 },

	// health
	{ "doom_stimpack",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a stimpack.", 10, IT_HEALTH, 0, "", "", 2011 },
	{ "doom_medikit",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a medikit.", 25, IT_HEALTH, 0, "", "", 2012 },
	{ "doom_healthbonus",	DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a health bonus.", 1, IT_HEALTH, 1, "", "", 2014 },
	{ "doom_soulsphere",	DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Supercharge!", 100, IT_HEALTH, 1, "", "", 2013 },

	// armor
	{ "doom_greenarmor",	DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up the armor.", 100, IT_ARMOR, 1, "", "", 2018 },
	{ "doom_bluearmor",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up the MegaArmor!", 200, IT_ARMOR, 2, "", "", 2019 },
	{ "doom_armorbonus",	DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up an armor bonus.", 1, IT_ARMOR, 0, "", "", 2015 },

	// keys
	{ "doom_bluecard",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a blue keycard.", 0, IT_KEY, DOOM_IT_BLUECARD, "", "", 5 },
	{ "doom_yellowcard",	DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a yellow keycard.", 0, IT_KEY, DOOM_IT_YELLOWCARD, "", "", 6 },
	{ "doom_redcard",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a red keycard.", 0, IT_KEY, DOOM_IT_REDCARD, "", "", 13 },
	{ "doom_blueskull",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a blue skull key.", 0, IT_KEY, DOOM_IT_BLUESKULL, "", "", 40 },
	{ "doom_yellowskull",	DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a yellow skull key.", 0, IT_KEY, DOOM_IT_YELLOWSKULL, "", "", 39 },
	{ "doom_redskull",		DOOM_SOUND( "DSITEMUP" ), { NULL }, NULL, "Picked up a red skull key.", 0, IT_KEY, DOOM_IT_REDSKULL, "", "", 38 },

	// powerups
	{ "doom_invulnerability", DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Invulnerability!", 0, IT_POWERUP, DPW_INVULNERABILITY, "", "", 2022 },
	{ "doom_berserk",		DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Berserk!", 0, IT_POWERUP, DPW_STRENGTH, "", "", 2023 },
	{ "doom_invisibility",	DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Partial Invisibility", 0, IT_POWERUP, DPW_INVISIBILITY, "", "", 2024 },
	{ "doom_radsuit",		DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Radiation Shielding Suit", 0, IT_POWERUP, DPW_IRONFEET, "", "", 2025 },
	{ "doom_computermap",	DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Computer Area Map", 0, IT_POWERUP, DPW_ALLMAP, "", "", 2026 },
	{ "doom_visor",			DOOM_SOUND( "DSGETPOW" ), { NULL }, NULL, "Light Amplification Visor", 0, IT_POWERUP, DPW_INFRARED, "", "", 2045 },

	// end of list marker
	{ NULL }
};

int		bg_numItems = sizeof(bg_itemlist) / sizeof(bg_itemlist[0]) - 1;


/*
==============
BG_FindItemForPowerup
==============
*/
gitem_t	*BG_FindItemForPowerup( powerup_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( (bg_itemlist[i].giType == IT_POWERUP || 
					bg_itemlist[i].giType == IT_TEAM ||
					bg_itemlist[i].giType == IT_PERSISTANT_POWERUP) && 
			bg_itemlist[i].giTag == pw ) {
			return &bg_itemlist[i];
		}
	}

	return NULL;
}


/*
==============
BG_FindItemForHoldable
==============
*/
gitem_t	*BG_FindItemForHoldable( holdable_t pw ) {
	int		i;

	for ( i = 0 ; i < bg_numItems ; i++ ) {
		if ( bg_itemlist[i].giType == IT_HOLDABLE && bg_itemlist[i].giTag == pw ) {
			return &bg_itemlist[i];
		}
	}

	Com_Error( ERR_DROP, "HoldableItem not found" );

	return NULL;
}


/*
===============
BG_FindItemForWeapon

===============
*/
gitem_t	*BG_FindItemForWeapon( weapon_t weapon ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++) {
		if ( it->giType == IT_WEAPON && it->giTag == weapon ) {
			return it;
		}
	}

	Com_Error( ERR_DROP, "Couldn't find item for weapon %i", weapon);
	return NULL;
}

/*
===============
BG_FindItem

===============
*/
gitem_t	*BG_FindItem( const char *pickupName ) {
	gitem_t	*it;
	
	for ( it = bg_itemlist + 1 ; it->classname ; it++ ) {
		if ( !Q_stricmp( it->pickup_name, pickupName ) )
			return it;
	}

	return NULL;
}

/*
============
BG_PlayerTouchesItem

Items can be picked up without actually touching their physical bounds to make
grabbing them easier
============
*/
qboolean	BG_PlayerTouchesItem( playerState_t *ps, entityState_t *item, int atTime ) {
	vec3_t		origin;
	float		feet;

	BG_EvaluateTrajectory( &item->pos, atTime, origin );

	// Q3Doom: Doom's touch (PIT_CheckThing, P_TouchSpecialThing): the player's
	// box against the item's 20 unit radius, the item from 8 units below the
	// feet up to the player's height; item origins are on the floor
	feet = ps->origin[2] + MINS_Z;
	if ( fabs( ps->origin[0] - origin[0] ) >= 36 || fabs( ps->origin[1] - origin[1] ) >= 36
		|| origin[2] - feet > 56 || origin[2] - feet < -8 ) {
		return qfalse;
	}
	return qtrue;
}



/*
================
BG_CanItemBeGrabbed

Returns false if the item should not be picked up.
This needs to be the same for client side prediction and server use.

Q3Doom: the cases where P_TouchSpecialThing leaves the item there
(single player and cooperative rules; dropped items: modelindex2)
================
*/
qboolean BG_CanItemBeGrabbed( int gametype, const entityState_t *ent, const playerState_t *ps ) {
	gitem_t	*item;
	int		ammo;

	if ( ent->modelindex < 1 || ent->modelindex >= bg_numItems ) {
		Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: index out of range" );
	}

	item = &bg_itemlist[ent->modelindex];

	switch( item->giType ) {
	case IT_WEAPON:
		// a weapon owned already only gives its ammo
		if ( !DOOM_OWNS( ps, item->giTag ) ) {
			return qtrue;
		}
		ammo = bg_doomWeapons[item->giTag].ammo;
		return ammo < DAM_NUMAMMO && ps->ammo[ammo] < BG_DoomMaxAmmo( ps, ammo );

	case IT_AMMO:
		if ( item->giTag == DAM_NUMAMMO ) {
			return qtrue;		// backpack
		}
		return ps->ammo[item->giTag] < BG_DoomMaxAmmo( ps, item->giTag );

	case IT_ARMOR:
		// armor bonuses are always taken; armor only if it is better
		return !item->giTag || ps->stats[STAT_ARMOR] < item->giTag * 100;

	case IT_HEALTH:
		// bonuses and the soulsphere are always taken
		return item->giTag || ps->stats[STAT_HEALTH] < 100;

	case IT_KEY:
		return qtrue;

	case IT_POWERUP:
		// the computer map only once
		return item->giTag != DPW_ALLMAP || !ps->powerups[DPW_ALLMAP];

	case IT_BAD:
		Com_Error( ERR_DROP, "BG_CanItemBeGrabbed: IT_BAD" );
	default:
		break;
	}

	return qfalse;
}

//======================================================================

/*
================
BG_EvaluateTrajectory

================
*/
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result ) {
	float		deltaTime;
	float		phase;

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorCopy( tr->trBase, result );
		break;
	case TR_LINEAR:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		phase = sin( deltaTime * M_PI * 2 );
		VectorMA( tr->trBase, phase, tr->trDelta, result );
		break;
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			atTime = tr->trTime + tr->trDuration;
		}
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		if ( deltaTime < 0 ) {
			deltaTime = 0;
		}
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorMA( tr->trBase, deltaTime, tr->trDelta, result );
		result[2] -= 0.5 * DEFAULT_GRAVITY * deltaTime * deltaTime;		// FIXME: local gravity...
		break;
	default:
		Com_Error( ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trTime );
		break;
	}
}

/*
================
BG_EvaluateTrajectoryDelta

For determining velocity at a given time
================
*/
void BG_EvaluateTrajectoryDelta( const trajectory_t *tr, int atTime, vec3_t result ) {
	float	deltaTime;
	float	phase;

	switch( tr->trType ) {
	case TR_STATIONARY:
	case TR_INTERPOLATE:
		VectorClear( result );
		break;
	case TR_LINEAR:
		VectorCopy( tr->trDelta, result );
		break;
	case TR_SINE:
		deltaTime = ( atTime - tr->trTime ) / (float) tr->trDuration;
		phase = cos( deltaTime * M_PI * 2 );	// derivative of sin = cos
		phase *= 0.5;
		VectorScale( tr->trDelta, phase, result );
		break;
	case TR_LINEAR_STOP:
		if ( atTime > tr->trTime + tr->trDuration ) {
			VectorClear( result );
			return;
		}
		VectorCopy( tr->trDelta, result );
		break;
	case TR_GRAVITY:
		deltaTime = ( atTime - tr->trTime ) * 0.001;	// milliseconds to seconds
		VectorCopy( tr->trDelta, result );
		result[2] -= DEFAULT_GRAVITY * deltaTime;		// FIXME: local gravity...
		break;
	default:
		Com_Error( ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime );
		break;
	}
}

char *eventnames[] = {
	"EV_NONE",

	"EV_FOOTSTEP",
	"EV_FOOTSTEP_METAL",
	"EV_FOOTSPLASH",
	"EV_FOOTWADE",
	"EV_SWIM",

	"EV_STEP_4",
	"EV_STEP_8",
	"EV_STEP_12",
	"EV_STEP_16",

	"EV_FALL_SHORT",
	"EV_FALL_MEDIUM",
	"EV_FALL_FAR",

	"EV_JUMP_PAD",			// boing sound at origin", jump sound on player

	"EV_JUMP",
	"EV_WATER_TOUCH",	// foot touches
	"EV_WATER_LEAVE",	// foot leaves
	"EV_WATER_UNDER",	// head touches
	"EV_WATER_CLEAR",	// head leaves

	"EV_ITEM_PICKUP",			// normal item pickups are predictable
	"EV_GLOBAL_ITEM_PICKUP",	// powerup / team sounds are broadcast to everyone

	"EV_NOAMMO",
	"EV_CHANGE_WEAPON",
	"EV_FIRE_WEAPON",

	"EV_USE_ITEM0",
	"EV_USE_ITEM1",
	"EV_USE_ITEM2",
	"EV_USE_ITEM3",
	"EV_USE_ITEM4",
	"EV_USE_ITEM5",
	"EV_USE_ITEM6",
	"EV_USE_ITEM7",
	"EV_USE_ITEM8",
	"EV_USE_ITEM9",
	"EV_USE_ITEM10",
	"EV_USE_ITEM11",
	"EV_USE_ITEM12",
	"EV_USE_ITEM13",
	"EV_USE_ITEM14",
	"EV_USE_ITEM15",

	"EV_ITEM_RESPAWN",
	"EV_ITEM_POP",
	"EV_PLAYER_TELEPORT_IN",
	"EV_PLAYER_TELEPORT_OUT",

	"EV_GRENADE_BOUNCE",		// eventParm will be the soundindex

	"EV_GENERAL_SOUND",
	"EV_GLOBAL_SOUND",		// no attenuation
	"EV_GLOBAL_TEAM_SOUND",

	"EV_BULLET_HIT_FLESH",
	"EV_BULLET_HIT_WALL",

	"EV_MISSILE_HIT",
	"EV_MISSILE_MISS",
	"EV_MISSILE_MISS_METAL",
	"EV_RAILTRAIL",
	"EV_SHOTGUN",
	"EV_BULLET",				// otherEntity is the shooter

	"EV_PAIN",
	"EV_DEATH1",
	"EV_DEATH2",
	"EV_DEATH3",
	"EV_OBITUARY",

	"EV_POWERUP_QUAD",
	"EV_POWERUP_BATTLESUIT",
	"EV_POWERUP_REGEN",

	"EV_GIB_PLAYER",			// gib a previously living player
	"EV_SCOREPLUM",			// score plum

//#ifdef MISSIONPACK
	"EV_PROXIMITY_MINE_STICK",
	"EV_PROXIMITY_MINE_TRIGGER",
	"EV_KAMIKAZE",			// kamikaze explodes
	"EV_OBELISKEXPLODE",		// obelisk explodes
	"EV_INVUL_IMPACT",		// invulnerability sphere impact
	"EV_JUICED",				// invulnerability juiced effect
	"EV_LIGHTNINGBOLT",		// lightning bolt bounced of invulnerability sphere
//#endif

	"EV_DEBUG_LINE",
	"EV_STOPLOOPINGSOUND",
	"EV_TAUNT"

};

/*
===============
BG_AddPredictableEventToPlayerstate

Handles the sequence numbers
===============
*/

void	trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

void BG_AddPredictableEventToPlayerstate( int newEvent, int eventParm, playerState_t *ps ) {

#ifdef _DEBUG
	{
		char buf[256];
		trap_Cvar_VariableStringBuffer("showevents", buf, sizeof(buf));
		if ( atof(buf) != 0 ) {
#ifdef QAGAME
			Com_Printf(" game event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#else
			Com_Printf("Cgame event svt %5d -> %5d: num = %20s parm %d\n", ps->pmove_framecount/*ps->commandTime*/, ps->eventSequence, eventnames[newEvent], eventParm);
#endif
		}
	}
#endif
	ps->events[ps->eventSequence & (MAX_PS_EVENTS-1)] = newEvent;
	ps->eventParms[ps->eventSequence & (MAX_PS_EVENTS-1)] = eventParm;
	ps->eventSequence++;
}

/*
========================
BG_TouchJumpPad
========================
*/
void BG_TouchJumpPad( playerState_t *ps, entityState_t *jumppad ) {
	vec3_t	angles;
	float p;
	int effectNum;

	// spectators don't use jump pads
	if ( ps->pm_type != PM_NORMAL ) {
		return;
	}

	// flying characters don't hit bounce pads
	if ( ps->powerups[PW_FLIGHT] ) {
		return;
	}

	// if we didn't hit this same jumppad the previous frame
	// then don't play the event sound again if we are in a fat trigger
	if ( ps->jumppad_ent != jumppad->number ) {

		vectoangles( jumppad->origin2, angles);
		p = fabs( AngleNormalize180( angles[PITCH] ) );
		if( p < 45 ) {
			effectNum = 0;
		} else {
			effectNum = 1;
		}
		BG_AddPredictableEventToPlayerstate( EV_JUMP_PAD, effectNum, ps );
	}
	// remember hitting this jumppad this frame
	ps->jumppad_ent = jumppad->number;
	ps->jumppad_frame = ps->pmove_framecount;
	// give the player the velocity from the jumppad
	VectorCopy( jumppad->origin2, ps->velocity );
}

/*
========================
BG_PlayerStateToEntityState

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityState( playerState_t *ps, entityState_t *s, qboolean snap ) {
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR ) {
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_INTERPOLATE;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction
	VectorCopy( ps->velocity, s->pos.trDelta );

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->loopSound = ps->loopSound;
	s->generic1 = ps->generic1;
}

/*
========================
BG_PlayerStateToEntityStateExtraPolate

This is done after each set of usercmd_t on the server,
and after local prediction on the client
========================
*/
void BG_PlayerStateToEntityStateExtraPolate( playerState_t *ps, entityState_t *s, int time, qboolean snap ) {
	int		i;

	if ( ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPECTATOR ) {
		s->eType = ET_INVISIBLE;
	} else if ( ps->stats[STAT_HEALTH] <= GIB_HEALTH ) {
		s->eType = ET_INVISIBLE;
	} else {
		s->eType = ET_PLAYER;
	}

	s->number = ps->clientNum;

	s->pos.trType = TR_LINEAR_STOP;
	VectorCopy( ps->origin, s->pos.trBase );
	if ( snap ) {
		SnapVector( s->pos.trBase );
	}
	// set the trDelta for flag direction and linear prediction
	VectorCopy( ps->velocity, s->pos.trDelta );
	// set the time for linear prediction
	s->pos.trTime = time;
	// set maximum extra polation time
	s->pos.trDuration = 50; // 1000 / sv_fps (default = 20)

	s->apos.trType = TR_INTERPOLATE;
	VectorCopy( ps->viewangles, s->apos.trBase );
	if ( snap ) {
		SnapVector( s->apos.trBase );
	}

	s->angles2[YAW] = ps->movementDir;
	s->legsAnim = ps->legsAnim;
	s->torsoAnim = ps->torsoAnim;
	s->clientNum = ps->clientNum;		// ET_PLAYER looks here instead of at number
										// so corpses can also reference the proper config
	s->eFlags = ps->eFlags;
	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		s->eFlags |= EF_DEAD;
	} else {
		s->eFlags &= ~EF_DEAD;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	s->weapon = ps->weapon;
	s->groundEntityNum = ps->groundEntityNum;

	s->powerups = 0;
	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if ( ps->powerups[ i ] ) {
			s->powerups |= 1 << i;
		}
	}

	s->loopSound = ps->loopSound;
	s->generic1 = ps->generic1;
}
