/*
===========================================================================
Q3Doom - the single player menus

SINGLE PLAYER opens a choice between the Doom Campaign and DoomFrag.
The Campaign screen shows the 36 levels of The Ultimate Doom as tiles, a
row per episode: the level's first frame (captured the first time it is
played), its name and the best medal earned; locked levels are dark with
a padlock. A click selects a level, Play or a double click starts it at
the difficulty picked below the tiles. The progress comes from
campaign.dat, written by the game (libdoom's doom_progress_*).

The DoomFrag screen lists every map with the best time for the options
picked below the list (doomfrag.dat, libdoom's doom_records_*) and the par
time; Start or a double click runs the map (the doomfrag command).

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "ui_local.h"
#include "../../libdoom/doom.h"

#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"

#define NUM_EPISODES		4
#define NUM_MAPS			9
#define NUM_LEVELS			( NUM_EPISODES * NUM_MAPS )

#define TILE_W				64
#define TILE_H				48
#define TILE_GAP			4
#define TILE_X				( ( 640 - NUM_MAPS * TILE_W - ( NUM_MAPS - 1 ) * TILE_GAP ) / 2 )
#define TILE_Y				70
#define ROW_H				78

#define DOUBLECLICK_MSEC	400

enum {
	ID_CAMPAIGN = 10,
	ID_DOOMFRAG,
	ID_BACK,
	ID_PLAY,
	ID_SKILL,
	ID_CAMPAIGNWEAPONS,
	ID_MAPLIST,
	ID_PHYSICS,
	ID_WEAPONS,
	ID_LOADOUT,
	ID_MONSTERS,
	ID_FINISH,
	ID_START,
	ID_TILE0 = 100
};

// the episode and level names (d_englsh.h)
static const char *episodeNames[NUM_EPISODES] = {
	"Knee-Deep in the Dead", "The Shores of Hell", "Inferno", "Thy Flesh Consumed"
};

static const char *levelNames[NUM_EPISODES][NUM_MAPS] = {
	{ "Hangar", "Nuclear Plant", "Toxin Refinery", "Command Control", "Phobos Lab",
	  "Central Processing", "Computer Station", "Phobos Anomaly", "Military Base" },
	{ "Deimos Anomaly", "Containment Area", "Refinery", "Deimos Lab", "Command Center",
	  "Halls of the Damned", "Spawning Vats", "Tower of Babel", "Fortress of Mystery" },
	{ "Hell Keep", "Slough of Despair", "Pandemonium", "House of Pain", "Unholy Cathedral",
	  "Mt. Erebus", "Limbo", "Dis", "Warrens" },
	{ "Hell Beneath", "Perfect Hatred", "Sever the Wicked", "Unruly Evil", "They Will Repent",
	  "Against Thee Wickedly", "And Hell Followed", "Unto the Cruel", "Fear" }
};

static const char *skillNames[] = {
	"I'm too young to die.", "Hey, not too rough.", "Hurt me plenty.", "Ultra-Violence.", "Nightmare!", NULL
};

static const char *campaignWeaponNames[] = { "Doom", "Quake 3", NULL };

static const char *medalNames[] = { "", "Bronze", "Silver", "Gold", "Skull" };

static vec4_t	tileDark = { 0.12f, 0.08f, 0.06f, 1 };
static vec4_t	tileShade = { 0, 0, 0, 0.65f };
static vec4_t	doomRed = { 0.85f, 0.1f, 0.05f, 1 };

/*
=====================================================================

SINGLE PLAYER: the Doom Campaign or DoomFrag

=====================================================================
*/

static struct {
	menuframework_s	menu;
	menutext_s		banner;
	menutext_s		campaign;
	menutext_s		doomfrag;
	menubitmap_s	back;
} s_doomsp;

static void DoomSP_Event( void *ptr, int notification ) {
	if ( notification != QM_ACTIVATED ) {
		return;
	}
	switch ( ( (menucommon_s *)ptr )->id ) {
	case ID_CAMPAIGN:
		UI_DoomCampaignMenu();
		break;
	case ID_DOOMFRAG:
		UI_DoomFragMenu();
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

void UI_DoomSinglePlayerMenu( void ) {
	memset( &s_doomsp, 0, sizeof( s_doomsp ) );
	s_doomsp.menu.fullscreen = qtrue;
	s_doomsp.menu.wrapAround = qtrue;

	s_doomsp.banner.generic.type = MTYPE_BTEXT;
	s_doomsp.banner.generic.x = 320;
	s_doomsp.banner.generic.y = 16;
	s_doomsp.banner.string = "SINGLE PLAYER";
	s_doomsp.banner.color = color_white;
	s_doomsp.banner.style = UI_CENTER;

	s_doomsp.campaign.generic.type = MTYPE_PTEXT;
	s_doomsp.campaign.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_doomsp.campaign.generic.x = 320;
	s_doomsp.campaign.generic.y = 196;
	s_doomsp.campaign.generic.id = ID_CAMPAIGN;
	s_doomsp.campaign.generic.callback = DoomSP_Event;
	s_doomsp.campaign.string = "DOOM CAMPAIGN";
	s_doomsp.campaign.color = color_red;
	s_doomsp.campaign.style = UI_CENTER;

	s_doomsp.doomfrag.generic.type = MTYPE_PTEXT;
	s_doomsp.doomfrag.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS;
	s_doomsp.doomfrag.generic.x = 320;
	s_doomsp.doomfrag.generic.y = 196 + 40;
	s_doomsp.doomfrag.generic.id = ID_DOOMFRAG;
	s_doomsp.doomfrag.generic.callback = DoomSP_Event;
	s_doomsp.doomfrag.string = "DOOMFRAG";
	s_doomsp.doomfrag.color = color_red;
	s_doomsp.doomfrag.style = UI_CENTER;

	s_doomsp.back.generic.type = MTYPE_BITMAP;
	s_doomsp.back.generic.name = ART_BACK0;
	s_doomsp.back.generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_doomsp.back.generic.id = ID_BACK;
	s_doomsp.back.generic.callback = DoomSP_Event;
	s_doomsp.back.generic.x = 0;
	s_doomsp.back.generic.y = 480 - 64;
	s_doomsp.back.width = 128;
	s_doomsp.back.height = 64;
	s_doomsp.back.focuspic = ART_BACK1;

	Menu_AddItem( &s_doomsp.menu, &s_doomsp.banner );
	Menu_AddItem( &s_doomsp.menu, &s_doomsp.campaign );
	Menu_AddItem( &s_doomsp.menu, &s_doomsp.doomfrag );
	Menu_AddItem( &s_doomsp.menu, &s_doomsp.back );
	UI_PushMenu( &s_doomsp.menu );
}

/*
=====================================================================

DOOM CAMPAIGN

=====================================================================
*/

static struct {
	menuframework_s	menu;
	menutext_s		banner;
	menubitmap_s	tiles[NUM_LEVELS];
	menulist_s		skill;
	menulist_s		weapons;
	menutext_s		play;
	menubitmap_s	back;

	doom_progress_t	progress;
	qhandle_t		shots[NUM_LEVELS];
	qhandle_t		lock, medals[4], skull;
	int				selected;
	int				lastClick, lastClickTime;
} s_campaign;

static const char *LevelMap( int level ) {
	return va( "E%dM%d", level / NUM_MAPS + 1, level % NUM_MAPS + 1 );
}

static const doom_progress_level_t *LevelProgress( int level ) {
	return doom_progress_level( &s_campaign.progress, LevelMap( level ), 0 );
}

static qboolean LevelUnlocked( int level ) {
	const doom_progress_level_t *l = LevelProgress( level );

	return l && l->unlocked;
}

static void LoadProgress( void ) {
	fileHandle_t	f;
	int				len = trap_FS_FOpenFile( "campaign.dat", &f, FS_READ );
	static char		text[16384];

	text[0] = 0;
	if ( f ) {
		if ( len > 0 && len < (int)sizeof( text ) ) {
			trap_FS_Read( text, len, f );
			text[len] = 0;
		}
		trap_FS_FCloseFile( f );
	}
	doom_progress_parse( text, &s_campaign.progress );
}

static void DrawMedal( float x, float y, float size, int medal ) {
	if ( medal <= DOOM_MEDAL_NONE || medal > DOOM_MEDAL_SKULL ) {
		return;
	}
	UI_DrawHandlePic( x, y, size, size, s_campaign.medals[medal - 1] );
	if ( medal == DOOM_MEDAL_SKULL ) {
		UI_DrawHandlePic( x + size * 0.15f, y + size * 0.15f, size * 0.7f, size * 0.7f, s_campaign.skull );
	}
}

/*
=================
Tile_Draw

A level: its picture (or its name on a dark tile), its map number, the
best medal; locked levels are shaded with a padlock
=================
*/
static void Tile_Draw( void *self ) {
	menubitmap_s				*b = (menubitmap_s *)self;
	int							level = b->generic.id - ID_TILE0;
	float						x = b->generic.x, y = b->generic.y, w = b->width, h = b->height;
	const doom_progress_level_t	*l = LevelProgress( level );
	qboolean					focused = Menu_ItemAtCursor( &s_campaign.menu ) == self;

	if ( s_campaign.shots[level] ) {
		UI_DrawHandlePic( x, y, w, h, s_campaign.shots[level] );
	} else {
		UI_FillRect( x, y, w, h, tileDark );
	}
	UI_DrawString( x + 3, y + h - SMALLCHAR_HEIGHT - 1, LevelMap( level ), UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW, colorWhite );

	if ( !l || !l->unlocked ) {
		UI_FillRect( x, y, w, h, tileShade );
		UI_DrawHandlePic( x + w * 0.5f - 12, y + h * 0.5f - 14, 24, 24, s_campaign.lock );
	} else {
		DrawMedal( x + w - 19, y + 3, 16, l->medal );
	}

	if ( level == s_campaign.selected ) {
		UI_DrawRect( x - 2, y - 2, w + 4, h + 4, colorYellow );
		UI_DrawRect( x - 1, y - 1, w + 2, h + 2, colorYellow );
	} else if ( focused ) {
		UI_DrawRect( x - 1, y - 1, w + 2, h + 2, colorWhite );
	}
}

static void Campaign_Play( void ) {
	if ( !LevelUnlocked( s_campaign.selected ) ) {
		return;
	}
	trap_Cvar_SetValue( "g_doomSkill", s_campaign.skill.curvalue );
	trap_Cvar_SetValue( "g_doomCampaignWeapons", s_campaign.weapons.curvalue );
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "doom_campaign %s %d\n", LevelMap( s_campaign.selected ), s_campaign.skill.curvalue ) );
}

static void Campaign_Event( void *ptr, int notification ) {
	int		id = ( (menucommon_s *)ptr )->id;

	if ( notification != QM_ACTIVATED ) {
		return;
	}
	if ( id >= ID_TILE0 && id < ID_TILE0 + NUM_LEVELS ) {
		int level = id - ID_TILE0;

		if ( !LevelUnlocked( level ) ) {
			return;
		}
		// a second click on the same tile plays it
		if ( level == s_campaign.selected && s_campaign.lastClick == level && uis.realtime - s_campaign.lastClickTime < DOUBLECLICK_MSEC ) {
			Campaign_Play();
			return;
		}
		s_campaign.selected = level;
		s_campaign.lastClick = level;
		s_campaign.lastClickTime = uis.realtime;
		return;
	}
	switch ( id ) {
	case ID_PLAY:
		Campaign_Play();
		break;
	case ID_SKILL:
		trap_Cvar_SetValue( "g_doomSkill", s_campaign.skill.curvalue );
		break;
	case ID_CAMPAIGNWEAPONS:
		trap_Cvar_SetValue( "g_doomCampaignWeapons", s_campaign.weapons.curvalue );
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
Campaign_Draw

The episode names over their rows, the selected level's details
=================
*/
static void Campaign_Draw( void ) {
	const doom_progress_level_t	*l = LevelProgress( s_campaign.selected );
	int							e, par, y = TILE_Y + NUM_EPISODES * ROW_H - 8;
	char						best[32];

	for ( e = 0 ; e < NUM_EPISODES ; e++ ) {
		UI_DrawString( TILE_X, TILE_Y + e * ROW_H - SMALLCHAR_HEIGHT - 4, va( "Episode %d: %s", e + 1, episodeNames[e] ),
			UI_LEFT | UI_SMALLFONT, doomRed );
	}

	// the selected level
	UI_DrawString( 320, y, va( "%s: %s", LevelMap( s_campaign.selected ),
		levelNames[s_campaign.selected / NUM_MAPS][s_campaign.selected % NUM_MAPS] ), UI_CENTER | UI_SMALLFONT, colorWhite );
	par = doom_par_time( s_campaign.selected / NUM_MAPS + 1, s_campaign.selected % NUM_MAPS + 1 );
	if ( l && l->best_tics ) {
		Com_sprintf( best, sizeof( best ), "%d:%02d", l->best_tics / 35 / 60, l->best_tics / 35 % 60 );
	} else {
		Q_strncpyz( best, "-", sizeof( best ) );
	}
	UI_DrawString( 320, y + 20, va( "Medal: %s   Best time: %s   Par: %s", l && l->medal ? medalNames[l->medal] : "none", best,
		par ? va( "%d:%02d", par / 60, par % 60 ) : "-" ), UI_CENTER | UI_SMALLFONT, colorLtGrey );
	if ( l && l->medal ) {
		DrawMedal( 320 - 120, y + 18, 18, l->medal );
	}

	Menu_Draw( &s_campaign.menu );
}

void UI_DoomCampaignMenu( void ) {
	int		i, last = 0;

	memset( &s_campaign, 0, sizeof( s_campaign ) );
	s_campaign.menu.fullscreen = qtrue;
	s_campaign.menu.draw = Campaign_Draw;
	s_campaign.lastClick = -1;
	LoadProgress();

	s_campaign.lock = trap_R_RegisterShaderNoMip( "doom/gen/LOCK" );
	for ( i = 0 ; i < 4 ; i++ ) {
		s_campaign.medals[i] = trap_R_RegisterShaderNoMip( va( "doom/gen/MEDAL%d", i + 1 ) );
	}
	s_campaign.skull = trap_R_RegisterShaderNoMip( "doom/patch/M_SKULL1" );

	s_campaign.banner.generic.type = MTYPE_BTEXT;
	s_campaign.banner.generic.x = 320;
	s_campaign.banner.generic.y = 16;
	s_campaign.banner.string = "DOOM CAMPAIGN";
	s_campaign.banner.color = color_white;
	s_campaign.banner.style = UI_CENTER;
	Menu_AddItem( &s_campaign.menu, &s_campaign.banner );

	for ( i = 0 ; i < NUM_LEVELS ; i++ ) {
		menubitmap_s *t = &s_campaign.tiles[i];

		t->generic.type = MTYPE_BITMAP;
		t->generic.flags = QMF_LEFT_JUSTIFY | ( LevelUnlocked( i ) ? 0 : QMF_INACTIVE );
		t->generic.id = ID_TILE0 + i;
		t->generic.callback = Campaign_Event;
		t->generic.ownerdraw = Tile_Draw;
		t->generic.x = TILE_X + ( i % NUM_MAPS ) * ( TILE_W + TILE_GAP );
		t->generic.y = TILE_Y + ( i / NUM_MAPS ) * ROW_H;
		t->width = TILE_W;
		t->height = TILE_H;
		Menu_AddItem( &s_campaign.menu, t );

		// the first frame of the level, once played
		s_campaign.shots[i] = trap_R_RegisterShaderNoMip( va( "levelshots/doom/%s", LevelMap( i ) ) );
		if ( LevelUnlocked( i ) ) {
			last = i;
		}
	}
	// the furthest level reached
	s_campaign.selected = last;

	s_campaign.skill.generic.type = MTYPE_SPINCONTROL;
	s_campaign.skill.generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_campaign.skill.generic.name = "Difficulty:";
	s_campaign.skill.generic.id = ID_SKILL;
	s_campaign.skill.generic.callback = Campaign_Event;
	s_campaign.skill.generic.x = 320;
	s_campaign.skill.generic.y = TILE_Y + NUM_EPISODES * ROW_H + 40;
	s_campaign.skill.itemnames = skillNames;
	s_campaign.skill.curvalue = (int)Com_Clamp( 0, 4, trap_Cvar_VariableValue( "g_doomSkill" ) );
	Menu_AddItem( &s_campaign.menu, &s_campaign.skill );

	// Doom's or Quake 3's weapons: either set progresses the campaign
	s_campaign.weapons.generic.type = MTYPE_SPINCONTROL;
	s_campaign.weapons.generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s_campaign.weapons.generic.name = "Weapons:";
	s_campaign.weapons.generic.id = ID_CAMPAIGNWEAPONS;
	s_campaign.weapons.generic.callback = Campaign_Event;
	s_campaign.weapons.generic.x = 320;
	s_campaign.weapons.generic.y = s_campaign.skill.generic.y + 18;
	s_campaign.weapons.itemnames = campaignWeaponNames;
	s_campaign.weapons.curvalue = (int)Com_Clamp( 0, 1, trap_Cvar_VariableValue( "g_doomCampaignWeapons" ) );
	Menu_AddItem( &s_campaign.menu, &s_campaign.weapons );

	s_campaign.play.generic.type = MTYPE_PTEXT;
	s_campaign.play.generic.flags = QMF_RIGHT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_campaign.play.generic.id = ID_PLAY;
	s_campaign.play.generic.callback = Campaign_Event;
	s_campaign.play.generic.x = 640 - 24;
	s_campaign.play.generic.y = 480 - 48;
	s_campaign.play.string = "PLAY";
	s_campaign.play.color = color_red;
	s_campaign.play.style = UI_RIGHT;
	Menu_AddItem( &s_campaign.menu, &s_campaign.play );

	s_campaign.back.generic.type = MTYPE_BITMAP;
	s_campaign.back.generic.name = ART_BACK0;
	s_campaign.back.generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_campaign.back.generic.id = ID_BACK;
	s_campaign.back.generic.callback = Campaign_Event;
	s_campaign.back.generic.x = 0;
	s_campaign.back.generic.y = 480 - 64;
	s_campaign.back.width = 128;
	s_campaign.back.height = 64;
	s_campaign.back.focuspic = ART_BACK1;
	Menu_AddItem( &s_campaign.menu, &s_campaign.back );

	UI_PushMenu( &s_campaign.menu );
	Menu_SetCursorToItem( &s_campaign.menu, &s_campaign.tiles[s_campaign.selected] );
}

/*
=====================================================================

DOOMFRAG

=====================================================================
*/

#define FRAG_ROWS			16
#define FRAG_LIST_Y			72
#define FRAG_COLS			46		// characters in a row of the list

static const char *physicsNames[] = { "VQ3", "CPM", NULL };
static const char *weaponSetNames[] = { "Doom", "Quake 3", "None", NULL };
static const char *loadoutNames[] = { "Pick up", "All from the start", NULL };
static const char *monsterNames[] = {
	"I'm too young to die.", "Hey, not too rough.", "Hurt me plenty.", "Ultra-Violence.", "Nightmare!", "No monsters", NULL
};
static const char *finishNames[] = { "Exit", "100% kills, then exit", NULL };

static struct {
	menuframework_s	menu;
	menutext_s		banner;
	menulist_s		maps;
	menulist_s		physics;
	menulist_s		weapons;
	menulist_s		loadout;
	menulist_s		monsters;
	menulist_s		finish;
	menutext_s		start;
	menubitmap_s	back;

	doom_records_t	records;
	char			rows[NUM_LEVELS][FRAG_COLS + 1];
	const char		*rowPtrs[NUM_LEVELS + 1];
	int				lastClickTime, lastClickRow;
} s_frag;

static int	fragLastMap;		// the map picked the last time the screen was open

static const char *FragTime( int msec ) {
	return va( "%d:%02d.%03d", msec / 60000, msec / 1000 % 60, msec % 1000 );
}

static void Frag_LoadRecords( void ) {
	fileHandle_t	f;
	int				len = trap_FS_FOpenFile( "doomfrag.dat", &f, FS_READ );
	static char		text[65536];

	text[0] = 0;
	if ( f ) {
		if ( len > 0 && len < (int)sizeof( text ) ) {
			trap_FS_Read( text, len, f );
			text[len] = 0;
		}
		trap_FS_FCloseFile( f );
	}
	doom_records_parse( text, &s_frag.records );
}

// the options picked, as recorded with the times (g_doom_frag.c)
static void Frag_Options( char *out, int size ) {
	doom_frag_options( out, size, s_frag.physics.curvalue, s_frag.weapons.curvalue, s_frag.loadout.curvalue,
		s_frag.monsters.curvalue, s_frag.finish.curvalue );
}

/*
=================
Frag_BuildRows

A row per map: the map, its name, the best time with the options picked
and the par time
=================
*/
static void Frag_BuildRows( void ) {
	char	options[DOOM_OPTIONS_LEN + 1];
	int		i;

	Frag_Options( options, sizeof( options ) );
	for ( i = 0 ; i < NUM_LEVELS ; i++ ) {
		int		best = doom_record_get( &s_frag.records, LevelMap( i ), options );
		int		par = doom_par_time( i / NUM_MAPS + 1, i % NUM_MAPS + 1 );

		Com_sprintf( s_frag.rows[i], sizeof( s_frag.rows[i] ), "%-5s %-21.21s %10s %6s", LevelMap( i ),
			levelNames[i / NUM_MAPS][i % NUM_MAPS], best ? FragTime( best ) : "-", par ? va( "%d:%02d", par / 60, par % 60 ) : "-" );
		s_frag.rowPtrs[i] = s_frag.rows[i];
	}
	s_frag.rowPtrs[NUM_LEVELS] = NULL;
}

static void Frag_Start( void ) {
	int		monsters = s_frag.monsters.curvalue;

	fragLastMap = s_frag.maps.curvalue;
	trap_Cvar_SetValue( "g_doomPhysics", s_frag.physics.curvalue );
	trap_Cvar_SetValue( "g_doomWeapons", s_frag.weapons.curvalue );
	trap_Cvar_SetValue( "g_doomLoadout", s_frag.loadout.curvalue );
	trap_Cvar_SetValue( "g_doomFinish", s_frag.finish.curvalue );
	trap_Cvar_SetValue( "g_doomMonsters", monsters < 5 );
	if ( monsters < 5 ) {
		trap_Cvar_SetValue( "g_doomSkill", monsters );
	}
	trap_Cmd_ExecuteText( EXEC_APPEND, va( "doomfrag %s\n", LevelMap( s_frag.maps.curvalue ) ) );
}

static void Frag_Event( void *ptr, int notification ) {
	int		id = ( (menucommon_s *)ptr )->id;

	if ( id == ID_MAPLIST ) {
		fragLastMap = s_frag.maps.curvalue;
		return;
	}
	if ( notification != QM_ACTIVATED ) {
		return;
	}
	switch ( id ) {
	case ID_PHYSICS:
	case ID_WEAPONS:
	case ID_LOADOUT:
	case ID_MONSTERS:
	case ID_FINISH:
		Frag_BuildRows();
		break;
	case ID_START:
		Frag_Start();
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
Frag_Key

A double click on a map runs it; the mouse wheel scrolls the list
=================
*/
static sfxHandle_t Frag_Key( int key ) {
	if ( Menu_ItemAtCursor( &s_frag.menu ) == &s_frag.maps ) {
		if ( key == K_MWHEELUP ) {
			key = K_UPARROW;
		} else if ( key == K_MWHEELDOWN ) {
			key = K_DOWNARROW;
		} else if ( key == K_ENTER || key == K_KP_ENTER ) {
			Frag_Start();
			return menu_in_sound;
		} else if ( key == K_MOUSE1 ) {
			int			before = s_frag.maps.curvalue;
			sfxHandle_t	s = Menu_DefaultKey( &s_frag.menu, key );

			if ( s_frag.maps.curvalue == before && s_frag.lastClickRow == before
				&& uis.realtime - s_frag.lastClickTime < DOUBLECLICK_MSEC ) {
				Frag_Start();
				return menu_in_sound;
			}
			s_frag.lastClickRow = s_frag.maps.curvalue;
			s_frag.lastClickTime = uis.realtime;
			return s;
		}
	}
	return Menu_DefaultKey( &s_frag.menu, key );
}

static void Frag_Draw( void ) {
	UI_DrawString( s_frag.maps.generic.x, FRAG_LIST_Y - SMALLCHAR_HEIGHT - 6,
		va( "%-5s %-21s %10s %6s", "MAP", "NAME", "BEST", "PAR" ), UI_LEFT | UI_SMALLFONT, doomRed );
	UI_DrawRect( s_frag.maps.generic.x - 4, FRAG_LIST_Y - 4, FRAG_COLS * SMALLCHAR_WIDTH + 8, FRAG_ROWS * SMALLCHAR_HEIGHT + 8, colorMdGrey );
	Menu_Draw( &s_frag.menu );
}

static void Frag_Spin( menulist_s *s, const char *name, int id, const char **items, int value, int y ) {
	s->generic.type = MTYPE_SPINCONTROL;
	s->generic.flags = QMF_PULSEIFFOCUS | QMF_SMALLFONT;
	s->generic.name = name;
	s->generic.id = id;
	s->generic.callback = Frag_Event;
	s->generic.x = 320;
	s->generic.y = y;
	s->itemnames = items;
	s->curvalue = value;
	Menu_AddItem( &s_frag.menu, s );
}

void UI_DoomFragMenu( void ) {
	int		y = FRAG_LIST_Y + FRAG_ROWS * SMALLCHAR_HEIGHT + 20;

	memset( &s_frag, 0, sizeof( s_frag ) );
	s_frag.menu.fullscreen = qtrue;
	s_frag.menu.draw = Frag_Draw;
	s_frag.menu.key = Frag_Key;
	s_frag.lastClickRow = -1;
	Frag_LoadRecords();

	s_frag.banner.generic.type = MTYPE_BTEXT;
	s_frag.banner.generic.x = 320;
	s_frag.banner.generic.y = 16;
	s_frag.banner.string = "DOOMFRAG";
	s_frag.banner.color = color_white;
	s_frag.banner.style = UI_CENTER;
	Menu_AddItem( &s_frag.menu, &s_frag.banner );

	// the options first: the rows show the records for them
	Frag_Spin( &s_frag.physics, "Physics:", ID_PHYSICS, physicsNames, (int)Com_Clamp( 0, 1, trap_Cvar_VariableValue( "g_doomPhysics" ) ), y );
	Frag_Spin( &s_frag.weapons, "Weapons:", ID_WEAPONS, weaponSetNames, (int)Com_Clamp( 0, 2, trap_Cvar_VariableValue( "g_doomWeapons" ) ), y + 18 );
	Frag_Spin( &s_frag.loadout, "Loadout:", ID_LOADOUT, loadoutNames, (int)Com_Clamp( 0, 1, trap_Cvar_VariableValue( "g_doomLoadout" ) ), y + 36 );
	Frag_Spin( &s_frag.monsters, "Monsters:", ID_MONSTERS, monsterNames,
		trap_Cvar_VariableValue( "g_doomMonsters" ) ? (int)Com_Clamp( 0, 4, trap_Cvar_VariableValue( "g_doomSkill" ) ) : 5, y + 54 );
	Frag_Spin( &s_frag.finish, "Finish:", ID_FINISH, finishNames, (int)Com_Clamp( 0, 1, trap_Cvar_VariableValue( "g_doomFinish" ) ), y + 72 );
	Frag_BuildRows();

	s_frag.maps.generic.type = MTYPE_SCROLLLIST;
	s_frag.maps.generic.flags = QMF_PULSEIFFOCUS;
	s_frag.maps.generic.id = ID_MAPLIST;
	s_frag.maps.generic.callback = Frag_Event;
	s_frag.maps.generic.x = 320 - FRAG_COLS * SMALLCHAR_WIDTH / 2;
	s_frag.maps.generic.y = FRAG_LIST_Y;
	s_frag.maps.width = FRAG_COLS;
	s_frag.maps.height = FRAG_ROWS;
	s_frag.maps.numitems = NUM_LEVELS;
	s_frag.maps.itemnames = s_frag.rowPtrs;
	Menu_AddItem( &s_frag.menu, &s_frag.maps );
	s_frag.maps.curvalue = fragLastMap;
	s_frag.maps.top = fragLastMap >= FRAG_ROWS ? fragLastMap - FRAG_ROWS + 1 : 0;

	s_frag.start.generic.type = MTYPE_PTEXT;
	s_frag.start.generic.flags = QMF_RIGHT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_frag.start.generic.id = ID_START;
	s_frag.start.generic.callback = Frag_Event;
	s_frag.start.generic.x = 640 - 24;
	s_frag.start.generic.y = 480 - 48;
	s_frag.start.string = "START";
	s_frag.start.color = color_red;
	s_frag.start.style = UI_RIGHT;
	Menu_AddItem( &s_frag.menu, &s_frag.start );

	s_frag.back.generic.type = MTYPE_BITMAP;
	s_frag.back.generic.name = ART_BACK0;
	s_frag.back.generic.flags = QMF_LEFT_JUSTIFY | QMF_PULSEIFFOCUS;
	s_frag.back.generic.id = ID_BACK;
	s_frag.back.generic.callback = Frag_Event;
	s_frag.back.generic.x = 0;
	s_frag.back.generic.y = 480 - 64;
	s_frag.back.width = 128;
	s_frag.back.height = 64;
	s_frag.back.focuspic = ART_BACK1;
	Menu_AddItem( &s_frag.menu, &s_frag.back );

	UI_PushMenu( &s_frag.menu );
	Menu_SetCursorToItem( &s_frag.menu, &s_frag.maps );
}