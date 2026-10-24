/*
===========================================================================
Q3Doom - the Doom status bar, messages, palette and automap (st_stuff.c,
hu_stuff.c, am_map.c)

Everything is drawn with Doom's own pictures in Doom's 320x200 layout,
scaled into a 4:3 area of the screen (Doom's tall pixels); outside it the
status bar row is filled with Doom's border flat. With the Doom HUD the
3D view stops above the status bar, as in Doom. cg_hudStyle 1 keeps the
Quake 3 status bar instead (the rest still applies).

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "cg_local.h"
#include "../game/bg_doom.h"

#define ST_HEIGHT			32
#define ST_Y				( 200 - ST_HEIGHT )

// faces (st_stuff.c)
#define ST_NUMPAINFACES		5
#define ST_NUMSTRAIGHTFACES	3
#define ST_FACESTRIDE		8
#define ST_NUMFACES			( ST_FACESTRIDE * ST_NUMPAINFACES + 2 )
#define ST_TURNOFFSET		3
#define ST_OUCHOFFSET		5
#define ST_EVILGRINOFFSET	6
#define ST_RAMPAGEOFFSET	7
#define ST_GODFACE			( ST_NUMPAINFACES * ST_FACESTRIDE )
#define ST_DEADFACE			( ST_GODFACE + 1 )
#define ST_EVILGRINCOUNT	( 2 * DOOM_TICRATE )
#define ST_STRAIGHTFACECOUNT	( DOOM_TICRATE / 2 )
#define ST_TURNCOUNT		( 1 * DOOM_TICRATE )
#define ST_RAMPAGEDELAY		( 2 * DOOM_TICRATE )
#define ST_MUCHPAIN			20

#define BONUSADD			6
#define HU_MSGTIMEOUT		( 4 * 1000 )
#define HU_FONTSTART		'!'
#define HU_FONTEND			'_'

#define AM_INITSCALE		( 1600.0f / 320 )	// map units per Doom pixel (INITSCALEMTOF .2)

static struct {
	qboolean			active;
	const doom_wad_t	*wad;
	const doom_map_t	*map;
	qhandle_t			*lumpShader;	// 2D shaders per lump, registered on first use

	// pictures
	int					sbar, starms, tallPercent, tallMinus;
	int					tallNum[10], shortNum[10], greyNum[10];
	int					keys[6];
	int					faces[ST_NUMFACES];
	int					font[HU_FONTEND - HU_FONTSTART + 1];
	qhandle_t			borderFlat, white, invert, amLine;

	// palettes 1-13 as a blend over the view: color and alpha
	vec4_t				palette[14];

	// the player as the status bar sees it, at 35 tics a second
	int					lastTic;
	int					damageCount, bonusCount;
	int					attackerYaw;		// damage direction byte, 255 none
	int					faceIndex, faceCount, facePriority, lastAttackDown;
	int					oldHealth, oldWeapons;
	int					randomNumber;

	// message line
	char				message[128];
	int					messageTime;

	// automap
	qboolean			automap;
	byte				*mapped;			// per linedef: seen (ML_MAPPED)
	int					nextCheck;
} hud;

/*
=====================================================================

PICTURES

=====================================================================
*/

/*
=================
CG_DoomHudScale

Doom's 320x200 screen in pixels: x = x0 + x * sx, y = y * sy
=================
*/
void CG_DoomHudScale( float *x0, float *sx, float *sy ) {
	float	w = cgs.glconfig.vidHeight * 4.0f / 3;

	if ( w > cgs.glconfig.vidWidth ) {
		w = cgs.glconfig.vidWidth;
	}
	*sx = w / 320;
	*sy = cgs.glconfig.vidHeight / 200.0f;
	*x0 = ( cgs.glconfig.vidWidth - w ) * 0.5f;
}

qhandle_t CG_DoomPatchShader( int lump ) {
	if ( !hud.lumpShader || lump < 0 || lump >= doom_wad_num_lumps( hud.wad ) ) {
		return 0;
	}
	if ( !hud.lumpShader[lump] ) {
		hud.lumpShader[lump] = trap_R_RegisterShaderNoMip( va( "doom/patch/%s", doom_wad_lump( hud.wad, lump )->name ) );
	}
	return hud.lumpShader[lump];
}

static int Lump( const char *name ) {
	return doom_wad_find( hud.wad, name );
}

static void PatchSize( int lump, int *width, int *height, int *left, int *top ) {
	const byte *h = doom_wad_lump_data( hud.wad, lump );

	*width = (short)( h[0] | ( h[1] << 8 ) );
	*height = (short)( h[2] | ( h[3] << 8 ) );
	*left = (short)( h[4] | ( h[5] << 8 ) );
	*top = (short)( h[6] | ( h[7] << 8 ) );
}

static int PatchWidth( int lump ) {
	int		w, h, l, t;

	if ( lump < 0 ) {
		return 0;
	}
	PatchSize( lump, &w, &h, &l, &t );
	return w;
}

// V_DrawPatch: at Doom screen coordinates, offsets applied
static void DrawPatch( float x, float y, int lump ) {
	float	x0, sx, sy;
	int		w, h, left, top;

	if ( lump < 0 ) {
		return;
	}
	CG_DoomHudScale( &x0, &sx, &sy );
	PatchSize( lump, &w, &h, &left, &top );
	trap_R_DrawStretchPic( x0 + ( x - left ) * sx, ( y - top ) * sy, w * sx, h * sy, 0, 0, 1, 1, CG_DoomPatchShader( lump ) );
}

// for the intermission and the finale (cg_doom_wi.c)
int CG_DoomLump( const char *name ) {
	return hud.active ? Lump( name ) : -1;
}

void CG_DoomDrawPatch( float x, float y, int lump ) {
	if ( hud.active ) {
		DrawPatch( x, y, lump );
	}
}

void CG_DoomPatchSize( int lump, int *width, int *height, int *left, int *top ) {
	*width = *height = *left = *top = 0;
	if ( hud.active && lump >= 0 ) {
		PatchSize( lump, width, height, left, top );
	}
}

int CG_DoomFontLump( int c ) {
	c = toupper( c );
	return c < HU_FONTSTART || c > HU_FONTEND ? -1 : hud.font[c - HU_FONTSTART];
}

// columns 'from' to 'to' of a 320 wide full screen picture, drawn at x (F_BunnyScroll)
void CG_DoomDrawPatchColumns( int lump, float x, float from, float to ) {
	float	x0, sx, sy;
	int		w, h, l, t;

	if ( !hud.active || lump < 0 || to <= from ) {
		return;
	}
	CG_DoomHudScale( &x0, &sx, &sy );
	PatchSize( lump, &w, &h, &l, &t );
	trap_R_DrawStretchPic( x0 + x * sx, 0, ( to - from ) * sx, h * sy, from / w, 0, to / w, 1, CG_DoomPatchShader( lump ) );
}

// Doom's whole screen: black outside the 4:3 area, a color or a tiled flat inside
void CG_DoomFillScreen( qhandle_t flat ) {
	float	x0, sx, sy, x, y;

	trap_R_SetColor( colorBlack );
	trap_R_DrawStretchPic( 0, 0, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, 0, 0, 1, 1, hud.white );
	trap_R_SetColor( NULL );
	if ( !flat ) {
		return;
	}
	CG_DoomHudScale( &x0, &sx, &sy );
	for ( y = 0 ; y < 200 ; y += 64 ) {
		for ( x = 0 ; x < 320 ; x += 64 ) {
			float w = x + 64 > 320 ? 320 - x : 64, h = y + 64 > 200 ? 200 - y : 64;

			trap_R_DrawStretchPic( x0 + x * sx, y * sy, w * sx, h * sy, 0, 0, w / 64, h / 64, flat );
		}
	}
}

/*
=================
DrawNum

STlib_drawNum: right aligned at 'x', up to 'digits' digits
=================
*/
static void DrawNum( int x, int y, int value, int digits, const int *nums ) {
	int		w = PatchWidth( nums[0] );
	qboolean	neg = value < 0;

	if ( neg ) {
		if ( digits == 2 && value < -9 ) {
			value = -9;
		} else if ( digits == 3 && value < -99 ) {
			value = -99;
		}
		value = -value;
	}
	if ( !value ) {
		DrawPatch( x - w, y, nums[0] );
	}
	while ( value && digits-- ) {
		x -= w;
		DrawPatch( x, y, nums[value % 10] );
		value /= 10;
	}
	if ( neg && nums == hud.tallNum ) {
		DrawPatch( x - 8, y, hud.tallMinus );
	}
}

/*
=================
DrawText

HUlib: Doom's small font, upper case, at Doom screen coordinates
=================
*/
static void DrawText( int x, int y, const char *text ) {
	for ( ; *text ; text++ ) {
		int c = toupper( *(const unsigned char *)text );

		if ( c < HU_FONTSTART || c > HU_FONTEND || hud.font[c - HU_FONTSTART] < 0 ) {
			x += 4;
			continue;
		}
		DrawPatch( x, y, hud.font[c - HU_FONTSTART] );
		x += PatchWidth( hud.font[c - HU_FONTSTART] );
		if ( x >= 320 ) {
			break;
		}
	}
}

/*
=====================================================================

SETUP

=====================================================================
*/

// the blend over palette 0 that gives each other palette (base * (1 - a) + c * a)
static void ComputePaletteBlends( void ) {
	const byte	*base = doom_palette( hud.wad, 0 );
	int			black = 0, white = 0, i, p, c;

	if ( !base ) {
		return;
	}
	for ( i = 1 ; i < 256 ; i++ ) {
		if ( base[i * 3] + base[i * 3 + 1] + base[i * 3 + 2] > base[white * 3] + base[white * 3 + 1] + base[white * 3 + 2] ) {
			white = i;
		}
		if ( base[i * 3] + base[i * 3 + 1] + base[i * 3 + 2] < base[black * 3] + base[black * 3 + 1] + base[black * 3 + 2] ) {
			black = i;
		}
	}
	for ( p = 1 ; p < 14 && p < doom_num_palettes( hud.wad ) ; p++ ) {
		const byte	*pal = doom_palette( hud.wad, p );
		float		a = 0;

		for ( c = 0 ; c < 3 ; c++ ) {
			float range = base[white * 3 + c] - base[black * 3 + c];

			if ( range > 0 ) {
				a += 1.0f - ( pal[white * 3 + c] - pal[black * 3 + c] ) / range;
			}
		}
		a /= 3;
		if ( a < 0.01f ) {
			continue;
		}
		for ( c = 0 ; c < 3 ; c++ ) {
			float v = ( pal[black * 3 + c] - base[black * 3 + c] * ( 1 - a ) ) / a / 255.0f;
			hud.palette[p][c] = v < 0 ? 0 : v > 1 ? 1 : v;
		}
		hud.palette[p][3] = a;
	}
}

void CG_DoomHudInit( void ) {
	int		i, p;

	memset( &hud, 0, sizeof( hud ) );
	hud.wad = CG_DoomWad();
	hud.map = CG_DoomMapData();
	if ( !hud.wad || !hud.map ) {
		return;
	}
	hud.active = qtrue;
	hud.lumpShader = calloc( doom_wad_num_lumps( hud.wad ), sizeof( qhandle_t ) );
	hud.mapped = calloc( hud.map->num_linedefs, 1 );

	hud.sbar = Lump( "STBAR" );
	hud.starms = Lump( "STARMS" );
	hud.tallPercent = Lump( "STTPRCNT" );
	hud.tallMinus = Lump( "STTMINUS" );
	for ( i = 0 ; i < 10 ; i++ ) {
		hud.tallNum[i] = Lump( va( "STTNUM%d", i ) );
		hud.shortNum[i] = Lump( va( "STYSNUM%d", i ) );
		hud.greyNum[i] = Lump( va( "STGNUM%d", i ) );
	}
	for ( i = 0 ; i < 6 ; i++ ) {
		hud.keys[i] = Lump( va( "STKEYS%d", i ) );
	}
	for ( p = 0, i = 0 ; p < ST_NUMPAINFACES ; p++ ) {
		hud.faces[i++] = Lump( va( "STFST%d0", p ) );
		hud.faces[i++] = Lump( va( "STFST%d1", p ) );
		hud.faces[i++] = Lump( va( "STFST%d2", p ) );
		hud.faces[i++] = Lump( va( "STFTR%d0", p ) );
		hud.faces[i++] = Lump( va( "STFTL%d0", p ) );
		hud.faces[i++] = Lump( va( "STFOUCH%d", p ) );
		hud.faces[i++] = Lump( va( "STFEVL%d", p ) );
		hud.faces[i++] = Lump( va( "STFKILL%d", p ) );
	}
	hud.faces[i++] = Lump( "STFGOD0" );
	hud.faces[i++] = Lump( "STFDEAD0" );
	for ( i = HU_FONTSTART ; i <= HU_FONTEND ; i++ ) {
		hud.font[i - HU_FONTSTART] = Lump( va( "STCFN%.3d", i ) );
	}
	hud.borderFlat = trap_R_RegisterShaderNoMip( "doom/flat/FLOOR7_2" );
	hud.white = trap_R_RegisterShaderNoMip( "white" );
	hud.invert = trap_R_RegisterShaderNoMip( "doom/invert" );
	hud.amLine = trap_R_RegisterShader( "doom/amline" );

	ComputePaletteBlends();

	hud.attackerYaw = 255;
	hud.oldHealth = -1;
	hud.lastAttackDown = -1;
	hud.lastTic = cg.time * DOOM_TICRATE / 1000;
}

void CG_DoomHudShutdown( void ) {
	free( hud.lumpShader );
	free( hud.mapped );
	memset( &hud, 0, sizeof( hud ) );
}

/*
=================
CG_DoomHudActive

The Doom status bar is up: the 3D view stops above it
=================
*/
qboolean CG_DoomHudActive( void ) {
	// DoomFrag's Quake 3 weapons have no place on Doom's status bar
	return hud.active && !cg_hudStyle.integer && cgs.doomWeapons != DOOM_WEAPONS_Q3 && cg.snap && cg.snap->ps.pm_type != PM_INTERMISSION
		&& cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR;
}

/*
=====================================================================

EVENTS FROM THE GAME

=====================================================================
*/

void CG_DoomMessage( const char *text ) {
	if ( !hud.active ) {
		return;
	}
	Q_strncpyz( hud.message, text, sizeof( hud.message ) );
	hud.messageTime = cg.time;
	CG_Printf( "%s\n", text );
}

// an item picked up: the gold flash (P_TouchSpecialThing)
void CG_DoomBonus( void ) {
	hud.bonusCount += BONUSADD;
}

// the player was hurt: the red flash and the face (P_DamageMobj)
void CG_DoomDamage( int damage, int yawByte ) {
	hud.damageCount += damage;
	if ( hud.damageCount > 100 ) {
		hud.damageCount = 100;
	}
	hud.attackerYaw = yawByte;
}

void CG_DoomAutomap_f( void ) {
	hud.automap = !hud.automap;
}

/*
=====================================================================

THE FACE (ST_updateFaceWidget)

=====================================================================
*/

static int PainOffset( int health ) {
	if ( health > 100 ) {
		health = 100;
	}
	if ( health < 0 ) {
		health = 0;
	}
	return ST_FACESTRIDE * ( ( ( 100 - health ) * ST_NUMPAINFACES ) / 101 );
}

static void FaceTic( const playerState_t *ps ) {
	int		health = ps->stats[STAT_HEALTH] > 0 ? ps->stats[STAT_HEALTH] : 0;
	int		weapons = ps->stats[STAT_WEAPONS];

	if ( hud.facePriority < 10 && !health ) {
		// dead
		hud.facePriority = 9;
		hud.faceIndex = ST_DEADFACE;
		hud.faceCount = 1;
	}

	if ( hud.facePriority < 9 && hud.bonusCount ) {
		// evil grin when a new weapon was picked up
		if ( hud.oldWeapons != weapons ) {
			hud.oldWeapons = weapons;
			hud.facePriority = 8;
			hud.faceCount = ST_EVILGRINCOUNT;
			hud.faceIndex = PainOffset( health ) + ST_EVILGRINOFFSET;
		}
	}

	if ( hud.facePriority < 8 && hud.damageCount && hud.attackerYaw != 255 ) {
		// being attacked: turn toward the attacker (Doom's "ouch" test
		// compares the wrong way and never shows; kept as it was)
		hud.facePriority = 7;
		if ( health - hud.oldHealth > ST_MUCHPAIN ) {
			hud.faceCount = ST_TURNCOUNT;
			hud.faceIndex = PainOffset( health ) + ST_OUCHOFFSET;
		} else {
			float	diff = AngleNormalize360( hud.attackerYaw * 360.0f / 256 + 180 - ps->viewangles[YAW] );
			float	diffang = diff > 180 ? 360 - diff : diff;

			hud.faceCount = ST_TURNCOUNT;
			hud.faceIndex = PainOffset( health );
			if ( diffang < 45 ) {
				hud.faceIndex += ST_RAMPAGEOFFSET;		// head-on
			} else if ( diff > 180 ) {
				hud.faceIndex += ST_TURNOFFSET;			// to the right
			} else {
				hud.faceIndex += ST_TURNOFFSET + 1;		// to the left
			}
		}
	}

	if ( hud.facePriority < 7 && hud.damageCount ) {
		// hurt by your own damn stupidity
		if ( health - hud.oldHealth > ST_MUCHPAIN ) {
			hud.facePriority = 7;
			hud.faceCount = ST_TURNCOUNT;
			hud.faceIndex = PainOffset( health ) + ST_OUCHOFFSET;
		} else {
			hud.facePriority = 6;
			hud.faceCount = ST_TURNCOUNT;
			hud.faceIndex = PainOffset( health ) + ST_RAMPAGEOFFSET;
		}
	}

	if ( hud.facePriority < 6 ) {
		// rapid firing
		if ( ps->doomWeaponFlags & DOOMWF_ATTACKDOWN ) {
			if ( hud.lastAttackDown == -1 ) {
				hud.lastAttackDown = ST_RAMPAGEDELAY;
			} else if ( !--hud.lastAttackDown ) {
				hud.facePriority = 5;
				hud.faceIndex = PainOffset( health ) + ST_RAMPAGEOFFSET;
				hud.faceCount = 1;
				hud.lastAttackDown = 1;
			}
		} else {
			hud.lastAttackDown = -1;
		}
	}

	if ( hud.facePriority < 5 && ps->powerups[DPW_INVULNERABILITY] ) {
		hud.facePriority = 4;
		hud.faceIndex = ST_GODFACE;
		hud.faceCount = 1;
	}

	// look left or right when the face timed out
	if ( !hud.faceCount ) {
		hud.faceIndex = PainOffset( health ) + hud.randomNumber % 3;
		hud.faceCount = ST_STRAIGHTFACECOUNT;
		hud.facePriority = 0;
	}
	hud.faceCount--;
}

// ST_Ticker and the counters P_PlayerThink drops every tic
static void HudTics( void ) {
	const playerState_t	*ps = &cg.predictedPlayerState;
	int					tic = cg.time * DOOM_TICRATE / 1000, steps = 0;

	if ( tic < hud.lastTic ) {
		hud.lastTic = tic;
	}
	if ( hud.oldHealth < 0 ) {
		hud.oldHealth = ps->stats[STAT_HEALTH];
		hud.oldWeapons = ps->stats[STAT_WEAPONS];
	}
	while ( hud.lastTic < tic && steps++ < DOOM_TICRATE ) {
		hud.lastTic++;
		hud.randomNumber = rand() & 255;
		FaceTic( ps );
		hud.oldHealth = ps->stats[STAT_HEALTH] > 0 ? ps->stats[STAT_HEALTH] : 0;
		if ( hud.damageCount ) {
			hud.damageCount--;
		}
		if ( hud.bonusCount ) {
			hud.bonusCount--;
		}
	}
	hud.lastTic = tic;
	if ( !hud.damageCount ) {
		hud.attackerYaw = 255;
	}
}

/*
=====================================================================

DRAWING

=====================================================================
*/

/*
=================
FillBorder

The status bar row outside the 4:3 area: Doom's border flat
=================
*/
static void FillBorder( void ) {
	float	x0, sx, sy, tw, th, y, x;

	CG_DoomHudScale( &x0, &sx, &sy );
	if ( x0 < 1 ) {
		return;
	}
	tw = 64 * sx;
	th = 64 * sy;
	y = ST_Y * sy;
	for ( x = x0 ; x > 0 ; x -= tw ) {
		float left = x - tw < 0 ? 0 : x - tw;

		trap_R_DrawStretchPic( left, y, x - left, ST_HEIGHT * sy, 1 - ( x - left ) / tw, 0, 1, ST_HEIGHT / 64.0f, hud.borderFlat );
	}
	for ( x = x0 + 320 * sx ; x < cgs.glconfig.vidWidth ; x += tw ) {
		float right = x + tw > cgs.glconfig.vidWidth ? cgs.glconfig.vidWidth : x + tw;

		trap_R_DrawStretchPic( x, y, right - x, ST_HEIGHT * sy, 0, 0, ( right - x ) / tw, ST_HEIGHT / 64.0f, hud.borderFlat );
	}
}

/*
=================
DrawStatusBar

ST_drawWidgets: ammo, health, arms, face, armor, keys, ammo counts
=================
*/
static void DrawStatusBar( const playerState_t *ps ) {
	static const int	ammoY[DAM_NUMAMMO] = { 173, 179, 191, 185 };	// clip, shell, cell, misl
	int					weapon = DOOM_DWP( ps->weapon ), i;

	FillBorder();
	DrawPatch( 0, ST_Y, hud.sbar );

	// ready weapon's ammo
	if ( weapon >= 0 && weapon < DWP_NUMWEAPONS && bg_doomWeapons[weapon].ammo < DAM_NUMAMMO ) {
		DrawNum( 44, 171, ps->ammo[bg_doomWeapons[weapon].ammo], 3, hud.tallNum );
	}

	// health and armor percents
	DrawNum( 90, 171, ps->stats[STAT_HEALTH] > 0 ? ps->stats[STAT_HEALTH] : 0, 3, hud.tallNum );
	DrawPatch( 90, 171, hud.tallPercent );
	DrawNum( 221, 171, ps->stats[STAT_ARMOR], 3, hud.tallNum );
	DrawPatch( 221, 171, hud.tallPercent );

	// arms: 2-7, yellow when owned
	DrawPatch( 104, 168, hud.starms );
	for ( i = 0 ; i < 6 ; i++ ) {
		DrawPatch( 111 + ( i % 3 ) * 12, 172 + ( i / 3 ) * 10, DOOM_OWNS( ps, DWP_PISTOL + i ) ? hud.shortNum[i + 2] : hud.greyNum[i + 2] );
	}

	// face
	if ( hud.faceIndex >= 0 && hud.faceIndex < ST_NUMFACES ) {
		DrawPatch( 143, 168, hud.faces[hud.faceIndex] );
	}

	// keys: the skull shows over the card
	for ( i = 0 ; i < 3 ; i++ ) {
		int keys = ps->stats[STAT_DOOM_KEYS];

		if ( keys & ( 1 << ( i + 3 ) ) ) {
			DrawPatch( 239, 171 + i * 10, hud.keys[i + 3] );
		} else if ( keys & ( 1 << i ) ) {
			DrawPatch( 239, 171 + i * 10, hud.keys[i] );
		}
	}

	// ammo and maximum ammo
	for ( i = 0 ; i < DAM_NUMAMMO ; i++ ) {
		DrawNum( 288, ammoY[i], ps->ammo[i], 3, hud.shortNum );
		DrawNum( 314, ammoY[i], BG_DoomMaxAmmo( ps, i ), 3, hud.shortNum );
	}
}

/*
=================
PaletteBlend

ST_doPaletteStuff: red when hurt or berserk, gold on pickups, green in
the radiation suit, over the whole screen like Doom's palette change
=================
*/
static void PaletteBlend( const playerState_t *ps ) {
	int		cnt = hud.damageCount, palette = 0;

	if ( ps->powerups[DPW_STRENGTH] ) {
		// the berserk red fades out slowly
		int bzc = 12 - ( ( cg.time - ps->powerups[DPW_STRENGTH] ) * DOOM_TICRATE / 1000 >> 6 );

		if ( bzc > cnt ) {
			cnt = bzc;
		}
	}
	if ( cnt ) {
		palette = ( cnt + 7 ) >> 3;
		if ( palette >= 8 ) {
			palette = 7;
		}
		palette += 1;
	} else if ( hud.bonusCount ) {
		palette = ( hud.bonusCount + 7 ) >> 3;
		if ( palette >= 4 ) {
			palette = 3;
		}
		palette += 9;
	} else if ( CG_DoomPowerActive( DPW_IRONFEET ) ) {
		palette = 13;
	}
	if ( !palette || hud.palette[palette][3] <= 0 ) {
		return;
	}
	trap_R_SetColor( hud.palette[palette] );
	trap_R_DrawStretchPic( 0, 0, cgs.glconfig.vidWidth, cgs.glconfig.vidHeight, 0, 0, 1, 1, hud.white );
	trap_R_SetColor( NULL );
}

static void DrawMessage( void ) {
	if ( !hud.message[0] || cg.time - hud.messageTime > HU_MSGTIMEOUT || cg.time < hud.messageTime ) {
		return;
	}
	DrawText( 0, 0, hud.message );
}

/*
=====================================================================

AUTOMAP (am_map.c)

The lines are polys in a scene seen from above (the plane of the map is
parallel to the screen, so the view is an exact scale), drawn over a
black view; walls show once seen, or all with the computer map.

=====================================================================
*/

// ML_MAPPED: lines in front of the player and in sight, a few a frame
static void MarkSeenLines( void ) {
	const doom_map_t	*map = hud.map;
	vec3_t				eye, forward;
	int					i, n;

	VectorCopy( cg.refdef.vieworg, eye );
	AngleVectors( cg.refdefViewAngles, forward, NULL, NULL );
	n = map->num_linedefs < 48 ? map->num_linedefs : 48;
	for ( i = 0 ; i < n ; i++ ) {
		int						line = hud.nextCheck++ % map->num_linedefs;
		const doom_linedef_t	*ld = &map->linedefs[line];
		const doom_vertex_t		*a = &map->vertexes[ld->v1], *b = &map->vertexes[ld->v2];
		int						s = ld->side[0] != DOOM_NO_SIDE ? map->sidedefs[ld->side[0]].sector : -1;
		vec3_t					mid, dir;
		trace_t					tr;

		if ( hud.mapped[line] || s < 0 ) {
			continue;
		}
		VectorSet( mid, ( a->x + b->x ) * 0.5f, ( a->y + b->y ) * 0.5f, eye[2] );
		VectorSubtract( mid, eye, dir );
		if ( VectorLength( dir ) > 2048 || DotProduct( dir, forward ) < 0 ) {
			continue;
		}
		// pull the end a little toward the eye: the line itself is solid
		VectorMA( mid, -2.0f / ( VectorLength( dir ) + 1 ), dir, mid );
		CG_Trace( &tr, eye, NULL, NULL, mid, cg.predictedPlayerState.clientNum, MASK_SOLID );
		if ( tr.fraction > 0.98f ) {
			hud.mapped[line] = 1;
		}
	}
}

static void AddLine( float x1, float y1, float x2, float y2, float width, const byte *color ) {
	polyVert_t	verts[4];
	float		dx = x2 - x1, dy = y2 - y1, len = sqrt( dx * dx + dy * dy ), nx, ny;
	int			i;

	if ( len < 0.01f ) {
		return;
	}
	nx = -dy / len * width * 0.5f;
	ny = dx / len * width * 0.5f;
	VectorSet( verts[0].xyz, x1 + nx, y1 + ny, 0 );
	VectorSet( verts[1].xyz, x2 + nx, y2 + ny, 0 );
	VectorSet( verts[2].xyz, x2 - nx, y2 - ny, 0 );
	VectorSet( verts[3].xyz, x1 - nx, y1 - ny, 0 );
	for ( i = 0 ; i < 4 ; i++ ) {
		verts[i].st[0] = verts[i].st[1] = 0;
		verts[i].modulate[0] = color[0];
		verts[i].modulate[1] = color[1];
		verts[i].modulate[2] = color[2];
		verts[i].modulate[3] = 255;
	}
	trap_R_AddPolyToScene( hud.amLine, 4, verts );
}

static void DrawAutomap( const playerState_t *ps ) {
	const doom_map_t	*map = hud.map;
	const byte			*pal = doom_palette( hud.wad, 0 );
	refdef_t			rd;
	float				x0, sx, sy, unitsPerPixel, width, height, yaw;
	int					i;
	static const float	arrow[][4] = {		// player_arrow, in units of 8 (am_map.c)
		{ -7, 0, 8, 0 }, { 8, 0, 4, 2 }, { 8, 0, 4, -2 },
		{ -7, 0, -9, 2 }, { -7, 0, -9, -2 }, { -5, 0, -7, 2 }, { -5, 0, -7, -2 }
	};

	if ( !pal ) {
		return;
	}
	// black over the view
	trap_R_SetColor( colorBlack );
	trap_R_DrawStretchPic( cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, hud.white );
	trap_R_SetColor( NULL );

	CG_DoomHudScale( &x0, &sx, &sy );
	unitsPerPixel = AM_INITSCALE / sx;
	width = cg.refdef.width * unitsPerPixel;
	height = cg.refdef.height * unitsPerPixel;

	trap_R_ClearScene();
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		const doom_vertex_t		*a = &map->vertexes[ld->v1], *b = &map->vertexes[ld->v2];
		int						front = ld->side[0] != DOOM_NO_SIDE ? map->sidedefs[ld->side[0]].sector : -1;
		int						back = ld->side[1] != DOOM_NO_SIDE ? map->sidedefs[ld->side[1]].sector : -1;
		int						color;

		if ( ld->flags & DOOM_ML_DONTDRAW ) {
			continue;
		}
		if ( hud.mapped[i] ) {
			if ( back < 0 || front < 0 ) {
				color = 176;						// walls: red
			} else if ( ld->special == 39 ) {
				color = 176 + 8;					// teleporters
			} else if ( ld->flags & DOOM_ML_SECRET ) {
				color = 176;						// secret doors look like walls
			} else if ( map->sectors[front].floor_height != map->sectors[back].floor_height ) {
				color = 64;							// floor steps: brown
			} else if ( map->sectors[front].ceiling_height != map->sectors[back].ceiling_height ) {
				color = 231;						// ceiling steps: yellow
			} else {
				continue;
			}
		} else if ( ps->powerups[DPW_ALLMAP] ) {
			color = 96 + 3;							// the computer map: grey
		} else {
			continue;
		}
		AddLine( a->x, a->y, b->x, b->y, unitsPerPixel * sx, &pal[color * 3] );
	}

	// the player: a white arrow
	yaw = DEG2RAD( ps->viewangles[YAW] );
	for ( i = 0 ; i < (int)( sizeof( arrow ) / sizeof( arrow[0] ) ) ; i++ ) {
		float	c = cos( yaw ) * 2, s = sin( yaw ) * 2;
		float	ax = arrow[i][0], ay = arrow[i][1], bx = arrow[i][2], by = arrow[i][3];

		AddLine( ps->origin[0] + ax * c - ay * s, ps->origin[1] + ax * s + ay * c,
			ps->origin[0] + bx * c - by * s, ps->origin[1] + bx * s + by * c, unitsPerPixel * sx, &pal[209 * 3] );
	}

	// looking straight down from high above the player, north up
	memset( &rd, 0, sizeof( rd ) );
	rd.x = cg.refdef.x;
	rd.y = cg.refdef.y;
	rd.width = cg.refdef.width;
	rd.height = cg.refdef.height;
	rd.fov_x = 60;
	rd.fov_y = RAD2DEG( 2 * atan( tan( DEG2RAD( rd.fov_x ) * 0.5f ) * height / width ) );
	VectorSet( rd.vieworg, ps->origin[0], ps->origin[1], width * 0.5f / tan( DEG2RAD( rd.fov_x ) * 0.5f ) );
	VectorSet( rd.viewaxis[0], 0, 0, -1 );		// forward: down
	VectorSet( rd.viewaxis[1], -1, 0, 0 );		// left: west
	VectorSet( rd.viewaxis[2], 0, 1, 0 );		// up: north
	rd.time = cg.time;
	rd.rdflags = RDF_NOWORLDMODEL;
	trap_R_RenderScene( &rd );
}

/*
=================
CG_DoomDraw2D

Doom's screen over the 3D view: the automap or the weapon, the
invulnerability colormap, the status bar, the message line and the
palette flashes
=================
*/
void CG_DoomDraw2D( void ) {
	const playerState_t	*ps;

	if ( !hud.active ) {
		return;
	}
	ps = &cg.predictedPlayerState;
	HudTics();

	if ( ps->persistant[PERS_TEAM] != TEAM_SPECTATOR && ps->pm_type != PM_INTERMISSION ) {
		MarkSeenLines();
		if ( hud.automap ) {
			DrawAutomap( ps );
		} else {
			if ( cgs.doomWeapons == DOOM_WEAPONS_DOOM ) {
				CG_DoomDrawWeapon();
			}
			// invulnerability: Doom's inverse colormap over the view
			if ( CG_DoomPowerActive( DPW_INVULNERABILITY ) ) {
				trap_R_DrawStretchPic( cg.refdef.x, cg.refdef.y, cg.refdef.width, cg.refdef.height, 0, 0, 1, 1, hud.invert );
			}
		}
		if ( CG_DoomHudActive() ) {
			DrawStatusBar( ps );
		}
	}
	DrawMessage();
	PaletteBlend( ps );
}
