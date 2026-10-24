/*
===========================================================================
Q3Doom - Doom items (p_inter.c, g_game.c)

Map things that are items become Quake 3 item entities (bg_itemlist holds
the Doom items), drawn by the cgame with their Doom sprites. Touching one
applies P_TouchSpecialThing: the same limits, ammo amounts, weapon
switches and messages as Doom. Items do not respawn (single player and
cooperative rules; deathmatch comes with M7).

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

int Pickup_Powerup( gentity_t *ent, gentity_t *other );		// g_items.c

/*
=====================================================================

GIVING (P_GiveAmmo, P_GiveWeapon, P_GiveBody, P_GiveArmor, P_GivePower)

=====================================================================
*/

/*
=================
GiveAmmo

'num' is a number of clips, 0 for half a clip (dropped by a monster).
An empty player switches to a better weapon like Doom does.
=================
*/
static qboolean GiveAmmo( playerState_t *ps, int ammo, int num ) {
	int		old, max, ready = DOOM_DWP( ps->weapon );

	if ( ammo < 0 || ammo >= DAM_NUMAMMO ) {
		return qfalse;
	}
	max = BG_DoomMaxAmmo( ps, ammo );
	if ( ps->ammo[ammo] >= max ) {
		return qfalse;
	}
	num = num ? num * bg_doomClipAmmo[ammo] : bg_doomClipAmmo[ammo] / 2;
	// extra ammo in baby mode and nightmare
	if ( g_doomSkill.integer == DOOM_SK_BABY || g_doomSkill.integer == DOOM_SK_NIGHTMARE ) {
		num <<= 1;
	}
	old = ps->ammo[ammo];
	ps->ammo[ammo] += num;
	if ( ps->ammo[ammo] > max ) {
		ps->ammo[ammo] = max;
	}
	// only switch when the player was out of this ammo
	if ( old ) {
		return qtrue;
	}
	switch ( ammo ) {
	case DAM_CLIP:
		if ( ready == DWP_FIST ) {
			ps->doomPendingWeapon = DOOM_WP( DOOM_OWNS( ps, DWP_CHAINGUN ) ? DWP_CHAINGUN : DWP_PISTOL );
		}
		break;
	case DAM_SHELL:
		if ( ( ready == DWP_FIST || ready == DWP_PISTOL ) && DOOM_OWNS( ps, DWP_SHOTGUN ) ) {
			ps->doomPendingWeapon = DOOM_WP( DWP_SHOTGUN );
		}
		break;
	case DAM_CELL:
		if ( ( ready == DWP_FIST || ready == DWP_PISTOL ) && DOOM_OWNS( ps, DWP_PLASMA ) ) {
			ps->doomPendingWeapon = DOOM_WP( DWP_PLASMA );
		}
		break;
	case DAM_MISL:
		if ( ready == DWP_FIST && DOOM_OWNS( ps, DWP_MISSILE ) ) {
			ps->doomPendingWeapon = DOOM_WP( DWP_MISSILE );
		}
		break;
	}
	return qtrue;
}

// a dropped weapon has one clip, a placed one two; a new weapon is selected
static qboolean GiveWeapon( playerState_t *ps, int weapon, qboolean dropped ) {
	int			ammo = bg_doomWeapons[weapon].ammo;
	qboolean	gaveAmmo = qfalse, gaveWeapon = qfalse;

	if ( ammo != DAM_NOAMMO ) {
		gaveAmmo = GiveAmmo( ps, ammo, dropped ? 1 : 2 );
	}
	if ( !DOOM_OWNS( ps, weapon ) ) {
		gaveWeapon = qtrue;
		ps->stats[STAT_WEAPONS] |= 1 << DOOM_WP( weapon );
		ps->doomPendingWeapon = DOOM_WP( weapon );
	}
	return gaveWeapon || gaveAmmo;
}

static qboolean GiveBody( gentity_t *ent, int num, int max ) {
	if ( ent->health >= max ) {
		return qfalse;
	}
	ent->health += num;
	if ( ent->health > max ) {
		ent->health = max;
	}
	ent->client->ps.stats[STAT_HEALTH] = ent->health;
	return qtrue;
}

static qboolean GivePower( gentity_t *ent, int power ) {
	playerState_t	*ps = &ent->client->ps;
	int				now = level.time > 0 ? level.time : 1;

	switch ( power ) {
	case DPW_INVULNERABILITY:
		ps->powerups[power] = now + DOOM_TICS_MSEC( DOOM_INVULNTICS );
		return qtrue;
	case DPW_INVISIBILITY:
		ps->powerups[power] = now + DOOM_TICS_MSEC( DOOM_INVISTICS );
		return qtrue;
	case DPW_INFRARED:
		ps->powerups[power] = now + DOOM_TICS_MSEC( DOOM_INFRATICS );
		return qtrue;
	case DPW_IRONFEET:
		ps->powerups[power] = now + DOOM_TICS_MSEC( DOOM_IRONTICS );
		return qtrue;
	case DPW_STRENGTH:
		// full health, and berserk until death (the red fades from the pickup time)
		GiveBody( ent, 100, 100 );
		ps->powerups[power] = now;
		return qtrue;
	default:
		if ( ps->powerups[power] ) {
			return qfalse;
		}
		ps->powerups[power] = 1;
		return qtrue;
	}
}

/*
=================
GiveQ3

The Quake 3 weapon set: a Doom weapon gives its Quake 3 counterpart
(bg_doomQ3Weapon) with that weapon's ammo, and Doom ammo gives the ammo of
the Quake 3 weapons that took the place of the Doom ones using it:
	clip, box of bullets	machinegun bullets, railgun slugs (the chaingun)
	shells					shotgun shells
	rocket, box of rockets	rockets
	cell, cell pack			plasma cells, lightning ammo, BFG ammo
A backpack gives one clip of each.
=================
*/
static void AddQ3Ammo( playerState_t *ps, int weapon, int count ) {
	ps->ammo[weapon] += count;
	if ( ps->ammo[weapon] > 200 ) {
		ps->ammo[weapon] = 200;
	}
}

static qboolean GiveQ3( playerState_t *ps, const gitem_t *item ) {
	int		clips = item->quantity > 0 ? item->quantity : 1, ammo = item->giTag, i;

	if ( item->giType == IT_WEAPON ) {
		int w = bg_doomQ3Weapon[item->giTag];

		ps->stats[STAT_WEAPONS] |= 1 << w;
		if ( w == WP_GAUNTLET ) {
			ps->ammo[w] = -1;
		} else {
			AddQ3Ammo( ps, w, BG_FindItemForWeapon( w )->quantity );
		}
		return qtrue;
	}
	for ( i = 0 ; i < DAM_NUMAMMO ; i++ ) {
		// a backpack gives a clip of each
		if ( ammo != DAM_NUMAMMO && ammo != i ) {
			continue;
		}
		switch ( i ) {
		case DAM_CLIP:
			AddQ3Ammo( ps, WP_MACHINEGUN, 10 * clips );
			AddQ3Ammo( ps, WP_RAILGUN, 2 * clips );
			break;
		case DAM_SHELL:
			AddQ3Ammo( ps, WP_SHOTGUN, 4 * clips );
			break;
		case DAM_MISL:
			AddQ3Ammo( ps, WP_ROCKET_LAUNCHER, clips );
			break;
		case DAM_CELL:
			AddQ3Ammo( ps, WP_PLASMAGUN, 20 * clips );
			AddQ3Ammo( ps, WP_LIGHTNING, 20 * clips );
			AddQ3Ammo( ps, WP_BFG, clips );
			break;
		}
	}
	return qtrue;
}

/*
=================
Doom_TouchItem

P_TouchSpecialThing once BG_CanItemBeGrabbed allowed it: gives the item,
returns qfalse if the player could not take it after all
=================
*/
int Doom_TouchItem( gentity_t *ent, gentity_t *other ) {
	playerState_t	*ps = &other->client->ps;
	gitem_t			*item = ent->item;
	int				i;

	if ( DOOM_WEAPONSET( ps ) == DOOM_WEAPONS_Q3 && ( item->giType == IT_WEAPON || item->giType == IT_AMMO ) ) {
		return GiveQ3( ps, item );
	}
	if ( DOOM_WEAPONSET( ps ) == DOOM_WEAPONS_NONE && ( item->giType == IT_WEAPON || item->giType == IT_AMMO ) ) {
		return qfalse;
	}

	switch ( item->giType ) {
	case IT_WEAPON:
		return GiveWeapon( ps, item->giTag, ent->doomDropped );

	case IT_AMMO:
		if ( item->giTag == DAM_NUMAMMO ) {
			// backpack: double limits once, and a clip of each ammo
			ps->stats[STAT_DOOM_BACKPACK] = 1;
			for ( i = 0 ; i < DAM_NUMAMMO ; i++ ) {
				GiveAmmo( ps, i, 1 );
			}
			return qtrue;
		}
		return GiveAmmo( ps, item->giTag, ent->doomDropped ? 0 : item->quantity );

	case IT_HEALTH:
		if ( !item->giTag ) {
			return GiveBody( other, item->quantity, 100 );
		}
		// health bonus and soulsphere: always taken, up to 200
		other->health += item->quantity;
		if ( other->health > 200 ) {
			other->health = 200;
		}
		ps->stats[STAT_HEALTH] = other->health;
		return qtrue;

	case IT_ARMOR:
		if ( !item->giTag ) {
			// armor bonus
			if ( ++ps->stats[STAT_ARMOR] > 200 ) {
				ps->stats[STAT_ARMOR] = 200;
			}
			if ( !ps->stats[STAT_DOOM_ARMORTYPE] ) {
				ps->stats[STAT_DOOM_ARMORTYPE] = 1;
			}
			return qtrue;
		}
		if ( ps->stats[STAT_ARMOR] >= item->quantity ) {
			return qfalse;
		}
		ps->stats[STAT_DOOM_ARMORTYPE] = item->giTag;
		ps->stats[STAT_ARMOR] = item->quantity;
		return qtrue;

	case IT_KEY:
		ps->stats[STAT_DOOM_KEYS] |= 1 << item->giTag;
		return qtrue;

	case IT_POWERUP:
		// Quake 3's quad (Quake 3 maps): Quake 3's timer
		if ( !item->doomednum ) {
			return Pickup_Powerup( ent, other ) != 0;
		}
		if ( !GivePower( other, item->giTag ) ) {
			return qfalse;
		}
		if ( item->giTag == DPW_STRENGTH && ps->weapon != DOOM_WP( DWP_FIST ) ) {
			ps->doomPendingWeapon = DOOM_WP( DWP_FIST );
		}
		return qtrue;

	default:
		return qfalse;
	}
}

/*
=================
Doom_GivePlayerStart

G_PlayerReborn: 100 health, the fist and the pistol with 50 bullets
=================
*/
void Doom_GivePlayerStart( gclient_t *client ) {
	playerState_t	*ps = &client->ps;
	int				set = doomLevel.weaponSet, i;

	// the weapon set and DoomFrag's physics, in the player state for the prediction
	ps->stats[STAT_DOOM_RULES] = set | ( g_doomFrag.integer && g_doomPhysics.integer ? DOOMRULES_CPM : 0 );

	memset( ps->ammo, 0, sizeof( ps->ammo ) );
	ps->stats[STAT_ARMOR] = 0;
	ps->stats[STAT_DOOM_ARMORTYPE] = 0;
	ps->stats[STAT_DOOM_KEYS] = 0;
	ps->stats[STAT_DOOM_BACKPACK] = 0;
	ps->stats[STAT_HEALTH] = 100;
	memset( ps->powerups, 0, sizeof( ps->powerups ) );
	ps->doomPendingWeapon = 0;
	ps->doomWeaponFlags = 0;
	ps->doomTicFrac = 0;
	ps->doomWeaponState = S_NULL;
	ps->doomFlashState = S_NULL;
	ps->doomCmdWeapon = client->pers.cmd.weapon;

	switch ( set ) {
	case DOOM_WEAPONS_Q3:
		// Quake 3's start: the gauntlet and the machinegun, or everything
		ps->stats[STAT_WEAPONS] = ( 1 << WP_GAUNTLET ) | ( 1 << WP_MACHINEGUN );
		ps->ammo[WP_MACHINEGUN] = 100;
		if ( g_doomFrag.integer && g_doomLoadout.integer ) {
			for ( i = WP_MACHINEGUN ; i <= WP_BFG ; i++ ) {
				ps->stats[STAT_WEAPONS] |= 1 << i;
				ps->ammo[i] = 200;
			}
		}
		ps->ammo[WP_GAUNTLET] = -1;
		ps->weapon = WP_MACHINEGUN;
		ps->weaponstate = WEAPON_READY;
		return;

	case DOOM_WEAPONS_NONE:
		ps->stats[STAT_WEAPONS] = 0;
		ps->weapon = WP_NONE;
		return;

	default:
		// G_PlayerReborn: 100 health, the fist and the pistol with 50 bullets
		ps->stats[STAT_WEAPONS] = ( 1 << DOOM_WP( DWP_FIST ) ) | ( 1 << DOOM_WP( DWP_PISTOL ) );
		ps->ammo[DAM_CLIP] = 50;
		if ( g_doomFrag.integer && g_doomLoadout.integer ) {
			for ( i = 0 ; i < DWP_NUMWEAPONS ; i++ ) {
				if ( i != DWP_SUPERSHOTGUN ) {
					ps->stats[STAT_WEAPONS] |= 1 << DOOM_WP( i );
				}
			}
			ps->stats[STAT_DOOM_BACKPACK] = 1;
			for ( i = 0 ; i < DAM_NUMAMMO ; i++ ) {
				ps->ammo[i] = BG_DoomMaxAmmo( ps, i );
			}
		}
		ps->weapon = DOOM_WP( DWP_PISTOL );
		BG_DoomBringUpWeapon( ps );
		return;
	}
}

/*
=================
Doom_GiveAll

"give all" and the idfa/idkfa cheats: every weapon, a backpack full of
ammo, blue armor; and the keys with idkfa
=================
*/
void Doom_GiveAll( gentity_t *ent, qboolean keys ) {
	playerState_t	*ps = &ent->client->ps;
	int				i;

	for ( i = 0 ; i < DWP_NUMWEAPONS ; i++ ) {
		if ( i != DWP_SUPERSHOTGUN ) {
			ps->stats[STAT_WEAPONS] |= 1 << DOOM_WP( i );
		}
	}
	ps->stats[STAT_DOOM_BACKPACK] = 1;
	for ( i = 0 ; i < DAM_NUMAMMO ; i++ ) {
		ps->ammo[i] = BG_DoomMaxAmmo( ps, i );
	}
	ps->stats[STAT_ARMOR] = 200;
	ps->stats[STAT_DOOM_ARMORTYPE] = 2;
	if ( keys ) {
		ps->stats[STAT_DOOM_KEYS] = ( 1 << ( DOOM_IT_REDSKULL + 1 ) ) - 1;
	}
}

/*
=====================================================================

ITEM ENTITIES

=====================================================================
*/

/*
=================
Doom_SpawnItems

The item things of the map on this skill, standing on their sector floor
=================
*/
void Doom_SpawnItems( void ) {
	const doom_map_t	*map = doomLevel.map;
	int					i, count = 0;

	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t	*t = &map->things[i];
		int					itemNum;
		gentity_t			*ent;

		if ( !BG_DoomThingOnSkill( t, g_doomSkill.integer ) ) {
			continue;
		}
		itemNum = BG_DoomItemForThing( t->type );
		if ( !itemNum ) {
			continue;
		}
		// DoomFrag without weapons: no weapons and ammo to pick up
		if ( doomLevel.weaponSet == DOOM_WEAPONS_NONE && ( bg_itemlist[itemNum].giType == IT_WEAPON || bg_itemlist[itemNum].giType == IT_AMMO ) ) {
			continue;
		}
		ent = Doom_SpawnItemAt( itemNum, t->x + 0.25f, t->y + 0.25f, qfalse );
		if ( !ent ) {
			continue;
		}
		ent->s.angles[YAW] = t->angle;
		if ( doom_mobjinfo[doom_mobjtype_for_ednum( t->type )].flags & MF_COUNTITEM ) {
			doomLevel.totalItems++;
		}
		count++;
	}
	G_Printf( "Doom items: %d\n", count );
}

/*
=================
Doom_SpawnItemAt

An item standing on the floor at x, y; 'dropped' by a monster (MF_DROPPED:
half a clip, one clip with a weapon)
=================
*/
gentity_t *Doom_SpawnItemAt( int itemNum, float x, float y, qboolean dropped ) {
	int			sector = doom_map_sector_at( doomLevel.map, x, y );
	gentity_t	*ent;
	vec3_t		origin;

	if ( itemNum <= 0 || itemNum >= bg_numItems || sector < 0 ) {
		return NULL;
	}
	ent = G_Spawn();
	ent->classname = bg_itemlist[itemNum].classname;
	ent->item = &bg_itemlist[itemNum];
	RegisterItem( ent->item );
	ent->s.eType = ET_ITEM;
	ent->s.modelindex = itemNum;
	ent->s.modelindex2 = dropped;
	ent->doomDropped = dropped;
	ent->doomSector = sector;
	// only for finding it: BG_PlayerTouchesItem decides
	VectorSet( ent->r.mins, -21, -21, 0 );
	VectorSet( ent->r.maxs, 21, 21, 8 );
	ent->r.contents = CONTENTS_TRIGGER;
	ent->touch = Touch_Item;
	VectorSet( origin, x, y, doomLevel.floor[sector] );
	G_SetOrigin( ent, origin );
	trap_LinkEntity( ent );
	return ent;
}

/*
=================
Doom_UpdateItems

Items on moving floors ride them (P_ThingHeightClip)
=================
*/
void Doom_UpdateItems( void ) {
	int		i;

	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		gentity_t	*ent = &g_entities[i];
		vec3_t		origin;

		if ( !ent->inuse || ent->s.eType != ET_ITEM || !ent->item || ent->doomSector < 0 || ent->doomSector >= doomLevel.numSectors
			|| !doomLevel.floorMover[ent->doomSector] ) {
			continue;
		}
		if ( ent->r.currentOrigin[2] == doomLevel.floor[ent->doomSector] ) {
			continue;
		}
		VectorCopy( ent->r.currentOrigin, origin );
		origin[2] = doomLevel.floor[ent->doomSector];
		G_SetOrigin( ent, origin );
		trap_LinkEntity( ent );
	}
}
