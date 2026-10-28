/*
===========================================================================
libdoom tests. Run with the path to an IWAD; exits 77 (skipped) if the
WAD is not available, so the build never depends on commercial data.

Part of Q3Doom. Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom.h"
#include "doom_info.h"

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

/*
=================
TestFragCeilings

DoomFrag copies (DF_<level>): ceilings raised to 106 above the highest
floor with room to stand under them, where they are lower; sectors no one
can stand in (closed doors) left alone; nothing else changed.
=================
*/
static int FragBase( const doom_sector_t *s, const doom_plane_range_t *r ) {
	if ( s->ceiling_height - r->floor_max >= 56 ) {
		return r->floor_max > s->floor_height ? r->floor_max : s->floor_height;
	}
	return s->ceiling_height - s->floor_height >= 56 ? s->floor_height : -100000;
}

static void TestFragCeilings( const doom_wad_t *wad ) {
	char		names[64][DOOM_LUMP_NAME_LEN + 1], err[256];
	doom_map_t	orig, frag;
	int			i, j, n;

	CHECK( !doom_map_load( wad, "E1M4", &orig, err, sizeof( err ) ), "E1M4: %s", err );
	CHECK( !doom_map_load( wad, "DF_E1M4", &frag, err, sizeof( err ) ), "DF_E1M4: %s", err );
	if ( frag.num_sectors == orig.num_sectors && frag.num_sectors > 108 ) {
		CHECK( !strcmp( frag.name, "E1M4" ), "the copy is named %s", frag.name );
		// corridor 82 (floor 128, ceiling 216) raised to 128 + 106
		CHECK( frag.sectors[82].ceiling_height == 234, "sector 82 ceiling %d", frag.sectors[82].ceiling_height );
		// the EXIT sign's gap 56 (192..248): exactly 56 high, raised
		CHECK( frag.sectors[56].ceiling_height == 298, "sector 56 ceiling %d", frag.sectors[56].ceiling_height );
		// the exit door 58 (192, closed at 192): untouched
		CHECK( frag.sectors[58].ceiling_height == 192, "door 58 ceiling %d", frag.sectors[58].ceiling_height );
		// lift 88 (top at 128, ceiling 200): raised from the top of its range
		CHECK( frag.sectors[88].ceiling_height == 234, "lift 88 ceiling %d", frag.sectors[88].ceiling_height );
		// tall room 6 (136..328): untouched
		CHECK( frag.sectors[6].ceiling_height == 328, "sector 6 ceiling %d", frag.sectors[6].ceiling_height );
	} else {
		CHECK( 0, "DF_E1M4 has %d sectors, E1M4 %d", frag.num_sectors, orig.num_sectors );
	}
	doom_map_free( &orig );
	doom_map_free( &frag );

	CHECK( doom_map_load( wad, "DF_E9M9", &frag, err, sizeof( err ) ), "DF_E9M9 should not load" );
	CHECK( doom_map_load( wad, "DF_", &frag, err, sizeof( err ) ), "DF_ should not load" );

	// the rule on every sector of every level
	n = doom_wad_list_maps( wad, names, 64 );
	for ( i = 0 ; i < n && i < 64 ; i++ ) {
		doom_plane_range_t	*ranges;
		char				name[DOOM_LUMP_NAME_LEN + 4];

		snprintf( name, sizeof( name ), "DF_%s", names[i] );
		if ( doom_map_load( wad, names[i], &orig, err, sizeof( err ) ) ) {
			continue;
		}
		if ( doom_map_load( wad, name, &frag, err, sizeof( err ) ) ) {
			CHECK( 0, "%s: %s", name, err );
			doom_map_free( &orig );
			continue;
		}
		ranges = calloc( orig.num_sectors, sizeof( *ranges ) );
		doom_sector_ranges( wad, &orig, ranges );
		for ( j = 0 ; j < orig.num_sectors ; j++ ) {
			const doom_sector_t	*o = &orig.sectors[j], *f = &frag.sectors[j];
			int					base = FragBase( o, &ranges[j] );
			int					want = base > -100000 && o->ceiling_height < base + 106 ? base + 106 : o->ceiling_height;

			CHECK( f->floor_height == o->floor_height, "%s sector %d: floor %d -> %d", name, j, o->floor_height, f->floor_height );
			CHECK( f->ceiling_height == want, "%s sector %d: ceiling %d -> %d, want %d", name, j, o->ceiling_height, f->ceiling_height, want );
		}
		free( ranges );
		doom_map_free( &orig );
		doom_map_free( &frag );
	}
}

/*
=================
TestBuilder

Every map builds, its BSP cells tile the bounds exactly (nothing is left
unclassified), and building twice gives the same bytes.
=================
*/
static void TestBuilder( const doom_wad_t *wad ) {
	char	names[64][DOOM_LUMP_NAME_LEN + 1];
	int		i, n;

	n = doom_wad_list_maps( wad, names, 64 );
	for ( i = 0 ; i < n && i < 64 ; i++ ) {
		doom_map_t			map;
		doom_build_stats_t	st;
		uint8_t				*a = NULL, *b = NULL;
		size_t				asize = 0, bsize = 0;
		char				err[256];

		if ( doom_map_load( wad, names[i], &map, err, sizeof( err ) ) ) {
			continue;	// reported by TestMaps
		}
		CHECK( !doom_build_bsp( wad, &map, &a, &asize, &st, err, sizeof( err ) ), "build %s: %s", names[i], err );
		CHECK( !doom_build_bsp( wad, &map, &b, &bsize, NULL, err, sizeof( err ) ), "rebuild %s: %s", names[i], err );
		CHECK( a && b && asize == bsize && !memcmp( a, b, asize ), "%s: build is not deterministic", names[i] );
		CHECK( asize > 8 && !memcmp( a, "IBSP", 4 ) && a[4] == 46, "%s: not an IBSP v46", names[i] );
		CHECK( st.covered_area > st.bounds_area * 0.99999 && st.covered_area < st.bounds_area * 1.00001,
			"%s: leafs cover %.6f of the bounds", names[i], st.covered_area / st.bounds_area );
		CHECK( st.sector_leafs > 0 && st.void_leafs > 0 && st.brushes > 0, "%s: empty build", names[i] );
		free( a );
		free( b );
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

// QuakeDoom Duel: the monsters at the Quake 3 item spots, by the prize they guard
static void CheckSpot( const char *classname, int numTypes, int t0, int t1, int t2, int count ) {
	const doom_spot_monsters_t	*s = doom_quake_spot_monsters( classname );
	const int					types[3] = { t0, t1, t2 };
	int							i;

	CHECK( s != NULL, "%s should have monsters", classname );
	if ( !s ) {
		return;
	}
	CHECK( s->numTypes == numTypes && s->count == count, "%s: %d types, %d monsters", classname, s->numTypes, s->count );
	for ( i = 0 ; i < numTypes && i < s->numTypes ; i++ ) {
		CHECK( s->types[i] == types[i], "%s: type %d is %d", classname, i, s->types[i] );
	}
}

static void TestQuakeSpots( void ) {
	static const char	*light[] = { "weapon_gauntlet", "weapon_shotgun", "weapon_machinegun" };
	static const char	*heavy[] = { "weapon_grenadelauncher", "weapon_rocketlauncher", "weapon_lightning", "weapon_railgun", "weapon_plasmagun" };
	static const char	*powerups[] = { "item_quad", "item_haste", "item_regen", "item_invis", "item_enviro", "item_flight" };
	static const char	*none[] = { "item_armor_shard", "item_health", "item_health_mega", "ammo_rockets", "holdable_medkit",
		"team_CTF_redflag", "", "Weapon_Shotgun" };
	int					i;

	for ( i = 0 ; i < 3 ; i++ ) {
		CheckSpot( light[i], 3, MT_POSSESSED, MT_SHOTGUY, MT_TROOP, 1 );
	}
	for ( i = 0 ; i < 5 ; i++ ) {
		CheckSpot( heavy[i], 3, MT_SERGEANT, MT_SHADOWS, MT_HEAD, 1 );
	}
	CheckSpot( "item_armor_combat", 1, MT_HEAD, 0, 0, 1 );
	CheckSpot( "item_armor_body", 1, MT_BRUISER, 0, 0, 1 );
	CheckSpot( "weapon_bfg", 1, MT_BRUISER, 0, 0, 1 );
	for ( i = 0 ; i < 6 ; i++ ) {
		CheckSpot( powerups[i], 1, MT_SKULL, 0, 0, 2 );
	}
	for ( i = 0 ; i < (int)( sizeof( none ) / sizeof( none[0] ) ) ; i++ ) {
		CHECK( doom_quake_spot_monsters( none[i] ) == NULL, "%s should have no monster", none[i] );
	}
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
	TestFragCeilings( wad );
	TestQuakeSpots();
	TestBuilder( wad );
	TestRobustness( wad );

	doom_wad_close( wad );

	printf( "%d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}
