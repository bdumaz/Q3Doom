/*
===========================================================================
libdoom - palettes, pictures (patches), flats, sprites, composite textures

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"

#define PALETTE_SIZE		768
#define COLORMAP_SIZE		256
#define FLAT_SIZE			64
#define MAPTEXTURE_SIZE		22		// name, masked, width, height, columndirectory, patchcount
#define MAPPATCH_SIZE		10		// originx, originy, patch, stepdir, colormap

void doom_image_free( doom_image_t *image ) {
	if ( image ) {
		free( image->rgba );
		memset( image, 0, sizeof( *image ) );
	}
}

static int AllocImage( doom_image_t *out, int width, int height ) {
	memset( out, 0, sizeof( *out ) );
	if ( width <= 0 || height <= 0 || width > 4096 || height > 4096 ) {
		return -1;
	}
	out->width = width;
	out->height = height;
	out->rgba = calloc( (size_t)width * height, 4 );	// fully transparent
	return out->rgba ? 0 : -1;
}

static void PutPixel( doom_image_t *img, int x, int y, uint8_t index, const uint8_t *palette ) {
	uint8_t		*p;

	if ( x < 0 || y < 0 || x >= img->width || y >= img->height ) {
		return;
	}
	p = img->rgba + ( (size_t)y * img->width + x ) * 4;
	p[0] = palette[index * 3 + 0];
	p[1] = palette[index * 3 + 1];
	p[2] = palette[index * 3 + 2];
	p[3] = 255;
}

/*
=====================================================================

PALETTES

=====================================================================
*/

int doom_num_palettes( const doom_wad_t *wad ) {
	int		lump = doom_wad_find( wad, "PLAYPAL" );

	return lump < 0 ? 0 : (int)( wad->lumps[lump].size / PALETTE_SIZE );
}

const uint8_t *doom_palette( const doom_wad_t *wad, int index ) {
	int		lump = doom_wad_find( wad, "PLAYPAL" );

	if ( lump < 0 || index < 0 || (uint32_t)( index + 1 ) * PALETTE_SIZE > wad->lumps[lump].size ) {
		return NULL;
	}
	return doom_wad_lump_data( wad, lump ) + index * PALETTE_SIZE;
}

const uint8_t *doom_colormap( const doom_wad_t *wad, int index ) {
	int		lump = doom_wad_find( wad, "COLORMAP" );

	if ( lump < 0 || index < 0 || (uint32_t)( index + 1 ) * COLORMAP_SIZE > wad->lumps[lump].size ) {
		return NULL;
	}
	return doom_wad_lump_data( wad, lump ) + index * COLORMAP_SIZE;
}

/*
=====================================================================

PICTURES

A picture is a header (width, height, left/top offset), a column offset
table, then per column a list of posts: topdelta, length, pad, pixels,
pad, terminated by topdelta 0xFF.

=====================================================================
*/

/*
=================
DrawPatchColumns

Draws a validated patch into 'img' at (ox, oy). Supports the "tall patch"
convention (a topdelta not greater than the previous one is relative),
which vanilla data never triggers.
=================
*/
static int DrawPatch( const uint8_t *data, size_t size, const uint8_t *palette, doom_image_t *img, int ox, int oy ) {
	int		width, x;

	if ( size < 8 ) {
		return -1;
	}
	width = doom_s16( data );
	if ( width <= 0 || 8 + (size_t)width * 4 > size ) {
		return -1;
	}

	for ( x = 0 ; x < width ; x++ ) {
		uint32_t	ofs = doom_u32( data + 8 + x * 4 );
		int			top = -1;
		int			posts = 0;

		while ( 1 ) {
			int		delta, length, i;

			if ( ofs >= size ) {
				return -1;
			}
			delta = data[ofs];
			if ( delta == 0xFF ) {
				break;
			}
			if ( ofs + 3 > size ) {
				return -1;
			}
			length = data[ofs + 1];
			if ( ofs + 3 + (size_t)length + 1 > size ) {
				return -1;
			}

			top = ( delta <= top ) ? top + delta : delta;
			for ( i = 0 ; i < length ; i++ ) {
				PutPixel( img, ox + x, oy + top + i, data[ofs + 3 + i], palette );
			}

			ofs += 4 + length;
			if ( ++posts > 1024 ) {
				return -1;	// runaway column
			}
		}
	}
	return 0;
}

int doom_decode_patch( const uint8_t *data, size_t size, const uint8_t *palette, doom_image_t *out ) {
	int		width, height;

	memset( out, 0, sizeof( *out ) );
	if ( !palette || size < 8 ) {
		return -1;
	}
	width = doom_s16( data );
	height = doom_s16( data + 2 );
	if ( AllocImage( out, width, height ) ) {
		return -1;
	}
	out->left_offset = doom_s16( data + 4 );
	out->top_offset = doom_s16( data + 6 );

	if ( DrawPatch( data, size, palette, out, 0, 0 ) ) {
		doom_image_free( out );
		return -1;
	}
	return 0;
}

static int DecodeLump( const doom_wad_t *wad, int lump, doom_image_t *out ) {
	memset( out, 0, sizeof( *out ) );
	if ( lump < 0 ) {
		return -1;
	}
	return doom_decode_patch( doom_wad_lump_data( wad, lump ), wad->lumps[lump].size, doom_palette( wad, 0 ), out );
}

int doom_load_patch( const doom_wad_t *wad, const char *name, doom_image_t *out ) {
	return DecodeLump( wad, doom_wad_find( wad, name ), out );
}

int doom_load_sprite( const doom_wad_t *wad, const char *name, doom_image_t *out ) {
	return DecodeLump( wad, doom_wad_find_ns( wad, DOOM_NS_SPRITES, name ), out );
}

/*
=====================================================================

FLATS

Raw 64x64 palette indices, no transparency.

=====================================================================
*/

int doom_load_flat( const doom_wad_t *wad, const char *name, doom_image_t *out ) {
	int				lump = doom_wad_find_ns( wad, DOOM_NS_FLATS, name );
	const uint8_t	*palette = doom_palette( wad, 0 );
	const uint8_t	*src;
	int				i;

	memset( out, 0, sizeof( *out ) );
	if ( lump < 0 || !palette || wad->lumps[lump].size < FLAT_SIZE * FLAT_SIZE ) {
		return -1;
	}
	if ( AllocImage( out, FLAT_SIZE, FLAT_SIZE ) ) {
		return -1;
	}
	src = doom_wad_lump_data( wad, lump );
	for ( i = 0 ; i < FLAT_SIZE * FLAT_SIZE ; i++ ) {
		PutPixel( out, i % FLAT_SIZE, i / FLAT_SIZE, src[i], palette );
	}
	return 0;
}

/*
=====================================================================

COMPOSITE TEXTURES

=====================================================================
*/

static void ParsePnames( doom_wad_t *wad ) {
	int				lump = doom_wad_find( wad, "PNAMES" );
	const uint8_t	*data;
	uint32_t		count, i;

	if ( lump < 0 || wad->lumps[lump].size < 4 ) {
		return;
	}
	data = doom_wad_lump_data( wad, lump );
	count = doom_u32( data );
	if ( count > ( wad->lumps[lump].size - 4 ) / 8 ) {
		count = ( wad->lumps[lump].size - 4 ) / 8;
	}
	wad->pnames = calloc( count ? count : 1, sizeof( *wad->pnames ) );
	wad->num_pnames = (int)count;
	for ( i = 0 ; i < count ; i++ ) {
		doom_copy_name( wad->pnames[i], data + 4 + i * 8 );
	}
}

static void ParseTextureLump( doom_wad_t *wad, const char *lumpname ) {
	int				lump = doom_wad_find( wad, lumpname );
	const uint8_t	*data;
	uint32_t		size, count, i;
	doom_texture_t	*grown;

	if ( lump < 0 || wad->lumps[lump].size < 4 ) {
		return;
	}
	data = doom_wad_lump_data( wad, lump );
	size = wad->lumps[lump].size;
	count = doom_u32( data );
	if ( count > ( size - 4 ) / 4 ) {
		return;
	}

	grown = realloc( wad->textures, sizeof( doom_texture_t ) * ( wad->num_textures + count ) );
	if ( !grown ) {
		return;
	}
	wad->textures = grown;

	for ( i = 0 ; i < count ; i++ ) {
		uint32_t		ofs = doom_u32( data + 4 + i * 4 );
		doom_texture_t	*tex;
		int				p, numpatches;

		if ( ofs > size || size - ofs < MAPTEXTURE_SIZE ) {
			continue;
		}
		numpatches = doom_s16( data + ofs + 20 );
		if ( numpatches < 0 || ofs + MAPTEXTURE_SIZE + (size_t)numpatches * MAPPATCH_SIZE > size ) {
			continue;
		}

		tex = &wad->textures[wad->num_textures++];
		doom_copy_name( tex->name, data + ofs );
		tex->width = doom_s16( data + ofs + 12 );
		tex->height = doom_s16( data + ofs + 14 );
		tex->num_patches = numpatches;
		tex->patches = calloc( numpatches ? numpatches : 1, sizeof( doom_texture_patch_t ) );
		for ( p = 0 ; p < numpatches ; p++ ) {
			const uint8_t *mp = data + ofs + MAPTEXTURE_SIZE + p * MAPPATCH_SIZE;

			tex->patches[p].origin_x = doom_s16( mp );
			tex->patches[p].origin_y = doom_s16( mp + 2 );
			tex->patches[p].patch = doom_s16( mp + 4 );
		}
	}
}

void doom_textures_init( doom_wad_t *wad ) {
	ParsePnames( wad );
	ParseTextureLump( wad, "TEXTURE1" );
	ParseTextureLump( wad, "TEXTURE2" );
}

void doom_textures_free( doom_wad_t *wad ) {
	int		i;

	for ( i = 0 ; i < wad->num_textures ; i++ ) {
		free( wad->textures[i].patches );
	}
	free( wad->textures );
	free( wad->pnames );
	wad->textures = NULL;
	wad->pnames = NULL;
	wad->num_textures = wad->num_pnames = 0;
}

int doom_num_textures( const doom_wad_t *wad ) {
	return wad->num_textures;
}

const doom_texture_t *doom_texture( const doom_wad_t *wad, int index ) {
	if ( index < 0 || index >= wad->num_textures ) {
		return NULL;
	}
	return &wad->textures[index];
}

int doom_find_texture( const doom_wad_t *wad, const char *name ) {
	int		i;

	// first definition wins, as in R_TextureNumForName
	for ( i = 0 ; i < wad->num_textures ; i++ ) {
		if ( doom_name_equal( wad->textures[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}

const char *doom_pname( const doom_wad_t *wad, int index ) {
	if ( index < 0 || index >= wad->num_pnames ) {
		return NULL;
	}
	return wad->pnames[index];
}

/*
=================
doom_load_texture

Draws every patch at its origin over a transparent background. Vanilla
Doom renders multi-patch columns opaque (holes show garbage); keeping the
transparency is what modern ports do and what masked mid-textures need.
=================
*/
int doom_load_texture( const doom_wad_t *wad, const char *name, doom_image_t *out ) {
	const doom_texture_t	*tex = doom_texture( wad, doom_find_texture( wad, name ) );
	const uint8_t			*palette = doom_palette( wad, 0 );
	int						p;

	memset( out, 0, sizeof( *out ) );
	if ( !tex || !palette || AllocImage( out, tex->width, tex->height ) ) {
		return -1;
	}

	for ( p = 0 ; p < tex->num_patches ; p++ ) {
		const char	*pname = doom_pname( wad, tex->patches[p].patch );
		int			lump = pname ? doom_wad_find( wad, pname ) : -1;

		if ( lump < 0 ) {
			continue;	// missing patch: leave the area transparent
		}
		DrawPatch( doom_wad_lump_data( wad, lump ), wad->lumps[lump].size, palette, out,
			tex->patches[p].origin_x, tex->patches[p].origin_y );
	}
	return 0;
}
