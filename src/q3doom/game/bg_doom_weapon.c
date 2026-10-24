/*
===========================================================================
Q3Doom - Doom weapon sprites in Pmove (p_pspr.c)

The weapon and its muzzle flash are Doom "psprites": they step through the
Doom state table at 35 tics a second, and the state actions raise, lower,
fire and refire the weapon exactly like Doom. It runs inside Pmove, on the
player state, so the client predicts it and the server agrees.

The actions that hit something are done by the game (EV_FIRE_WEAPON with
the action in eventParm); ammo, flashes and timing happen here. Sounds the
cgame cannot derive from the fire event go out as EV_DOOM_SOUND.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "bg_doom.h"

#define LOWERSPEED		6
#define RAISESPEED		6

enum { PS_WEAPON, PS_FLASH };

static playerState_t	*wps;
static qboolean			wattack;		// trigger held this tic
static qboolean			wevents;		// running in Pmove: events can be added

static void SetPsprite( int position, int stnum );

static void WeaponEvent( int event, int parm ) {
	if ( wevents ) {
		BG_AddPredictableEventToPlayerstate( event, parm, wps );
	}
}

static int ReadyWeapon( void ) {
	int w = DOOM_DWP( wps->weapon );

	return ( w >= 0 && w < DWP_NUMWEAPONS ) ? w : DWP_FIST;
}

static void UseAmmo( int count ) {
	int ammo = bg_doomWeapons[ReadyWeapon()].ammo;

	if ( ammo < DAM_NUMAMMO ) {
		wps->ammo[ammo] -= count;
		if ( wps->ammo[ammo] < 0 ) {
			wps->ammo[ammo] = 0;
		}
	}
}

static int Refire( void ) {
	return wps->doomWeaponFlags & DOOMWF_REFIRE;
}

static void SetExtraLight( int light ) {
	wps->doomWeaponFlags = ( wps->doomWeaponFlags & ~DOOMWF_LIGHTMASK ) | ( light << DOOMWF_LIGHTSHIFT );
}

/*
=================
BringUpWeapon

P_BringUpWeapon: the pending weapon becomes the ready one and rises
from the bottom of the screen
=================
*/
static void BringUpWeapon( void ) {
	int		w;

	if ( wps->doomPendingWeapon ) {
		wps->weapon = wps->doomPendingWeapon;
	}
	wps->doomPendingWeapon = 0;
	w = ReadyWeapon();
	wps->weapon = DOOM_WP( w );
	if ( w == DWP_CHAINSAW ) {
		WeaponEvent( EV_DOOM_SOUND, sfx_sawup );
	}
	wps->doomWeaponY = DOOM_WEAPONBOTTOM;
	SetPsprite( PS_WEAPON, bg_doomWeapons[w].upstate );
}

void BG_DoomBringUpWeapon( playerState_t *ps ) {
	wps = ps;
	wevents = qfalse;
	wattack = qfalse;
	ps->doomFlashState = S_NULL;
	ps->doomFlashTics = 0;
	BringUpWeapon();
}

/*
=================
CheckAmmo

P_CheckAmmo: enough for one shot, or lower the weapon and switch
=================
*/
static qboolean CheckAmmo( void ) {
	int		w = ReadyWeapon();
	int		ammo = bg_doomWeapons[w].ammo;
	int		count = w == DWP_BFG ? DOOM_BFGCELLS : w == DWP_SUPERSHOTGUN ? 2 : 1;

	if ( ammo == DAM_NOAMMO || wps->ammo[ammo] >= count ) {
		return qtrue;
	}
	wps->doomPendingWeapon = DOOM_WP( BG_DoomBestWeapon( wps ) );
	SetPsprite( PS_WEAPON, bg_doomWeapons[w].downstate );
	return qfalse;
}

static void FireWeapon( void ) {
	if ( !CheckAmmo() ) {
		return;
	}
	SetPsprite( PS_WEAPON, bg_doomWeapons[ReadyWeapon()].atkstate );
}

/*
=================
Action

The state's action function, called when a psprite enters the state
=================
*/
static void Action( int position, int state ) {
	const doomWeaponInfo_t	*info = &bg_doomWeapons[ReadyWeapon()];
	int						action = doom_states[state].action;

	switch ( action ) {
	case ACT_Light0:
		SetExtraLight( 0 );
		break;
	case ACT_Light1:
		SetExtraLight( 1 );
		break;
	case ACT_Light2:
		SetExtraLight( 2 );
		break;

	case ACT_WeaponReady:
		if ( ReadyWeapon() == DWP_CHAINSAW && state == S_SAW ) {
			WeaponEvent( EV_DOOM_SOUND, sfx_sawidl );
		}
		// a weapon change, or death, puts the weapon away
		if ( wps->doomPendingWeapon || wps->stats[STAT_HEALTH] <= 0 ) {
			SetPsprite( PS_WEAPON, info->downstate );
			return;
		}
		// the missile launcher and the BFG do not auto fire
		if ( wattack ) {
			if ( !( wps->doomWeaponFlags & DOOMWF_ATTACKDOWN ) || ( ReadyWeapon() != DWP_MISSILE && ReadyWeapon() != DWP_BFG ) ) {
				wps->doomWeaponFlags |= DOOMWF_ATTACKDOWN;
				FireWeapon();
				return;
			}
		} else {
			wps->doomWeaponFlags &= ~DOOMWF_ATTACKDOWN;
		}
		// the bob is drawn by the cgame
		wps->doomWeaponY = DOOM_WEAPONTOP;
		break;

	case ACT_ReFire:
		if ( wattack && !wps->doomPendingWeapon && wps->stats[STAT_HEALTH] > 0 ) {
			if ( Refire() < DOOMWF_REFIRE ) {
				wps->doomWeaponFlags++;
			}
			FireWeapon();
		} else {
			wps->doomWeaponFlags &= ~DOOMWF_REFIRE;
			CheckAmmo();
		}
		break;

	case ACT_CheckReload:
		CheckAmmo();
		break;

	case ACT_Lower:
		wps->doomWeaponY += LOWERSPEED;
		if ( wps->doomWeaponY < DOOM_WEAPONBOTTOM ) {
			return;
		}
		// dead: the weapon stays down
		if ( wps->pm_type == PM_DEAD || wps->stats[STAT_HEALTH] <= 0 ) {
			wps->doomWeaponY = DOOM_WEAPONBOTTOM;
			return;
		}
		BringUpWeapon();
		break;

	case ACT_Raise:
		wps->doomWeaponY -= RAISESPEED;
		if ( wps->doomWeaponY > DOOM_WEAPONTOP ) {
			return;
		}
		wps->doomWeaponY = DOOM_WEAPONTOP;
		SetPsprite( PS_WEAPON, info->readystate );
		break;

	case ACT_GunFlash:
		SetPsprite( PS_FLASH, info->flashstate );
		break;

	// the hits are the game's
	case ACT_Punch:
	case ACT_Saw:
		WeaponEvent( EV_FIRE_WEAPON, action );
		break;

	case ACT_FirePistol:
	case ACT_FireShotgun:
		UseAmmo( 1 );
		SetPsprite( PS_FLASH, info->flashstate );
		WeaponEvent( EV_FIRE_WEAPON, action | ( !Refire() && action == ACT_FirePistol ? DOOM_FIRE_ACCURATE : 0 ) );
		break;

	case ACT_FireCGun:
		if ( !wps->ammo[info->ammo] ) {
			return;
		}
		UseAmmo( 1 );
		// the flash matching the chaingun frame (S_CHAIN1 or S_CHAIN2)
		SetPsprite( PS_FLASH, info->flashstate + ( state - S_CHAIN1 ) );
		WeaponEvent( EV_FIRE_WEAPON, action | ( !Refire() ? DOOM_FIRE_ACCURATE : 0 ) );
		break;

	case ACT_FireMissile:
		UseAmmo( 1 );
		WeaponEvent( EV_FIRE_WEAPON, action );
		break;

	case ACT_FirePlasma:
		UseAmmo( 1 );
		// one of the two flashes, at random in Doom: from the time here so
		// the client predicts the same one
		SetPsprite( PS_FLASH, info->flashstate + ( ( wps->commandTime >> 5 ) & 1 ) );
		WeaponEvent( EV_FIRE_WEAPON, action );
		break;

	case ACT_BFGsound:
		WeaponEvent( EV_DOOM_SOUND, sfx_bfg );
		break;

	case ACT_FireBFG:
		UseAmmo( DOOM_BFGCELLS );
		WeaponEvent( EV_FIRE_WEAPON, action );
		break;

	default:
		break;
	}
}

/*
=================
SetPsprite

P_SetPsprite: enter a state, run its action; zero-tic states are passed
through at once
=================
*/
static void SetPsprite( int position, int stnum ) {
	int		*st = position == PS_WEAPON ? &wps->doomWeaponState : &wps->doomFlashState;
	int		*tics = position == PS_WEAPON ? &wps->doomWeaponTics : &wps->doomFlashTics;
	int		guard = 0;

	do {
		const doom_state_t *state;

		if ( stnum <= S_NULL || stnum >= NUMSTATES ) {
			*st = S_NULL;
			*tics = 0;
			break;
		}
		state = &doom_states[stnum];
		*st = stnum;
		*tics = state->tics;
		// coordinate set (only the height matters here)
		if ( state->misc1 && position == PS_WEAPON ) {
			wps->doomWeaponY = state->misc2;
		}
		if ( state->action != ACT_NULL ) {
			Action( position, stnum );
			if ( *st == S_NULL ) {
				break;
			}
		}
		stnum = doom_states[*st].nextstate;
	} while ( !*tics && ++guard < 64 );
}

static void MovePsprite( int position ) {
	int		*st = position == PS_WEAPON ? &wps->doomWeaponState : &wps->doomFlashState;
	int		*tics = position == PS_WEAPON ? &wps->doomWeaponTics : &wps->doomFlashTics;

	// a -1 tic count never changes
	if ( *st == S_NULL || *tics == -1 ) {
		return;
	}
	if ( --*tics <= 0 ) {
		SetPsprite( position, doom_states[*st].nextstate );
	}
}

/*
=================
PM_DoomWeapon

Every Pmove: weapon selection, then the Doom tics this move covers
=================
*/
void PM_DoomWeapon( void ) {
	int		weapon = pm->cmd.weapon;

	wps = pm->ps;
	wevents = qtrue;

	if ( wps->persistant[PERS_TEAM] == TEAM_SPECTATOR || wps->pm_type == PM_SPECTATOR ) {
		return;
	}

	// a new selection of a weapon the player owns (P_PlayerThink); the
	// game and the empty-weapon rule may change weapons too, so only
	// changes of the command count
	if ( weapon != wps->doomCmdWeapon ) {
		wps->doomCmdWeapon = weapon;
		if ( weapon > 0 && weapon <= DWP_NUMWEAPONS && ( wps->stats[STAT_WEAPONS] & ( 1 << weapon ) )
			&& wps->stats[STAT_HEALTH] > 0 ) {
			wps->doomPendingWeapon = weapon == wps->weapon ? 0 : weapon;
		}
	}

	// no attack until all buttons are up after a respawn
	wattack = ( pm->cmd.buttons & BUTTON_ATTACK ) && !( wps->pm_flags & PMF_RESPAWNED ) && wps->stats[STAT_HEALTH] > 0;

	// a living player always holds a weapon
	if ( wps->doomWeaponState == S_NULL && wps->stats[STAT_HEALTH] > 0 && wps->weapon ) {
		BringUpWeapon();
	}

	wps->doomTicFrac += pml.msec * DOOM_TICRATE;
	while ( wps->doomTicFrac >= 1000 ) {
		wps->doomTicFrac -= 1000;
		MovePsprite( PS_WEAPON );
		MovePsprite( PS_FLASH );
	}
}
