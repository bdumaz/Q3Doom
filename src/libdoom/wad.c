/*
===========================================================================
libdoom - WAD directory and lump namespaces

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"

#include <ctype.h>
#include <stdarg.h>

#define WAD_HEADER_SIZE		12
#define WAD_DIRENTRY_SIZE	16

void doom_set_error( char *err, size_t errlen, const char *fmt, ... ) {
	va_list		ap;

	if ( !err || !errlen ) {
		return;
	}
	va_start( ap, fmt );
	vsnprintf( err, errlen, fmt, ap );
	va_end( ap );
}

void doom_copy_name( char dst[DOOM_LUMP_NAME_LEN + 1], const uint8_t *src ) {
	int		i;

	for ( i = 0 ; i < DOOM_LUMP_NAME_LEN && src[i] ; i++ ) {
		dst[i] = (char)toupper( src[i] );
	}
	dst[i] = 0;
}

int doom_name_equal( const char *a, const char *b ) {
	int		i;

	for ( i = 0 ; i < DOOM_LUMP_NAME_LEN ; i++ ) {
		if ( toupper( (unsigned char)a[i] ) != toupper( (unsigned char)b[i] ) ) {
			return 0;
		}
		if ( !a[i] ) {
			return 1;
		}
	}
	return 1;
}

/*
=================
BuildNamespace

Collects the lumps between start/end markers. Doom accepts both the
single-letter (F_START) and double-letter (FF_START) PWAD markers, and the
nested sub-markers (F1_START ...) which are skipped as entries.
=================
*/
static void BuildNamespace( doom_wad_t *wad, doom_ns_list_t *list, char letter ) {
	int		i, inside;
	char	start1[9], start2[9], end1[9], end2[9];

	snprintf( start1, sizeof( start1 ), "%c_START", letter );
	snprintf( start2, sizeof( start2 ), "%c%c_START", letter, letter );
	snprintf( end1, sizeof( end1 ), "%c_END", letter );
	snprintf( end2, sizeof( end2 ), "%c%c_END", letter, letter );

	list->lumps = malloc( sizeof( int ) * ( wad->num_lumps + 1 ) );
	list->count = 0;
	inside = 0;

	for ( i = 0 ; i < wad->num_lumps ; i++ ) {
		const char *name = wad->lumps[i].name;

		if ( !strcmp( name, start1 ) || !strcmp( name, start2 ) ) {
			inside = 1;
			continue;
		}
		if ( !strcmp( name, end1 ) || !strcmp( name, end2 ) ) {
			inside = 0;
			continue;
		}
		if ( !inside || wad->lumps[i].size == 0 ) {
			continue;	// sub-markers like F1_START are empty lumps
		}
		list->lumps[list->count++] = i;
	}
}

doom_wad_t *doom_wad_open( void *data, size_t size, char *err, size_t errlen ) {
	doom_wad_t		*wad;
	const uint8_t	*bytes = data;
	uint32_t		num, dir;
	int				i;

	if ( size < WAD_HEADER_SIZE || ( memcmp( bytes, "IWAD", 4 ) && memcmp( bytes, "PWAD", 4 ) ) ) {
		doom_set_error( err, errlen, "not a WAD file" );
		free( data );
		return NULL;
	}

	num = doom_u32( bytes + 4 );
	dir = doom_u32( bytes + 8 );
	if ( num > 1000000 || dir > size || (uint64_t)num * WAD_DIRENTRY_SIZE > size - dir ) {
		doom_set_error( err, errlen, "corrupt WAD directory" );
		free( data );
		return NULL;
	}

	wad = calloc( 1, sizeof( *wad ) );
	wad->data = data;
	wad->size = size;
	wad->iwad = !memcmp( bytes, "IWAD", 4 );
	wad->num_lumps = (int)num;
	wad->lumps = calloc( num ? num : 1, sizeof( doom_lump_t ) );

	for ( i = 0 ; i < wad->num_lumps ; i++ ) {
		const uint8_t	*e = bytes + dir + i * WAD_DIRENTRY_SIZE;
		doom_lump_t		*l = &wad->lumps[i];

		l->offset = doom_u32( e );
		l->size = doom_u32( e + 4 );
		doom_copy_name( l->name, e + 8 );

		if ( l->offset > size || l->size > size - l->offset ) {
			doom_set_error( err, errlen, "lump %s is outside the file", l->name );
			doom_wad_close( wad );
			return NULL;
		}
	}

	BuildNamespace( wad, &wad->flats, 'F' );
	BuildNamespace( wad, &wad->sprites, 'S' );
	doom_textures_init( wad );

	return wad;
}

void doom_wad_close( doom_wad_t *wad ) {
	if ( !wad ) {
		return;
	}
	doom_textures_free( wad );
	free( wad->flats.lumps );
	free( wad->sprites.lumps );
	free( wad->lumps );
	free( wad->data );
	free( wad );
}

int doom_wad_is_iwad( const doom_wad_t *wad ) {
	return wad->iwad;
}

int doom_wad_num_lumps( const doom_wad_t *wad ) {
	return wad->num_lumps;
}

const doom_lump_t *doom_wad_lump( const doom_wad_t *wad, int index ) {
	if ( index < 0 || index >= wad->num_lumps ) {
		return NULL;
	}
	return &wad->lumps[index];
}

const uint8_t *doom_wad_lump_data( const doom_wad_t *wad, int index ) {
	if ( index < 0 || index >= wad->num_lumps ) {
		return NULL;
	}
	return wad->data + wad->lumps[index].offset;
}

int doom_wad_find( const doom_wad_t *wad, const char *name ) {
	int		i;

	for ( i = wad->num_lumps - 1 ; i >= 0 ; i-- ) {
		if ( doom_name_equal( wad->lumps[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}

static const doom_ns_list_t *Namespace( const doom_wad_t *wad, doom_namespace_t ns ) {
	switch ( ns ) {
	case DOOM_NS_FLATS:		return &wad->flats;
	case DOOM_NS_SPRITES:	return &wad->sprites;
	default:				return NULL;
	}
}

int doom_wad_find_ns( const doom_wad_t *wad, doom_namespace_t ns, const char *name ) {
	const doom_ns_list_t	*list = Namespace( wad, ns );
	int						i;

	if ( !list ) {
		return doom_wad_find( wad, name );
	}
	for ( i = list->count - 1 ; i >= 0 ; i-- ) {
		if ( doom_name_equal( wad->lumps[list->lumps[i]].name, name ) ) {
			return list->lumps[i];
		}
	}
	return -1;
}

int doom_wad_ns_count( const doom_wad_t *wad, doom_namespace_t ns ) {
	const doom_ns_list_t	*list = Namespace( wad, ns );

	return list ? list->count : wad->num_lumps;
}

int doom_wad_ns_lump( const doom_wad_t *wad, doom_namespace_t ns, int n ) {
	const doom_ns_list_t	*list = Namespace( wad, ns );

	if ( !list ) {
		return ( n >= 0 && n < wad->num_lumps ) ? n : -1;
	}
	return ( n >= 0 && n < list->count ) ? list->lumps[n] : -1;
}
