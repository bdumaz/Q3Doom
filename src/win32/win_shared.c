/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"
#include <math.h>

/*
================
Sys_Milliseconds
================
*/
int			sys_timeBase;
int Sys_Milliseconds (void)
{
	int			sys_curtime;
	static qboolean	initialized = qfalse;

	if (!initialized) {
		sys_timeBase = timeGetTime();
		initialized = qtrue;
	}
	sys_curtime = timeGetTime() - sys_timeBase;

	return sys_curtime;
}

/*
================
Sys_SnapVector

Round to nearest (even), like the original x87 fistp code
================
*/
void Sys_SnapVector( float *v )
{
	v[0] = (float)lrintf( v[0] );
	v[1] = (float)lrintf( v[1] );
	v[2] = (float)lrintf( v[2] );
}

/*
================
Sys_GetProcessorId

All the CPU-specific code paths are 32-bit x86 assembly, so a 64-bit
build is always "generic".
================
*/
int Sys_GetProcessorId( void )
{
	return CPUID_GENERIC;
}

//============================================

char *Sys_GetCurrentUser( void )
{
	static char s_userName[1024];
	unsigned long size = sizeof( s_userName );


	if ( !GetUserName( s_userName, &size ) )
		strcpy( s_userName, "player" );

	if ( !s_userName[0] )
	{
		strcpy( s_userName, "player" );
	}

	return s_userName;
}

char	*Sys_DefaultHomePath(void) {
	return NULL;
}

/*
================
Sys_DefaultInstallPath

The folder holding the executable (fs_basepath default), so shortcuts and
double-clicking work from any working directory
================
*/
char *Sys_DefaultInstallPath(void)
{
	static char	exeDir[MAX_OSPATH];
	char		*slash;

	if ( !exeDir[0] ) {
		if ( GetModuleFileName( NULL, exeDir, sizeof( exeDir ) ) == 0 ) {
			return Sys_Cwd();
		}
		slash = strrchr( exeDir, '\\' );
		if ( slash ) {
			*slash = 0;
		}
	}
	return exeDir;
}
