/*
===========================================================================
Q3Doom - Doom rules shared by the game and the cgame

Both sides must agree exactly (prediction), so these only depend on the
map, the skill, the player state and the Doom tables. Include after
bg_public.h.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#ifndef BG_DOOM_H
#define BG_DOOM_H

#include "../../libdoom/doom.h"
#include "../../libdoom/doom_info.h"

#define DOOM_DEFAULT_SKILL	"2"		// "Hurt me plenty"
#define DOOM_TICRATE		35

// skills (skill_t)
enum { DOOM_SK_BABY, DOOM_SK_EASY, DOOM_SK_MEDIUM, DOOM_SK_HARD, DOOM_SK_NIGHTMARE };

/*
=====================================================================

WEAPONS AND AMMO (d_items.c, p_pspr.c)

ps->weapon and usercmd weapons are the Doom weapon + 1 (0 = none), and
STAT_WEAPONS has bit 1 << ps->weapon for each weapon owned. ps->ammo[] is
indexed by the Doom ammo type.

=====================================================================
*/

typedef enum {
	DWP_FIST,
	DWP_PISTOL,
	DWP_SHOTGUN,
	DWP_CHAINGUN,
	DWP_MISSILE,
	DWP_PLASMA,
	DWP_BFG,
	DWP_CHAINSAW,
	DWP_SUPERSHOTGUN,		// Doom II only, never given here
	DWP_NUMWEAPONS
} doomWeapon_t;

#define DOOM_WP( dwp )			( (dwp) + 1 )		// Doom weapon -> ps->weapon
#define DOOM_DWP( wp )			( (wp) - 1 )		// ps->weapon -> Doom weapon
#define DOOM_OWNS( ps, dwp )	( ( (ps)->stats[STAT_WEAPONS] & ( 1 << DOOM_WP( dwp ) ) ) != 0 )

typedef enum {
	DAM_CLIP,
	DAM_SHELL,
	DAM_CELL,
	DAM_MISL,
	DAM_NUMAMMO,
	DAM_NOAMMO
} doomAmmo_t;

typedef struct {
	int		ammo;
	int		upstate, downstate, readystate, atkstate, flashstate;
} doomWeaponInfo_t;

extern const doomWeaponInfo_t	bg_doomWeapons[DWP_NUMWEAPONS];
extern const int				bg_doomMaxAmmo[DAM_NUMAMMO];	// doubled by a backpack
extern const int				bg_doomClipAmmo[DAM_NUMAMMO];

#define DOOM_WEAPONTOP		32
#define DOOM_WEAPONBOTTOM	128
#define DOOM_BFGCELLS		40
#define DOOM_MELEERANGE		64
#define DOOM_MISSILERANGE	2048		// 32*64

// ps->doomWeaponFlags
#define DOOMWF_REFIRE		0xff		// times the trigger was held through a refire
#define DOOMWF_ATTACKDOWN	0x100
#define DOOMWF_LIGHTSHIFT	9			// extra light (0-2) from the gun flash
#define DOOMWF_LIGHTMASK	( 3 << DOOMWF_LIGHTSHIFT )

// EV_FIRE_WEAPON eventParm: the Doom action (ACT_*) | DOOM_FIRE_ACCURATE
#define DOOM_FIRE_ACCURATE	0x80

// EV_DOOM_SOUND eventParm: the sound (sfx_*) | DOOM_SOUND_FULL for full
// volume everywhere (the bosses' sight and death sounds)
#define DOOM_SOUND_FULL		0x80

int			BG_DoomMaxAmmo( const playerState_t *ps, int ammo );
// the weapon P_CheckAmmo switches to when the current one is empty
int			BG_DoomBestWeapon( const playerState_t *ps );
// starts raising ps->doomPendingWeapon (or the current weapon) from the bottom
void		BG_DoomBringUpWeapon( playerState_t *ps );

/*
=====================================================================

ITEMS AND KEYS

=====================================================================
*/

// cards[] (it_bluecard ...): bits of STAT_DOOM_KEYS
enum { DOOM_IT_BLUECARD, DOOM_IT_YELLOWCARD, DOOM_IT_REDCARD, DOOM_IT_BLUESKULL, DOOM_IT_YELLOWSKULL, DOOM_IT_REDSKULL };

// Doom powers (powertype_t), offsets of DPW_INVULNERABILITY in ps->powerups
#define DOOM_INVULNTICS		( 30 * DOOM_TICRATE )
#define DOOM_INVISTICS		( 60 * DOOM_TICRATE )
#define DOOM_INFRATICS		( 120 * DOOM_TICRATE )
#define DOOM_IRONTICS		( 60 * DOOM_TICRATE )

// the bg_itemlist entry for a map thing number, 0 if it is not an item
int			BG_DoomItemForThing( int doomednum );

/*
=====================================================================

MAP THINGS

=====================================================================
*/

// a map thing that is a static decoration in this milestone: its mobj
// type, or -1 if it is not spawned that way (wrong skill, multiplayer only,
// player starts, monsters, items, invisible markers)
int			BG_DoomStaticThing( const doom_thing_t *t, int skill );
// is the thing on this skill in single player
qboolean	BG_DoomThingOnSkill( const doom_thing_t *t, int skill );

// a static thing that blocks movement: fills its box and returns qtrue
qboolean	BG_DoomThingBlocks( const doom_map_t *map, const doom_thing_t *t, int type, vec3_t mins, vec3_t maxs );

#endif
