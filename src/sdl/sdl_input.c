/*
===========================================================================
Q3Doom - SDL2 keyboard and mouse input (replaces win32/win_input.c and
the keyboard handling of win32/win_wndproc.c)

This file is part of Q3Doom, based on Quake III Arena source code
Copyright (C) 1999-2005 Id Software, Inc.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../client/client.h"
#include "sdl_local.h"

static cvar_t	*in_mouse;
static cvar_t	*in_nograb;

static qboolean	mouseActive = qfalse;

/*
===============
IN_TranslateKey

Maps an SDL key to a Quake 3 key number. Printable keys use the
lowercase character of the current layout; 0 means "not mapped".
===============
*/
static int IN_TranslateKey( const SDL_Keysym *keysym )
{
	// the key below Esc always toggles the console, whatever the layout
	if ( keysym->scancode == SDL_SCANCODE_GRAVE ) {
		return '`';
	}

	switch ( keysym->sym ) {
	case SDLK_TAB:			return K_TAB;
	case SDLK_RETURN:		return K_ENTER;
	case SDLK_ESCAPE:		return K_ESCAPE;
	case SDLK_SPACE:		return K_SPACE;
	case SDLK_BACKSPACE:	return K_BACKSPACE;
	case SDLK_CAPSLOCK:		return K_CAPSLOCK;
	case SDLK_PAUSE:		return K_PAUSE;

	case SDLK_UP:			return K_UPARROW;
	case SDLK_DOWN:			return K_DOWNARROW;
	case SDLK_LEFT:			return K_LEFTARROW;
	case SDLK_RIGHT:		return K_RIGHTARROW;

	case SDLK_LALT:
	case SDLK_RALT:			return K_ALT;
	case SDLK_LCTRL:
	case SDLK_RCTRL:		return K_CTRL;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:		return K_SHIFT;
	case SDLK_INSERT:		return K_INS;
	case SDLK_DELETE:		return K_DEL;
	case SDLK_PAGEDOWN:		return K_PGDN;
	case SDLK_PAGEUP:		return K_PGUP;
	case SDLK_HOME:			return K_HOME;
	case SDLK_END:			return K_END;

	case SDLK_F1:			return K_F1;
	case SDLK_F2:			return K_F2;
	case SDLK_F3:			return K_F3;
	case SDLK_F4:			return K_F4;
	case SDLK_F5:			return K_F5;
	case SDLK_F6:			return K_F6;
	case SDLK_F7:			return K_F7;
	case SDLK_F8:			return K_F8;
	case SDLK_F9:			return K_F9;
	case SDLK_F10:			return K_F10;
	case SDLK_F11:			return K_F11;
	case SDLK_F12:			return K_F12;
	case SDLK_F13:			return K_F13;
	case SDLK_F14:			return K_F14;
	case SDLK_F15:			return K_F15;

	case SDLK_KP_7:			return K_KP_HOME;
	case SDLK_KP_8:			return K_KP_UPARROW;
	case SDLK_KP_9:			return K_KP_PGUP;
	case SDLK_KP_4:			return K_KP_LEFTARROW;
	case SDLK_KP_5:			return K_KP_5;
	case SDLK_KP_6:			return K_KP_RIGHTARROW;
	case SDLK_KP_1:			return K_KP_END;
	case SDLK_KP_2:			return K_KP_DOWNARROW;
	case SDLK_KP_3:			return K_KP_PGDN;
	case SDLK_KP_ENTER:		return K_KP_ENTER;
	case SDLK_KP_0:			return K_KP_INS;
	case SDLK_KP_PERIOD:	return K_KP_DEL;
	case SDLK_KP_DIVIDE:	return K_KP_SLASH;
	case SDLK_KP_MINUS:		return K_KP_MINUS;
	case SDLK_KP_PLUS:		return K_KP_PLUS;
	case SDLK_NUMLOCKCLEAR:	return K_KP_NUMLOCK;
	case SDLK_KP_MULTIPLY:	return K_KP_STAR;
	case SDLK_KP_EQUALS:	return K_KP_EQUALS;

	default:
		break;
	}

	if ( keysym->sym > 32 && keysym->sym < 127 ) {
		return tolower( keysym->sym );
	}
	return 0;
}

/*
===============
IN_ActivateMouse / IN_DeactivateMouse
===============
*/
static void IN_ActivateMouse( void )
{
	if ( mouseActive || !SDL_window || !in_mouse->integer ) {
		return;
	}
	if ( !in_nograb->integer ) {
		SDL_SetRelativeMouseMode( SDL_TRUE );
	}
	mouseActive = qtrue;
}

static void IN_DeactivateMouse( void )
{
	if ( !mouseActive ) {
		return;
	}
	SDL_SetRelativeMouseMode( SDL_FALSE );
	mouseActive = qfalse;
}

/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	Com_Printf( "\n------- Input Initialization -------\n" );

	in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE );
	in_nograb = Cvar_Get( "in_nograb", "0", CVAR_ARCHIVE );

	if ( !SDL_WasInit( SDL_INIT_EVENTS ) ) {
		SDL_InitSubSystem( SDL_INIT_EVENTS );
	}
	SDL_StartTextInput();

	Com_Printf( "------------------------------------\n" );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
	IN_DeactivateMouse();
	SDL_StopTextInput();
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
	SDL_Event	e;
	int			key;
	int			mx = 0, my = 0;
	const char	*c;

	while ( SDL_PollEvent( &e ) ) {
		switch ( e.type ) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			key = IN_TranslateKey( &e.key.keysym );
			if ( key ) {
				Sys_QueEvent( 0, SE_KEY, key, e.type == SDL_KEYDOWN, 0, NULL );
			}
			if ( e.type == SDL_KEYDOWN ) {
				// the console field editor expects control characters as char events
				if ( key == K_BACKSPACE ) {
					Sys_QueEvent( 0, SE_CHAR, '\b', 0, 0, NULL );
				} else if ( ( e.key.keysym.mod & KMOD_CTRL ) && key >= 'a' && key <= 'z' ) {
					Sys_QueEvent( 0, SE_CHAR, key - 'a' + 1, 0, 0, NULL );
				}
			}
			break;

		case SDL_TEXTINPUT:
			// only plain ASCII is meaningful to the Quake 3 font
			for ( c = e.text.text ; *c ; c++ ) {
				if ( (unsigned char)*c >= 32 && (unsigned char)*c < 127 && *c != '`' ) {
					Sys_QueEvent( 0, SE_CHAR, *c, 0, 0, NULL );
				}
			}
			break;

		case SDL_MOUSEMOTION:
			if ( mouseActive ) {
				mx += e.motion.xrel;
				my += e.motion.yrel;
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			switch ( e.button.button ) {
			case SDL_BUTTON_LEFT:	key = K_MOUSE1; break;
			case SDL_BUTTON_RIGHT:	key = K_MOUSE2; break;
			case SDL_BUTTON_MIDDLE:	key = K_MOUSE3; break;
			case SDL_BUTTON_X1:		key = K_MOUSE4; break;
			case SDL_BUTTON_X2:		key = K_MOUSE5; break;
			default:				key = 0; break;
			}
			if ( key ) {
				Sys_QueEvent( 0, SE_KEY, key, e.type == SDL_MOUSEBUTTONDOWN, 0, NULL );
			}
			break;

		case SDL_MOUSEWHEEL:
			if ( e.wheel.y > 0 ) {
				Sys_QueEvent( 0, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
				Sys_QueEvent( 0, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
			} else if ( e.wheel.y < 0 ) {
				Sys_QueEvent( 0, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
				Sys_QueEvent( 0, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
			}
			break;

		case SDL_WINDOWEVENT:
			if ( e.window.event == SDL_WINDOWEVENT_FOCUS_LOST ) {
				IN_DeactivateMouse();
				Key_ClearStates();
			}
			break;

		case SDL_QUIT:
			Cbuf_ExecuteText( EXEC_NOW, "quit Closed window\n" );
			break;

		default:
			break;
		}
	}

	if ( mx || my ) {
		Sys_QueEvent( 0, SE_MOUSE, mx, my, 0, NULL );
	}
}

/*
===============
IN_Frame

Called once per frame by the main loop
===============
*/
void IN_Frame( void )
{
	qboolean	wantMouse;

	if ( !SDL_window ) {
		// dedicated server or no renderer yet: still drain the queue
		SDL_PumpEvents();
		SDL_FlushEvents( SDL_FIRSTEVENT, SDL_LASTEVENT );
		IN_DeactivateMouse();
		return;
	}

	// release the mouse while the console is down in windowed mode,
	// or when the window does not have focus
	wantMouse = ( SDL_GetWindowFlags( SDL_window ) & SDL_WINDOW_INPUT_FOCUS ) != 0;
	if ( ( cls.keyCatchers & KEYCATCH_CONSOLE ) && !Cvar_VariableIntegerValue( "r_fullscreen" ) ) {
		wantMouse = qfalse;
	}

	if ( wantMouse ) {
		IN_ActivateMouse();
	} else {
		IN_DeactivateMouse();
	}

	IN_ProcessEvents();
}
