/*
===========================================================================
Q3Doom - SDL2 platform layer (window, OpenGL context, input, sound output)

This file is part of Q3Doom, based on Quake III Arena source code
Copyright (C) 1999-2005 Id Software, Inc.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#ifndef __SDL_LOCAL_H
#define __SDL_LOCAL_H

#define SDL_MAIN_HANDLED
#include <SDL.h>

// created by GLimp_Init, NULL for a dedicated server
extern SDL_Window	*SDL_window;

void	Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );

// input
void	IN_Init( void );
void	IN_Shutdown( void );
void	IN_Frame( void );		// pumps SDL events, called once per frame

#endif
