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
// vm.c -- game module loading
//
// Q3Doom: game, cgame and ui are always native DLLs. A dll has one imported
// function (VM_DllSyscall, handed over through dllEntry) and one exported
// function (vmMain). All syscall arguments travel as intptr_t so pointers
// survive on 64-bit; int arguments are truncated back by the handlers.

#include "vm_local.h"


vm_t	*currentVM = NULL;
vm_t	*lastVM    = NULL;
int		vm_debugLevel;

#define	MAX_VM		3
vm_t	vmTable[MAX_VM];

// enough for the largest syscall (CG_R_DRAWSTRETCHPIC and friends)
#define	MAX_VMSYSCALL_ARGS	16
#define	MAX_VMMAIN_ARGS		12


void VM_VmInfo_f( void );


void VM_Debug( int level ) {
	vm_debugLevel = level;
}

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
	Cmd_AddCommand ("vminfo", VM_VmInfo_f );

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}

/*
============
VM_DllSyscall

Dlls will call this directly. The arguments are collected with stdarg
instead of assuming they sit contiguously on the stack, which is not
true on x86_64.
============
*/
static intptr_t QDECL VM_DllSyscall( intptr_t arg, ... ) {
	intptr_t	args[MAX_VMSYSCALL_ARGS];
	va_list		ap;
	int			i;

	args[0] = arg;

	va_start( ap, arg );
	for ( i = 1 ; i < MAX_VMSYSCALL_ARGS ; i++ ) {
		args[i] = va_arg( ap, intptr_t );
	}
	va_end( ap );

	return currentVM->systemCall( args );
}

/*
=================
VM_Restart

DLLs can't be restarted in place, so this reloads the module.
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	char		name[MAX_QPATH];
	intptr_t	(*systemCall)( intptr_t *parms );

	systemCall = vm->systemCall;
	Q_strncpyz( name, vm->name, sizeof( name ) );

	VM_Free( vm );

	return VM_Create( name, systemCall );
}

/*
================
VM_Create

Loads <module><ARCH_STRING>.dll from the game directory
================
*/
vm_t *VM_Create( const char *module, intptr_t (*systemCalls)(intptr_t *) ) {
	vm_t		*vm;
	int			i;

	if ( !module || !module[0] || !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	// see if we already have the VM
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		if (!Q_stricmp(vmTable[i].name, module)) {
			vm = &vmTable[i];
			return vm;
		}
	}

	// find a free vm
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		if ( !vmTable[i].name[0] ) {
			break;
		}
	}

	if ( i == MAX_VM ) {
		Com_Error( ERR_FATAL, "VM_Create: no free vm_t" );
	}

	vm = &vmTable[i];

	Q_strncpyz( vm->name, module, sizeof( vm->name ) );
	vm->systemCall = systemCalls;

	Com_Printf( "Loading dll file %s.\n", vm->name );
	vm->dllHandle = Sys_LoadDll( module, vm->fqpath , &vm->entryPoint, VM_DllSyscall );
	if ( !vm->dllHandle ) {
		Com_Printf( "Failed to load dll %s.\n", vm->name );
		VM_Free( vm );
		return NULL;
	}

	return vm;
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {
	if ( vm->dllHandle ) {
		Sys_UnloadDll( vm->dllHandle );
	}
	Com_Memset( vm, 0, sizeof( *vm ) );

	currentVM = NULL;
	lastVM = NULL;
}

void VM_Clear(void) {
	int i;
	for (i=0;i<MAX_VM; i++) {
		if ( vmTable[i].dllHandle ) {
			Sys_UnloadDll( vmTable[i].dllHandle );
		}
		Com_Memset( &vmTable[i], 0, sizeof( vm_t ) );
	}
	currentVM = NULL;
	lastVM = NULL;
}

// native modules pass real pointers, so these are plain casts
void *VM_ArgPtr( intptr_t intValue ) {
	return (void *)intValue;
}

void *VM_ExplicitArgPtr( vm_t *vm, intptr_t intValue ) {
	return (void *)intValue;
}


/*
==============
VM_Call

vmMain takes int arguments only; the return value may carry a pointer
(e.g. GAME_CLIENT_CONNECT returns the deny message).
==============
*/
intptr_t QDECL VM_Call( vm_t *vm, int callnum, ... ) {
	vm_t		*oldVM;
	intptr_t	r;
	int			i;
	int			args[MAX_VMMAIN_ARGS];
	va_list		ap;

	if ( !vm || !vm->entryPoint ) {
		Com_Error( ERR_FATAL, "VM_Call with NULL vm" );
	}

	oldVM = currentVM;
	currentVM = vm;
	lastVM = vm;

	if ( vm_debugLevel ) {
	  Com_Printf( "VM_Call( %i )\n", callnum );
	}

	va_start( ap, callnum );
	for ( i = 0 ; i < MAX_VMMAIN_ARGS ; i++ ) {
		args[i] = va_arg( ap, int );
	}
	va_end( ap );

	r = vm->entryPoint( callnum, args[0], args[1], args[2], args[3],
						args[4], args[5], args[6], args[7],
						args[8], args[9], args[10], args[11] );

	if ( oldVM != NULL )
	  currentVM = oldVM;
	return r;
}

/*
==============
VM_VmInfo_f
==============
*/
void VM_VmInfo_f( void ) {
	vm_t	*vm;
	int		i;

	Com_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name[0] ) {
			break;
		}
		Com_Printf( "%s : native (%s)\n", vm->name, vm->fqpath );
	}
}
