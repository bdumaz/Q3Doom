/*
===========================================================================
libdoom - campaign progress: the levels unlocked, medals and best times

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "doom.h"

static int NameEquals( const char *a, const char *b ) {
	for ( ; *a && *b ; a++, b++ ) {
		if ( toupper( (unsigned char)*a ) != toupper( (unsigned char)*b ) ) {
			return 0;
		}
	}
	return *a == *b;
}

doom_progress_level_t *doom_progress_level( doom_progress_t *p, const char *map, int create ) {
	int		i;

	for ( i = 0 ; i < p->count ; i++ ) {
		if ( NameEquals( p->levels[i].map, map ) ) {
			return &p->levels[i];
		}
	}
	if ( !create || p->count >= DOOM_PROGRESS_MAX || !map[0] ) {
		return NULL;
	}
	memset( &p->levels[p->count], 0, sizeof( p->levels[0] ) );
	for ( i = 0 ; map[i] && i < DOOM_LUMP_NAME_LEN ; i++ ) {
		p->levels[p->count].map[i] = (char)toupper( (unsigned char)map[i] );
	}
	return &p->levels[p->count++];
}

int doom_progress_unlocked( const doom_progress_t *p, const char *map ) {
	int		i;

	for ( i = 0 ; i < p->count ; i++ ) {
		if ( NameEquals( p->levels[i].map, map ) ) {
			return p->levels[i].unlocked;
		}
	}
	return 0;
}

void doom_progress_parse( const char *text, doom_progress_t *p ) {
	memset( p, 0, sizeof( *p ) );
	while ( text && *text ) {
		char	map[16];
		int		unlocked, medal, best;

		if ( sscanf( text, "level %15s unlocked %d medal %d best %d", map, &unlocked, &medal, &best ) == 4 ) {
			doom_progress_level_t *l = doom_progress_level( p, map, 1 );

			if ( l ) {
				l->unlocked = unlocked != 0;
				l->medal = medal < 0 ? 0 : medal > DOOM_MEDAL_SKULL ? DOOM_MEDAL_SKULL : medal;
				l->best_tics = best > 0 ? best : 0;
			}
		}
		text = strchr( text, '\n' );
		if ( text ) {
			text++;
		}
	}
	// a new game starts with the first level
	doom_progress_level( p, "E1M1", 1 )->unlocked = 1;
}

int doom_progress_write( const doom_progress_t *p, char *buf, size_t size ) {
	size_t	len = 0;
	int		i;

	if ( !size ) {
		return 0;
	}
	buf[0] = 0;
	len += snprintf( buf + len, size - len, "// Q3Doom campaign progress\n" );
	for ( i = 0 ; i < p->count && len < size ; i++ ) {
		const doom_progress_level_t *l = &p->levels[i];

		len += snprintf( buf + len, size - len, "level %s unlocked %d medal %d best %d\n", l->map, l->unlocked, l->medal, l->best_tics );
	}
	if ( len >= size ) {
		len = size - 1;
		buf[len] = 0;
	}
	return (int)len;
}

int doom_skill_medal( int skill ) {
	if ( skill <= 1 ) {
		return DOOM_MEDAL_BRONZE;
	}
	if ( skill == 2 ) {
		return DOOM_MEDAL_SILVER;
	}
	if ( skill == 3 ) {
		return DOOM_MEDAL_GOLD;
	}
	return DOOM_MEDAL_SKULL;
}

int doom_par_time( int episode, int map ) {
	static const int pars[3][9] = {
		{ 30, 75, 120, 90, 165, 180, 180, 30, 165 },
		{ 90, 90, 90, 120, 90, 360, 240, 30, 170 },
		{ 90, 45, 90, 150, 90, 90, 165, 30, 135 }
	};

	if ( episode < 1 || episode > 3 || map < 1 || map > 9 ) {
		return 0;
	}
	return pars[episode - 1][map - 1];
}
