/*
===========================================================================
Q3Doom - Doom map rendering in the cgame

- sector lights: Doom's light thinkers (p_lights.c) run here at 35 tics a
  second on the base levels (map, or CS_DOOM_LIGHTS once a line special
  changed them); every frame the levels go to the renderer, which lights
  the map surfaces by sector (CGEN_DOOM_SECTOR)
- static things (decorations, items until M5): sprites drawn as billboards
  facing the view like Doom's, with its 8 rotations, mirrored frames,
  offsets and animations (states)
- the solid ones are boxes the player prediction collides with, the same
  ones the server links (BG_DoomThingBlocks)

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "cg_local.h"
#include "../game/bg_doom.h"

#define MAX_DOOM_THINGS		2048
#define MAX_SPRITE_FRAMES	29
#define DOOM_TICRATE		35

typedef struct {
	short		lump;			// sprite lump, -1 none
	byte		flip;
} spriteRot_t;

typedef struct {
	int			type;			// mobj type
	vec3_t		origin;			// on the floor (or hanging from the ceiling)
	float		angle;			// degrees
	int			sector;
} doomThing_t;

typedef struct {
	int			level;			// current light
	int			min, max;
	int			count;			// tics to the next change
	int			direction;		// glow
	int			effect;			// the sector special that drives it
} lightThinker_t;

static struct {
	const doom_map_t	*map;
	const doom_wad_t	*wad;
	int					skill;

	// sprites: lump and mirroring per sprite, frame and rotation
	spriteRot_t			(*frames)[MAX_SPRITE_FRAMES][8];
	qhandle_t			*lumpShader;		// per WAD lump, registered on first use

	int					numThings;
	doomThing_t			things[MAX_DOOM_THINGS];

	int					numBlockers;
	vec3_t				blockMins[MAX_DOOM_THINGS], blockMaxs[MAX_DOOM_THINGS];

	// lights
	int					numSectors;
	int					*baseLight;
	lightThinker_t		*lights;
	byte				*levels;
	int					lastTic;
	unsigned			random;
} cgDoom;

#define DOOM_MAP()	( cgDoom.map != NULL )

static int DoomRandom( void ) {
	cgDoom.random = cgDoom.random * 1103515245 + 12345;
	return ( cgDoom.random >> 16 ) & 255;
}

/*
=====================================================================

SECTOR LIGHTS (p_lights.c)

=====================================================================
*/

#define GLOWSPEED		8
#define STROBEBRIGHT	5
#define FASTDARK		15
#define SLOWDARK		35

static void StartLight( int s ) {
	lightThinker_t	*l = &cgDoom.lights[s];
	int				base = cgDoom.baseLight[s];
	int				min = doom_find_min_surrounding_light( cgDoom.map, s, base, cgDoom.baseLight );

	l->level = l->max = base;
	l->min = min;
	l->direction = -1;
	switch ( l->effect ) {
	case 1:		// random flicker (P_SpawnLightFlash)
		l->count = ( DoomRandom() & 64 ) + 1;
		break;
	case 2: case 3: case 4: case 12: case 13:	// strobes (P_SpawnStrobeFlash)
		if ( l->min == l->max ) {
			l->min = 0;
		}
		l->count = ( l->effect == 12 || l->effect == 13 ) ? 1 : ( DoomRandom() & 7 ) + 1;
		break;
	case 17:	// fire flicker
		l->min = min + 16;
		l->count = 4;
		break;
	default:
		l->count = 0;
		break;
	}
}

static void LightTic( int s ) {
	lightThinker_t *l = &cgDoom.lights[s];

	switch ( l->effect ) {
	case 1:
		if ( --l->count ) {
			return;
		}
		if ( l->level == l->max ) {
			l->level = l->min;
			l->count = ( DoomRandom() & 7 ) + 1;
		} else {
			l->level = l->max;
			l->count = ( DoomRandom() & 64 ) + 1;
		}
		break;
	case 2: case 3: case 4: case 12: case 13: {
		int dark = ( l->effect == 3 || l->effect == 12 ) ? SLOWDARK : FASTDARK;

		if ( --l->count ) {
			return;
		}
		if ( l->level == l->min ) {
			l->level = l->max;
			l->count = STROBEBRIGHT;
		} else {
			l->level = l->min;
			l->count = dark;
		}
		break;
	}
	case 8:		// glow
		if ( l->direction == -1 ) {
			l->level -= GLOWSPEED;
			if ( l->level <= l->min ) {
				l->level += GLOWSPEED;
				l->direction = 1;
			}
		} else {
			l->level += GLOWSPEED;
			if ( l->level >= l->max ) {
				l->level -= GLOWSPEED;
				l->direction = -1;
			}
		}
		break;
	case 17: {
		int amount;

		if ( --l->count ) {
			return;
		}
		amount = ( DoomRandom() & 3 ) * 16;
		l->level = l->max - amount < l->min ? l->min : l->max - amount;
		l->count = 4;
		break;
	}
	default:
		l->level = cgDoom.baseLight[s];
		break;
	}
}

/*
=================
CG_DoomLightsChanged

CS_DOOM_LIGHTS: "sector level special ..." for the sectors line specials changed
=================
*/
void CG_DoomLightsChanged( void ) {
	const char	*s;
	char		*token;
	int			i;

	if ( !DOOM_MAP() ) {
		return;
	}
	for ( i = 0 ; i < cgDoom.numSectors ; i++ ) {
		cgDoom.baseLight[i] = cgDoom.map->sectors[i].light;
		cgDoom.lights[i].effect = cgDoom.map->sectors[i].special;
	}
	s = CG_ConfigString( CS_DOOM_LIGHTS );
	while ( 1 ) {
		int sector, level, special;

		token = COM_Parse( (char **)&s );
		if ( !token[0] ) {
			break;
		}
		sector = atoi( token );
		level = atoi( COM_Parse( (char **)&s ) );
		special = atoi( COM_Parse( (char **)&s ) );
		if ( sector >= 0 && sector < cgDoom.numSectors ) {
			cgDoom.baseLight[sector] = level;
			cgDoom.lights[sector].effect = special;
		}
	}
	for ( i = 0 ; i < cgDoom.numSectors ; i++ ) {
		StartLight( i );
	}
}

static void UpdateLights( void ) {
	int		tic = cg.time * DOOM_TICRATE / 1000, i, steps = 0;

	if ( tic < cgDoom.lastTic ) {
		cgDoom.lastTic = tic;		// map restart
	}
	while ( cgDoom.lastTic < tic && steps++ < DOOM_TICRATE ) {
		cgDoom.lastTic++;
		for ( i = 0 ; i < cgDoom.numSectors ; i++ ) {
			LightTic( i );
		}
	}
	cgDoom.lastTic = tic;

	for ( i = 0 ; i < cgDoom.numSectors ; i++ ) {
		int l = cgDoom.lights[i].level;
		cgDoom.levels[i] = l < 0 ? 0 : l > 255 ? 255 : l;
	}
	trap_R_DoomSectorLights( cgDoom.levels, cgDoom.numSectors );
}

/*
=====================================================================

SPRITES

=====================================================================
*/

/*
=================
BuildSpriteFrames

Sprite lumps are named SSSSFR or SSSSFRFR: sprite, frame letter and
rotation (0 = all angles), the second pair being the mirrored use of the
same picture (R_InitSprites).
=================
*/
static void BuildSpriteFrames( void ) {
	int		i, n, s, f, r;

	cgDoom.frames = malloc( sizeof( *cgDoom.frames ) * NUMSPRITES );
	for ( s = 0 ; s < NUMSPRITES ; s++ ) {
		for ( f = 0 ; f < MAX_SPRITE_FRAMES ; f++ ) {
			for ( r = 0 ; r < 8 ; r++ ) {
				cgDoom.frames[s][f][r].lump = -1;
				cgDoom.frames[s][f][r].flip = 0;
			}
		}
	}

	n = doom_wad_ns_count( cgDoom.wad, DOOM_NS_SPRITES );
	for ( i = 0 ; i < n ; i++ ) {
		int				lump = doom_wad_ns_lump( cgDoom.wad, DOOM_NS_SPRITES, i );
		const char		*name = doom_wad_lump( cgDoom.wad, lump )->name;
		int				pair;

		if ( strlen( name ) < 6 ) {
			continue;
		}
		for ( s = 0 ; s < NUMSPRITES ; s++ ) {
			if ( !Q_strncmp( name, doom_sprnames[s], 4 ) ) {
				break;
			}
		}
		if ( s == NUMSPRITES ) {
			continue;
		}
		for ( pair = 0 ; pair < 2 ; pair++ ) {
			int frame, rot;

			if ( pair && strlen( name ) < 8 ) {
				break;
			}
			frame = name[4 + pair * 2] - 'A';
			rot = name[5 + pair * 2] - '0';
			if ( frame < 0 || frame >= MAX_SPRITE_FRAMES || rot < 0 || rot > 8 ) {
				continue;
			}
			if ( rot == 0 ) {
				for ( r = 0 ; r < 8 ; r++ ) {
					cgDoom.frames[s][frame][r].lump = lump;
					cgDoom.frames[s][frame][r].flip = (byte)pair;
				}
			} else {
				cgDoom.frames[s][frame][rot - 1].lump = lump;
				cgDoom.frames[s][frame][rot - 1].flip = (byte)pair;
			}
		}
	}

	cgDoom.lumpShader = calloc( doom_wad_num_lumps( cgDoom.wad ), sizeof( qhandle_t ) );
}

static qhandle_t LumpShader( int lump ) {
	if ( !cgDoom.lumpShader[lump] ) {
		cgDoom.lumpShader[lump] = trap_R_RegisterShader( va( "doom/thing/%s", doom_wad_lump( cgDoom.wad, lump )->name ) );
	}
	return cgDoom.lumpShader[lump];
}

// the state a thing has reached 'tics' after it spawned in 'state'
static int AnimatedState( int state, int tics ) {
	int		guard = 0;

	while ( guard++ < 256 ) {
		int length = doom_states[state].tics;

		if ( length < 0 || tics < length ) {
			return state;
		}
		tics -= length > 0 ? length : 1;
		state = doom_states[state].nextstate;
		if ( state == S_NULL ) {
			return S_NULL;
		}
	}
	return state;
}

/*
=================
AddSprite

A Doom sprite billboard: it faces the view direction (around the vertical
axis only, as in Doom), rotation picked from the angle between the viewer
and the thing (R_ProjectSprite).
=================
*/
static void AddSprite( const vec3_t origin, float angle, int state, int light ) {
	const doom_state_t	*st = &doom_states[state];
	const spriteRot_t	*rot;
	const uint8_t		*header;
	polyVert_t			verts[4];
	float				viewToThing, right[2], width, height, left, top;
	int					frame = st->frame & DOOM_FF_FRAMEMASK, r, i, color;

	if ( state == S_NULL || st->sprite < 0 || st->sprite >= NUMSPRITES || frame >= MAX_SPRITE_FRAMES ) {
		return;
	}
	viewToThing = RAD2DEG( atan2( origin[1] - cg.refdef.vieworg[1], origin[0] - cg.refdef.vieworg[0] ) );
	r = (int)floor( AngleNormalize360( viewToThing - angle + 202.5f ) / 45.0f ) & 7;
	rot = &cgDoom.frames[st->sprite][frame][r];
	if ( rot->lump < 0 ) {
		return;
	}

	header = doom_wad_lump_data( cgDoom.wad, rot->lump );
	width = (short)( header[0] | ( header[1] << 8 ) );
	height = (short)( header[2] | ( header[3] << 8 ) );
	left = (short)( header[4] | ( header[5] << 8 ) );
	top = (short)( header[6] | ( header[7] << 8 ) );

	// screen right, flattened
	right[0] = -cg.refdef.viewaxis[1][0];
	right[1] = -cg.refdef.viewaxis[1][1];
	{
		float len = sqrt( right[0] * right[0] + right[1] * right[1] );
		if ( len < 0.001f ) {
			return;
		}
		right[0] /= len;
		right[1] /= len;
	}

	color = ( st->frame & DOOM_FF_FULLBRIGHT ) ? 255 : light;
	for ( i = 0 ; i < 4 ; i++ ) {
		float	x = ( i == 1 || i == 2 ) ? width - left : -left;
		float	z = ( i < 2 ) ? top : top - height;
		float	s = ( i == 1 || i == 2 ) ? 1.0f : 0.0f;

		verts[i].xyz[0] = origin[0] + right[0] * x;
		verts[i].xyz[1] = origin[1] + right[1] * x;
		verts[i].xyz[2] = origin[2] + z;
		verts[i].st[0] = rot->flip ? 1.0f - s : s;
		verts[i].st[1] = ( i < 2 ) ? 0.0f : 1.0f;
		verts[i].modulate[0] = verts[i].modulate[1] = verts[i].modulate[2] = color;
		verts[i].modulate[3] = 255;
	}
	trap_R_AddPolyToScene( LumpShader( rot->lump ), 4, verts );
}

/*
=====================================================================

SETUP AND FRAME

=====================================================================
*/

void CG_DoomInit( void ) {
	char		mapname[MAX_QPATH];
	const char	*info;
	int			i;

	memset( &cgDoom, 0, sizeof( cgDoom ) );
	COM_StripExtension( COM_SkipPath( cgs.mapname ), mapname );
	cgDoom.map = trap_Doom_GetMap( mapname );
	if ( !cgDoom.map ) {
		return;
	}
	cgDoom.wad = trap_Doom_GetWad();
	info = CG_ConfigString( CS_SERVERINFO );
	cgDoom.skill = atoi( Info_ValueForKey( info, "g_doomSkill" ) );
	cgDoom.random = 0x1234;

	// lights
	cgDoom.numSectors = cgDoom.map->num_sectors;
	cgDoom.baseLight = calloc( cgDoom.numSectors, sizeof( int ) );
	cgDoom.lights = calloc( cgDoom.numSectors, sizeof( lightThinker_t ) );
	cgDoom.levels = calloc( cgDoom.numSectors, 1 );
	CG_DoomLightsChanged();

	// things
	BuildSpriteFrames();
	for ( i = 0 ; i < cgDoom.map->num_things && cgDoom.numThings < MAX_DOOM_THINGS ; i++ ) {
		const doom_thing_t		*t = &cgDoom.map->things[i];
		int						type = BG_DoomStaticThing( t, cgDoom.skill );
		doomThing_t				*th;
		int						sector;
		float					height;

		if ( type < 0 ) {
			continue;
		}
		sector = doom_map_sector_at( cgDoom.map, t->x + 0.25, t->y + 0.25 );
		if ( sector < 0 ) {
			continue;
		}
		th = &cgDoom.things[cgDoom.numThings++];
		th->type = type;
		th->angle = t->angle;
		th->sector = sector;
		height = (float)doom_mobjinfo[type].height / DOOM_FRACUNIT;
		VectorSet( th->origin, t->x, t->y, ( doom_mobjinfo[type].flags & MF_SPAWNCEILING )
			? cgDoom.map->sectors[sector].ceiling_height - height : cgDoom.map->sectors[sector].floor_height );

		if ( cgDoom.numBlockers < MAX_DOOM_THINGS
			&& BG_DoomThingBlocks( cgDoom.map, t, type, cgDoom.blockMins[cgDoom.numBlockers], cgDoom.blockMaxs[cgDoom.numBlockers] ) ) {
			cgDoom.numBlockers++;
		}
	}
	CG_Printf( "Doom map %s: %d things drawn, %d solid\n", cgDoom.map->name, cgDoom.numThings, cgDoom.numBlockers );
}

void CG_DoomShutdown( void ) {
	free( cgDoom.frames );
	free( cgDoom.lumpShader );
	free( cgDoom.baseLight );
	free( cgDoom.lights );
	free( cgDoom.levels );
	memset( &cgDoom, 0, sizeof( cgDoom ) );
}

/*
=================
CG_DoomAddToScene

Every frame, before the scene is rendered
=================
*/
void CG_DoomAddToScene( void ) {
	int		i, tics;

	if ( !DOOM_MAP() ) {
		return;
	}
	UpdateLights();

	tics = cg.time * DOOM_TICRATE / 1000;
	for ( i = 0 ; i < cgDoom.numThings ; i++ ) {
		const doomThing_t *th = &cgDoom.things[i];

		AddSprite( th->origin, th->angle, AnimatedState( doom_mobjinfo[th->type].spawnstate, tics ), cgDoom.levels[th->sector] );
	}
}

/*
=================
CG_DoomClipMove

Player prediction against the solid things (see SpawnThingBlockers in
the game): the same boxes, so client and server agree.
=================
*/
void CG_DoomClipMove( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int mask, trace_t *tr ) {
	int		i;

	// temp box models have CONTENTS_BODY, which player movement collides with
	if ( !DOOM_MAP() || !( mask & CONTENTS_BODY ) ) {
		return;
	}
	for ( i = 0 ; i < cgDoom.numBlockers ; i++ ) {
		trace_t		trace;
		clipHandle_t cmodel = trap_CM_TempBoxModel( cgDoom.blockMins[i], cgDoom.blockMaxs[i] );

		trap_CM_BoxTrace( &trace, start, end, mins, maxs, cmodel, mask );
		if ( trace.allsolid || trace.fraction < tr->fraction ) {
			trace.entityNum = ENTITYNUM_WORLD;
			*tr = trace;
		} else if ( trace.startsolid ) {
			tr->startsolid = qtrue;
		}
		if ( tr->allsolid ) {
			return;
		}
	}
}

int CG_DoomPointContents( const vec3_t point ) {
	int		i;

	if ( !DOOM_MAP() ) {
		return 0;
	}
	for ( i = 0 ; i < cgDoom.numBlockers ; i++ ) {
		if ( point[0] >= cgDoom.blockMins[i][0] && point[0] <= cgDoom.blockMaxs[i][0]
			&& point[1] >= cgDoom.blockMins[i][1] && point[1] <= cgDoom.blockMaxs[i][1]
			&& point[2] >= cgDoom.blockMins[i][2] && point[2] <= cgDoom.blockMaxs[i][2] ) {
			return CONTENTS_BODY;
		}
	}
	return 0;
}
