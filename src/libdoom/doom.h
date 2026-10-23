/*
===========================================================================
libdoom - Doom WAD reading, asset decoding and the editable map model

Part of Q3Doom. Plain C99, no engine dependencies, so the engine, the game
modules and the tools all link the same code. Formats follow id Software's
Doom source release (linuxdoom-1.10).

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#ifndef LIBDOOM_H
#define LIBDOOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
=====================================================================

WAD

=====================================================================
*/

#define DOOM_LUMP_NAME_LEN	8

typedef struct {
	char		name[DOOM_LUMP_NAME_LEN + 1];	// uppercase, NUL terminated
	uint32_t	offset;
	uint32_t	size;
} doom_lump_t;

typedef struct doom_wad_s doom_wad_t;

// Opens a WAD held in memory. libdoom takes ownership of 'data' (it must
// come from malloc) and frees it in doom_wad_close. Returns NULL on error
// and writes a message to 'err'.
doom_wad_t			*doom_wad_open( void *data, size_t size, char *err, size_t errlen );
void				doom_wad_close( doom_wad_t *wad );

int					doom_wad_is_iwad( const doom_wad_t *wad );
int					doom_wad_num_lumps( const doom_wad_t *wad );
const doom_lump_t	*doom_wad_lump( const doom_wad_t *wad, int index );
const uint8_t		*doom_wad_lump_data( const doom_wad_t *wad, int index );

// Case-insensitive lookup; the last lump with that name wins, like Doom's
// W_CheckNumForName. Returns -1 when missing.
int					doom_wad_find( const doom_wad_t *wad, const char *name );

// Lump namespaces delimited by marker lumps (F_START/F_END, S_START/S_END).
typedef enum {
	DOOM_NS_GLOBAL,
	DOOM_NS_FLATS,
	DOOM_NS_SPRITES
} doom_namespace_t;

int					doom_wad_find_ns( const doom_wad_t *wad, doom_namespace_t ns, const char *name );
// Iterates a namespace: returns the lump index of the n-th entry, or -1.
int					doom_wad_ns_lump( const doom_wad_t *wad, doom_namespace_t ns, int n );
int					doom_wad_ns_count( const doom_wad_t *wad, doom_namespace_t ns );

/*
=====================================================================

GRAPHICS

All images are decoded to 8-bit RGBA through PLAYPAL palette 0.
Transparent pixels (holes in patches) have alpha 0.

=====================================================================
*/

typedef struct {
	int			width;
	int			height;
	int			left_offset;	// patch/sprite origin, as in the patch header
	int			top_offset;
	uint8_t		*rgba;			// width * height * 4, malloc'ed
} doom_image_t;

void				doom_image_free( doom_image_t *image );

// PLAYPAL has 14 palettes (0 = normal, 1-8 damage red, 9-12 pickup, 13 radsuit)
int					doom_num_palettes( const doom_wad_t *wad );
const uint8_t		*doom_palette( const doom_wad_t *wad, int index );	// 768 bytes or NULL
// COLORMAP: 34 maps of 256 indices (0 = full bright ... 31 = dark, 32 = invuln)
const uint8_t		*doom_colormap( const doom_wad_t *wad, int index );

// Decodes a lump in Doom "patch" (picture) format. Returns 0 on success.
int					doom_decode_patch( const uint8_t *data, size_t size, const uint8_t *palette, doom_image_t *out );

// Loaders return 0 on success, -1 if the resource is missing or invalid.
int					doom_load_patch( const doom_wad_t *wad, const char *name, doom_image_t *out );	// any picture lump
int					doom_load_sprite( const doom_wad_t *wad, const char *name, doom_image_t *out );	// S_START..S_END
int					doom_load_flat( const doom_wad_t *wad, const char *name, doom_image_t *out );	// F_START..F_END
int					doom_load_texture( const doom_wad_t *wad, const char *name, doom_image_t *out );	// TEXTURE1/2

// Composite wall textures from TEXTURE1/TEXTURE2 + PNAMES
typedef struct {
	int			origin_x;
	int			origin_y;
	int			patch;			// index into the PNAMES table
} doom_texture_patch_t;

typedef struct {
	char					name[DOOM_LUMP_NAME_LEN + 1];
	int						width;
	int						height;
	int						num_patches;
	doom_texture_patch_t	*patches;
} doom_texture_t;

int					doom_num_textures( const doom_wad_t *wad );
const doom_texture_t *doom_texture( const doom_wad_t *wad, int index );
int					doom_find_texture( const doom_wad_t *wad, const char *name );	// -1 if missing
const char			*doom_pname( const doom_wad_t *wad, int index );					// PNAMES entry

/*
=====================================================================

SOUND

DMX format sound effects (DS* lumps): unsigned 8-bit mono PCM.

=====================================================================
*/

typedef struct {
	int				rate;			// samples per second, normally 11025
	int				num_samples;
	const uint8_t	*samples;		// points into the WAD data, do not free
} doom_sound_t;

int					doom_load_sound( const doom_wad_t *wad, const char *name, doom_sound_t *out );

/*
=====================================================================

MAP MODEL

The editable, in-memory form of a Doom level. Only the "source" lumps are
read (THINGS, LINEDEFS, SIDEDEFS, VERTEXES, SECTORS); the precompiled
NODES/SEGS/SSECTORS/BLOCKMAP/REJECT lumps are ignored on purpose so the
world can be rebuilt after any edit.

=====================================================================
*/

#define DOOM_NO_SIDE		-1

// linedef flags (ML_* in doomdata.h)
#define DOOM_ML_BLOCKING		0x0001
#define DOOM_ML_BLOCKMONSTERS	0x0002
#define DOOM_ML_TWOSIDED		0x0004
#define DOOM_ML_DONTPEGTOP		0x0008
#define DOOM_ML_DONTPEGBOTTOM	0x0010
#define DOOM_ML_SECRET			0x0020
#define DOOM_ML_SOUNDBLOCK		0x0040
#define DOOM_ML_DONTDRAW		0x0080
#define DOOM_ML_MAPPED			0x0100

// thing flags (MTF_* in doomdata.h)
#define DOOM_MTF_EASY			0x0001
#define DOOM_MTF_NORMAL			0x0002
#define DOOM_MTF_HARD			0x0004
#define DOOM_MTF_AMBUSH			0x0008
#define DOOM_MTF_NOTSINGLE		0x0010

typedef struct {
	int32_t		x, y;
} doom_vertex_t;

typedef struct {
	int32_t		v1, v2;
	int32_t		flags;
	int32_t		special;
	int32_t		tag;
	int32_t		side[2];			// front, back (DOOM_NO_SIDE if none)
} doom_linedef_t;

typedef struct {
	int32_t		x_offset, y_offset;
	char		top[DOOM_LUMP_NAME_LEN + 1];		// "-" means no texture
	char		bottom[DOOM_LUMP_NAME_LEN + 1];
	char		mid[DOOM_LUMP_NAME_LEN + 1];
	int32_t		sector;
} doom_sidedef_t;

typedef struct {
	int32_t		floor_height;
	int32_t		ceiling_height;
	char		floor_flat[DOOM_LUMP_NAME_LEN + 1];
	char		ceiling_flat[DOOM_LUMP_NAME_LEN + 1];
	int32_t		light;
	int32_t		special;
	int32_t		tag;
} doom_sector_t;

typedef struct {
	int32_t		x, y;
	int32_t		angle;				// degrees, 0 = east, 90 = north
	int32_t		type;				// doomednum
	int32_t		flags;
} doom_thing_t;

typedef struct doom_map_s {
	char			name[DOOM_LUMP_NAME_LEN + 1];

	int				num_vertexes;
	doom_vertex_t	*vertexes;
	int				num_linedefs;
	doom_linedef_t	*linedefs;
	int				num_sidedefs;
	doom_sidedef_t	*sidedefs;
	int				num_sectors;
	doom_sector_t	*sectors;
	int				num_things;
	doom_thing_t	*things;
} doom_map_t;

// Loads a map by marker name ("E1M1", "MAP01"). Returns 0 on success.
int					doom_map_load( const doom_wad_t *wad, const char *name, doom_map_t *out, char *err, size_t errlen );
void				doom_map_free( doom_map_t *map );
// Checks that every index refers to something that exists. Returns 0 if valid.
int					doom_map_validate( const doom_map_t *map, char *err, size_t errlen );
// Map marker lumps present in the WAD, in WAD order. Returns the count.
int					doom_wad_list_maps( const doom_wad_t *wad, char names[][DOOM_LUMP_NAME_LEN + 1], int max_names );

// Sector containing a point, or -1 for the void outside the map.
int					doom_map_sector_at( const doom_map_t *map, double x, double y );

/*
=====================================================================

WORLD BUILDER

Builds a Quake 3 BSP (IBSP version 46) from a map model: collision brushes,
BSP tree, vertex-lit render surfaces, light grid and entities. Shaders are
named doom/wall/<TEX>, doom/flat/<FLAT> and doom/sky/SKY<n>.
Deterministic: the same input always gives the same bytes.

=====================================================================
*/

typedef struct {
	int			nodes;
	int			sector_leafs;
	int			void_leafs;
	int			degenerate_leafs;	// numerical slivers, no brushes
	int			brushes;
	int			surfaces;
	int			planes;
	int			verts;
	double		covered_area;		// sum of leaf areas, should equal bounds_area
	double		bounds_area;
} doom_build_stats_t;

// Returns 0 on success; *data is malloc'ed (free it). 'stats' may be NULL.
int					doom_build_bsp( const doom_wad_t *wad, const doom_map_t *map, uint8_t **data, size_t *size,
						doom_build_stats_t *stats, char *err, size_t errlen );

#ifdef __cplusplus
}
#endif

#endif
