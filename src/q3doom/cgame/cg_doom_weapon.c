/*
===========================================================================
Q3Doom - the Doom weapon in the cgame

- the weapon and muzzle flash sprites of the predicted player state drawn
  like R_DrawPSprite: Doom's position, bob and lighting
- weapon selection: Doom's slots (1: fist or chainsaw ... 7: BFG)
- the Doom side of entity events: weapon and Doom sounds, effects,
  explosions, pickups, player sounds (see PLAN.md 2.4)

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "cg_local.h"
#include "../game/bg_doom.h"

#define SARGE_SOUND( s )	"sound/player/sarge/" s ".wav"

static struct {
	float		bobX, bobY;		// bob kept from the last ready tic (Doom keeps sx/sy while firing)
	int			lastWeapon;		// predicted weapon last frame, to follow changes made by the game
} cw;

/*
=====================================================================

WEAPON SPRITES (r_things.c R_DrawPSprite, p_pspr.c A_WeaponReady)

=====================================================================
*/

/*
=================
PspriteLight

The brightness of a weapon sprite: Doom lights it with the colormap of
the sector light (plus the gun flash) at the nearest distance, which is
full bright except in dark sectors
=================
*/
static float PspriteLight( void ) {
	int		level = CG_DoomLightAt( cg.predictedPlayerState.origin );
	int		lightnum = level >> 4, map;

	if ( lightnum > 15 ) {
		lightnum = 15;
	}
	// scalelight[lightnum][MAXLIGHTSCALE-1]: startmap - 47 * 320 / 320 / 2
	map = ( 15 - lightnum ) * 4 - 23;
	if ( map < 0 ) {
		map = 0;
	}
	return 1.0f - map / 32.0f;
}

static void DrawPsprite( int state, float sx, float sy ) {
	qboolean	flip;
	int			lump = CG_DoomSpriteLump( state, 0, &flip );
	const byte	*header;
	float		left, top, width, height, x0, sX, sY, viewTop, centery, light;
	vec4_t		color;

	if ( lump < 0 ) {
		return;
	}
	header = doom_wad_lump_data( CG_DoomWad(), lump );
	width = (short)( header[0] | ( header[1] << 8 ) );
	height = (short)( header[2] | ( header[3] << 8 ) );
	left = (short)( header[4] | ( header[5] << 8 ) );
	top = (short)( header[6] | ( header[7] << 8 ) );

	// Doom's 320x200 in a 4:3 area of the screen; the view is above the
	// status bar (168 rows, centered at 84) or the whole screen (at 100)
	CG_DoomHudScale( &x0, &sX, &sY );
	viewTop = cg.refdef.y;
	centery = cg.refdef.height / sY * 0.5f;

	light = ( doom_states[state].frame & DOOM_FF_FULLBRIGHT ) || CG_DoomPowerActive( DPW_INFRARED ) ? 1.0f : PspriteLight();
	color[0] = color[1] = color[2] = light;
	color[3] = 1;
	// partial invisibility: Doom's fuzz, a dark shadow
	if ( CG_DoomPowerActive( DPW_INVISIBILITY ) ) {
		color[0] = color[1] = color[2] = 0.1f;
		color[3] = 0.6f;
	}
	trap_R_SetColor( color );
	trap_R_DrawStretchPic( x0 + ( sx - left ) * sX, viewTop + ( centery - 100 + sy - top ) * sY, width * sX, height * sY,
		flip ? 1 : 0, 0, flip ? 0 : 1, 1, CG_DoomPatchShader( lump ) );
	trap_R_SetColor( NULL );
}

/*
=================
CG_DoomDrawWeapon

The weapon and its flash, bobbing while ready
=================
*/
void CG_DoomDrawWeapon( void ) {
	const playerState_t	*ps = &cg.predictedPlayerState;
	float				sx, sy;

	if ( !CG_DoomGame() || !cg_drawGun.integer || cg.renderingThirdPerson || ps->persistant[PERS_TEAM] == TEAM_SPECTATOR
		|| ps->pm_type == PM_INTERMISSION || ps->doomWeaponState == S_NULL ) {
		return;
	}

	// A_WeaponReady: the bob from the player's momentum
	if ( doom_states[ps->doomWeaponState].action == ACT_WeaponReady ) {
		float	momx = ps->velocity[0] / DOOM_TICRATE, momy = ps->velocity[1] / DOOM_TICRATE;
		float	bob = ( momx * momx + momy * momy ) * 0.25f;
		float	angle = ( 128 * ( cg.time * DOOM_TICRATE / 1000 ) & 8191 ) * ( 2 * M_PI / 8192 );

		if ( bob > 16 ) {
			bob = 16;
		}
		cw.bobX = bob * cos( angle );
		cw.bobY = bob * fabs( sin( angle ) );
	}
	sx = 1 + cw.bobX;
	sy = ps->doomWeaponY + ( ps->doomWeaponY == DOOM_WEAPONTOP ? cw.bobY : 0 );

	DrawPsprite( ps->doomWeaponState, sx, sy );
	if ( ps->doomFlashState != S_NULL ) {
		DrawPsprite( ps->doomFlashState, sx, sy );
	}
}

/*
=====================================================================

WEAPON SELECTION

=====================================================================
*/

// the order weapnext and weapprev go through
static const int cycle[] = { DWP_FIST, DWP_CHAINSAW, DWP_PISTOL, DWP_SHOTGUN, DWP_CHAINGUN, DWP_MISSILE, DWP_PLASMA, DWP_BFG };

static void Select( int weapon ) {
	cg.weaponSelectTime = cg.time;
	cg.weaponSelect = DOOM_WP( weapon );
}

/*
=================
CG_DoomWeapon_f

"weapon <slot>": Doom's number keys. 1 is the chainsaw when owned, unless
already holding it with berserk (P_PlayerThink).
=================
*/
void CG_DoomWeapon_f( void ) {
	const playerState_t	*ps;
	int					slot = atoi( CG_Argv( 1 ) ), weapon;

	if ( !cg.snap ) {
		return;
	}
	ps = &cg.snap->ps;
	if ( ps->pm_flags & PMF_FOLLOW ) {
		return;
	}
	if ( slot < 1 || slot > 7 ) {
		return;
	}
	weapon = slot - 1;		// slots 1-7 are the weapons in Doom's order

	// the Quake 3 set: the same slots with the Quake 3 weapons (the lightning
	// gun in place of the chainsaw: 1 switches between it and the gauntlet)
	if ( cgs.doomWeapons == DOOM_WEAPONS_Q3 ) {
		int		q3 = bg_doomQ3Weapon[weapon];

		if ( weapon == DWP_FIST && ( ps->stats[STAT_WEAPONS] & ( 1 << WP_LIGHTNING ) ) && ps->ammo[WP_LIGHTNING]
			&& cg.weaponSelect != WP_LIGHTNING ) {
			q3 = WP_LIGHTNING;
		}
		if ( ( ps->stats[STAT_WEAPONS] & ( 1 << q3 ) ) && ps->ammo[q3] ) {
			cg.weaponSelectTime = cg.time;
			cg.weaponSelect = q3;
		}
		return;
	}
	if ( cgs.doomWeapons == DOOM_WEAPONS_NONE ) {
		return;
	}

	if ( weapon == DWP_FIST && DOOM_OWNS( ps, DWP_CHAINSAW )
		&& !( ps->weapon == DOOM_WP( DWP_CHAINSAW ) && ps->powerups[DPW_STRENGTH] ) ) {
		weapon = DWP_CHAINSAW;
	}
	if ( DOOM_OWNS( ps, weapon ) ) {
		Select( weapon );
	}
}

static void Cycle( int dir ) {
	const playerState_t	*ps;
	int					n = sizeof( cycle ) / sizeof( cycle[0] ), i, current = 0;

	if ( !cg.snap || cgs.doomWeapons == DOOM_WEAPONS_NONE ) {
		return;
	}
	if ( cgs.doomWeapons == DOOM_WEAPONS_Q3 ) {
		if ( dir > 0 ) {
			CG_NextWeapon_f();
		} else {
			CG_PrevWeapon_f();
		}
		return;
	}
	ps = &cg.snap->ps;
	if ( ps->pm_flags & PMF_FOLLOW ) {
		return;
	}
	for ( i = 0 ; i < n ; i++ ) {
		if ( DOOM_WP( cycle[i] ) == cg.weaponSelect ) {
			current = i;
		}
	}
	for ( i = 1 ; i < n ; i++ ) {
		int w = cycle[( current + dir * i + n * 2 ) % n];

		if ( DOOM_OWNS( ps, w ) ) {
			Select( w );
			return;
		}
	}
}

void CG_DoomNextWeapon_f( void ) {
	Cycle( 1 );
}

void CG_DoomPrevWeapon_f( void ) {
	Cycle( -1 );
}

/*
=================
CG_DoomWeaponFrame

The game changes weapons too (pickups, empty weapons): follow it, so the
selection sent with the commands stays the weapon held
=================
*/
void CG_DoomWeaponFrame( void ) {
	if ( cg.predictedPlayerState.weapon != cw.lastWeapon ) {
		cw.lastWeapon = cg.predictedPlayerState.weapon;
		if ( cw.lastWeapon ) {
			cg.weaponSelect = cw.lastWeapon;
		}
	}
}

/*
=====================================================================

EVENTS

=====================================================================
*/

static void StartDoomSound( int entityNum, int channel, int sfx ) {
	sfxHandle_t	h = CG_DoomSound( sfx );

	if ( h ) {
		trap_S_StartSound( NULL, entityNum, channel, h );
	}
}

/*
=================
CG_DoomEntityEvent

The Doom handling of an entity event; qfalse lets the Quake 3 code
handle it
=================
*/
qboolean CG_DoomEntityEvent( centity_t *cent, vec3_t position, int event ) {
	entityState_t	*es = &cent->currentState;
	qboolean		local = es->number == cg.snap->ps.clientNum;

	if ( !CG_DoomGame() ) {
		return qfalse;
	}

	// DoomFrag's Quake 3 weapons fire and switch the Quake 3 way
	if ( cgs.doomWeapons == DOOM_WEAPONS_Q3 && ( event == EV_FIRE_WEAPON || event == EV_NOAMMO || event == EV_CHANGE_WEAPON ) ) {
		return qfalse;
	}

	switch ( event ) {
	case EV_FIRE_WEAPON:
		cent->muzzleFlashTime = cg.time;	// the marine's attack frames (CG_DoomPlayer)
		if ( es->powerups & ( 1 << PW_QUAD ) ) {
			trap_S_StartSound( NULL, es->number, CHAN_ITEM, cgs.media.quadSound );
		}
		// the sounds of the attacks that always make one (punch and saw
		// sounds depend on the hit: the game sends them)
		switch ( es->eventParm & ~DOOM_FIRE_ACCURATE ) {
		case ACT_FirePistol:
		case ACT_FireCGun:
			StartDoomSound( es->number, CHAN_WEAPON, sfx_pistol );
			break;
		case ACT_FireShotgun:
			StartDoomSound( es->number, CHAN_WEAPON, sfx_shotgn );
			break;
		case ACT_FireMissile:
			StartDoomSound( es->number, CHAN_WEAPON, doom_mobjinfo[MT_ROCKET].seesound );
			break;
		case ACT_FirePlasma:
			StartDoomSound( es->number, CHAN_WEAPON, doom_mobjinfo[MT_PLASMA].seesound );
			break;
		}
		return qtrue;

	case EV_DOOM_SOUND:
		// the bosses are heard at full volume everywhere
		if ( es->eventParm & DOOM_SOUND_FULL ) {
			sfxHandle_t h = CG_DoomSound( es->eventParm & ~DOOM_SOUND_FULL );

			if ( h ) {
				trap_S_StartLocalSound( h, CHAN_ANNOUNCER );
			}
			return qtrue;
		}
		StartDoomSound( es->number, CHAN_AUTO, es->eventParm );
		return qtrue;

	case EV_DOOM_FX:
		CG_DoomSpawnFx( position, es->frame, es->eventParm );
		return qtrue;

	case EV_MISSILE_HIT:
	case EV_MISSILE_MISS:
	case EV_MISSILE_MISS_METAL:
		if ( es->generic1 <= 0 || es->generic1 >= NUMMOBJTYPES ) {
			return qfalse;
		}
		// P_ExplodeMissile: the death states and sound of the missile
		CG_DoomSpawnFx( position, doom_mobjinfo[es->generic1].deathstate, DOOMFX_RANDOMTICS );
		StartDoomSound( es->number, CHAN_AUTO, doom_mobjinfo[es->generic1].deathsound );
		return qtrue;

	case EV_ITEM_PICKUP:
		if ( es->eventParm < 1 || es->eventParm >= bg_numItems ) {
			return qtrue;
		}
		// Doom plays pickup sounds for the player who picked up only
		if ( local ) {
			// the CTF flags have none: the global pickup event announces them
			if ( bg_itemlist[es->eventParm].pickup_sound ) {
				trap_S_StartSound( NULL, es->number, CHAN_AUTO, trap_S_RegisterSound( bg_itemlist[es->eventParm].pickup_sound, qfalse ) );
			}
			CG_DoomMessage( bg_itemlist[es->eventParm].pickup_name );
			CG_DoomBonus();
			// the Quake 3 set switches to the matching Quake 3 weapon (the
			// Doom set switches in Pmove)
			if ( cgs.doomWeapons == DOOM_WEAPONS_Q3 && bg_itemlist[es->eventParm].giType == IT_WEAPON && cg_autoswitch.integer ) {
				int q3 = bg_doomQ3Weapon[bg_itemlist[es->eventParm].giTag];

				if ( q3 != WP_MACHINEGUN && q3 != cg.weaponSelect ) {
					cg.weaponSelectTime = cg.time;
					cg.weaponSelect = q3;
				}
			}
		}
		return qtrue;

	case EV_NOAMMO:
	case EV_CHANGE_WEAPON:
		return qtrue;

	case EV_PLAYER_TELEPORT_IN:
	case EV_PLAYER_TELEPORT_OUT:
		// Doom's teleport fog, but nothing when the level starts
		if ( cg.time > cgs.levelStartTime + 2000 ) {
			CG_DoomSpawnFx( position, doom_mobjinfo[MT_TFOG].spawnstate, 0 );
			StartDoomSound( es->number, CHAN_AUTO, sfx_telept );
		}
		return qtrue;

	case EV_JUMP:
		trap_S_StartSound( NULL, es->number, CHAN_VOICE, trap_S_RegisterSound( SARGE_SOUND( "jump1" ), qfalse ) );
		return qtrue;

	case EV_FALL_SHORT:
	case EV_FALL_MEDIUM:
	case EV_FALL_FAR:
		// Doom's "oof" on hard landings; Sarge's when the fall hurts
		if ( event != EV_FALL_SHORT && atoi( Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_fallDamage" ) ) ) {
			trap_S_StartSound( NULL, es->number, CHAN_VOICE, trap_S_RegisterSound( SARGE_SOUND( "fall1" ), qfalse ) );
			cent->pe.painTime = cg.time;	// no pain sound right after this
		} else {
			StartDoomSound( es->number, CHAN_VOICE, sfx_oof );
		}
		if ( es->clientNum == cg.predictedPlayerState.clientNum ) {
			cg.landChange = event == EV_FALL_SHORT ? -8 : event == EV_FALL_MEDIUM ? -16 : -24;
			cg.landTime = cg.time;
		}
		return qtrue;

	case EV_DEATH1:
	case EV_DEATH2:
	case EV_DEATH3:
		StartDoomSound( es->number, CHAN_VOICE, sfx_pldeth );
		if ( local ) {
			CG_DoomLocalDeath( qfalse );
		}
		return qtrue;

	case EV_GIB_PLAYER:
		StartDoomSound( es->number, CHAN_BODY, sfx_slop );
		if ( local ) {
			CG_DoomLocalDeath( qtrue );
		}
		return qtrue;

	case EV_ITEM_RESPAWN:
		StartDoomSound( es->number, CHAN_AUTO, sfx_itmbk );
		return qtrue;

	default:
		return qfalse;
	}
}

/*
=================
CG_DoomPainSound

Doom's player pain sound, for CG_PainEvent
=================
*/
qboolean CG_DoomPainSound( centity_t *cent ) {
	if ( !CG_DoomGame() ) {
		return qfalse;
	}
	StartDoomSound( cent->currentState.number, CHAN_VOICE, sfx_plpain );
	return qtrue;
}
