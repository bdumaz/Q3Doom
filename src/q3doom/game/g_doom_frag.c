/*
===========================================================================
Q3Doom - DoomFrag: Defrag-style time trials on Doom maps

A run (g_doomFrag, set by the doomfrag command) starts when the player
first moves, jumps or fires: the monsters wait until then. Taking the exit
stops the clock; with the "100% kills" finish rule (g_doomFinish) the exit
only counts once every monster is dead. The result is compared with the
best time for the map and the options and saved in doomfrag.dat (libdoom's
doom_records_*). The clients follow in CS_DOOM_FRAG:
	"<state> <start> <end> <best> <par> <kills> <totalkills> <newrecord>"
state 0 waiting, 1 running, 2 finished; times are server msec.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

#define RECORDS_FILE	"doomfrag.dat"		// .dat: readable from the game directory on pure servers

static struct {
	qboolean	active;
	int			state;
	int			start, end;
	int			best;			// msec before this run, 0 none
	int			par;			// msec, 0 none
	qboolean	newRecord;
	char		map[MAX_QPATH];
	char		options[DOOM_OPTIONS_LEN + 1];
} frag;

static void SendState( void ) {
	trap_SetConfigstring( CS_DOOM_FRAG, frag.active ? va( "%d %d %d %d %d %d %d %d", frag.state, frag.start, frag.end,
		frag.best, frag.par, doomLevel.kills, doomLevel.totalKills, frag.newRecord ) : "" );
}

static void LoadRecords( doom_records_t *r ) {
	fileHandle_t	f;
	int				len = trap_FS_FOpenFile( RECORDS_FILE, &f, FS_READ );
	static char		text[65536];

	text[0] = 0;
	if ( f ) {
		if ( len > 0 && len < (int)sizeof( text ) ) {
			trap_FS_Read( text, len, f );
			text[len] = 0;
		}
		trap_FS_FCloseFile( f );
	}
	doom_records_parse( text, r );
}

static void SaveRecords( const doom_records_t *r ) {
	fileHandle_t	f;
	static char		text[65536];
	int				len = doom_records_write( r, text, sizeof( text ) );

	if ( trap_FS_FOpenFile( RECORDS_FILE, &f, FS_WRITE ) >= 0 && f ) {
		trap_FS_Write( text, len, f );
		trap_FS_FCloseFile( f );
	}
}

/*
=================
Doom_InitFrag

At level start: a run waiting for the player
=================
*/
void Doom_InitFrag( void ) {
	static doom_records_t	records;
	int						e, m;

	memset( &frag, 0, sizeof( frag ) );
	frag.active = g_doomFrag.integer != 0;
	if ( frag.active ) {
		trap_Cvar_VariableStringBuffer( "mapname", frag.map, sizeof( frag.map ) );
		Q_strupr( frag.map );
		doom_frag_options( frag.options, sizeof( frag.options ), g_doomPhysics.integer != 0, (int)Com_Clamp( 0, 2, g_doomWeapons.integer ),
			g_doomLoadout.integer != 0, g_doomMonsters.integer ? (int)Com_Clamp( 0, 4, g_doomSkill.integer ) : 5, g_doomFinish.integer != 0 );
		LoadRecords( &records );
		frag.best = doom_record_get( &records, frag.map, frag.options );
		// the par of the level (a DoomFrag copy's records are its own, its par is the level's)
		if ( Doom_LevelName()[0] == 'E' && Doom_LevelName()[2] == 'M' ) {
			e = Doom_LevelName()[1] - '0';
			m = Doom_LevelName()[3] - '0';
			frag.par = doom_par_time( e, m ) * 1000;
		}
		G_Printf( "DoomFrag: %s, options %s, best %d msec\n", frag.map, frag.options, frag.best );
	}
	SendState();
}

// the monsters wait for the run to start
qboolean Doom_FragWaiting( void ) {
	return frag.active && frag.state == 0;
}

/*
=================
Doom_FragInput

A player's command: the first move, jump or shot starts the clock (at the
time of that command)
=================
*/
void Doom_FragInput( gentity_t *ent, const usercmd_t *cmd ) {
	if ( !frag.active || frag.state != 0 || ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}
	if ( !cmd->forwardmove && !cmd->rightmove && !cmd->upmove && !( cmd->buttons & ( BUTTON_ATTACK | BUTTON_USE ) ) ) {
		return;
	}
	frag.state = 1;
	frag.start = cmd->serverTime;
	SendState();
}

void Doom_FragUpdate( void ) {
	if ( frag.active ) {
		SendState();
	}
}

/*
=================
Doom_FragExit

The exit of a DoomFrag run: refused while monsters live with the 100% kills
rule, else the clock stops and the time is recorded. Returns qtrue if the
run ended.
=================
*/
qboolean Doom_FragExit( gentity_t *activator ) {
	static doom_records_t	records;
	int						msec;

	if ( frag.state == 2 ) {
		return qtrue;
	}
	if ( g_doomFinish.integer && doomLevel.kills < doomLevel.totalKills ) {
		if ( activator && activator->client && level.time - activator->doomMessageTime > 1000 ) {
			activator->doomMessageTime = level.time;
			Doom_Message( activator, va( "Kill all the monsters first: %d left", doomLevel.totalKills - doomLevel.kills ) );
			G_AddEvent( activator, EV_DOOM_SOUND, sfx_noway );
		}
		return qfalse;
	}
	if ( frag.state == 0 ) {
		frag.start = level.time;
	}
	frag.state = 2;
	frag.end = activator && activator->client ? activator->client->ps.commandTime : level.time;
	if ( frag.end < frag.start ) {
		frag.end = frag.start;
	}
	msec = frag.end - frag.start;

	LoadRecords( &records );
	frag.newRecord = doom_record_set( &records, frag.map, frag.options, msec );
	if ( frag.newRecord ) {
		SaveRecords( &records );
	}
	G_Printf( "DoomFrag: %s finished in %d msec%s\n", frag.map, msec, frag.newRecord ? " (new record)" : "" );

	// everyone stops and sees the result
	doomLevel.intermission = qtrue;
	SendState();
	return qtrue;
}

/*
=================
Doom_FragCommand

"doom_restart": run the map again
=================
*/
qboolean Doom_FragCommand( gentity_t *ent, const char *cmd ) {
	if ( Q_stricmp( cmd, "doom_restart" ) ) {
		return qfalse;
	}
	if ( frag.active && !doomLevel.restarting ) {
		doomLevel.restarting = qtrue;
		trap_SendConsoleCommand( EXEC_INSERT, "map_restart 0\n" );
	}
	return qtrue;
}

qboolean Doom_FragActive( void ) {
	return frag.active;
}
