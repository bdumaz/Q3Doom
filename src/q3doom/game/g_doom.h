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
} doomLevel_t;

extern doomLevel_t	doomLevel;

#define DOOM_LEVEL()	( doomLevel.map != NULL )

// g_doom.c
void		Doom_InitLevel( void );		// before the entities spawn
void		Doom_StartLevel( void );	// after
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
