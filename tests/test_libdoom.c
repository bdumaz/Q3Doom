/*
===========================================================================
libdoom tests. Run with the path to an IWAD; exits 77 (skipped) if the
WAD is not available, so the build never depends on commercial data.

Part of Q3Doom. Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SKIP_CODE	77

static int failures;
static int checks;

#define CHECK( cond, ... ) do { \
	checks++; \
	if ( !( cond ) ) { \
		failures++; \
		printf( "FAIL %s:%d: ", __FILE__, __LINE__ ); \
		printf( __VA_ARGS__ ); \
		printf( "\n" ); \
	} \
} while ( 0 )

static void *ReadFileData( const char *path, size_t *size ) {
	FILE	*f = fopen( path, "rb" );
	void	*data;
	long	len;

	if ( !f ) {
		return NULL;
	}
	fseek( f, 0, SEEK_END );
	len = ftell( f );
	fseek( f, 0, SEEK_SET );
	data = malloc( len );
	if ( fread( data, 1, len, f ) != (size_t)len ) {
		free( data );
		data = NULL;
	}
	fclose( f );
	*size = (size_t)len;
	return data;
}

static void TestBasics( const doom_wad_t *wad ) {
	CHECK( doom_wad_is_iwad( wad ), "expected an IWAD" );
	CHECK( doom_num_palettes( wad ) == 14, "PLAYPAL has %d palettes", doom_num_palettes( wad ) );
	CHECK( doom_palette( wad, 0 ) != NULL, "palette 0 missing" );
	CHECK( doom_palette( wad, 14 ) == NULL, "palette 14 should not exist" );
	CHECK( doom_colormap( wad, 0 ) && doom_colormap( wad, 33 ), "COLORMAP should have 34 maps" );
	CHECK( doom_wad_find( wad, "playpal" ) == doom_wad_find( wad, "PLAYPAL" ), "lookup must be case-insensitive" );
	CHECK( doom_wad_find( wad, "NOSUCHLUMP" ) == -1, "missing lump should return -1" );
}

static void TestTextures( const doom_wad_t *wad ) {
	int				i, p;
	doom_image_t	img;

	CHECK( doom_num_textures( wad ) > 0, "no textures" );
	for ( i = 0 ; i < doom_num_textures( wad ) ; i++ ) {
		const doom_texture_t *tex = doom_texture( wad, i );

		for ( p = 0 ; p < tex->num_patches ; p++ ) {
			const char *pname = doom_pname( wad, tex->patches[p].patch );
			CHECK( pname && doom_wad_find( wad, pname ) >= 0, "texture %s: missing patch %d", tex->name, p );
		}
		CHECK( !doom_load_texture( wad, tex->name, &img ), "texture %s failed to decode", tex->name );
		CHECK( img.width == tex->width && img.height == tex->height, "texture %s has the wrong size", tex->name );
		doom_image_free( &img );
	}
}

static void TestNamespace( const doom_wad_t *wad, doom_namespace_t ns, const char *what,
	int (*load)( const doom_wad_t *, const char *, doom_image_t * ) ) {
	int				i, n = doom_wad_ns_count( wad, ns );
	doom_image_t	img;

	CHECK( n > 0, "no %s", what );
	for ( i = 0 ; i < n ; i++ ) {
		const char *name = doom_wad_lump( wad, doom_wad_ns_lump( wad, ns, i ) )->name;

		CHECK( !load( wad, name, &img ) && img.rgba, "%s %s failed to decode", what, name );
		doom_image_free( &img );
	}
}

static void TestSounds( const doom_wad_t *wad ) {
	int				i, count = 0;
	doom_sound_t	snd;

	for ( i = 0 ; i < doom_wad_num_lumps( wad ) ; i++ ) {
		const char *name = doom_wad_lump( wad, i )->name;

		if ( strncmp( name, "DS", 2 ) || doom_wad_find( wad, name ) != i ) {
			continue;
		}
		count++;
		CHECK( !doom_load_sound( wad, name, &snd ), "sound %s failed to decode", name );
		// all Doom sounds are 11025 Hz except DSITMBK (22050 Hz)
		CHECK( snd.rate == 11025 || snd.rate == 22050, "sound %s has rate %d", name, snd.rate );
	}
	CHECK( count > 50, "only %d sounds", count );
	CHECK( doom_load_sound( wad, "PLAYPAL", &snd ) == -1, "PLAYPAL is not a sound" );
}

static void TestMaps( const doom_wad_t *wad ) {
	char		names[64][DOOM_LUMP_NAME_LEN + 1];
	int			i, j, n;

	n = doom_wad_list_maps( wad, names, 64 );
	CHECK( n > 0, "no maps" );
	for ( i = 0 ; i < n && i < 64 ; i++ ) {
		doom_map_t	map;
		char		err[256];

		CHECK( !doom_map_load( wad, names[i], &map, err, sizeof( err ) ), "map %s: %s", names[i], err );
		CHECK( !doom_map_validate( &map, err, sizeof( err ) ), "map %s invalid: %s", names[i], err );

		for ( j = 0 ; j < map.num_sidedefs ; j++ ) {
			const doom_sidedef_t *s = &map.sidedefs[j];
			CHECK( !strcmp( s->top, "-" ) || doom_find_texture( wad, s->top ) >= 0, "%s sidedef %d: no texture %s", map.name, j, s->top );
			CHECK( !strcmp( s->mid, "-" ) || doom_find_texture( wad, s->mid ) >= 0, "%s sidedef %d: no texture %s", map.name, j, s->mid );
			CHECK( !strcmp( s->bottom, "-" ) || doom_find_texture( wad, s->bottom ) >= 0, "%s sidedef %d: no texture %s", map.name, j, s->bottom );
		}
		for ( j = 0 ; j < map.num_sectors ; j++ ) {
			const doom_sector_t *s = &map.sectors[j];
			CHECK( doom_wad_find_ns( wad, DOOM_NS_FLATS, s->floor_flat ) >= 0, "%s sector %d: no flat %s", map.name, j, s->floor_flat );
			CHECK( doom_wad_find_ns( wad, DOOM_NS_FLATS, s->ceiling_flat ) >= 0, "%s sector %d: no flat %s", map.name, j, s->ceiling_flat );
		}
		for ( j = 0 ; j < map.num_things ; j++ ) {
			if ( map.things[j].type == 1 ) {
				break;
			}
		}
		CHECK( j < map.num_things, "map %s has no player 1 start", map.name );

		doom_map_free( &map );
	}
}

static void TestRobustness( const doom_wad_t *wad ) {
	doom_image_t	img;
	doom_map_t		map;
	char			err[256];
	int				lump, len;
	uint8_t			*garbage;

	// not a WAD
	garbage = malloc( 64 );
	memset( garbage, 0xAB, 64 );
	CHECK( doom_wad_open( garbage, 64, err, sizeof( err ) ) == NULL, "garbage accepted as a WAD" );

	// every truncation of a sprite must either fail cleanly or (when only
	// trailing padding was cut) decode to exactly the same picture
	lump = doom_wad_find_ns( wad, DOOM_NS_SPRITES, "TROOA1" );
	CHECK( lump >= 0, "TROOA1 missing" );
	if ( lump >= 0 ) {
		const uint8_t	*data = doom_wad_lump_data( wad, lump );
		int				size = (int)doom_wad_lump( wad, lump )->size;
		doom_image_t	full;

		CHECK( !doom_decode_patch( data, size, doom_palette( wad, 0 ), &full ), "TROOA1 failed to decode" );
		for ( len = 0 ; len < size ; len++ ) {
			if ( !doom_decode_patch( data, len, doom_palette( wad, 0 ), &img ) ) {
				CHECK( img.width == full.width && img.height == full.height
					&& !memcmp( img.rgba, full.rgba, (size_t)full.width * full.height * 4 ),
					"TROOA1 truncated to %d bytes decoded differently", len );
			}
			doom_image_free( &img );
		}
		doom_image_free( &full );
	}

	// validation catches broken references
	CHECK( !doom_map_load( wad, "E1M1", &map, err, sizeof( err ) ), "E1M1: %s", err );
	if ( map.num_linedefs ) {
		map.linedefs[0].v2 = map.num_vertexes;
		CHECK( doom_map_validate( &map, err, sizeof( err ) ) == -1, "bad vertex index not detected" );
		map.linedefs[0].v2 = 0;
		map.sidedefs[0].sector = -5;
		CHECK( doom_map_validate( &map, err, sizeof( err ) ) == -1, "bad sector index not detected" );
	}
	doom_map_free( &map );

	CHECK( doom_map_load( wad, "E9M9", &map, err, sizeof( err ) ) == -1, "missing map should fail" );
}

int main( int argc, char **argv ) {
	void		*data;
	size_t		size;
	char		err[256];
	doom_wad_t	*wad;

	if ( argc < 2 || !( data = ReadFileData( argv[1], &size ) ) ) {
		printf( "SKIP: IWAD not found (%s)\n", argc > 1 ? argv[1] : "no path given" );
		return SKIP_CODE;
	}
	wad = doom_wad_open( data, size, err, sizeof( err ) );
	if ( !wad ) {
		printf( "FAIL: %s\n", err );
		return 1;
	}

	TestBasics( wad );
	TestTextures( wad );
	TestNamespace( wad, DOOM_NS_FLATS, "flat", doom_load_flat );
	TestNamespace( wad, DOOM_NS_SPRITES, "sprite", doom_load_sprite );
	TestSounds( wad );
	TestMaps( wad );
	TestRobustness( wad );

	doom_wad_close( wad );

	printf( "%d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}
