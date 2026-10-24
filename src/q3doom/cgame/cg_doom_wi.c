/*
===========================================================================
Q3Doom - Doom's intermission and episode finale in the cgame
(wi_stuff.c, f_finale.c)

When a level ends the game sends the tally in CS_DOOM_INTERMISSION and
freezes the players. The intermission runs here at 35 tics a second like
Doom's: the stats count up, then the episode map shows where you go next;
fire or use speeds it up. When it is done the game is told
("doom_worlddone") and loads the next map. After an episode's last map
comes its finale: the story text, then the end picture (the bunny scroll
for episode 3); a press leaves it ("doom_finaledone").

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "cg_local.h"
#include "../game/bg_doom.h"

#define NUMMAPS				9
#define WI_TITLEY			2
#define SP_STATSX			50
#define SP_STATSY			50
#define SP_TIMEX			16
#define SP_TIMEY			( 200 - 32 )
#define SHOWNEXTLOCDELAY	4
#define TEXTSPEED			3
#define TEXTWAIT			250

typedef enum { ANIM_ALWAYS, ANIM_RANDOM, ANIM_LEVEL } animenum_t;

typedef struct {
	animenum_t	type;
	int			period;
	int			nanims;
	int			x, y;
	int			data1;
	int			p[3];			// lumps
	int			nexttic;
	int			ctr;
} wianim_t;

static const int lnodes[3][NUMMAPS][2] = {
	{ { 185, 164 }, { 148, 143 }, { 69, 122 }, { 209, 102 }, { 116, 89 }, { 166, 55 }, { 71, 56 }, { 135, 29 }, { 71, 24 } },
	{ { 254, 25 }, { 97, 50 }, { 188, 64 }, { 128, 78 }, { 214, 92 }, { 133, 130 }, { 208, 136 }, { 148, 140 }, { 235, 158 } },
	{ { 156, 168 }, { 48, 154 }, { 174, 95 }, { 265, 75 }, { 130, 48 }, { 279, 23 }, { 198, 48 }, { 140, 25 }, { 281, 136 } }
};

static const wianim_t animinfo[3][10] = {
	{
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 224, 104 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 184, 160 },
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 112, 136 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 72, 112 },
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 88, 96 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 64, 48 },
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 192, 40 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 136, 16 },
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 80, 16 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 64, 24 }
	},
	{
		{ ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 1 }, { ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 2 },
		{ ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 3 }, { ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 4 },
		{ ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 5 }, { ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 6 },
		{ ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 7 }, { ANIM_LEVEL, DOOM_TICRATE / 3, 3, 192, 144, 8 },
		{ ANIM_LEVEL, DOOM_TICRATE / 3, 1, 128, 136, 8 }
	},
	{
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 104, 168 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 40, 136 },
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 160, 96 }, { ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 104, 80 },
		{ ANIM_ALWAYS, DOOM_TICRATE / 3, 3, 120, 32 }, { ANIM_ALWAYS, DOOM_TICRATE / 4, 3, 40, 0 }
	}
};
static const int numanims[3] = { 10, 9, 6 };

// the episode texts (d_englsh.h)
static const char *finaleTexts[4] = {
	"Once you beat the big badasses and\n"
	"clean out the moon base you're supposed\n"
	"to win, aren't you? Aren't you? Where's\n"
	"your fat reward and ticket home? What\n"
	"the hell is this? It's not supposed to\n"
	"end this way!\n"
	"\n"
	"It stinks like rotten meat, but looks\n"
	"like the lost Deimos base.  Looks like\n"
	"you're stuck on The Shores of Hell.\n"
	"The only way out is through.\n"
	"\n"
	"To continue the DOOM experience, play\n"
	"The Shores of Hell and its amazing\n"
	"sequel, Inferno!\n",

	"You've done it! The hideous cyber-\n"
	"demon lord that ruled the lost Deimos\n"
	"moon base has been slain and you\n"
	"are triumphant! But ... where are\n"
	"you? You clamber to the edge of the\n"
	"moon and look down to see the awful\n"
	"truth.\n"
	"\n"
	"Deimos floats above Hell itself!\n"
	"You've never heard of anyone escaping\n"
	"from Hell, but you'll make the bastards\n"
	"sorry they ever heard of you! Quickly,\n"
	"you rappel down to  the surface of\n"
	"Hell.\n"
	"\n"
	"Now, it's on to the final chapter of\n"
	"DOOM! -- Inferno.",

	"The loathsome spiderdemon that\n"
	"masterminded the invasion of the moon\n"
	"bases and caused so much death has had\n"
	"its ass kicked for all time.\n"
	"\n"
	"A hidden doorway opens and you enter.\n"
	"You've proven too tough for Hell to\n"
	"contain, and now Hell at last plays\n"
	"fair -- for you emerge from the door\n"
	"to see the green fields of Earth!\n"
	"Home at last.\n"
	"\n"
	"You wonder what's been happening on\n"
	"Earth while you were battling evil\n"
	"unleashed. It's good that no Hell-\n"
	"spawn could have come through that\n"
	"door with you ...",

	"the spider mastermind must have sent forth\n"
	"its legions of hellspawn before your\n"
	"final confrontation with that terrible\n"
	"beast from hell.  but you stepped forward\n"
	"and brought forth eternal damnation and\n"
	"suffering upon the horde as a true hero\n"
	"would in the face of something so evil.\n"
	"\n"
	"besides, someone was gonna pay for what\n"
	"happened to daisy, your pet rabbit.\n"
	"\n"
	"but now, you see spread before you more\n"
	"potential pain and gibbitude as a nation\n"
	"of demons run amok among our cities.\n"
	"\n"
	"next stop, hell on earth!"
};
static const char *finaleFlats[4] = { "FLOOR4_8", "SFLR6_1", "MFLR8_4", "MFLR8_3" };

typedef enum { WI_NONE, WI_STATCOUNT, WI_SHOWNEXTLOC, WI_NOSTATE, WI_FINALE } wistate_t;

static struct {
	wistate_t	state;
	int			lastTic;			// cg.time tics run so far
	qboolean	sent;				// the game was told we are done
	int			buttons;			// last usercmd buttons, for presses
	int			accelerate;

	// the tally (wbstartstruct_t)
	int			epsd, last, next, didsecret;
	int			kills, maxkills, items, maxitems, secrets, maxsecrets, stime, partime;

	// stats counting
	int			spState, cnt, bcnt, cntPause;
	int			cntKills, cntItems, cntSecret, cntTime, cntPar;
	qboolean	pointerOn;
	wianim_t	anims[10];

	// pictures
	int			bg, yah[2], splat, percent, colon, num[10], minus, finished, entering;
	int			kills_, items_, secret, time, par, sucks;
	int			lnames[NUMMAPS];

	// finale
	int			finaleStage, finaleCount, bunnyStage;
	qhandle_t	finaleFlat;
	const char	*finaleText;
} wi;

static void LocalSound( int sfx ) {
	sfxHandle_t h = CG_DoomSound( sfx );

	if ( h ) {
		trap_S_StartLocalSound( h, CHAN_LOCAL_SOUND );
	}
}

static int PatchWidth( int lump ) {
	int w, h, l, t;

	CG_DoomPatchSize( lump, &w, &h, &l, &t );
	return w;
}

static int PatchHeight( int lump ) {
	int w, h, l, t;

	CG_DoomPatchSize( lump, &w, &h, &l, &t );
	return h;
}

/*
=====================================================================

SETUP

=====================================================================
*/

static void LoadData( void ) {
	int		i, j;

	wi.bg = CG_DoomLump( wi.epsd < 3 ? va( "WIMAP%d", wi.epsd ) : "INTERPIC" );
	for ( i = 0 ; i < NUMMAPS ; i++ ) {
		wi.lnames[i] = CG_DoomLump( va( "WILV%d%d", wi.epsd, i ) );
	}
	wi.yah[0] = CG_DoomLump( "WIURH0" );
	wi.yah[1] = CG_DoomLump( "WIURH1" );
	wi.splat = CG_DoomLump( "WISPLAT" );
	if ( wi.epsd < 3 ) {
		for ( j = 0 ; j < numanims[wi.epsd] ; j++ ) {
			wi.anims[j] = animinfo[wi.epsd][j];
			for ( i = 0 ; i < wi.anims[j].nanims ; i++ ) {
				// MONDO HACK: the last episode 2 animation uses the frames of the 5th
				wi.anims[j].p[i] = CG_DoomLump( wi.epsd == 1 && j == 8 ? va( "WIA1%.2d%.2d", 4, i ) : va( "WIA%d%.2d%.2d", wi.epsd, j, i ) );
			}
		}
	}
	wi.minus = CG_DoomLump( "WIMINUS" );
	for ( i = 0 ; i < 10 ; i++ ) {
		wi.num[i] = CG_DoomLump( va( "WINUM%d", i ) );
	}
	wi.percent = CG_DoomLump( "WIPCNT" );
	wi.finished = CG_DoomLump( "WIF" );
	wi.entering = CG_DoomLump( "WIENTER" );
	wi.kills_ = CG_DoomLump( "WIOSTK" );
	wi.secret = CG_DoomLump( "WISCRT2" );
	wi.items_ = CG_DoomLump( "WIOSTI" );
	wi.colon = CG_DoomLump( "WICOLON" );
	wi.time = CG_DoomLump( "WITIME" );
	wi.sucks = CG_DoomLump( "WISUCKS" );
	wi.par = CG_DoomLump( "WIPAR" );
}

static void InitAnimatedBack( void ) {
	int		i;

	if ( wi.epsd > 2 ) {
		return;
	}
	for ( i = 0 ; i < numanims[wi.epsd] ; i++ ) {
		wianim_t *a = &wi.anims[i];

		a->ctr = -1;
		a->nexttic = a->type == ANIM_LEVEL ? wi.bcnt + 1 : wi.bcnt + 1 + rand() % a->period;
	}
}

/*
=================
CG_DoomIntermissionChanged

CS_DOOM_INTERMISSION: a level ended (the tally), an episode ended, or a
new level started (empty)
=================
*/
void CG_DoomIntermissionChanged( void ) {
	const char	*s = CG_ConfigString( CS_DOOM_INTERMISSION );
	int			episode;

	memset( &wi, 0, sizeof( wi ) );
	wi.lastTic = cg.time * DOOM_TICRATE / 1000;
	wi.buttons = -1;		// a button held from the level is not a press

	if ( sscanf( s, "wi %d %d %d %d %d %d %d %d %d %d %d %d", &wi.epsd, &wi.last, &wi.next, &wi.didsecret,
		&wi.kills, &wi.maxkills, &wi.items, &wi.maxitems, &wi.secrets, &wi.maxsecrets, &wi.stime, &wi.partime ) == 12 ) {
		wi.maxkills = wi.maxkills ? wi.maxkills : 1;
		wi.maxitems = wi.maxitems ? wi.maxitems : 1;
		wi.maxsecrets = wi.maxsecrets ? wi.maxsecrets : 1;
		wi.epsd = Com_Clamp( 0, 3, wi.epsd );
		wi.last = Com_Clamp( 0, NUMMAPS - 1, wi.last );
		wi.next = Com_Clamp( 0, NUMMAPS - 1, wi.next );
		LoadData();
		// WI_initStats
		wi.state = WI_STATCOUNT;
		wi.spState = 1;
		wi.cntKills = wi.cntItems = wi.cntSecret = -1;
		wi.cntTime = wi.cntPar = -1;
		wi.cntPause = DOOM_TICRATE;
		InitAnimatedBack();
		trap_S_StartBackgroundTrack( "doom/music/D_INTER", "" );
		return;
	}
	if ( sscanf( s, "finale %d", &episode ) == 1 ) {
		episode = Com_Clamp( 1, 4, episode );
		trap_S_StartBackgroundTrack( "doom/music/D_VICTOR", "" );
		wi.state = WI_FINALE;
		wi.epsd = episode - 1;
		wi.finaleText = finaleTexts[episode - 1];
		wi.finaleFlat = trap_R_RegisterShaderNoMip( va( "doom/flat/%s", finaleFlats[episode - 1] ) );
	}
}

/*
=====================================================================

TICKER (WI_Ticker, F_Ticker)

=====================================================================
*/

static void UpdateAnimatedBack( void ) {
	int		i;

	if ( wi.epsd > 2 ) {
		return;
	}
	for ( i = 0 ; i < numanims[wi.epsd] ; i++ ) {
		wianim_t *a = &wi.anims[i];

		if ( wi.bcnt != a->nexttic ) {
			continue;
		}
		switch ( a->type ) {
		case ANIM_ALWAYS:
			if ( ++a->ctr >= a->nanims ) {
				a->ctr = 0;
			}
			a->nexttic = wi.bcnt + a->period;
			break;
		case ANIM_LEVEL:
			// gawd-awful hack for level anims
			if ( !( wi.state == WI_STATCOUNT && i == 7 ) && wi.next == a->data1 ) {
				a->ctr++;
				if ( a->ctr == a->nanims ) {
					a->ctr--;
				}
				a->nexttic = wi.bcnt + a->period;
			}
			break;
		default:
			break;
		}
	}
}

static void Done( const char *command ) {
	if ( !wi.sent ) {
		wi.sent = qtrue;
		trap_SendClientCommand( command );
	}
}

static void UpdateStats( void ) {
	int		pk = wi.kills * 100 / wi.maxkills, pi = wi.items * 100 / wi.maxitems, ps = wi.secrets * 100 / wi.maxsecrets;
	int		t = wi.stime / DOOM_TICRATE, p = wi.partime / DOOM_TICRATE;

	UpdateAnimatedBack();

	if ( wi.accelerate && wi.spState != 10 ) {
		wi.accelerate = 0;
		wi.cntKills = pk;
		wi.cntItems = pi;
		wi.cntSecret = ps;
		wi.cntTime = t;
		wi.cntPar = p;
		LocalSound( sfx_barexp );
		wi.spState = 10;
	}
	if ( wi.spState == 2 || wi.spState == 4 || wi.spState == 6 ) {
		int	*cnt = wi.spState == 2 ? &wi.cntKills : wi.spState == 4 ? &wi.cntItems : &wi.cntSecret;
		int	target = wi.spState == 2 ? pk : wi.spState == 4 ? pi : ps;

		*cnt += 2;
		if ( !( wi.bcnt & 3 ) ) {
			LocalSound( sfx_pistol );
		}
		if ( *cnt >= target ) {
			*cnt = target;
			LocalSound( sfx_barexp );
			wi.spState++;
		}
	} else if ( wi.spState == 8 ) {
		if ( !( wi.bcnt & 3 ) ) {
			LocalSound( sfx_pistol );
		}
		wi.cntTime += 3;
		if ( wi.cntTime >= t ) {
			wi.cntTime = t;
		}
		wi.cntPar += 3;
		if ( wi.cntPar >= p ) {
			wi.cntPar = p;
			if ( wi.cntTime >= t ) {
				LocalSound( sfx_barexp );
				wi.spState++;
			}
		}
	} else if ( wi.spState == 10 ) {
		if ( wi.accelerate ) {
			LocalSound( sfx_sgcock );
			// WI_initShowNextLoc
			wi.state = WI_SHOWNEXTLOC;
			wi.accelerate = 0;
			wi.cnt = SHOWNEXTLOCDELAY * DOOM_TICRATE;
			InitAnimatedBack();
		}
	} else if ( wi.spState & 1 ) {
		if ( !--wi.cntPause ) {
			wi.spState++;
			wi.cntPause = DOOM_TICRATE;
		}
	}
}

static void Tic( void ) {
	wi.bcnt++;
	switch ( wi.state ) {
	case WI_STATCOUNT:
		UpdateStats();
		break;
	case WI_SHOWNEXTLOC:
		UpdateAnimatedBack();
		if ( !--wi.cnt || wi.accelerate ) {
			// WI_initNoState
			wi.state = WI_NOSTATE;
			wi.accelerate = 0;
			wi.cnt = 10;
		} else {
			wi.pointerOn = ( wi.cnt & 31 ) < 20;
		}
		break;
	case WI_NOSTATE:
		UpdateAnimatedBack();
		if ( wi.cnt > 0 && !--wi.cnt ) {
			Done( "doom_worlddone" );
		}
		break;
	case WI_FINALE:
		wi.finaleCount++;
		if ( !wi.finaleStage && wi.finaleCount > (int)strlen( wi.finaleText ) * TEXTSPEED + TEXTWAIT ) {
			wi.finaleCount = 0;
			wi.finaleStage = 1;
		}
		// a press on the end picture leaves it
		if ( wi.finaleStage && wi.accelerate && wi.finaleCount > DOOM_TICRATE ) {
			Done( "doom_finaledone" );
		}
		wi.accelerate = 0;
		break;
	default:
		break;
	}
}

// WI_checkForAccelerate: fire or use pressed
static void CheckForAccelerate( void ) {
	usercmd_t	cmd;
	int			buttons;

	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &cmd );
	buttons = cmd.buttons & ( BUTTON_ATTACK | BUTTON_USE );
	if ( wi.buttons >= 0 && ( buttons & ~wi.buttons ) ) {
		wi.accelerate = 1;
	}
	wi.buttons = buttons;
}

/*
=====================================================================

DRAWING (WI_Drawer, F_Drawer)

=====================================================================
*/

static void DrawAnimatedBack( void ) {
	int		i;

	if ( wi.epsd > 2 ) {
		return;
	}
	for ( i = 0 ; i < numanims[wi.epsd] ; i++ ) {
		wianim_t *a = &wi.anims[i];

		if ( a->ctr >= 0 ) {
			CG_DoomDrawPatch( a->x, a->y, a->p[a->ctr] );
		}
	}
}

// WI_drawNum: right aligned at x; digits < 0: as many as needed
static int DrawNum( int x, int y, int n, int digits ) {
	int		fontwidth = PatchWidth( wi.num[0] ), neg, temp;

	if ( digits < 0 ) {
		if ( !n ) {
			digits = 1;
		} else {
			for ( digits = 0, temp = n ; temp ; temp /= 10 ) {
				digits++;
			}
		}
	}
	neg = n < 0;
	if ( neg ) {
		n = -n;
	}
	while ( digits-- ) {
		x -= fontwidth;
		CG_DoomDrawPatch( x, y, wi.num[n % 10] );
		n /= 10;
	}
	if ( neg ) {
		CG_DoomDrawPatch( x -= 8, y, wi.minus );
	}
	return x;
}

static void DrawPercent( int x, int y, int p ) {
	if ( p < 0 ) {
		return;
	}
	CG_DoomDrawPatch( x, y, wi.percent );
	DrawNum( x, y, p, -1 );
}

// WI_drawTime: m:ss, or "sucks"
static void DrawTime( int x, int y, int t ) {
	int		div, n;

	if ( t < 0 ) {
		return;
	}
	if ( t <= 61 * 59 ) {
		div = 1;
		do {
			n = ( t / div ) % 60;
			x = DrawNum( x, y, n, 2 ) - PatchWidth( wi.colon );
			div *= 60;
			if ( div == 60 || t / div ) {
				CG_DoomDrawPatch( x, y, wi.colon );
			}
		} while ( t / div );
	} else {
		CG_DoomDrawPatch( x - PatchWidth( wi.sucks ), y, wi.sucks );
	}
}

// "<level> Finished!" and "Entering <level>"
static void DrawTitle( int first, int second ) {
	int		y = WI_TITLEY;

	CG_DoomDrawPatch( ( 320 - PatchWidth( first ) ) / 2, y, first );
	y += ( 5 * PatchHeight( first == wi.entering ? second : first ) ) / 4;
	CG_DoomDrawPatch( ( 320 - PatchWidth( second ) ) / 2, y, second );
}

// a patch on a level's place on the map, if it fits on the screen
static void DrawOnLnode( int n, const int *c, int count ) {
	int		i;

	for ( i = 0 ; i < count ; i++ ) {
		int w, h, l, t, left, top;

		CG_DoomPatchSize( c[i], &w, &h, &l, &t );
		left = lnodes[wi.epsd][n][0] - l;
		top = lnodes[wi.epsd][n][1] - t;
		if ( left >= 0 && left + w < 320 && top >= 0 && top + h < 200 ) {
			CG_DoomDrawPatch( lnodes[wi.epsd][n][0], lnodes[wi.epsd][n][1], c[i] );
			return;
		}
	}
}

static void DrawStats( void ) {
	int		lh = ( 3 * PatchHeight( wi.num[0] ) ) / 2;

	CG_DoomDrawPatch( 0, 0, wi.bg );
	DrawAnimatedBack();
	DrawTitle( wi.lnames[wi.last], wi.finished );

	CG_DoomDrawPatch( SP_STATSX, SP_STATSY, wi.kills_ );
	DrawPercent( 320 - SP_STATSX, SP_STATSY, wi.cntKills );
	CG_DoomDrawPatch( SP_STATSX, SP_STATSY + lh, wi.items_ );
	DrawPercent( 320 - SP_STATSX, SP_STATSY + lh, wi.cntItems );
	CG_DoomDrawPatch( SP_STATSX, SP_STATSY + 2 * lh, wi.secret );
	DrawPercent( 320 - SP_STATSX, SP_STATSY + 2 * lh, wi.cntSecret );

	CG_DoomDrawPatch( SP_TIMEX, SP_TIMEY, wi.time );
	DrawTime( 320 / 2 - SP_TIMEX, SP_TIMEY, wi.cntTime );
	if ( wi.epsd < 3 ) {
		CG_DoomDrawPatch( 320 / 2 + SP_TIMEX, SP_TIMEY, wi.par );
		DrawTime( 320 - SP_TIMEX, SP_TIMEY, wi.cntPar );
	}
}

static void DrawShowNextLoc( void ) {
	int		i, last;

	CG_DoomDrawPatch( 0, 0, wi.bg );
	DrawAnimatedBack();
	if ( wi.epsd <= 2 ) {
		last = wi.last == 8 ? wi.next - 1 : wi.last;
		// a splat on the levels done
		for ( i = 0 ; i <= last ; i++ ) {
			DrawOnLnode( i, &wi.splat, 1 );
		}
		if ( wi.didsecret ) {
			DrawOnLnode( 8, &wi.splat, 1 );
		}
		// the flashing "you are here"
		if ( wi.pointerOn ) {
			DrawOnLnode( wi.next, wi.yah, 2 );
		}
	}
	DrawTitle( wi.entering, wi.lnames[wi.next] );
}

// F_TextWrite
static void DrawFinaleText( void ) {
	const char	*ch = wi.finaleText;
	int			cx = 10, cy = 10, count = ( wi.finaleCount - 10 ) / TEXTSPEED;

	CG_DoomFillScreen( wi.finaleFlat );
	for ( ; count > 0 ; count-- ) {
		int c = *ch++, lump, w, h, l, t;

		if ( !c ) {
			break;
		}
		if ( c == '\n' ) {
			cx = 10;
			cy += 11;
			continue;
		}
		lump = CG_DoomFontLump( c );
		if ( lump < 0 ) {
			cx += 4;
			continue;
		}
		CG_DoomPatchSize( lump, &w, &h, &l, &t );
		if ( cx + w > 320 ) {
			break;
		}
		CG_DoomDrawPatch( cx, cy, lump );
		cx += w;
	}
}

// F_BunnyScroll: the green fields scroll by, then "THE END" is shot in
static void DrawBunnyScroll( void ) {
	int		scrolled = 320 - ( wi.finaleCount - 230 ) / 2, stage;

	scrolled = scrolled > 320 ? 320 : scrolled < 0 ? 0 : scrolled;
	CG_DoomDrawPatchColumns( CG_DoomLump( "PFUB2" ), 0, scrolled, 320 );
	CG_DoomDrawPatchColumns( CG_DoomLump( "PFUB1" ), 320 - scrolled, 0, scrolled );

	if ( wi.finaleCount < 1130 ) {
		return;
	}
	if ( wi.finaleCount < 1180 ) {
		CG_DoomDrawPatch( ( 320 - 13 * 8 ) / 2, ( 200 - 8 * 8 ) / 2, CG_DoomLump( "END0" ) );
		wi.bunnyStage = 0;
		return;
	}
	stage = ( wi.finaleCount - 1180 ) / 5;
	if ( stage > 6 ) {
		stage = 6;
	}
	if ( stage > wi.bunnyStage ) {
		LocalSound( sfx_pistol );
		wi.bunnyStage = stage;
	}
	CG_DoomDrawPatch( ( 320 - 13 * 8 ) / 2, ( 200 - 8 * 8 ) / 2, CG_DoomLump( va( "END%i", stage ) ) );
}

static void DrawFinale( void ) {
	if ( !wi.finaleStage ) {
		DrawFinaleText();
		return;
	}
	CG_DoomFillScreen( 0 );
	switch ( wi.epsd ) {
	case 0:
		CG_DoomDrawPatch( 0, 0, CG_DoomLump( "CREDIT" ) );
		break;
	case 1:
		CG_DoomDrawPatch( 0, 0, CG_DoomLump( "VICTORY2" ) );
		break;
	case 2:
		DrawBunnyScroll();
		break;
	default:
		CG_DoomDrawPatch( 0, 0, CG_DoomLump( "ENDPIC" ) );
		break;
	}
}

/*
=================
CG_DoomDrawIntermission

Runs and draws the intermission or the finale; qfalse when there is none
=================
*/
qboolean CG_DoomDrawIntermission( void ) {
	int		tic = cg.time * DOOM_TICRATE / 1000, steps = 0;

	if ( wi.state == WI_NONE ) {
		return qfalse;
	}
	CheckForAccelerate();
	if ( tic < wi.lastTic ) {
		wi.lastTic = tic;
	}
	while ( wi.lastTic < tic && steps++ < DOOM_TICRATE ) {
		wi.lastTic++;
		Tic();
	}
	wi.lastTic = tic;

	if ( wi.state == WI_FINALE ) {
		DrawFinale();
		return qtrue;
	}
	CG_DoomFillScreen( 0 );
	if ( wi.state == WI_STATCOUNT ) {
		DrawStats();
	} else {
		if ( wi.state == WI_NOSTATE ) {
			wi.pointerOn = qtrue;
		}
		DrawShowNextLoc();
	}
	return qtrue;
}
