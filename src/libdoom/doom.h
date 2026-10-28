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
// a sprite in a marine colour: 0 green (as is), 1 gray, 2 brown, 3 red, 4 blue
#define DOOM_NUM_TRANSLATIONS	5
int					doom_load_sprite_translated( const doom_wad_t *wad, const char *name, int translation, doom_image_t *out );
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

MUSIC

MUS lumps (D_E1M1 ...) played with the GENMIDI instruments on an OPL2-style
FM synthesizer (music.c). The player reads the WAD's data, which must stay
loaded while it plays.

=====================================================================
*/

typedef struct doom_music_s doom_music_t;

doom_music_t		*doom_music_open( const doom_wad_t *wad, const char *name, int rate, int loop );	// NULL if missing
int					doom_music_render( doom_music_t *m, int16_t *out, int frames );	// stereo; fewer frames at the end
void				doom_music_close( doom_music_t *m );

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

#define DOOM_MAX_STEP		24		// highest step a Doom player walks up

// An impassable-flagged window players may jump through (see map.c).
int					doom_linedef_is_jumpable_window( const doom_map_t *map, const doom_linedef_t *ld );

/*
=====================================================================

LINE SPECIALS AND SECTOR MOVEMENT (specials.c)

=====================================================================
*/

typedef enum {
	DOOM_TRIG_NONE,
	DOOM_TRIG_WALK,			// crossing the line (W1/WR)
	DOOM_TRIG_SWITCH,		// using the line (S1/SR), acts on tagged sectors
	DOOM_TRIG_MANUAL,		// using a door line, acts on its back sector (D1/DR)
	DOOM_TRIG_GUN			// shooting the line (G1/GR)
} doom_trigger_t;

typedef enum {
	DOOM_KIND_NONE,
	DOOM_KIND_DOOR,
	DOOM_KIND_PLAT,
	DOOM_KIND_FLOOR,
	DOOM_KIND_CEILING,
	DOOM_KIND_STAIRS,
	DOOM_KIND_LIGHT,
	DOOM_KIND_EXIT,			// type 1: secret exit
	DOOM_KIND_TELEPORT,
	DOOM_KIND_DONUT,
	DOOM_KIND_STOP,			// type: the kind it stops (perpetual plats, crushers)
	DOOM_KIND_SCROLL		// texture scrolls left, no trigger
} doom_kind_t;

// vldoor_e
enum { DOOM_DOOR_NORMAL, DOOM_DOOR_CLOSE30THENOPEN, DOOM_DOOR_CLOSE, DOOM_DOOR_OPEN, DOOM_DOOR_RAISEIN5MINS,
	DOOM_DOOR_BLAZERAISE, DOOM_DOOR_BLAZEOPEN, DOOM_DOOR_BLAZECLOSE };
// plattype_e
enum { DOOM_PLAT_PERPETUALRAISE, DOOM_PLAT_DOWNWAITUPSTAY, DOOM_PLAT_RAISEANDCHANGE,
	DOOM_PLAT_RAISETONEARESTANDCHANGE, DOOM_PLAT_BLAZEDWUS };
// floor_e
enum { DOOM_FLOOR_LOWER, DOOM_FLOOR_LOWERTOLOWEST, DOOM_FLOOR_TURBOLOWER, DOOM_FLOOR_RAISE, DOOM_FLOOR_RAISETONEAREST,
	DOOM_FLOOR_RAISETOTEXTURE, DOOM_FLOOR_LOWERANDCHANGE, DOOM_FLOOR_RAISE24, DOOM_FLOOR_RAISE24ANDCHANGE,
	DOOM_FLOOR_RAISECRUSH, DOOM_FLOOR_RAISETURBO, DOOM_FLOOR_RAISE512 };
// ceiling_e
enum { DOOM_CEIL_LOWERTOFLOOR, DOOM_CEIL_RAISETOHIGHEST, DOOM_CEIL_LOWERANDCRUSH, DOOM_CEIL_CRUSHANDRAISE,
	DOOM_CEIL_FASTCRUSHANDRAISE, DOOM_CEIL_SILENTCRUSHANDRAISE };
// stair_e
enum { DOOM_STAIRS_BUILD8, DOOM_STAIRS_TURBO16 };
// lights
enum { DOOM_LIGHT_TURNON, DOOM_LIGHT_TURNOFF, DOOM_LIGHT_STROBE };
// keys
enum { DOOM_KEY_NONE, DOOM_KEY_BLUE, DOOM_KEY_YELLOW, DOOM_KEY_RED };

typedef struct {
	int				special;
	doom_trigger_t	trigger;
	int				repeatable;
	int				monsters;	// 1 monsters can trigger it too, 2 monsters only
	doom_kind_t		kind;
	int				type;		// DOOM_DOOR_*, DOOM_PLAT_*, ... for the kind
	int				amount;		// key for doors, raise amount for plats, light level for lights
} doom_special_t;

// NULL if the special does nothing
const doom_special_t *doom_line_special( int special );

// Doom's P_Find*Surrounding; height arrays are the current heights of all
// sectors (NULL: the heights stored in the map)
int		doom_find_lowest_floor_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings );
int		doom_find_highest_floor_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings );
int		doom_find_next_highest_floor( const doom_map_t *map, int sector, int current, const int *floors, const int *ceilings );
int		doom_find_lowest_ceiling_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings );
int		doom_find_highest_ceiling_surrounding( const doom_map_t *map, int sector, const int *floors, const int *ceilings );
int		doom_find_min_surrounding_light( const doom_map_t *map, int sector, int max, const int *lights );
int		doom_shortest_lower_texture( const doom_wad_t *wad, const doom_map_t *map, int sector );
// sectors raised by a stair builder starting at 'sector'; 'visited' (one byte per sector) must start cleared
int		doom_stair_chain( const doom_map_t *map, int sector, int *out, int max, uint8_t *visited );

// The heights each sector's floor and ceiling can reach through the map's
// specials (Doom's target rules on the initial heights). min == max: static.
typedef struct {
	int		floor_min, floor_max;
	int		ceiling_min, ceiling_max;
} doom_plane_range_t;

void	doom_sector_ranges( const doom_wad_t *wad, const doom_map_t *map, doom_plane_range_t *ranges );

// DoomFrag copies: doom_map_load( wad, "DF_E1M1", ... ) loads E1M1 (keeping
// that name) with every ceiling a player can stand under raised to at least
// DOOM_FRAG_CLEARANCE above the highest floor that leaves standing room: a
// VQ3 standing jump (46) of a Doom player (56), plus Doom's 4 under the
// ceiling for the doors around. Returns the number of sectors raised.
#define DOOM_FRAG_PREFIX		"DF_"
#define DOOM_FRAG_CLEARANCE		106
#define DOOM_PLAYER_HEIGHT		56

int		doom_map_raise_ceilings( const doom_wad_t *wad, doom_map_t *map, int clearance );

// Animated textures and flats: the cycle as seen from 'name' (0 if not animated).
// Doom advances them every 8 tics.
#define DOOM_ANIM_TICS		8
int		doom_anim_frames( const doom_wad_t *wad, int is_texture, const char *name, char frames[][DOOM_LUMP_NAME_LEN + 1], int max_frames );
// SW1xxx <-> SW2xxx; returns 1 and the other texture if 'name' is a switch
int		doom_switch_texture( const doom_wad_t *wad, const char *name, char out[DOOM_LUMP_NAME_LEN + 1] );

/*
=====================================================================

CAMPAIGN PROGRESS (progress.c)

The levels unlocked, the best medal and the best time of each, kept in a
small text file ("level E1M1 unlocked 1 medal 2 best 4520" per line).

=====================================================================
*/

#define DOOM_PROGRESS_MAX	64

// medals by skill: none, bronze (skills 0-1), silver (2), gold (3), skull (4)
enum { DOOM_MEDAL_NONE, DOOM_MEDAL_BRONZE, DOOM_MEDAL_SILVER, DOOM_MEDAL_GOLD, DOOM_MEDAL_SKULL };

typedef struct {
	char		map[DOOM_LUMP_NAME_LEN + 1];
	int			unlocked;
	int			medal;
	int			best_tics;			// best time in tics, 0 none
} doom_progress_level_t;

typedef struct {
	int						count;
	doom_progress_level_t	levels[DOOM_PROGRESS_MAX];
} doom_progress_t;

// 'text' may be NULL or empty: a new game (E1M1 unlocked)
void					doom_progress_parse( const char *text, doom_progress_t *p );
// returns the length written (always NUL terminated)
int						doom_progress_write( const doom_progress_t *p, char *buf, size_t size );
// NULL if the map has no entry and 'create' is 0
doom_progress_level_t	*doom_progress_level( doom_progress_t *p, const char *map, int create );
int						doom_progress_unlocked( const doom_progress_t *p, const char *map );
int						doom_skill_medal( int skill );

// Doom's par time of ExMy in seconds (g_game.c pars[]), 0 if it has none
int						doom_par_time( int episode, int map );

// DoomFrag records: the best time of a map with a set of options
// ("record E1M1 p0w0l0m2f0 31250" per line, in msec)
#define DOOM_RECORDS_MAX	1024
#define DOOM_OPTIONS_LEN	15

typedef struct {
	char		map[DOOM_LUMP_NAME_LEN + 1];
	char		options[DOOM_OPTIONS_LEN + 1];
	int			msec;
} doom_record_t;

typedef struct {
	int				count;
	doom_record_t	records[DOOM_RECORDS_MAX];
} doom_records_t;

void					doom_records_parse( const char *text, doom_records_t *r );
int						doom_records_write( const doom_records_t *r, char *buf, size_t size );
int						doom_record_get( const doom_records_t *r, const char *map, const char *options );	// 0 none
int						doom_record_set( doom_records_t *r, const char *map, const char *options, int msec );	// 1: new best
// the options key: physics 0-1, weapons 0-2, loadout 0-1, monsters (skill 0-4, 5 none), finish 0-1
void					doom_frag_options( char *out, size_t size, int physics, int weapons, int loadout, int monsters, int finish );

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
	int			movers;				// moving floors and ceilings (inline models 1..movers)
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
