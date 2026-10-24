/*
===========================================================================
Q3Doom - Doom rules shared by the game and the cgame

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "q_shared.h"
#include "bg_public.h"
#include "bg_doom.h"

/*
=====================================================================

WEAPONS AND AMMO

=====================================================================
*/

// weaponinfo[] (d_items.c)
const doomWeaponInfo_t bg_doomWeapons[DWP_NUMWEAPONS] = {
	{ DAM_NOAMMO,	S_PUNCHUP,		S_PUNCHDOWN,	S_PUNCH,	S_PUNCH1,	S_NULL },
	{ DAM_CLIP,		S_PISTOLUP,		S_PISTOLDOWN,	S_PISTOL,	S_PISTOL1,	S_PISTOLFLASH },
	{ DAM_SHELL,	S_SGUNUP,		S_SGUNDOWN,		S_SGUN,		S_SGUN1,	S_SGUNFLASH1 },
	{ DAM_CLIP,		S_CHAINUP,		S_CHAINDOWN,	S_CHAIN,	S_CHAIN1,	S_CHAINFLASH1 },
	{ DAM_MISL,		S_MISSILEUP,	S_MISSILEDOWN,	S_MISSILE,	S_MISSILE1,	S_MISSILEFLASH1 },
	{ DAM_CELL,		S_PLASMAUP,		S_PLASMADOWN,	S_PLASMA,	S_PLASMA1,	S_PLASMAFLASH1 },
	{ DAM_CELL,		S_BFGUP,		S_BFGDOWN,		S_BFG,		S_BFG1,		S_BFGFLASH1 },
	{ DAM_NOAMMO,	S_SAWUP,		S_SAWDOWN,		S_SAW,		S_SAW1,		S_NULL },
	{ DAM_SHELL,	S_DSGUNUP,		S_DSGUNDOWN,	S_DSGUN,	S_DSGUN1,	S_DSGUNFLASH1 }
};

// maxammo[] and clipammo[] (p_inter.c)
const int bg_doomMaxAmmo[DAM_NUMAMMO] = { 200, 50, 300, 50 };
const int bg_doomClipAmmo[DAM_NUMAMMO] = { 10, 4, 20, 1 };

int BG_DoomMaxAmmo( const playerState_t *ps, int ammo ) {
	if ( ammo < 0 || ammo >= DAM_NUMAMMO ) {
		return 0;
	}
	return ps->stats[STAT_DOOM_BACKPACK] ? bg_doomMaxAmmo[ammo] * 2 : bg_doomMaxAmmo[ammo];
}

/*
=================
BG_DoomBestWeapon

P_CheckAmmo's preferences when the current weapon is empty
(Ultimate Doom: no super shotgun)
=================
*/
int BG_DoomBestWeapon( const playerState_t *ps ) {
	if ( DOOM_OWNS( ps, DWP_PLASMA ) && ps->ammo[DAM_CELL] ) {
		return DWP_PLASMA;
	}
	if ( DOOM_OWNS( ps, DWP_CHAINGUN ) && ps->ammo[DAM_CLIP] ) {
		return DWP_CHAINGUN;
	}
	if ( DOOM_OWNS( ps, DWP_SHOTGUN ) && ps->ammo[DAM_SHELL] ) {
		return DWP_SHOTGUN;
	}
	if ( ps->ammo[DAM_CLIP] && DOOM_OWNS( ps, DWP_PISTOL ) ) {
		return DWP_PISTOL;
	}
	if ( DOOM_OWNS( ps, DWP_CHAINSAW ) ) {
		return DWP_CHAINSAW;
	}
	if ( DOOM_OWNS( ps, DWP_MISSILE ) && ps->ammo[DAM_MISL] ) {
		return DWP_MISSILE;
	}
	if ( DOOM_OWNS( ps, DWP_BFG ) && ps->ammo[DAM_CELL] > DOOM_BFGCELLS ) {
		return DWP_BFG;
	}
	return DWP_FIST;
}

/*
=====================================================================

ITEMS

=====================================================================
*/

int BG_DoomItemForThing( int doomednum ) {
	int		i;

	for ( i = 1 ; i < bg_numItems ; i++ ) {
		if ( bg_itemlist[i].doomednum == doomednum ) {
			return i;
		}
	}
	return 0;
}

/*
=====================================================================

MAP THINGS

=====================================================================
*/

qboolean BG_DoomThingOnSkill( const doom_thing_t *t, int skill ) {
	int		flags;

	// skill levels (P_SpawnMapThing): baby and easy, medium, hard and nightmare
	if ( skill <= DOOM_SK_EASY ) {
		flags = DOOM_MTF_EASY;
	} else if ( skill == DOOM_SK_MEDIUM ) {
		flags = DOOM_MTF_NORMAL;
	} else {
		flags = DOOM_MTF_HARD;
	}
	return ( t->flags & flags ) && !( t->flags & DOOM_MTF_NOTSINGLE );
}

int BG_DoomStaticThing( const doom_thing_t *t, int skill ) {
	int		type;

	if ( !BG_DoomThingOnSkill( t, skill ) ) {
		return -1;
	}
	type = doom_mobjtype_for_ednum( t->type );
	if ( type < 0 || type == MT_PLAYER ) {
		return -1;
	}
	// monsters come with M6, items are entities; invisible things
	// (teleport destinations) are markers
	if ( doom_mobjinfo[type].flags & ( MF_COUNTKILL | MF_SPECIAL | MF_NOSECTOR ) ) {
		return -1;
	}
	return type;
}

qboolean BG_DoomThingBlocks( const doom_map_t *map, const doom_thing_t *t, int type, vec3_t mins, vec3_t maxs ) {
	const doom_mobjinfo_t	*info = &doom_mobjinfo[type];
	int						sector;
	float					radius, height, z;

	// items are picked up, not walked around
	if ( !( info->flags & MF_SOLID ) || ( info->flags & MF_SPECIAL ) ) {
		return qfalse;
	}
	radius = (float)info->radius / DOOM_FRACUNIT;
	height = (float)info->height / DOOM_FRACUNIT;
	sector = doom_map_sector_at( map, t->x + 0.25, t->y + 0.25 );
	if ( sector < 0 ) {
		return qfalse;
	}
	z = ( info->flags & MF_SPAWNCEILING ) ? map->sectors[sector].ceiling_height - height : map->sectors[sector].floor_height;

	VectorSet( mins, t->x - radius, t->y - radius, z );
	VectorSet( maxs, t->x + radius, t->y + radius, z + height );
	return qtrue;
}
