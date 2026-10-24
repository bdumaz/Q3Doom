/*
===========================================================================
Q3Doom - the single player menus

SINGLE PLAYER opens a choice between the Doom Campaign and DoomFrag (M9).
The Campaign screen shows the 36 levels of The Ultimate Doom as tiles, a
row per episode: the level's first frame (captured the first time it is
played), its name and the best medal earned; locked levels are dark with
a padlock. A click selects a level, Play or a double click starts it at
the difficulty picked below the tiles. The progress comes from
campaign.dat, written by the game (libdoom's doom_progress_*).

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

	// the time trials come with M9
	s_doomsp.doomfrag.generic.type = MTYPE_PTEXT;
	s_doomsp.doomfrag.generic.flags = QMF_CENTER_JUSTIFY | QMF_GRAYED;
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
