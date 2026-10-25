/*
===========================================================================
Q3Doom - Linux: time, paths and user (the counterpart of win32/win_shared.c)

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"

#include <math.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

/*
================
Sys_Milliseconds

Milliseconds since the first call, from the monotonic clock
================
*/
int		sys_timeBase;
int Sys_Milliseconds( void ) {
	struct timespec	ts;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	if ( !sys_timeBase ) {
		sys_timeBase = (int)ts.tv_sec;
		return (int)( ts.tv_nsec / 1000000 );
	}
	return (int)( ( ts.tv_sec - sys_timeBase ) * 1000 + ts.tv_nsec / 1000000 );
}

/*
================
Sys_SnapVector

Round to nearest (even), like the original x87 fistp code
================
*/
void Sys_SnapVector( float *v ) {
	v[0] = (float)lrintf( v[0] );
	v[1] = (float)lrintf( v[1] );
	v[2] = (float)lrintf( v[2] );
}

int Sys_GetProcessorId( void ) {
	return CPUID_GENERIC;
}

char *Sys_GetCurrentUser( void ) {
	struct passwd	*p = getpwuid( getuid() );

	if ( p && p->pw_name && p->pw_name[0] ) {
		return p->pw_name;
	}
	return "player";
}

/*
================
Sys_DefaultHomePath

Settings, saves, records and screenshots go to ~/.q3doom (the folder the
game is installed in may not be writable)
================
*/
char *Sys_DefaultHomePath( void ) {
	static char	home[MAX_OSPATH];
	const char	*h;

	if ( !home[0] ) {
		h = getenv( "HOME" );
		if ( !h || !h[0] ) {
			return NULL;
		}
		Com_sprintf( home, sizeof( home ), "%s/.q3doom", h );
		Sys_Mkdir( home );
	}
	return home;
}

/*
================
Sys_DefaultInstallPath

The folder holding the executable (fs_basepath default), so the game runs
from any working directory
================
*/
int Sys_RunProcess( const char *exe, const char *args, const char *workdir ) {
	char	cmdline[MAX_OSPATH * 4];
	pid_t	pid;
	int		status;

	Com_sprintf( cmdline, sizeof( cmdline ), "\"%s\" %s", exe, args );
	pid = fork();
	if ( pid < 0 ) {
		return -1;
	}
	if ( pid == 0 ) {
		if ( workdir && chdir( workdir ) ) {
			_exit( 127 );
		}
		execl( "/bin/sh", "sh", "-c", cmdline, (char *)NULL );
		_exit( 127 );
	}
	if ( waitpid( pid, &status, 0 ) < 0 || !WIFEXITED( status ) ) {
		return -1;
	}
	return WEXITSTATUS( status ) == 127 ? -1 : WEXITSTATUS( status );
}

char *Sys_DefaultInstallPath( void ) {
	static char	exeDir[MAX_OSPATH];
	char		*slash;
	ssize_t		len;

	if ( !exeDir[0] ) {
		len = readlink( "/proc/self/exe", exeDir, sizeof( exeDir ) - 1 );
		if ( len <= 0 ) {
			return Sys_Cwd();
		}
		exeDir[len] = 0;
		slash = strrchr( exeDir, '/' );
		if ( slash ) {
			*slash = 0;
		}
	}
	return exeDir;
}
