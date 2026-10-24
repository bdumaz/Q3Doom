/*
===========================================================================
Q3Doom - Doom map logic in the game module

Doom levels are played on Quake 3 systems: moving floors and ceilings are
Quake 3 movers (func_doom_plane, built by libdoom), line specials are
triggered by walking over, using or shooting lines, and the Doom thinkers
(p_doors.c, p_plats.c, p_floor.c, p_ceilng.c) drive the movers.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#ifndef G_DOOM_H
#define G_DOOM_H

#include "../../libdoom/doom.h"
#include "bg_doom.h"

#define DOOM_TICRATE		35

#ifndef MIN
#define MIN( a, b )			( (a) < (b) ? (a) : (b) )
#define MAX( a, b )			( (a) > (b) ? (a) : (b) )
#endif
#define DOOM_TICS_MSEC( t )	( (t) * 1000 / DOOM_TICRATE )
#define DOOM_TICSPEED( u )	( (u) * DOOM_TICRATE )		// units per tic -> units per second

// Doom speeds (units per tic) and waits (tics)
#define VDOORSPEED		2
#define VDOORWAIT		150
#define PLATSPEED		1
#define PLATWAIT		3			// seconds
#define FLOORSPEED		1
#define CEILSPEED		1
#define BUTTONTIME		35
#define USERANGE		64

// a moving floor or ceiling: the state of the Doom thinker acting on it
typedef struct doomPlane_s {
	int			kind;			// DOOM_KIND_*, DOOM_KIND_NONE when idle
	int			type;			// DOOM_DOOR_*, DOOM_PLAT_*, ...
	int			direction;		// 1 up, -1 down, 0 waiting, 2 initial wait
	float		speed;			// units per tic
	float		low, high;		// destinations
	int			waitTics;		// wait at the end of a move
	int			resumeTime;		// level.time when a wait ends
	qboolean	crush;
	qboolean	inStasis;		// stopped perpetual plat or crusher
	int			oldDirection;	// direction before stasis
	int			tag;			// line tag that started it (stop specials)
	char		texture[DOOM_LUMP_NAME_LEN + 1];	// flat after a move ("AndChange")
	int			newSpecial;		// sector special after a move, -1 unchanged
	int			lastCrushTime;
	qboolean	reachPending;	// a move of zero distance: finish it on the next think
} doomPlane_t;

// a Doom thing that acts (mobj_t): monsters and barrels. Its position is the
// entity's r.currentOrigin, at its feet; its health is the entity's.
typedef struct doomMobj_s {
	int					type;			// mobjtype_t
	const doom_mobjinfo_t	*info;
	int					flags;			// MF_*
	int					state;
	int					tics;
	float				angle;			// degrees
	vec3_t				mom;			// units per tic
	float				radius, height;
	float				floorz, ceilingz;
	gentity_t			*target;		// what it attacks or chases
	int					threshold;		// tics before it may change target
	int					movedir;		// 0-7 (east, counter-clockwise), 8 none
	int					movecount;		// steps before a new direction
	int					reactiontime;	// tics before it may attack
	int					lastlook;
	float				spawnX, spawnY, spawnAngle;	// nightmare respawn
	int					spawnFlags;		// map thing flags
} doomMobj_t;

typedef struct {
	const doom_map_t	*map;
	const doom_wad_t	*wad;
	doom_plane_range_t	*ranges;

	int					numSectors;
	gentity_t			**floorMover;	// per sector, NULL if the floor never moves
	gentity_t			**ceilingMover;
	float				*floor;			// current heights, updated every frame
	float				*ceiling;
	int					*floorInt;		// same, rounded, for libdoom searches
	int					*ceilingInt;
	int					*special;		// current sector specials (secrets get cleared)
	char				(*floorFlat)[DOOM_LUMP_NAME_LEN + 1];	// current floor flat of moving floors
	int					*light;			// current base light levels
	qboolean			*lightChanged;	// sent in CS_DOOM_LIGHTS

	int					*lineSpecial;	// current specials (once-only ones get cleared)
	int					*switchUntil;	// level.time when a repeatable switch turns back

	int					lastTic;		// Doom tics run so far
	int					exitTime;		// level.time to change map, 0 none
	char				nextMap[16];
	int					secretsFound;
	int					kills, totalKills;
	int					items, totalItems;
	int					totalSecrets;

	// campaign (g_doom_campaign.c)
	qboolean			campaign;		// a campaign level: progress is saved
	qboolean			intermission;	// the level is over: players wait on Doom's screens
	qboolean			finale;			// the episode is over
	int					exitTics;		// the level time, for the tally
	qboolean			restarting;		// the player died, the level reloads

	int					weaponSet;		// DOOM_WEAPONS_*: DoomFrag's or the campaign's option
	qboolean			deathmatch;		// g_doomDeathmatch, outside the campaign and DoomFrag
} doomLevel_t;

extern doomLevel_t	doomLevel;

#define DOOM_LEVEL()	( doomLevel.map != NULL )

// g_doom.c
void		Doom_InitLevel( void );		// before the entities spawn
void		Doom_StartLevel( void );	// after
const char	*Doom_LevelMusic( void );
void		Doom_ShutdownLevel( void );
void		Doom_StatusCommand( void );
void		Doom_RunFrame( void );
void		Doom_PlayerMoved( gentity_t *ent, const vec3_t oldOrigin );
void		Doom_PlayerUse( gentity_t *ent );
void		Doom_ShotTrace( gentity_t *attacker, const vec3_t start, const vec3_t end );
qboolean	Doom_PlayerHasKey( gentity_t *ent, int key );
void		Doom_KeyMessage( gentity_t *ent, int key );
qboolean	Doom_ActivateLine( gentity_t *activator, int line, int side, doom_trigger_t trigger );
void		Doom_ChangeSwitchTexture( int line, qboolean useAgain );
void		Doom_SetFlat( int sector, const char *flat );
void		Doom_PlaySectorSound( int sector, const char *sound );

void		Doom_Message( gentity_t *ent, const char *message );

qboolean	Doom_TeleportMobj( gentity_t *ent, int tag );
void		Doom_PlayerBump( gentity_t *ent );
void		Doom_ShootSwitch( gentity_t *attacker, const vec3_t start, const vec3_t end );

// g_doom_frag.c
void		Doom_InitFrag( void );
qboolean	Doom_FragActive( void );
qboolean	Doom_FragWaiting( void );
void		Doom_FragInput( gentity_t *ent, const usercmd_t *cmd );
void		Doom_FragUpdate( void );
qboolean	Doom_FragExit( gentity_t *activator );
qboolean	Doom_FragCommand( gentity_t *ent, const char *cmd );

// g_doom_campaign.c
void		Doom_InitCampaign( void );
qboolean	Doom_ExitLevel( gentity_t *activator, qboolean secret );
void		Doom_CampaignFrame( void );
qboolean	Doom_CampaignCommand( gentity_t *ent, const char *cmd );
qboolean	Doom_PlayerDied( gentity_t *ent );
void		Doom_ApplyCarry( gclient_t *client );

// g_doom_weapon.c
int			Doom_Random( void );
qboolean	Doom_IsTarget( gentity_t *ent );
qboolean	Doom_Bleeds( gentity_t *ent );
gentity_t	*Doom_AimLineAttack( gentity_t *shooter, const vec3_t eye, float yaw, float range, float *pitch );
gentity_t	*Doom_LineAttack( gentity_t *ent, const vec3_t eye, float yaw, float pitch, float range, int damage, int mod );
gentity_t	*Doom_LaunchMissile( gentity_t *owner, const vec3_t start, const vec3_t velocity, int type );
void		Doom_SpawnFx( const vec3_t origin, int state, int flags );
void		Doom_Sound( gentity_t *ent, int sfx );

// g_doom_mobj.c
void		Doom_InitMobjs( void );
void		Doom_SpawnMobjs( void );
void		Doom_RunMobjs( void );		// one Doom tic
gentity_t	*Doom_SpawnMobj( int type, float x, float y, float angle );
void		Doom_DamageMobj( gentity_t *target, gentity_t *inflictor, gentity_t *source, vec3_t dir, int damage, int dflags );
void		Doom_NoiseAlert( gentity_t *emitter );
float		Doom_ThingZ( gentity_t *ent );
float		Doom_ThingHeight( gentity_t *ent );
void		Doom_SpawnCommand( gentity_t *ent );
void		Doom_KillAllCommand( gentity_t *ent );
void		Doom_FireWeapon( gentity_t *ent, int parm );
void		Doom_MissileImpact( gentity_t *ent, trace_t *trace );

// g_doom_items.c
void		Doom_SpawnItems( void );
gentity_t	*Doom_SpawnItemAt( int itemNum, float x, float y, qboolean dropped );
void		Doom_UpdateItems( void );
int			Doom_TouchItem( gentity_t *ent, gentity_t *other );
void		Doom_GivePlayerStart( gclient_t *client );
void		Doom_GiveAll( gentity_t *ent, qboolean keys );

// g_doom_mover.c
void		SP_func_doom_plane( gentity_t *ent );
void		Doom_UpdateHeights( void );
qboolean	Doom_DoDoor( int sector, int type, int key );
void		Doom_ManualDoor( gentity_t *activator, int line, qboolean proximity );
qboolean	Doom_DoPlat( int sector, int type, int amount, int line );
qboolean	Doom_DoFloor( int sector, int type, int line );
qboolean	Doom_DoCeiling( int sector, int type, int line );
qboolean	Doom_BuildStairs( int sector, int type );
qboolean	Doom_DoDonut( int sector );
qboolean	Doom_StopMovers( int tag, int kind );
qboolean	Doom_ActivateInStasis( int tag, int kind );
void		Doom_SpawnSectorSpecials( void );
void		Doom_SpawnDoorTriggers( void );

#endif
