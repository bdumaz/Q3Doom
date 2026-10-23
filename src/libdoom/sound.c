/*
===========================================================================
libdoom - DMX sound effects

Lump layout: u16 format (3), u16 sample rate, u32 sample count, then
unsigned 8-bit samples. The count includes 16 padding bytes at each end
that DMX used for interpolation; they are not part of the sound.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"

#define DMX_HEADER_SIZE		8
#define DMX_PADDING			16

int doom_load_sound( const doom_wad_t *wad, const char *name, doom_sound_t *out ) {
	int				lump = doom_wad_find( wad, name );
	const uint8_t	*data;
	uint32_t		size, count;

	memset( out, 0, sizeof( *out ) );
	if ( lump < 0 ) {
		return -1;
	}
	data = doom_wad_lump_data( wad, lump );
	size = wad->lumps[lump].size;
	if ( size < DMX_HEADER_SIZE || doom_u16( data ) != 3 ) {
		return -1;
	}

	out->rate = doom_u16( data + 2 );
	count = doom_u32( data + 4 );
	if ( count > size - DMX_HEADER_SIZE ) {
		count = size - DMX_HEADER_SIZE;
	}
	out->samples = data + DMX_HEADER_SIZE;

	if ( count > DMX_PADDING * 2 ) {
		out->samples += DMX_PADDING;
		count -= DMX_PADDING * 2;
	}
	out->num_samples = (int)count;

	return ( out->rate > 0 && out->num_samples > 0 ) ? 0 : -1;
}
