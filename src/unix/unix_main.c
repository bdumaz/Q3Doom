/*
===========================================================================
Q3Doom - Linux: startup, event loop, files, game modules, console and crash
report (the counterpart of win32/win_main.c, win_syscon.c and win_crash.c).
SDL2 does the window, input and sound, as on Windows.

Based on Quake III Arena source code, Copyright (C) 1999-2005 Id Software, Inc.
Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../client/client.h"
#include "../qcommon/qcommon.h"
#include "../sdl/sdl_local.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#define MEM_THRESHOLD	( 96 * 1024 * 1024 )

void GLimp_RestoreGamma( void );	// sdl_glimp.c

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory( void ) {
	long	pages = sysconf( _SC_PHYS_PAGES ), size = sysconf( _SC_PAGE_SIZE );

	if ( pages <= 0 || size <= 0 ) {
		return qfalse;
	}
	return (double)pages * size <= MEM_THRESHOLD ? qtrue : qfalse;
}

void Sys_BeginProfiling( void ) {
}

/*
=====================================================================

CONSOLE

The terminal is the console: everything printed goes to stdout, and lines
typed there are console commands (a dedicated server is run this way).

=====================================================================
*/

static qboolean	ttyInput;

static void Sys_InitConsoleInput( void ) {
	int flags;

	// only read stdin when it is a terminal or a pipe kept open for commands
	ttyInput = isatty( STDIN_FILENO ) || ( com_dedicated && com_dedicated->integer );
	if ( ttyInput ) {
		flags = fcntl( STDIN_FILENO, F_GETFL, 0 );
		if ( flags != -1 ) {
			fcntl( STDIN_FILENO, F_SETFL, flags | O_NONBLOCK );
		}
	}
}

/*
================
Sys_ConsoleInput

A whole line typed on the terminal, or NULL
================
*/
char *Sys_ConsoleInput( void ) {
	static char	text[MAX_EDIT_LINE];
	static int	len;
	char		c;

	if ( !ttyInput ) {
		return NULL;
	}
	while ( read( STDIN_FILENO, &c, 1 ) == 1 ) {
		if ( c == '\n' ) {
			text[len] = 0;
			len = 0;
			return text;
		}
		if ( c != '\r' && len < (int)sizeof( text ) - 1 ) {
			text[len++] = c;
		}
	}
	return NULL;
}

void Sys_Print( const char *msg ) {
	char	clean[MAXPRINTMSG];
	int		i = 0;

	// no color codes on the terminal
	while ( *msg && i < (int)sizeof( clean ) - 1 ) {
		if ( Q_IsColorString( msg ) ) {
			msg += 2;
			continue;
		}
		clean[i++] = *msg++;
	}
	clean[i] = 0;
	fputs( clean, stdout );
	fflush( stdout );
}

void Sys_ShowConsole( int visLevel, qboolean quitOnClose ) {
}

void Sys_SetErrorText( const char *text ) {
}

/*
=============
Sys_Error

Print the error, show it in a message box (there may be no terminal), quit
=============
*/
void QDECL Sys_Error( const char *error, ... ) {
	va_list		argptr;
	char		text[4096];

	va_start( argptr, error );
	Q_vsnprintf( text, sizeof( text ), error, argptr );
	va_end( argptr );

	fprintf( stderr, "Sys_Error: %s\n", text );
	GLimp_RestoreGamma();
	IN_Shutdown();
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "Q3Doom", text, NULL );
	exit( 1 );
}

void Sys_Quit( void ) {
	IN_Shutdown();
	exit( 0 );
}

/*
=====================================================================

CRASH REPORT

A crash writes crash.txt (in the home folder) with a backtrace, and puts
the screen gamma back

=====================================================================
*/

static char	crashPath[MAX_OSPATH];

static void Sys_CrashHandler( int sig ) {
	void	*frames[64];
	int		n, fd;
	char	head[128];

	signal( sig, SIG_DFL );
	GLimp_RestoreGamma();

	n = backtrace( frames, 64 );
	Com_sprintf( head, sizeof( head ), "Signal %d (%s)\n", sig, strsignal( sig ) );
	fputs( head, stderr );
	backtrace_symbols_fd( frames, n, STDERR_FILENO );
	if ( crashPath[0] ) {
		fd = open( crashPath, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
		if ( fd >= 0 ) {
			if ( write( fd, head, strlen( head ) ) > 0 ) {
				backtrace_symbols_fd( frames, n, fd );
			}
			close( fd );
		}
	}
	raise( sig );
}

// SIGTERM (kill, timeout, a service manager), SIGINT and SIGHUP quit
// cleanly at the next frame. Set before SDL starts, which then leaves them
// alone (SDL's own handlers only post a quit event, that a dedicated server
// never reads: it ignored the signal)
static volatile sig_atomic_t	sys_quitSignal;

static void Sys_QuitSignalHandler( int sig ) {
	sys_quitSignal = sig;
}

static void Sys_InitCrashHandler( void ) {
	const char	*home = Sys_DefaultHomePath();

	if ( home ) {
		Com_sprintf( crashPath, sizeof( crashPath ), "%s/crash.txt", home );
	}
	signal( SIGSEGV, Sys_CrashHandler );
	signal( SIGBUS, Sys_CrashHandler );
	signal( SIGFPE, Sys_CrashHandler );
	signal( SIGILL, Sys_CrashHandler );
	signal( SIGABRT, Sys_CrashHandler );
	signal( SIGTERM, Sys_QuitSignalHandler );
	signal( SIGINT, Sys_QuitSignalHandler );
	signal( SIGHUP, Sys_QuitSignalHandler );
}

/*
=====================================================================

PATHS AND DIRECTORY SCANNING

=====================================================================
*/

void Sys_Mkdir( const char *path ) {
	mkdir( path, 0750 );
}

char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];

	if ( !getcwd( cwd, sizeof( cwd ) - 1 ) ) {
		cwd[0] = 0;
	}
	cwd[MAX_OSPATH - 1] = 0;
	return cwd;
}

char *Sys_DefaultCDPath( void ) {
	return "";
}

char *Sys_DefaultBasePath( void ) {
	return Sys_DefaultInstallPath();
}

#define	MAX_FOUND_FILES	0x1000

static qboolean IsDirectory( const char *path ) {
	struct stat st;

	return stat( path, &st ) == 0 && S_ISDIR( st.st_mode );
}

void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles ) {
	char			search[MAX_OSPATH], newsubdirs[MAX_OSPATH], filename[MAX_OSPATH], path[MAX_OSPATH];
	DIR				*dir;
	struct dirent	*d;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}
	if ( strlen( subdirs ) ) {
		Com_sprintf( search, sizeof( search ), "%s/%s", basedir, subdirs );
	} else {
		Com_sprintf( search, sizeof( search ), "%s", basedir );
	}
	if ( ( dir = opendir( search ) ) == NULL ) {
		return;
	}
	while ( ( d = readdir( dir ) ) != NULL ) {
		Com_sprintf( path, sizeof( path ), "%s/%s", search, d->d_name );
		if ( IsDirectory( path ) ) {
			if ( Q_stricmp( d->d_name, "." ) && Q_stricmp( d->d_name, ".." ) ) {
				if ( strlen( subdirs ) ) {
					Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s/%s", subdirs, d->d_name );
				} else {
					Com_sprintf( newsubdirs, sizeof( newsubdirs ), "%s", d->d_name );
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof( filename ), "%s/%s", subdirs, d->d_name );
		if ( !Com_FilterPath( filter, filename, qfalse ) ) {
			continue;
		}
		list[*numfiles] = CopyString( filename );
		( *numfiles )++;
	}
	closedir( dir );
}

static qboolean strgtr( const char *s0, const char *s1 ) {
	int l0 = strlen( s0 ), l1 = strlen( s1 ), i;

	if ( l1 < l0 ) {
		l0 = l1;
	}
	for ( i = 0 ; i < l0 ; i++ ) {
		if ( s1[i] > s0[i] ) {
			return qtrue;
		}
		if ( s1[i] < s0[i] ) {
			return qfalse;
		}
	}
	return qfalse;
}

/*
================
Sys_ListFiles

The files of a directory ending with 'extension' (case insensitive, like
the Windows version), or its subdirectories when extension is "/"
================
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
	int				nfiles = 0, i, extLen, nameLen;
	char			**listCopy;
	char			*list[MAX_FOUND_FILES];
	char			path[MAX_OSPATH];
	qboolean		dirsOnly;
	DIR				*dir;
	struct dirent	*d;

	if ( filter ) {
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );
		list[nfiles] = 0;
		*numfiles = nfiles;
		if ( !nfiles ) {
			return NULL;
		}
		listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;
		return listCopy;
	}

	if ( !extension ) {
		extension = "";
	}
	// passing a slash as extension will find directories
	dirsOnly = extension[0] == '/' && extension[1] == 0;
	if ( dirsOnly ) {
		extension = "";
	}
	extLen = strlen( extension );

	if ( ( dir = opendir( directory ) ) == NULL ) {
		*numfiles = 0;
		return NULL;
	}
	while ( ( d = readdir( dir ) ) != NULL ) {
		qboolean isDir;

		Com_sprintf( path, sizeof( path ), "%s/%s", directory, d->d_name );
		isDir = IsDirectory( path );
		if ( ( dirsOnly || wantsubs ) != isDir ) {
			continue;
		}
		nameLen = strlen( d->d_name );
		if ( extLen && ( nameLen < extLen || Q_stricmp( d->d_name + nameLen - extLen, extension ) ) ) {
			continue;
		}
		if ( nfiles == MAX_FOUND_FILES - 1 ) {
			break;
		}
		list[nfiles++] = CopyString( d->d_name );
	}
	list[nfiles] = 0;
	closedir( dir );

	*numfiles = nfiles;
	if ( !nfiles ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	// sorted, like the Windows version
	{
		qboolean swapped;

		do {
			swapped = qfalse;
			for ( i = 1 ; i < nfiles ; i++ ) {
				if ( strgtr( listCopy[i - 1], listCopy[i] ) ) {
					char *temp = listCopy[i];

					listCopy[i] = listCopy[i - 1];
					listCopy[i - 1] = temp;
					swapped = qtrue;
				}
			}
		} while ( swapped );
	}
	return listCopy;
}

void Sys_FreeFileList( char **list ) {
	int		i;

	if ( !list ) {
		return;
	}
	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}
	Z_Free( list );
}

qboolean Sys_ScanForCD( void ) {
	return qfalse;
}

qboolean Sys_CheckCD( void ) {
	return qtrue;
}

/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData( void ) {
	char	*text, *data = NULL;

	if ( !SDL_HasClipboardText() ) {
		return NULL;
	}
	text = SDL_GetClipboardText();
	if ( text && text[0] ) {
		int len = strlen( text ) + 1;

		data = Z_Malloc( len );
		Q_strncpyz( data, text, len );
		strtok( data, "\n\r\b" );
	}
	SDL_free( text );
	return data;
}

/*
=====================================================================

LOAD/UNLOAD GAME MODULES

=====================================================================
*/

void Sys_UnloadDll( void *dllHandle ) {
	if ( !dllHandle ) {
		return;
	}
	if ( dlclose( dllHandle ) ) {
		Com_Error( ERR_FATAL, "Sys_UnloadDll: dlclose failed: %s", dlerror() );
	}
}

/*
=================
Sys_LoadDll

Loads <name><ARCH_STRING>.so from the active game directory (fs_game, or
baseq3 when none is set), trying fs_homepath then fs_basepath.
fqpath (at least MAX_QPATH+1 bytes) receives the module file name.
=================
*/
extern char		*FS_BuildOSPath( const char *base, const char *game, const char *qpath );

void * QDECL Sys_LoadDll( const char *name, char *fqpath, intptr_t ( QDECL **entryPoint )( int, ... ),
				  intptr_t ( QDECL *systemcalls )( intptr_t, ... ) ) {
	void		*libHandle = NULL;
	void		( QDECL *dllEntry )( intptr_t ( QDECL *syscallptr )( intptr_t, ... ) );
	const char	*searchPaths[2];
	const char	*gamedirs[2];
	char		*fn;
	char		filename[MAX_QPATH];
	int			i, g;

	*fqpath = 0;
	Com_sprintf( filename, sizeof( filename ), "%s" ARCH_STRING ".so", name );

	// the active game directory first, then baseq3 for modules a mod does not replace
	gamedirs[0] = Cvar_VariableString( "fs_game" );
	if ( !gamedirs[0][0] ) {
		gamedirs[0] = BASEGAME;
	}
	gamedirs[1] = Q_stricmp( gamedirs[0], BASEGAME ) ? BASEGAME : NULL;
	searchPaths[0] = Cvar_VariableString( "fs_homepath" );
	searchPaths[1] = Cvar_VariableString( "fs_basepath" );

	for ( g = 0 ; g < 2 && gamedirs[g] && !libHandle ; g++ ) {
		for ( i = 0 ; i < 2 && !libHandle ; i++ ) {
			if ( !searchPaths[i][0] ) {
				continue;
			}
			fn = FS_BuildOSPath( searchPaths[i], gamedirs[g], filename );
			libHandle = dlopen( fn, RTLD_NOW );
			if ( libHandle ) {
				Com_DPrintf( "dlopen '%s' ok\n", fn );
			} else {
				Com_DPrintf( "dlopen '%s' failed: %s\n", fn, dlerror() );
			}
		}
	}
	if ( !libHandle ) {
		return NULL;
	}

	dllEntry = ( void ( QDECL * )( intptr_t ( QDECL * )( intptr_t, ... ) ) )dlsym( libHandle, "dllEntry" );
	*entryPoint = ( intptr_t ( QDECL * )( int, ... ) )dlsym( libHandle, "vmMain" );
	if ( !*entryPoint || !dllEntry ) {
		dlclose( libHandle );
		return NULL;
	}
	dllEntry( systemcalls );

	Q_strncpyz( fqpath, filename, MAX_QPATH );
	return libHandle;
}

/*
=====================================================================

BACKGROUND FILE STREAMING (unthreaded, as on Windows)

=====================================================================
*/

void Sys_InitStreamThread( void ) {
}

void Sys_ShutdownStreamThread( void ) {
}

void Sys_BeginStreamedFile( fileHandle_t f, int readAhead ) {
}

void Sys_EndStreamedFile( fileHandle_t f ) {
}

int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	return FS_Read( buffer, size * count, f );
}

void Sys_StreamSeek( fileHandle_t f, int offset, int origin ) {
	FS_Seek( f, offset, origin );
}

/*
=====================================================================

EVENT LOOP

=====================================================================
*/

#define	MAX_QUED_EVENTS		256
#define	MASK_QUED_EVENTS	( MAX_QUED_EVENTS - 1 )

static sysEvent_t	eventQue[MAX_QUED_EVENTS];
static int			eventHead, eventTail;
static byte			sys_packetReceived[MAX_MSGLEN];

qboolean Sys_GetPacket( netadr_t *net_from, msg_t *net_message );

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t	*ev;

	ev = &eventQue[eventHead & MASK_QUED_EVENTS];
	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		Com_Printf( "Sys_QueEvent: overflow\n" );
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 ) {
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

sysEvent_t Sys_GetEvent( void ) {
	sysEvent_t	ev;
	char		*s;
	msg_t		netmsg;
	netadr_t	adr;

	// return if we have data
	if ( eventHead > eventTail ) {
		eventTail++;
		return eventQue[( eventTail - 1 ) & MASK_QUED_EVENTS];
	}

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s ) {
		char	*b;
		int		len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		Q_strncpyz( b, s, len );
		Sys_QueEvent( 0, SE_CONSOLE, 0, 0, len, b );
	}

	// check for network packets
	MSG_Init( &netmsg, sys_packetReceived, sizeof( sys_packetReceived ) );
	if ( Sys_GetPacket( &adr, &netmsg ) ) {
		netadr_t	*buf;
		int			len;

		// copy out to a seperate buffer for qeueing
		// the readcount stepahead is for SOCKS support
		len = sizeof( netadr_t ) + netmsg.cursize - netmsg.readcount;
		buf = Z_Malloc( len );
		*buf = adr;
		memcpy( buf + 1, &netmsg.data[netmsg.readcount], netmsg.cursize - netmsg.readcount );
		Sys_QueEvent( 0, SE_PACKET, 0, 0, len, buf );
	}

	// return if we have data
	if ( eventHead > eventTail ) {
		eventTail++;
		return eventQue[( eventTail - 1 ) & MASK_QUED_EVENTS];
	}

	// create an empty event to return
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();
	return ev;
}

//================================================================

static void Sys_In_Restart_f( void ) {
	IN_Shutdown();
	IN_Init();
}

static void Sys_Net_Restart_f( void ) {
	NET_Restart();
}

/*
================
Sys_Init

Called after the common systems (cvars, files, etc) are initialized
================
*/
void Sys_Init( void ) {
	Cmd_AddCommand( "in_restart", Sys_In_Restart_f );
	Cmd_AddCommand( "net_restart", Sys_Net_Restart_f );

	Cvar_Set( "arch", "linux" );
	Cvar_Get( "sys_cpustring", "generic", 0 );
	Cvar_SetValue( "sys_cpuid", CPUID_GENERIC );
	Cvar_Set( "username", Sys_GetCurrentUser() );

	IN_Init();
}

//=======================================================================

/*
==================
main

The command line is rebuilt as one string for Com_Init, each argument
quoted so paths with spaces stay whole
==================
*/
int main( int argc, char **argv ) {
	char	cmdline[MAX_STRING_CHARS];
	int		i;

	cmdline[0] = 0;
	for ( i = 1 ; i < argc ; i++ ) {
		qboolean quote = strchr( argv[i], ' ' ) != NULL;

		if ( i > 1 ) {
			Q_strcat( cmdline, sizeof( cmdline ), " " );
		}
		if ( quote ) {
			Q_strcat( cmdline, sizeof( cmdline ), "\"" );
		}
		Q_strcat( cmdline, sizeof( cmdline ), argv[i] );
		if ( quote ) {
			Q_strcat( cmdline, sizeof( cmdline ), "\"" );
		}
	}

	SDL_SetMainReady();

	// get the initial time base
	Sys_Milliseconds();
	Sys_InitStreamThread();
	Sys_InitCrashHandler();

	Com_Init( cmdline );
	NET_Init();
	Sys_InitConsoleInput();

	Com_Printf( "Working directory: %s\n", Sys_Cwd() );

	// main game loop
	while ( 1 ) {
		// if not running as a game client, sleep a bit
		if ( ( SDL_window && ( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_MINIMIZED ) )
			|| ( com_dedicated && com_dedicated->integer ) ) {
			usleep( 5000 );
		}

		if ( sys_quitSignal ) {
			Com_Printf( "Signal %d: quitting\n", (int)sys_quitSignal );
			Com_Quit_f();
		}

		// make sure mouse and joystick are only called once a frame
		IN_Frame();

		// run the game
		Com_Frame();
	}

	return 0;
}
