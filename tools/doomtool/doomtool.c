/*
===========================================================================
doomtool - inspect and export Doom WAD contents with libdoom

  doomtool <wad> info                 summary of the WAD
  doomtool <wad> lumps                list every lump
  doomtool <wad> export <dir>         textures, flats, sprites, graphics as
                                      PNG, sounds as WAV, maps as SVG
  doomtool <wad> map <name> <out.svg> draw one map
  doomtool <wad> build <name|all> [dir] build Quake 3 BSPs, print stats
  doomtool <wad> music <lump> <out.wav> [seconds]
                                      render MUS music with the OPL synthesizer

Part of Q3Doom. Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define MakeDir( p )	_mkdir( p )
#else
#include <sys/stat.h>
#define MakeDir( p )	mkdir( p, 0755 )
#endif

#define MAX_MAPS	128

/*
=====================================================================

FILE OUTPUT

=====================================================================
*/

static uint32_t crcTable[256];

static void InitCrc( void ) {
	uint32_t	c, n, k;

	for ( n = 0 ; n < 256 ; n++ ) {
		c = n;
		for ( k = 0 ; k < 8 ; k++ ) {
			c = ( c & 1 ) ? 0xEDB88320u ^ ( c >> 1 ) : c >> 1;
		}
		crcTable[n] = c;
	}
}

static uint32_t Crc( uint32_t crc, const uint8_t *buf, size_t len ) {
	size_t	i;

	crc = ~crc;
	for ( i = 0 ; i < len ; i++ ) {
		crc = crcTable[( crc ^ buf[i] ) & 0xFF] ^ ( crc >> 8 );
	}
	return ~crc;
}

static void Put32BE( uint8_t *p, uint32_t v ) {
	p[0] = (uint8_t)( v >> 24 ); p[1] = (uint8_t)( v >> 16 ); p[2] = (uint8_t)( v >> 8 ); p[3] = (uint8_t)v;
}

static void WriteChunk( FILE *f, const char *type, const uint8_t *data, uint32_t len ) {
	uint8_t		hdr[8];
	uint32_t	crc;

	Put32BE( hdr, len );
	memcpy( hdr + 4, type, 4 );
	fwrite( hdr, 1, 8, f );
	if ( len ) {
		fwrite( data, 1, len, f );
	}
	crc = Crc( 0, hdr + 4, 4 );
	crc = Crc( crc, data, len );
	Put32BE( hdr, crc );
	fwrite( hdr, 1, 4, f );
}

/*
=================
WritePng

RGBA PNG with "stored" (uncompressed) deflate blocks: no zlib needed.
=================
*/
static int WritePng( const char *path, const doom_image_t *img ) {
	FILE		*f;
	size_t		rowlen = (size_t)img->width * 4 + 1;
	size_t		rawlen = rowlen * img->height;
	size_t		blocks = rawlen / 65535 + 1;
	size_t		zlen = 2 + rawlen + blocks * 5 + 4;
	uint8_t		*raw, *z, *zp, ihdr[13];
	uint32_t	a = 1, b = 0;
	size_t		i, left;
	int			y;

	raw = malloc( rawlen );
	z = malloc( zlen );
	for ( y = 0 ; y < img->height ; y++ ) {
		raw[y * rowlen] = 0;	// filter: none
		memcpy( raw + y * rowlen + 1, img->rgba + (size_t)y * img->width * 4, (size_t)img->width * 4 );
	}

	zp = z;
	*zp++ = 0x78;
	*zp++ = 0x01;
	for ( i = 0, left = rawlen ; ; ) {
		size_t	n = left > 65535 ? 65535 : left;

		*zp++ = ( left <= 65535 ) ? 1 : 0;
		*zp++ = (uint8_t)( n & 0xFF );
		*zp++ = (uint8_t)( n >> 8 );
		*zp++ = (uint8_t)( ~n & 0xFF );
		*zp++ = (uint8_t)( ( ~n >> 8 ) & 0xFF );
		memcpy( zp, raw + i, n );
		zp += n;
		i += n;
		left -= n;
		if ( !left ) {
			break;
		}
	}
	for ( i = 0 ; i < rawlen ; i++ ) {
		a = ( a + raw[i] ) % 65521;
		b = ( b + a ) % 65521;
	}
	Put32BE( zp, ( b << 16 ) | a );
	zp += 4;

	Put32BE( ihdr, img->width );
	Put32BE( ihdr + 4, img->height );
	ihdr[8] = 8;	// bit depth
	ihdr[9] = 6;	// RGBA
	ihdr[10] = ihdr[11] = ihdr[12] = 0;

	f = fopen( path, "wb" );
	if ( f ) {
		fwrite( "\x89PNG\r\n\x1a\n", 1, 8, f );
		WriteChunk( f, "IHDR", ihdr, 13 );
		WriteChunk( f, "IDAT", z, (uint32_t)( zp - z ) );
		WriteChunk( f, "IEND", NULL, 0 );
		fclose( f );
	}
	free( raw );
	free( z );
	return f ? 0 : -1;
}

static void Put32LE( uint8_t *p, uint32_t v ) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)( v >> 8 ); p[2] = (uint8_t)( v >> 16 ); p[3] = (uint8_t)( v >> 24 );
}

static int WriteWav( const char *path, const doom_sound_t *snd ) {
	FILE		*f = fopen( path, "wb" );
	uint8_t		h[44];

	if ( !f ) {
		return -1;
	}
	memcpy( h, "RIFF", 4 );
	Put32LE( h + 4, 36 + snd->num_samples );
	memcpy( h + 8, "WAVEfmt ", 8 );
	Put32LE( h + 16, 16 );
	h[20] = 1; h[21] = 0;			// PCM
	h[22] = 1; h[23] = 0;			// mono
	Put32LE( h + 24, snd->rate );
	Put32LE( h + 28, snd->rate );	// bytes per second
	h[32] = 1; h[33] = 0;			// block align
	h[34] = 8; h[35] = 0;			// bits per sample
	memcpy( h + 36, "data", 4 );
	Put32LE( h + 40, snd->num_samples );
	fwrite( h, 1, 44, f );
	fwrite( snd->samples, 1, snd->num_samples, f );
	fclose( f );
	return 0;
}

/*
=================
WriteMapSvg

One-sided lines red, two-sided grey, lines with a special yellow,
things as blue dots. Doom +Y is up, SVG +Y is down.
=================
*/
static int WriteMapSvg( const char *path, const doom_map_t *map ) {
	FILE	*f;
	int		i, minx = 0, miny = 0, maxx = 0, maxy = 0;

	for ( i = 0 ; i < map->num_vertexes ; i++ ) {
		const doom_vertex_t *v = &map->vertexes[i];

		if ( !i || v->x < minx ) minx = v->x;
		if ( !i || v->y < miny ) miny = v->y;
		if ( !i || v->x > maxx ) maxx = v->x;
		if ( !i || v->y > maxy ) maxy = v->y;
	}

	f = fopen( path, "w" );
	if ( !f ) {
		return -1;
	}
	fprintf( f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%d %d %d %d\" style=\"background:#000\">\n",
		minx - 64, -maxy - 64, maxx - minx + 128, maxy - miny + 128 );
	fprintf( f, "<g transform=\"scale(1,-1)\" stroke-width=\"3\">\n" );
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*l = &map->linedefs[i];
		const doom_vertex_t		*a = &map->vertexes[l->v1];
		const doom_vertex_t		*b = &map->vertexes[l->v2];
		const char				*color;

		color = l->special ? "#ff0" : ( l->side[1] == DOOM_NO_SIDE ? "#f33" : "#777" );
		fprintf( f, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\"/>\n", a->x, a->y, b->x, b->y, color );
	}
	for ( i = 0 ; i < map->num_things ; i++ ) {
		fprintf( f, "<circle cx=\"%d\" cy=\"%d\" r=\"8\" fill=\"#39f\"/>\n", map->things[i].x, map->things[i].y );
	}
	fprintf( f, "</g>\n</svg>\n" );
	fclose( f );
	return 0;
}

/*
=====================================================================

COMMANDS

=====================================================================
*/

// lump names can contain characters that are not valid in file names
static void FileName( char *out, size_t outlen, const char *dir, const char *name, const char *ext ) {
	char	clean[DOOM_LUMP_NAME_LEN + 1];
	int		i;

	for ( i = 0 ; name[i] ; i++ ) {
		clean[i] = strchr( "\\/:*?\"<>|", name[i] ) ? '_' : name[i];
	}
	clean[i] = 0;
	snprintf( out, outlen, "%s/%s.%s", dir, clean, ext );
}

static int IsSpecialLump( const char *name ) {
	static const char *names[] = {
		"PLAYPAL", "COLORMAP", "ENDOOM", "TEXTURE1", "TEXTURE2", "PNAMES", "GENMIDI", "DMXGUS",
		"THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS",
		"REJECT", "BLOCKMAP", NULL
	};
	int		i;

	for ( i = 0 ; names[i] ; i++ ) {
		if ( !strcmp( name, names[i] ) ) {
			return 1;
		}
	}
	// demos, music and sound effects
	return !strncmp( name, "DEMO", 4 ) || !strncmp( name, "D_", 2 )
		|| !strncmp( name, "DS", 2 ) || !strncmp( name, "DP", 2 );
}

static int InNamespace( const doom_wad_t *wad, doom_namespace_t ns, int lump ) {
	int		i, n = doom_wad_ns_count( wad, ns );

	for ( i = 0 ; i < n ; i++ ) {
		if ( doom_wad_ns_lump( wad, ns, i ) == lump ) {
			return 1;
		}
	}
	return 0;
}

static int CmdInfo( const doom_wad_t *wad ) {
	char	maps[MAX_MAPS][DOOM_LUMP_NAME_LEN + 1];
	int		i, n, sounds = 0;
	doom_sound_t snd;

	for ( i = 0 ; i < doom_wad_num_lumps( wad ) ; i++ ) {
		const char *name = doom_wad_lump( wad, i )->name;
		// count unique names: some IWADs carry duplicate sound lumps (the last wins)
		if ( !strncmp( name, "DS", 2 ) && doom_wad_find( wad, name ) == i && !doom_load_sound( wad, name, &snd ) ) {
			sounds++;
		}
	}

	printf( "type:      %s\n", doom_wad_is_iwad( wad ) ? "IWAD" : "PWAD" );
	printf( "lumps:     %d\n", doom_wad_num_lumps( wad ) );
	printf( "palettes:  %d\n", doom_num_palettes( wad ) );
	printf( "textures:  %d (from %s)\n", doom_num_textures( wad ),
		doom_wad_find( wad, "TEXTURE2" ) >= 0 ? "TEXTURE1+TEXTURE2" : "TEXTURE1" );
	printf( "flats:     %d\n", doom_wad_ns_count( wad, DOOM_NS_FLATS ) );
	printf( "sprites:   %d\n", doom_wad_ns_count( wad, DOOM_NS_SPRITES ) );
	printf( "sounds:    %d\n", sounds );

	n = doom_wad_list_maps( wad, maps, MAX_MAPS );
	printf( "maps:      %d\n", n );
	for ( i = 0 ; i < n && i < MAX_MAPS ; i++ ) {
		doom_map_t	map;
		char		err[256];

		if ( doom_map_load( wad, maps[i], &map, err, sizeof( err ) ) ) {
			printf( "  %-8s  ERROR: %s\n", maps[i], err );
			continue;
		}
		printf( "  %-8s  %4d vertexes %4d linedefs %4d sidedefs %3d sectors %3d things%s\n", map.name,
			map.num_vertexes, map.num_linedefs, map.num_sidedefs, map.num_sectors, map.num_things,
			doom_map_validate( &map, err, sizeof( err ) ) ? "  INVALID" : "" );
		doom_map_free( &map );
	}
	return 0;
}

static int CmdLumps( const doom_wad_t *wad ) {
	int		i;

	for ( i = 0 ; i < doom_wad_num_lumps( wad ) ; i++ ) {
		const doom_lump_t *l = doom_wad_lump( wad, i );
		printf( "%5d  %-8s  %8u\n", i, l->name, (unsigned)l->size );
	}
	return 0;
}

static int CmdExport( const doom_wad_t *wad, const char *dir ) {
	char			path[1024], sub[1024];
	char			maps[MAX_MAPS][DOOM_LUMP_NAME_LEN + 1];
	doom_image_t	img;
	doom_sound_t	snd;
	int				i, n, ok, failed = 0;

	MakeDir( dir );

	snprintf( sub, sizeof( sub ), "%s/textures", dir );
	MakeDir( sub );
	for ( i = ok = 0 ; i < doom_num_textures( wad ) ; i++ ) {
		const char *name = doom_texture( wad, i )->name;
		if ( doom_load_texture( wad, name, &img ) ) {
			fprintf( stderr, "texture %s: decode failed\n", name );
			failed++;
			continue;
		}
		FileName( path, sizeof( path ), sub, name, "png" );
		ok += !WritePng( path, &img );
		doom_image_free( &img );
	}
	printf( "textures: %d\n", ok );

	snprintf( sub, sizeof( sub ), "%s/flats", dir );
	MakeDir( sub );
	for ( i = ok = 0 ; i < doom_wad_ns_count( wad, DOOM_NS_FLATS ) ; i++ ) {
		const char *name = doom_wad_lump( wad, doom_wad_ns_lump( wad, DOOM_NS_FLATS, i ) )->name;
		if ( doom_load_flat( wad, name, &img ) ) {
			fprintf( stderr, "flat %s: decode failed\n", name );
			failed++;
			continue;
		}
		FileName( path, sizeof( path ), sub, name, "png" );
		ok += !WritePng( path, &img );
		doom_image_free( &img );
	}
	printf( "flats:    %d\n", ok );

	snprintf( sub, sizeof( sub ), "%s/sprites", dir );
	MakeDir( sub );
	for ( i = ok = 0 ; i < doom_wad_ns_count( wad, DOOM_NS_SPRITES ) ; i++ ) {
		const char *name = doom_wad_lump( wad, doom_wad_ns_lump( wad, DOOM_NS_SPRITES, i ) )->name;
		if ( doom_load_sprite( wad, name, &img ) ) {
			fprintf( stderr, "sprite %s: decode failed\n", name );
			failed++;
			continue;
		}
		FileName( path, sizeof( path ), sub, name, "png" );
		ok += !WritePng( path, &img );
		doom_image_free( &img );
	}
	printf( "sprites:  %d\n", ok );

	// everything else that decodes as a picture: patches, menus, status bar...
	snprintf( sub, sizeof( sub ), "%s/graphics", dir );
	MakeDir( sub );
	for ( i = ok = 0 ; i < doom_wad_num_lumps( wad ) ; i++ ) {
		const doom_lump_t *l = doom_wad_lump( wad, i );

		if ( l->size < 8 || IsSpecialLump( l->name ) || InNamespace( wad, DOOM_NS_FLATS, i )
			|| InNamespace( wad, DOOM_NS_SPRITES, i ) ) {
			continue;
		}
		if ( doom_decode_patch( doom_wad_lump_data( wad, i ), l->size, doom_palette( wad, 0 ), &img ) ) {
			continue;	// not a picture (map markers, etc.)
		}
		FileName( path, sizeof( path ), sub, l->name, "png" );
		ok += !WritePng( path, &img );
		doom_image_free( &img );
	}
	printf( "graphics: %d\n", ok );

	snprintf( sub, sizeof( sub ), "%s/sounds", dir );
	MakeDir( sub );
	for ( i = ok = 0 ; i < doom_wad_num_lumps( wad ) ; i++ ) {
		const char *name = doom_wad_lump( wad, i )->name;
		if ( strncmp( name, "DS", 2 ) ) {
			continue;
		}
		if ( doom_load_sound( wad, name, &snd ) ) {
			fprintf( stderr, "sound %s: decode failed\n", name );
			failed++;
			continue;
		}
		FileName( path, sizeof( path ), sub, name, "wav" );
		ok += !WriteWav( path, &snd );
	}
	printf( "sounds:   %d\n", ok );

	snprintf( sub, sizeof( sub ), "%s/maps", dir );
	MakeDir( sub );
	n = doom_wad_list_maps( wad, maps, MAX_MAPS );
	for ( i = ok = 0 ; i < n && i < MAX_MAPS ; i++ ) {
		doom_map_t	map;
		char		err[256];

		if ( doom_map_load( wad, maps[i], &map, err, sizeof( err ) ) ) {
			fprintf( stderr, "map %s: %s\n", maps[i], err );
			failed++;
			continue;
		}
		FileName( path, sizeof( path ), sub, map.name, "svg" );
		ok += !WriteMapSvg( path, &map );
		doom_map_free( &map );
	}
	printf( "maps:     %d\n", ok );

	if ( failed ) {
		printf( "%d resources failed to decode\n", failed );
	}
	return failed ? 1 : 0;
}

static int CmdMap( const doom_wad_t *wad, const char *name, const char *out ) {
	doom_map_t	map;
	char		err[256];

	if ( doom_map_load( wad, name, &map, err, sizeof( err ) ) ) {
		fprintf( stderr, "%s\n", err );
		return 1;
	}
	if ( doom_map_validate( &map, err, sizeof( err ) ) ) {
		fprintf( stderr, "warning: %s\n", err );
	}
	WriteMapSvg( out, &map );
	doom_map_free( &map );
	return 0;
}

/*
=================
CmdBuild

Builds Quake 3 BSPs ("all" for every map), prints statistics and
optionally writes the .bsp files into a directory.
=================
*/
static int CmdBuild( const doom_wad_t *wad, const char *name, const char *outdir ) {
	char	maps[MAX_MAPS][DOOM_LUMP_NAME_LEN + 1];
	int		i, n, failed = 0;

	if ( !strcmp( name, "all" ) ) {
		n = doom_wad_list_maps( wad, maps, MAX_MAPS );
	} else {
		snprintf( maps[0], sizeof( maps[0] ), "%s", name );
		n = 1;
	}
	if ( outdir ) {
		MakeDir( outdir );
	}

	for ( i = 0 ; i < n && i < MAX_MAPS ; i++ ) {
		doom_map_t			map;
		doom_build_stats_t	st;
		uint8_t				*data;
		size_t				size;
		char				err[256];

		if ( doom_map_load( wad, maps[i], &map, err, sizeof( err ) )
			|| doom_build_bsp( wad, &map, &data, &size, &st, err, sizeof( err ) ) ) {
			printf( "%-6s ERROR: %s\n", maps[i], err );
			failed++;
			continue;
		}
		printf( "%-6s %7zu bytes  %5d nodes  %4d sector + %4d void + %3d sliver leafs  %5d brushes  %4d surfaces  %3d movers  coverage %.4f\n",
			map.name, size, st.nodes, st.sector_leafs, st.void_leafs, st.degenerate_leafs, st.brushes,
			st.surfaces, st.movers, st.covered_area / st.bounds_area );
		if ( outdir ) {
			char	path[1024];
			FILE	*f;

			FileName( path, sizeof( path ), outdir, map.name, "bsp" );
			f = fopen( path, "wb" );
			if ( f ) {
				fwrite( data, 1, size, f );
				fclose( f );
			}
		}
		free( data );
		doom_map_free( &map );
	}
	return failed ? 1 : 0;
}

static void PrintSide( const doom_map_t *map, int side ) {
	const doom_sidedef_t	*sd = &map->sidedefs[side];
	const doom_sector_t		*sec = &map->sectors[sd->sector];

	printf( "    side %d: sector %d floor %d ceiling %d light %d | top %s mid %s bottom %s | offset %d %d\n",
		side, sd->sector, sec->floor_height, sec->ceiling_height, sec->light, sd->top, sd->mid, sd->bottom,
		sd->x_offset, sd->y_offset );
}

/*
=================
CmdLine

Prints a linedef, its sides and sectors, and every other line within
'radius' units of its midpoint (debugging collision and windows).
=================
*/
static int CmdLine( const doom_wad_t *wad, const char *name, int index ) {
	doom_map_t	map;
	char		err[256];
	int			i;
	double		mx, my;

	if ( doom_map_load( wad, name, &map, err, sizeof( err ) ) || index < 0 || index >= map.num_linedefs ) {
		fprintf( stderr, "%s\n", index < 0 ? "bad linedef" : err );
		return 1;
	}
	{
		const doom_linedef_t *ld = &map.linedefs[index];
		const doom_vertex_t *a = &map.vertexes[ld->v1], *c = &map.vertexes[ld->v2];

		printf( "linedef %d: (%d %d) -> (%d %d) flags 0x%x special %d tag %d jumpable window %d\n", index,
			a->x, a->y, c->x, c->y, ld->flags, ld->special, ld->tag, doom_linedef_is_jumpable_window( &map, ld ) );
		PrintSide( &map, ld->side[0] );
		if ( ld->side[1] != DOOM_NO_SIDE ) {
			PrintSide( &map, ld->side[1] );
		}
		mx = ( a->x + c->x ) * 0.5;
		my = ( a->y + c->y ) * 0.5;
	}
	printf( "  lines within 32 units of the midpoint:\n" );
	for ( i = 0 ; i < map.num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map.linedefs[i];
		const doom_vertex_t		*a = &map.vertexes[ld->v1], *c = &map.vertexes[ld->v2];
		double					dx = c->x - a->x, dy = c->y - a->y, t, px, py, d;

		if ( i == index ) {
			continue;
		}
		t = ( dx || dy ) ? ( ( mx - a->x ) * dx + ( my - a->y ) * dy ) / ( dx * dx + dy * dy ) : 0;
		t = t < 0 ? 0 : t > 1 ? 1 : t;
		px = a->x + t * dx - mx;
		py = a->y + t * dy - my;
		d = sqrt( px * px + py * py );
		if ( d <= 32 ) {
			printf( "    linedef %d at %.1f: (%d %d) -> (%d %d) flags 0x%x %s jumpable %d\n", i, d, a->x, a->y, c->x, c->y,
				ld->flags, ld->side[1] == DOOM_NO_SIDE ? "one-sided" : "two-sided", doom_linedef_is_jumpable_window( &map, ld ) );
		}
	}
	doom_map_free( &map );
	return 0;
}

static void PutU32( FILE *f, uint32_t v ) {
	fputc( v & 255, f ); fputc( ( v >> 8 ) & 255, f ); fputc( ( v >> 16 ) & 255, f ); fputc( v >> 24, f );
}

static void PutU16( FILE *f, int v ) {
	fputc( v & 255, f ); fputc( ( v >> 8 ) & 255, f );
}

/*
=================
CmdMusic

music <lump> <out.wav> [seconds]: a MUS lump through the OPL synthesizer,
as 44.1 kHz stereo; prints the peak and RMS level and the render speed
=================
*/
static int CmdMusic( const doom_wad_t *wad, const char *name, const char *out, int seconds ) {
	const int		rate = 44100;
	doom_music_t	*m = doom_music_open( wad, name, rate, 0 );
	int				total = rate * seconds, frames = 0, n, i, peak = 0;
	int16_t			buf[4096 * 2];
	double			sum = 0;
	clock_t			start;
	FILE			*f;

	if ( !m ) {
		fprintf( stderr, "can't open music %s\n", name );
		return 1;
	}
	f = fopen( out, "wb" );
	if ( !f ) {
		doom_music_close( m );
		return 1;
	}
	fwrite( "RIFF\0\0\0\0WAVEfmt ", 1, 16, f );
	PutU32( f, 16 ); PutU16( f, 1 ); PutU16( f, 2 ); PutU32( f, rate ); PutU32( f, rate * 4 ); PutU16( f, 4 ); PutU16( f, 16 );
	fwrite( "data\0\0\0\0", 1, 8, f );

	start = clock();
	while ( frames < total ) {
		n = doom_music_render( m, buf, total - frames < 4096 ? total - frames : 4096 );
		for ( i = 0 ; i < n * 2 ; i++ ) {
			int a = abs( buf[i] );

			if ( a > peak ) {
				peak = a;
			}
			sum += (double)buf[i] * buf[i];
		}
		fwrite( buf, 4, n, f );
		frames += n;
		if ( n < 4096 && frames < total ) {
			break;		// the score ended
		}
	}
	fseek( f, 4, SEEK_SET );
	PutU32( f, 36 + frames * 4 );
	fseek( f, 40, SEEK_SET );
	PutU32( f, frames * 4 );
	fclose( f );
	doom_music_close( m );

	printf( "%s: %.1f s, peak %d, rms %.0f, rendered at %.0fx real time\n", name, (double)frames / rate, peak,
		frames ? sqrt( sum / ( frames * 2.0 ) ) : 0.0, ( (double)frames / rate ) / ( (double)( clock() - start ) / CLOCKS_PER_SEC + 1e-9 ) );
	return 0;
}

static doom_wad_t *LoadWad( const char *path ) {
	FILE		*f = fopen( path, "rb" );
	void		*data;
	long		size;
	char		err[256];
	doom_wad_t	*wad;

	if ( !f ) {
		fprintf( stderr, "can't open %s\n", path );
		return NULL;
	}
	fseek( f, 0, SEEK_END );
	size = ftell( f );
	fseek( f, 0, SEEK_SET );
	data = malloc( size );
	if ( fread( data, 1, size, f ) != (size_t)size ) {
		fprintf( stderr, "can't read %s\n", path );
		fclose( f );
		free( data );
		return NULL;
	}
	fclose( f );

	wad = doom_wad_open( data, size, err, sizeof( err ) );
	if ( !wad ) {
		fprintf( stderr, "%s: %s\n", path, err );
	}
	return wad;
}

int main( int argc, char **argv ) {
	doom_wad_t	*wad;
	int			ret = 1;

	if ( argc < 3 ) {
		fprintf( stderr,
			"usage: doomtool <wad> info\n"
			"       doomtool <wad> lumps\n"
			"       doomtool <wad> export <dir>\n"
			"       doomtool <wad> map <name> <out.svg>\n"
			"       doomtool <wad> build <name|all> [outdir]\n"
			"       doomtool <wad> line <map> <linedef>\n"
			"       doomtool <wad> music <lump> <out.wav> [seconds]\n" );
		return 1;
	}

	InitCrc();
	wad = LoadWad( argv[1] );
	if ( !wad ) {
		return 1;
	}

	if ( !strcmp( argv[2], "info" ) ) {
		ret = CmdInfo( wad );
	} else if ( !strcmp( argv[2], "lumps" ) ) {
		ret = CmdLumps( wad );
	} else if ( !strcmp( argv[2], "export" ) && argc >= 4 ) {
		ret = CmdExport( wad, argv[3] );
	} else if ( !strcmp( argv[2], "map" ) && argc >= 5 ) {
		ret = CmdMap( wad, argv[3], argv[4] );
	} else if ( !strcmp( argv[2], "line" ) && argc >= 5 ) {
		ret = CmdLine( wad, argv[3], atoi( argv[4] ) );
	} else if ( !strcmp( argv[2], "build" ) && argc >= 4 ) {
		ret = CmdBuild( wad, argv[3], argc >= 5 ? argv[4] : NULL );
	} else if ( !strcmp( argv[2], "music" ) && argc >= 5 ) {
		ret = CmdMusic( wad, argv[3], argv[4], argc >= 6 ? atoi( argv[5] ) : 30 );
	} else {
		fprintf( stderr, "unknown command\n" );
	}

	doom_wad_close( wad );
	return ret;
}
