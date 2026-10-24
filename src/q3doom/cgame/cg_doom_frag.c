/*
===========================================================================
Q3Doom - the DoomFrag screen: the run timer and the result

The game sends the run in CS_DOOM_FRAG (g_doom_frag.c):
	"<state> <start> <end> <best> <par> <kills> <totalkills> <newrecord>"
state 0 waiting for the first move, 1 running, 2 finished. While running the
clock shows at the top of the screen; at the end the result shows over the
frozen view, and fire runs the map again.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "cg_local.h"

static struct {
	int			state;
	int			start, end;
	int			best, par;
	int			kills, totalKills;
	qboolean	newRecord;
	qboolean	killsRule;		// g_doomFinish: every monster must die before the exit

	int			finishTime;		// cg.time the result showed
	qboolean	attackHeld;
	qboolean	restartSent;
} fr;

static void ParseState( void ) {
	const char	*s = CG_ConfigString( CS_DOOM_FRAG );
	int			oldState = fr.state;

	fr.state = -1;
	if ( !cgs.doomFrag || !s[0] ) {
		return;
	}
	sscanf( s, "%d %d %d %d %d %d %d %d", &fr.state, &fr.start, &fr.end, &fr.best, &fr.par,
		&fr.kills, &fr.totalKills, &fr.newRecord );
	fr.killsRule = atoi( Info_ValueForKey( CG_ConfigString( CS_SERVERINFO ), "g_doomFinish" ) ) != 0;
	if ( fr.state == 2 && oldState != 2 ) {
		fr.finishTime = cg.time;
		fr.attackHeld = qtrue;		// a fresh press is needed to run again
	}
	if ( fr.state != 2 ) {
		fr.restartSent = qfalse;
	}
}

static const char *TimeString( int msec ) {
	if ( msec < 0 ) {
		msec = 0;
	}
	return va( "%d:%02d.%03d", msec / 60000, msec / 1000 % 60, msec % 1000 );
}

// Doom's font, scaled, in the 320x200 screen; x is the center
static void DrawText( float cx, float y, float scale, const char *text, const float *color ) {
	float	x0, sx, sy, x, width = 0;
	int		w, h, l, t;
	const char *p;

	for ( p = text ; *p ; p++ ) {
		int lump = CG_DoomFontLump( *p );

		if ( lump < 0 ) {
			width += 4;
		} else {
			CG_DoomPatchSize( lump, &w, &h, &l, &t );
			width += w;
		}
	}
	CG_DoomHudScale( &x0, &sx, &sy );
	x = cx - width * scale * 0.5f;
	trap_R_SetColor( color );
	for ( p = text ; *p ; p++ ) {
		int lump = CG_DoomFontLump( *p );

		if ( lump < 0 ) {
			x += 4 * scale;
			continue;
		}
		CG_DoomPatchSize( lump, &w, &h, &l, &t );
		trap_R_DrawStretchPic( x0 + ( x - l * scale ) * sx, ( y - t * scale ) * sy, w * scale * sx, h * scale * sy,
			0, 0, 1, 1, CG_DoomPatchShader( lump ) );
		x += w * scale;
	}
	trap_R_SetColor( NULL );
}

/*
=================
CG_DoomDrawFragTimer

The clock of the run, and the kills left with the "100% kills" rule
=================
*/
void CG_DoomDrawFragTimer( void ) {
	static const vec4_t	white = { 1, 1, 1, 1 }, yellow = { 1, 1, 0.3f, 1 };

	ParseState();
	if ( fr.state < 0 || fr.state == 2 || cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}
	// under Doom's message line
	DrawText( 160, 12, 2, TimeString( fr.state == 1 ? cg.time - fr.start : 0 ), white );
	if ( fr.killsRule ) {
		DrawText( 160, 30, 1, va( "KILLS %d/%d", fr.kills, fr.totalKills ), fr.kills >= fr.totalKills ? yellow : white );
	}
	if ( fr.state == 0 && ( cg.time >> 9 & 1 ) ) {
		DrawText( 160, 60, 1, "MOVE TO START THE CLOCK", yellow );
	}
}

/*
=================
CG_DoomDrawFragResult

The end of a run: the time against the best and the par. Fire runs the map
again. qfalse when no run is over.
=================
*/
qboolean CG_DoomDrawFragResult( void ) {
	static const vec4_t	dim = { 0, 0, 0, 0.6f }, white = { 1, 1, 1, 1 }, yellow = { 1, 1, 0.3f, 1 }, grey = { 0.7f, 0.7f, 0.7f, 1 };
	usercmd_t			cmd;
	int					msec;
	float				y = 50;

	ParseState();
	if ( fr.state != 2 ) {
		return qfalse;
	}
	msec = fr.end - fr.start;

	trap_R_SetColor( dim );
	CG_DrawPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cgs.media.whiteShader );
	trap_R_SetColor( NULL );

	DrawText( 160, y, 2, "FINISHED", white );
	y += 26;
	DrawText( 160, y, 3, TimeString( msec ), white );
	y += 32;
	if ( fr.newRecord ) {
		if ( cg.time >> 8 & 1 ) {
			DrawText( 160, y, 2, "NEW RECORD!", yellow );
		}
		if ( fr.best ) {
			DrawText( 160, y + 18, 1, va( "OLD BEST %s", TimeString( fr.best ) ), grey );
		}
	} else {
		DrawText( 160, y, 2, va( "BEST %s", TimeString( fr.best ) ), white );
		DrawText( 160, y + 18, 1, va( "+%s", TimeString( msec - fr.best ) ), grey );
	}
	y += 32;
	if ( fr.par ) {
		DrawText( 160, y, 1, va( "PAR %d:%02d", fr.par / 60000, fr.par / 1000 % 60 ), msec <= fr.par ? yellow : grey );
	}
	DrawText( 160, 180, 1, "FIRE: RUN AGAIN    ESC: MENU", grey );

	// a new press of fire, a moment after the end, runs the map again
	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &cmd );
	if ( !( cmd.buttons & BUTTON_ATTACK ) ) {
		fr.attackHeld = qfalse;
	} else if ( !fr.attackHeld && !fr.restartSent && cg.time - fr.finishTime > 1000 ) {
		fr.restartSent = qtrue;
		trap_SendClientCommand( "doom_restart" );
	}
	return qtrue;
}
