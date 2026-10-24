/*
===========================================================================
Q3Doom - the Doom campaign: level ends, intermission, episode ends, the
inventory between levels, deaths and saved progress (g_game.c)

Taking an exit stops the level and publishes the tally in
CS_DOOM_INTERMISSION; the clients play Doom's intermission (wi_stuff.c) or
the episode's finale (f_finale.c) and say when they are done
("doom_worlddone", "doom_finaledone"). The next map then loads with each
player's weapons, ammo, health and armor. Dying reloads the level with a
pistol start.

In a campaign (g_doomCampaign, set by doom_campaign), finishing a level
unlocks the next one and records its medal and best time in campaign.dat.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "g_local.h"

#define PROGRESS_FILE	"campaign.dat"		// .dat: readable from the game directory on pure servers

extern vmCvar_t	g_doomCampaign;

/*
=====================================================================

LEVEL ORDER

=====================================================================
*/

// ExMy: episode and map, 0 if the name is not one
static qboolean EpisodeMap( const char *map, int *episode, int *mapnum ) {
	if ( ( map[0] == 'E' || map[0] == 'e' ) && ( map[2] == 'M' || map[2] == 'm' ) && isdigit( map[1] ) && isdigit( map[3] ) && !map[4] ) {
		*episode = map[1] - '0';
		*mapnum = map[3] - '0';
		return qtrue;
	}
	return qfalse;
}

/*
=================
NextMapName

Doom's level order (G_DoCompleted): ExMy -> ExM(y+1), secret exits to M9,
and back from M9; Doom II MAPxx in order with its secret levels 31 and 32.
After an episode's last map, the next episode starts.
=================
*/
static void NextMapName( const char *map, qboolean secret, char *out, int outSize ) {
	int		e, m;

	if ( EpisodeMap( map, &e, &m ) ) {
		static const int fromSecret[5] = { 0, 4, 6, 7, 3 };	// M9 returns to

		if ( secret ) {
			m = 9;
		} else if ( m == 9 ) {
			m = ( e >= 1 && e <= 4 ) ? fromSecret[e] + 1 : 1;
		} else if ( m == 8 ) {
			e = e % 4 + 1;
			m = 1;
		} else {
			m++;
		}
		Com_sprintf( out, outSize, "E%dM%d", e, m );
		return;
	}
	if ( !Q_stricmpn( map, "MAP", 3 ) ) {
		m = atoi( map + 3 );
		if ( secret ) {
			m = m == 31 ? 32 : 31;
		} else if ( m == 31 || m == 32 ) {
			m = 16;
		} else if ( m == 30 ) {
			m = 1;
		} else {
			m++;
		}
		Com_sprintf( out, outSize, "MAP%02d", m );
		return;
	}
	Q_strncpyz( out, map, outSize );
}

/*
=====================================================================

PROGRESS (campaign.dat)

=====================================================================
*/

static void LoadProgress( doom_progress_t *p ) {
	fileHandle_t	f;
	int				len = trap_FS_FOpenFile( PROGRESS_FILE, &f, FS_READ );
	static char		text[16384];

	text[0] = 0;
	if ( f ) {
		if ( len > 0 && len < (int)sizeof( text ) ) {
			trap_FS_Read( text, len, f );
			text[len] = 0;
		}
		trap_FS_FCloseFile( f );
	}
	doom_progress_parse( text, p );
}

static void SaveProgress( const doom_progress_t *p ) {
	fileHandle_t	f;
	static char		text[16384];
	int				len = doom_progress_write( p, text, sizeof( text ) );

	if ( trap_FS_FOpenFile( PROGRESS_FILE, &f, FS_WRITE ) >= 0 && f ) {
		trap_FS_Write( text, len, f );
		trap_FS_FCloseFile( f );
	}
}

/*
=================
RecordLevel

A campaign level finished: its medal and best time, the next level
unlocked (and the next episode after an episode's last map)
=================
*/
static void RecordLevel( const char *map, const char *next, int tics ) {
	doom_progress_t			p;
	doom_progress_level_t	*l;
	int						medal = doom_skill_medal( g_doomSkill.integer );

	LoadProgress( &p );
	l = doom_progress_level( &p, map, 1 );
	if ( l ) {
		l->unlocked = 1;
		if ( medal > l->medal ) {
			l->medal = medal;
		}
		if ( !l->best_tics || tics < l->best_tics ) {
			l->best_tics = tics;
		}
	}
	l = doom_progress_level( &p, next, 1 );
	if ( l ) {
		l->unlocked = 1;
	}
	SaveProgress( &p );
	G_Printf( "Doom campaign: %s finished (medal %d, %d tics), %s unlocked\n", map, medal, tics, next );
}

/*
=====================================================================

THE INVENTORY BETWEEN LEVELS (G_PlayerFinishLevel)

=====================================================================
*/

static void SaveCarry( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		gclient_t		*cl = &level.clients[i];
		playerState_t	*ps = &cl->ps;

		if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR || ps->stats[STAT_HEALTH] <= 0 ) {
			trap_Cvar_Set( va( "g_doomCarry%d", i ), "" );
			continue;
		}
		trap_Cvar_Set( va( "g_doomCarry%d", i ), va( "%d %d %d %d %d %d %d %d %d %d", ps->stats[STAT_HEALTH], ps->stats[STAT_ARMOR],
			ps->stats[STAT_DOOM_ARMORTYPE], ps->stats[STAT_WEAPONS], ps->stats[STAT_DOOM_BACKPACK], ps->weapon,
			ps->ammo[DAM_CLIP], ps->ammo[DAM_SHELL], ps->ammo[DAM_CELL], ps->ammo[DAM_MISL] ) );
	}
}

static void ClearCarry( void ) {
	int		i;

	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		trap_Cvar_Set( va( "g_doomCarry%d", i ), "" );
	}
}

/*
=================
Doom_ApplyCarry

At a spawn after Doom_GivePlayerStart: what the player had at the end of
the previous level (once)
=================
*/
void Doom_ApplyCarry( gclient_t *client ) {
	playerState_t	*ps = &client->ps;
	char			buf[256];
	int				v[10], n, num = client - level.clients;

	trap_Cvar_VariableStringBuffer( va( "g_doomCarry%d", num ), buf, sizeof( buf ) );
	if ( !buf[0] ) {
		return;
	}
	trap_Cvar_Set( va( "g_doomCarry%d", num ), "" );
	n = sscanf( buf, "%d %d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9] );
	if ( n != 10 || v[0] <= 0 ) {
		return;
	}
	ps->stats[STAT_HEALTH] = g_entities[num].health = v[0];
	ps->stats[STAT_ARMOR] = v[1];
	ps->stats[STAT_DOOM_ARMORTYPE] = v[2];
	ps->stats[STAT_WEAPONS] = v[3];
	ps->stats[STAT_DOOM_BACKPACK] = v[4];
	ps->ammo[DAM_CLIP] = v[6];
	ps->ammo[DAM_SHELL] = v[7];
	ps->ammo[DAM_CELL] = v[8];
	ps->ammo[DAM_MISL] = v[9];
	if ( v[5] > 0 && v[5] <= DWP_NUMWEAPONS && ( v[3] & ( 1 << v[5] ) ) ) {
		ps->weapon = v[5];
		ps->doomPendingWeapon = 0;
		BG_DoomBringUpWeapon( ps );
	}
}

/*
=====================================================================

LEVEL END

=====================================================================
*/

void Doom_InitCampaign( void ) {
	char		map[MAX_QPATH];
	doom_progress_t	p;

	doomLevel.campaign = qfalse;
	trap_SetConfigstring( CS_DOOM_INTERMISSION, "" );
	if ( !g_doomCampaign.integer ) {
		return;
	}
	// only levels the campaign reached count
	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
	LoadProgress( &p );
	doomLevel.campaign = doom_progress_unlocked( &p, map );
	G_Printf( "Doom campaign: %s%s\n", map, doomLevel.campaign ? "" : " is not unlocked, progress is not saved" );
}

/*
=================
Doom_ExitLevel

An exit was taken (G_ExitLevel, G_SecretExitLevel): the play stops, the
tally goes to the clients. After an episode's last map comes its finale
(no tally in Doom).
=================
*/
void Doom_ExitLevel( qboolean secret ) {
	char	map[MAX_QPATH];
	int		episode, mapnum;

	if ( doomLevel.intermission ) {
		return;
	}
	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
	NextMapName( map, secret, doomLevel.nextMap, sizeof( doomLevel.nextMap ) );
	doomLevel.intermission = qtrue;
	doomLevel.exitTics = ( level.time - level.startTime ) * DOOM_TICRATE / 1000;
	G_Printf( "Doom %sexit: %s -> %s\n", secret ? "secret " : "", map, doomLevel.nextMap );

	if ( secret ) {
		trap_Cvar_Set( "g_doomDidSecret", "1" );
	}
	if ( doomLevel.campaign ) {
		RecordLevel( map, doomLevel.nextMap, doomLevel.exitTics );
	}

	if ( !EpisodeMap( map, &episode, &mapnum ) ) {
		// not an episode map: straight to the next one
		trap_SetConfigstring( CS_DOOM_INTERMISSION, "" );
		doomLevel.finale = qfalse;
		return;
	}
	if ( mapnum == 8 ) {
		doomLevel.finale = qtrue;
		trap_SetConfigstring( CS_DOOM_INTERMISSION, va( "finale %d", episode ) );
		return;
	}
	trap_SetConfigstring( CS_DOOM_INTERMISSION, va( "wi %d %d %d %d %d %d %d %d %d %d %d %d", episode - 1, mapnum - 1,
		doomLevel.nextMap[3] - '1', secret || mapnum == 9 || trap_Cvar_VariableIntegerValue( "g_doomDidSecret" ),
		doomLevel.kills, doomLevel.totalKills, doomLevel.items, doomLevel.totalItems,
		doomLevel.secretsFound, doomLevel.totalSecrets, doomLevel.exitTics, doom_par_time( episode, mapnum ) * DOOM_TICRATE ) );
}

// G_WorldDone: the next map, with what the players carry
static void WorldDone( void ) {
	SaveCarry();
	trap_SendConsoleCommand( EXEC_INSERT, va( "%s %s\n", g_cheats.integer ? "devmap" : "map", doomLevel.nextMap ) );
}

void Doom_CampaignFrame( void ) {
	char	cs[16];

	// a level that is not an episode map goes on at once
	if ( doomLevel.intermission && !doomLevel.finale ) {
		trap_GetConfigstring( CS_DOOM_INTERMISSION, cs, sizeof( cs ) );
		if ( !cs[0] && !doomLevel.restarting ) {
			doomLevel.restarting = qtrue;
			WorldDone();
		}
	}
}

/*
=================
Doom_CampaignCommand

The clients' "doom_worlddone" (the intermission is over) and
"doom_finaledone" (the player left the episode's end screen)
=================
*/
qboolean Doom_CampaignCommand( gentity_t *ent, const char *cmd ) {
	if ( !Q_stricmp( cmd, "doom_worlddone" ) ) {
		if ( doomLevel.intermission && !doomLevel.finale && !doomLevel.restarting ) {
			doomLevel.restarting = qtrue;
			WorldDone();
		}
		return qtrue;
	}
	if ( !Q_stricmp( cmd, "doom_finaledone" ) ) {
		if ( doomLevel.finale && !doomLevel.restarting ) {
			doomLevel.restarting = qtrue;
			ClearCarry();
			trap_Cvar_Set( "g_doomCampaign", "0" );
			trap_Cvar_Set( "g_doomDidSecret", "0" );
			// back to the menu
			trap_SendConsoleCommand( EXEC_INSERT, "disconnect\n" );
		}
		return qtrue;
	}
	return qfalse;
}

/*
=================
Doom_PlayerDied

A dead player pressed use or fire. Alone, Doom reloads the level with a
pistol start (G_DoReborn); with others, the player respawns (as in
cooperative play). Returns qtrue if the level reloads.
=================
*/
qboolean Doom_PlayerDied( gentity_t *ent ) {
	int		i, players = 0;

	if ( !DOOM_LEVEL() ) {
		return qfalse;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED && level.clients[i].sess.sessionTeam != TEAM_SPECTATOR ) {
			players++;
		}
	}
	if ( players > 1 ) {
		return qfalse;
	}
	if ( !doomLevel.restarting ) {
		doomLevel.restarting = qtrue;
		ClearCarry();
		trap_SendConsoleCommand( EXEC_INSERT, "map_restart 0\n" );
	}
	return qtrue;
}
