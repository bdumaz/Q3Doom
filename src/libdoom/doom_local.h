/*
===========================================================================
libdoom internal definitions

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#ifndef LIBDOOM_LOCAL_H
#define LIBDOOM_LOCAL_H

#include "doom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int		*lumps;		// lump indexes, WAD order
	int		count;
} doom_ns_list_t;

struct doom_wad_s {
	uint8_t			*data;
	size_t			size;
	int				iwad;

	int				num_lumps;
	doom_lump_t		*lumps;

	doom_ns_list_t	flats;
	doom_ns_list_t	sprites;

	// composite textures
	int				num_pnames;
	char			(*pnames)[DOOM_LUMP_NAME_LEN + 1];
	int				num_textures;
	doom_texture_t	*textures;
};

// little-endian readers; callers check bounds first
static inline int16_t doom_s16( const uint8_t *p ) {
	return (int16_t)( p[0] | ( p[1] << 8 ) );
}
static inline uint16_t doom_u16( const uint8_t *p ) {
	return (uint16_t)( p[0] | ( p[1] << 8 ) );
}
static inline int32_t doom_s32( const uint8_t *p ) {
	return (int32_t)( (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 ) );
}
static inline uint32_t doom_u32( const uint8_t *p ) {
	return (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 );
}

// copies an 8-byte, possibly unterminated lump name, uppercased and NUL terminated
void	doom_copy_name( char dst[DOOM_LUMP_NAME_LEN + 1], const uint8_t *src );
int		doom_name_equal( const char *a, const char *b );

void	doom_set_error( char *err, size_t errlen, const char *fmt, ... );

// texture tables (graphics.c), parsed when the WAD is opened
void	doom_textures_init( doom_wad_t *wad );
void	doom_textures_free( doom_wad_t *wad );

#endif
