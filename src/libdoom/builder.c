/*
===========================================================================
libdoom - world builder: Doom map model -> Quake 3 BSP (IBSP version 46)

1. A 2D BSP over the linedefs splits the map bounds into convex cells
   that contain no wall. Each cell is classified by a ray cast as void or
   as the sector it belongs to.
2. Every cell becomes brushes: a solid column for void, or a floor slab
   and a ceiling slab for a sector. The 2D BSP nodes become the vertical
   planes of the Quake 3 BSP tree, so the collision tree is exact.
3. Render surfaces are built per sector: floors and ceilings from the
   cells, walls from the sidedefs with Doom's texture alignment rules.
   Lighting is per vertex (sector light level) plus a light grid.

Only the map model is used, never Doom's NODES/SEGS lumps, so the output
can be rebuilt after any edit. The output is deterministic.

Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "doom_local.h"
#include "builder.h"

#include <math.h>
#include <stdarg.h>

#define EPSILON			0.0001		// on-plane tolerance for 2D geometry
#define MIN_LEAF_AREA	0.01		// smaller cells are numerical slivers
#define WORLD_MARGIN	64			// void border around the map
#define SLAB_DEPTH		64			// below the lowest floor / above the highest ceiling
#define CLIP_THICKNESS	1.0			// half thickness of impassable-line clip brushes

#define MAX_SURF_VERTS	900			// renderer limit is SHADER_MAX_VERTEXES (1000)
#define MAX_SURF_INDEXES 5400		// renderer limit is SHADER_MAX_INDEXES (6000)

#define LIGHTGRID_X		64			// renderer defaults (world_t lightGridSize)
#define LIGHTGRID_Y		64
#define LIGHTGRID_Z		128

/*
=====================================================================

GROWABLE ARRAYS

=====================================================================
*/

typedef struct {
	uint8_t		*data;
	int			count;
	int			capacity;
	size_t		elemsize;
} dbuf_t;

static void BufInit( dbuf_t *b, size_t elemsize ) {
	memset( b, 0, sizeof( *b ) );
	b->elemsize = elemsize;
}

static void *BufPush( dbuf_t *b ) {
	if ( b->count == b->capacity ) {
		b->capacity = b->capacity ? b->capacity * 2 : 64;
		b->data = realloc( b->data, b->capacity * b->elemsize );
	}
	memset( b->data + b->count * b->elemsize, 0, b->elemsize );
	return b->data + b->count++ * b->elemsize;
}

#define BufAt( b, type, i )	( (type *)( (b)->data ) + ( i ) )

static void BufFree( dbuf_t *b ) {
	free( b->data );
	memset( b, 0, sizeof( *b ) );
}

/*
=====================================================================

2D GEOMETRY

=====================================================================
*/

typedef struct {
	double	x, y;
} pt_t;

typedef struct {
	double	nx, ny, d;		// front side: nx*x + ny*y - d > 0
} line_t;

typedef struct {
	int		n;
	pt_t	*p;				// counter-clockwise
} poly_t;

typedef struct {
	pt_t	a, b;
	int		linedef;
} seg_t;

static double LineDist( const line_t *l, pt_t p ) {
	return l->nx * p.x + l->ny * p.y - l->d;
}

// Doom's front side is to the right of v1->v2, which is where the normal points
static line_t LineFromPoints( pt_t a, pt_t b ) {
	line_t	l;
	double	dx = b.x - a.x, dy = b.y - a.y;
	double	len = sqrt( dx * dx + dy * dy );

	l.nx = dy / len;
	l.ny = -dx / len;
	l.d = l.nx * a.x + l.ny * a.y;
	return l;
}

static pt_t Vertex( const doom_map_t *map, int v ) {
	pt_t	p;

	p.x = map->vertexes[v].x;
	p.y = map->vertexes[v].y;
	return p;
}

static void PolyFree( poly_t *p ) {
	free( p->p );
	p->p = NULL;
	p->n = 0;
}

static double PolyArea( const poly_t *p ) {
	double	a = 0;
	int		i;

	for ( i = 0 ; i < p->n ; i++ ) {
		const pt_t *u = &p->p[i], *v = &p->p[( i + 1 ) % p->n];
		a += u->x * v->y - v->x * u->y;
	}
	return a * 0.5;
}

static pt_t PolyCentroid( const poly_t *p ) {
	pt_t	c = { 0, 0 };
	double	a = 0;
	int		i;

	for ( i = 0 ; i < p->n ; i++ ) {
		const pt_t	*u = &p->p[i], *v = &p->p[( i + 1 ) % p->n];
		double		cross = u->x * v->y - v->x * u->y;

		a += cross;
		c.x += ( u->x + v->x ) * cross;
		c.y += ( u->y + v->y ) * cross;
	}
	if ( fabs( a ) < 1e-12 ) {
		c = p->p[0];
		return c;
	}
	c.x /= 3 * a;
	c.y /= 3 * a;
	return c;
}

/*
=================
ClipPoly

Splits a convex polygon by a line. Either output can come back empty.
=================
*/
static void ClipPoly( const poly_t *in, const line_t *l, poly_t *front, poly_t *back ) {
	int		i;

	front->n = back->n = 0;
	front->p = malloc( sizeof( pt_t ) * ( in->n + 2 ) );
	back->p = malloc( sizeof( pt_t ) * ( in->n + 2 ) );

	for ( i = 0 ; i < in->n ; i++ ) {
		pt_t	a = in->p[i], b = in->p[( i + 1 ) % in->n];
		double	da = LineDist( l, a ), db = LineDist( l, b );

		if ( da >= -EPSILON ) {
			front->p[front->n++] = a;
		}
		if ( da <= EPSILON ) {
			back->p[back->n++] = a;
		}
		if ( ( da > EPSILON && db < -EPSILON ) || ( da < -EPSILON && db > EPSILON ) ) {
			double	t = da / ( da - db );
			pt_t	m;

			m.x = a.x + t * ( b.x - a.x );
			m.y = a.y + t * ( b.y - a.y );
			front->p[front->n++] = m;
			back->p[back->n++] = m;
		}
	}
	if ( front->n < 3 ) {
		front->n = 0;
	}
	if ( back->n < 3 ) {
		back->n = 0;
	}
}

// drops consecutive (near) duplicate points
static void PolyClean( poly_t *p ) {
	int		i, out = 0;

	for ( i = 0 ; i < p->n ; i++ ) {
		pt_t a = p->p[i];

		if ( out && fabs( a.x - p->p[out - 1].x ) < 0.001 && fabs( a.y - p->p[out - 1].y ) < 0.001 ) {
			continue;
		}
		p->p[out++] = a;
	}
	while ( out > 1 && fabs( p->p[0].x - p->p[out - 1].x ) < 0.001 && fabs( p->p[0].y - p->p[out - 1].y ) < 0.001 ) {
		out--;
	}
	p->n = out >= 3 ? out : 0;
}

/*
=================
doom_map_sector_at

Casts a ray towards +X and takes the side of the first linedef it hits.
Returns the sector index, or -1 for the void outside the map.
=================
*/
int doom_map_sector_at( const doom_map_t *map, double px, double py ) {
	double	bestX = 1e30;
	int		best = -1, i, side;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		pt_t					a = Vertex( map, ld->v1 ), b = Vertex( map, ld->v2 );
		double					x;

		if ( ( a.y > py ) == ( b.y > py ) ) {
			continue;
		}
		x = a.x + ( py - a.y ) * ( b.x - a.x ) / ( b.y - a.y );
		if ( x > px && x < bestX ) {
			bestX = x;
			best = i;
		}
	}
	if ( best < 0 ) {
		return -1;
	}

	{
		const doom_linedef_t	*ld = &map->linedefs[best];
		pt_t					a = Vertex( map, ld->v1 ), b = Vertex( map, ld->v2 );
		double					cross = ( b.x - a.x ) * ( py - a.y ) - ( b.y - a.y ) * ( px - a.x );

		side = cross < 0 ? ld->side[0] : ld->side[1];	// right of v1->v2 is the front
	}
	return side == DOOM_NO_SIDE ? -1 : map->sidedefs[side].sector;
}

/*
=====================================================================

2D BSP

=====================================================================
*/

typedef struct {
	line_t	line;
	int		children[2];	// >= 0 node, < 0 is -(leaf + 1); [0] front
	double	mins[2], maxs[2];
} bspnode2_t;

typedef struct {
	poly_t	poly;
	int		sector;			// -1 void
	int		degenerate;		// too small to matter: no brushes, no surfaces
	double	area;
} bspleaf2_t;

typedef struct {
	const doom_map_t	*map;
	const doom_wad_t	*wad;
	dbuf_t				nodes;		// bspnode2_t
	dbuf_t				leafs;		// bspleaf2_t
} builder2d_t;

static int SegSide( const line_t *l, const seg_t *s, int *split ) {
	double	da = LineDist( l, s->a ), db = LineDist( l, s->b );

	*split = 0;
	if ( fabs( da ) <= EPSILON && fabs( db ) <= EPSILON ) {
		return 2;		// on the line
	}
	if ( da >= -EPSILON && db >= -EPSILON ) {
		return 0;
	}
	if ( da <= EPSILON && db <= EPSILON ) {
		return 1;
	}
	*split = 1;
	return 3;
}

/*
=================
ChooseSplitter

Fewest splits first, then balance; axial lines win ties (fewer slivers).
=================
*/
static int ChooseSplitter( const builder2d_t *b, const seg_t *segs, int count ) {
	int		best = 0, i, j, step;
	double	bestScore = 1e30;

	step = count > 300 ? count / 300 : 1;
	for ( i = 0 ; i < count ; i += step ) {
		line_t	l = LineFromPoints( Vertex( b->map, b->map->linedefs[segs[i].linedef].v1 ),
								Vertex( b->map, b->map->linedefs[segs[i].linedef].v2 ) );
		int		front = 0, back = 0, splits = 0, split;
		double	score;

		for ( j = 0 ; j < count ; j++ ) {
			switch ( SegSide( &l, &segs[j], &split ) ) {
			case 0: front++; break;
			case 1: back++; break;
			case 3: splits++; front++; back++; break;
			default: break;
			}
		}
		score = splits * 4.0 + abs( front - back );
		if ( fabs( l.nx ) > 1e-9 && fabs( l.ny ) > 1e-9 ) {
			score += 0.5;
		}
		if ( score < bestScore ) {
			bestScore = score;
			best = i;
		}
	}
	return best;
}

static int MakeLeaf( builder2d_t *b, poly_t *poly ) {
	bspleaf2_t	*leaf = BufPush( &b->leafs );
	int			index = b->leafs.count - 1;

	PolyClean( poly );
	leaf->poly = *poly;
	leaf->area = leaf->poly.n ? PolyArea( &leaf->poly ) : 0;
	leaf->degenerate = leaf->area < MIN_LEAF_AREA;
	if ( leaf->degenerate ) {
		leaf->sector = -1;
	} else {
		pt_t c = PolyCentroid( &leaf->poly );
		leaf->sector = doom_map_sector_at( b->map, c.x, c.y );
	}
	return -( index + 1 );
}

/*
=================
BuildNode

Takes ownership of 'segs' and 'cell'. Returns a node index (>= 0) or a
leaf reference -(leaf + 1).
=================
*/
static int BuildNode( builder2d_t *b, seg_t *segs, int count, poly_t *cell ) {
	seg_t	*front, *back;
	int		nf = 0, nb = 0, i, which, split, index;
	line_t	l;
	poly_t	fcell, bcell;
	const doom_linedef_t *ld;

	if ( !count ) {
		free( segs );
		return MakeLeaf( b, cell );
	}

	which = ChooseSplitter( b, segs, count );
	ld = &b->map->linedefs[segs[which].linedef];
	l = LineFromPoints( Vertex( b->map, ld->v1 ), Vertex( b->map, ld->v2 ) );

	front = malloc( sizeof( seg_t ) * count * 2 );
	back = malloc( sizeof( seg_t ) * count * 2 );
	for ( i = 0 ; i < count ; i++ ) {
		seg_t	*s = &segs[i];

		switch ( SegSide( &l, s, &split ) ) {
		case 0:
			front[nf++] = *s;
			break;
		case 1:
			back[nb++] = *s;
			break;
		case 2:
			break;		// lies on the partition: becomes a cell boundary
		default: {
			double	da = LineDist( &l, s->a ), db = LineDist( &l, s->b );
			double	t = da / ( da - db );
			seg_t	sa = *s, sb = *s;
			pt_t	m;

			m.x = s->a.x + t * ( s->b.x - s->a.x );
			m.y = s->a.y + t * ( s->b.y - s->a.y );
			sa.b = m;
			sb.a = m;
			if ( da > 0 ) {
				front[nf++] = sa;
				back[nb++] = sb;
			} else {
				back[nb++] = sa;
				front[nf++] = sb;
			}
			break;
		}
		}
	}
	free( segs );

	ClipPoly( cell, &l, &fcell, &bcell );
	PolyFree( cell );

	// a partition along the cell edge leaves one side empty: no node needed
	if ( !fcell.n || !bcell.n ) {
		poly_t	*keep = fcell.n ? &fcell : &bcell;
		poly_t	*drop = fcell.n ? &bcell : &fcell;

		PolyFree( drop );
		if ( fcell.n ) {
			free( back );
			return BuildNode( b, front, nf, keep );
		}
		free( front );
		return BuildNode( b, back, nb, keep );
	}

	index = b->nodes.count;
	( (bspnode2_t *)BufPush( &b->nodes ) )->line = l;
	{
		int child0 = BuildNode( b, front, nf, &fcell );
		int child1 = BuildNode( b, back, nb, &bcell );
		bspnode2_t *node = BufAt( &b->nodes, bspnode2_t, index );

		node->line = l;
		node->children[0] = child0;
		node->children[1] = child1;
	}
	return index;
}

static void NodeBounds( builder2d_t *b, int ref, double mins[2], double maxs[2] ) {
	int		i;

	if ( ref < 0 ) {
		const bspleaf2_t *leaf = BufAt( &b->leafs, bspleaf2_t, -ref - 1 );

		mins[0] = mins[1] = 1e30;
		maxs[0] = maxs[1] = -1e30;
		for ( i = 0 ; i < leaf->poly.n ; i++ ) {
			if ( leaf->poly.p[i].x < mins[0] ) mins[0] = leaf->poly.p[i].x;
			if ( leaf->poly.p[i].y < mins[1] ) mins[1] = leaf->poly.p[i].y;
			if ( leaf->poly.p[i].x > maxs[0] ) maxs[0] = leaf->poly.p[i].x;
			if ( leaf->poly.p[i].y > maxs[1] ) maxs[1] = leaf->poly.p[i].y;
		}
		if ( !leaf->poly.n ) {
			mins[0] = mins[1] = maxs[0] = maxs[1] = 0;
		}
		return;
	} else {
		bspnode2_t	*node = BufAt( &b->nodes, bspnode2_t, ref );
		double		m0[2], x0[2], m1[2], x1[2];

		NodeBounds( b, node->children[0], m0, x0 );
		NodeBounds( b, node->children[1], m1, x1 );
		node = BufAt( &b->nodes, bspnode2_t, ref );
		for ( i = 0 ; i < 2 ; i++ ) {
			node->mins[i] = mins[i] = m0[i] < m1[i] ? m0[i] : m1[i];
			node->maxs[i] = maxs[i] = x0[i] > x1[i] ? x0[i] : x1[i];
		}
	}
}

/*
=====================================================================

QUAKE 3 OUTPUT

=====================================================================
*/

typedef struct {
	char	name[64];
	int		surfaceFlags;
	int		contentFlags;
} oshader_t;

typedef struct {
	float	normal[3];
	float	dist;
} oplane_t;

typedef struct {
	int		planeNum;
	int		children[2];
	int		mins[3], maxs[3];
} onode_t;

typedef struct {
	int		cluster, area;
	int		mins[3], maxs[3];
	int		firstLeafSurface, numLeafSurfaces;
	int		firstLeafBrush, numLeafBrushes;
} oleaf_t;

typedef struct {
	float	mins[3], maxs[3];
	int		firstSurface, numSurfaces;
	int		firstBrush, numBrushes;
} omodel_t;

typedef struct {
	int		firstSide, numSides, shaderNum;
} obrush_t;

typedef struct {
	int		planeNum, shaderNum;
} obrushside_t;

typedef struct {
	float	xyz[3];
	float	st[2];
	float	lightmap[2];
	float	normal[3];
	uint8_t	color[4];
} overt_t;

typedef struct {
	int		shaderNum, fogNum, surfaceType;
	int		firstVert, numVerts;
	int		firstIndex, numIndexes;
	int		lightmapNum;
	int		lightmapX, lightmapY, lightmapWidth, lightmapHeight;
	float	lightmapOrigin[3];
	float	lightmapVecs[3][3];
	int		patchWidth, patchHeight;
} osurface_t;

#define MST_TRIANGLE_SOUP	3
#define LIGHTMAP_BY_VERTEX	-3

#define PLANE_HASH			4096

typedef struct {
	int		shader;
	int		sector;		// sector the surface is seen from (leaf surface lists), -1 none
	int		model;		// 0 world, else the mover model being built
	int		surface;	// current osurface_t, -1 when a new one must be started
} surfbuilder_t;

// a moving floor or ceiling: one inline model
typedef struct {
	int		sector;
	int		ceiling;		// 0 floor, 1 ceiling
	int		model;			// inline model number (*model)
	int		firstSurface, numSurfaces;
	int		firstBrush, numBrushes;
	float	mins[3], maxs[3];
} mover_t;

typedef struct {
	const doom_map_t	*map;
	const doom_wad_t	*wad;
	builder2d_t			bsp;

	double				zmin, zmax;		// world vertical extent
	double				bmins[2], bmaxs[2];	// world horizontal extent (with margin)
	char				skyShader[64];

	dbuf_t				shaders;		// oshader_t
	dbuf_t				planes;			// oplane_t
	int					planeHash[PLANE_HASH];
	dbuf_t				planeNext;		// int, hash chain per plane pair
	dbuf_t				nodes;			// onode_t
	dbuf_t				leafs;			// oleaf_t
	dbuf_t				leafSurfaces;	// int
	dbuf_t				leafBrushes;	// int
	dbuf_t				brushes;		// obrush_t
	dbuf_t				sides;			// obrushside_t
	dbuf_t				verts;			// overt_t
	dbuf_t				indexes;		// int
	dbuf_t				surfaces;		// osurface_t
	dbuf_t				entities;		// char

	dbuf_t				surfVerts;		// dbuf_t of overt_t per surface: surfaces are built
	dbuf_t				surfIndexes;	// in parallel, vertexes are made contiguous at the end
	dbuf_t				*sectorSurfaces;	// per sector: int surface indexes
	dbuf_t				*leafExtraBrushes;	// per 2D leaf: clip brushes
	dbuf_t				wallBuilders;		// surfbuilder_t, one per (texture, sector, model)
	int					shaderSolid, shaderNodraw, shaderPlayerClip, shaderMonsterClip;

	// moving sectors: the world uses each plane's extreme position (floors at
	// their lowest, ceilings at their highest), movers fill the rest
	doom_plane_range_t	*ranges;
	dbuf_t				movers;			// mover_t
	int					currentModel;	// model the surfaces being built belong to
} builder_t;

#define FLOOR_MOVES( b, s )		( (b)->ranges[s].floor_min != (b)->ranges[s].floor_max )
#define CEILING_MOVES( b, s )	( (b)->ranges[s].ceiling_min != (b)->ranges[s].ceiling_max )

#define MOVER_INSET		0.25	// mover faces sit this far inside their sector (no z-fighting with static walls)

/*
=================
FindShader
=================
*/
static int FindShader( builder_t *b, const char *name, int surfaceFlags, int contentFlags ) {
	int			i;
	oshader_t	*s;

	for ( i = 0 ; i < b->shaders.count ; i++ ) {
		s = BufAt( &b->shaders, oshader_t, i );
		if ( !strcmp( s->name, name ) && s->surfaceFlags == surfaceFlags && s->contentFlags == contentFlags ) {
			return i;
		}
	}
	s = BufPush( &b->shaders );
	snprintf( s->name, sizeof( s->name ), "%s", name );
	s->surfaceFlags = surfaceFlags;
	s->contentFlags = contentFlags;
	return b->shaders.count - 1;
}

static int FlatShader( builder_t *b, const char *flat, int contentFlags ) {
	char	name[64];

	if ( !strcmp( flat, "F_SKY1" ) ) {
		return FindShader( b, b->skyShader, Q3_SURF_SKY | Q3_SURF_NOIMPACT | Q3_SURF_NOMARKS | Q3_SURF_NOLIGHTMAP, contentFlags );
	}
	snprintf( name, sizeof( name ), "doom/flat/%s", flat );
	return FindShader( b, name, 0, contentFlags );
}

/*
=================
FindPlane

Planes are stored in pairs: n is the plane, n^1 its opposite.
=================
*/
static int FindPlane( builder_t *b, double nx, double ny, double nz, double dist ) {
	int			h, i, flip;
	oplane_t	*p;

	// canonical orientation: first non-zero component positive
	flip = ( nx < -1e-9 ) || ( fabs( nx ) <= 1e-9 && ny < -1e-9 ) || ( fabs( nx ) <= 1e-9 && fabs( ny ) <= 1e-9 && nz < 0 );
	if ( flip ) {
		nx = -nx; ny = -ny; nz = -nz; dist = -dist;
	}

	h = (int)( ( (int64_t)floor( dist * 4 + 0.5 ) * 31 + (int64_t)floor( nx * 1000 + 0.5 ) * 7 + (int64_t)floor( ny * 1000 + 0.5 ) ) & ( PLANE_HASH - 1 ) );
	for ( i = b->planeHash[h] ; i >= 0 ; i = *BufAt( &b->planeNext, int, i / 2 ) ) {
		p = BufAt( &b->planes, oplane_t, i );
		if ( fabs( p->normal[0] - nx ) < 1e-6 && fabs( p->normal[1] - ny ) < 1e-6
			&& fabs( p->normal[2] - nz ) < 1e-6 && fabs( p->dist - dist ) < 0.001 ) {
			return i | flip;
		}
	}

	p = BufPush( &b->planes );
	p->normal[0] = (float)nx; p->normal[1] = (float)ny; p->normal[2] = (float)nz; p->dist = (float)dist;
	p = BufPush( &b->planes );
	p->normal[0] = (float)-nx; p->normal[1] = (float)-ny; p->normal[2] = (float)-nz; p->dist = (float)-dist;
	i = b->planes.count - 2;
	*(int *)BufPush( &b->planeNext ) = b->planeHash[h];
	b->planeHash[h] = i;
	return i | flip;
}

/*
=================
AddPrismBrush

A vertical prism over a CCW polygon. The first six sides must be the
axial bounding planes in the order -X +X -Y +Y -Z +Z (CM_BoundBrush).
=================
*/
static int AddPrismBrush( builder_t *b, const poly_t *poly, double z0, double z1,
	int contentShader, int bottomShader, int topShader ) {
	obrush_t		*brush;
	obrushside_t	*side;
	double			mins[2] = { 1e30, 1e30 }, maxs[2] = { -1e30, -1e30 };
	int				i, first = b->sides.count;

	if ( poly->n < 3 || z1 <= z0 ) {
		return -1;
	}
	for ( i = 0 ; i < poly->n ; i++ ) {
		if ( poly->p[i].x < mins[0] ) mins[0] = poly->p[i].x;
		if ( poly->p[i].y < mins[1] ) mins[1] = poly->p[i].y;
		if ( poly->p[i].x > maxs[0] ) maxs[0] = poly->p[i].x;
		if ( poly->p[i].y > maxs[1] ) maxs[1] = poly->p[i].y;
	}

	side = BufPush( &b->sides ); side->planeNum = FindPlane( b, -1, 0, 0, -mins[0] ); side->shaderNum = b->shaderNodraw;
	side = BufPush( &b->sides ); side->planeNum = FindPlane( b, 1, 0, 0, maxs[0] ); side->shaderNum = b->shaderNodraw;
	side = BufPush( &b->sides ); side->planeNum = FindPlane( b, 0, -1, 0, -mins[1] ); side->shaderNum = b->shaderNodraw;
	side = BufPush( &b->sides ); side->planeNum = FindPlane( b, 0, 1, 0, maxs[1] ); side->shaderNum = b->shaderNodraw;
	side = BufPush( &b->sides ); side->planeNum = FindPlane( b, 0, 0, -1, -z0 ); side->shaderNum = bottomShader;
	side = BufPush( &b->sides ); side->planeNum = FindPlane( b, 0, 0, 1, z1 ); side->shaderNum = topShader;

	for ( i = 0 ; i < poly->n ; i++ ) {
		pt_t	p0 = poly->p[i], p1 = poly->p[( i + 1 ) % poly->n];
		double	dx = p1.x - p0.x, dy = p1.y - p0.y, len = sqrt( dx * dx + dy * dy );
		double	nx, ny;

		if ( len < 0.001 ) {
			continue;
		}
		nx = dy / len;		// outward for a counter-clockwise polygon
		ny = -dx / len;
		if ( fabs( fabs( nx ) - 1 ) < 1e-9 || fabs( fabs( ny ) - 1 ) < 1e-9 ) {
			continue;		// axial: already covered by the bounding sides
		}
		side = BufPush( &b->sides );
		side->planeNum = FindPlane( b, nx, ny, 0, nx * p0.x + ny * p0.y );
		side->shaderNum = b->shaderNodraw;
	}

	brush = BufPush( &b->brushes );
	brush->firstSide = first;
	brush->numSides = b->sides.count - first;
	brush->shaderNum = contentShader;
	return b->brushes.count - 1;
}

/*
=================
FilterBrushIntoLeafs

Records 'brush' in every 2D leaf its footprint touches.
=================
*/
static void FilterBrushIntoLeafs( builder_t *b, int ref, const poly_t *poly, int brush ) {
	int		i, front = 0, back = 0;

	if ( ref < 0 ) {
		*(int *)BufPush( &b->leafExtraBrushes[-ref - 1] ) = brush;
		return;
	}
	{
		const bspnode2_t *node = BufAt( &b->bsp.nodes, bspnode2_t, ref );
		for ( i = 0 ; i < poly->n ; i++ ) {
			double d = LineDist( &node->line, poly->p[i] );
			if ( d > EPSILON ) front = 1;
			if ( d < -EPSILON ) back = 1;
		}
		if ( front ) {
			FilterBrushIntoLeafs( b, node->children[0], poly, brush );
		}
		if ( back ) {
			FilterBrushIntoLeafs( b, BufAt( &b->bsp.nodes, bspnode2_t, ref )->children[1], poly, brush );
		}
	}
}

/*
=====================================================================

RENDER SURFACES

=====================================================================
*/

static uint8_t LightByte( int light ) {
	// the renderer shifts vertex colors up by r_mapOverBrightBits (2)
	// and the overbright bits; dividing by 4 makes 255 render as full bright
	if ( light < 0 ) light = 0;
	if ( light > 255 ) light = 255;
	return (uint8_t)( ( light + 2 ) >> 2 );
}

static void BeginSurface( builder_t *b, surfbuilder_t *sb, int shader, int sector ) {
	sb->shader = shader;
	sb->sector = sector;
	sb->model = b->currentModel;
	sb->surface = -1;
}

/*
=================
SurfacePolygon

Adds a convex polygon (fan triangulated) to the surface being built,
starting a new osurface_t when the renderer limits would be exceeded.
Vertices are in the order they should be wound (clockwise from the front).

Lighting: the vertex color is the sector light (renderers without Doom
lighting), and the unused lightmap coordinates carry the sector index and
the fake contrast offset so the renderer can light it live.
=================
*/
static void EmitPolygon( builder_t *b, surfbuilder_t *sb, int n, const float (*xyz)[3], const float (*st)[2],
	const float normal[3], int lightSector, int lightDelta ) {
	int			light = b->map->sectors[lightSector].light + lightDelta;
	osurface_t	*surf;
	dbuf_t		*verts, *indexes;
	int			i, base;

	if ( n < 3 ) {
		return;
	}
	if ( sb->surface >= 0 ) {
		surf = BufAt( &b->surfaces, osurface_t, sb->surface );
		if ( surf->numVerts + n > MAX_SURF_VERTS || surf->numIndexes + ( n - 2 ) * 3 > MAX_SURF_INDEXES ) {
			sb->surface = -1;
		}
	}
	if ( sb->surface < 0 ) {
		surf = BufPush( &b->surfaces );
		surf->shaderNum = sb->shader;
		surf->fogNum = -1;
		surf->surfaceType = MST_TRIANGLE_SOUP;
		surf->lightmapNum = LIGHTMAP_BY_VERTEX;
		sb->surface = b->surfaces.count - 1;
		BufInit( BufPush( &b->surfVerts ), sizeof( overt_t ) );
		BufInit( BufPush( &b->surfIndexes ), sizeof( int ) );
		// only world surfaces go in leafs; movers are drawn as entities
		if ( sb->sector >= 0 && sb->model == 0 ) {
			*(int *)BufPush( &b->sectorSurfaces[sb->sector] ) = sb->surface;
		}
	}
	surf = BufAt( &b->surfaces, osurface_t, sb->surface );
	verts = BufAt( &b->surfVerts, dbuf_t, sb->surface );
	indexes = BufAt( &b->surfIndexes, dbuf_t, sb->surface );
	base = verts->count;

	for ( i = 0 ; i < n ; i++ ) {
		overt_t *v = BufPush( verts );

		memcpy( v->xyz, xyz[i], sizeof( v->xyz ) );
		memcpy( v->st, st[i], sizeof( v->st ) );
		memcpy( v->normal, normal, sizeof( v->normal ) );
		v->color[0] = v->color[1] = v->color[2] = LightByte( light );
		v->color[3] = 255;
		v->lightmap[0] = (float)lightSector;
		v->lightmap[1] = (float)lightDelta;
	}
	for ( i = 1 ; i < n - 1 ; i++ ) {
		*(int *)BufPush( indexes ) = base;
		*(int *)BufPush( indexes ) = base + i;
		*(int *)BufPush( indexes ) = base + i + 1;
	}
	surf->numVerts = verts->count;
	surf->numIndexes = indexes->count;
}

/*
=================
SurfacePolygon

EmitPolygon after cutting the polygon on a grid of SUBDIVIDE_SIZE units
along the two axes of its plane: the renderer lights vertexes, and Doom's
light fading with distance (r_doomLightFade) needs them close together.
=================
*/
#define SUBDIVIDE_SIZE		64
#define SUBDIVIDE_VERTS		64

// cuts off the part below the first grid line inside the polygon along an
// axis into back[]; the polygon keeps the rest. qfalse if there is no cut.
static int CutPolygon( int *n, float (*xyz)[3], float (*st)[2], int axis, int *nb, float (*back)[3], float (*backSt)[2] ) {
	float	front[SUBDIVIDE_VERTS][3], frontSt[SUBDIVIDE_VERTS][2], mins = xyz[0][axis], maxs = xyz[0][axis], cut;
	int		i, k, nf = 0;

	for ( i = 1 ; i < *n ; i++ ) {
		mins = xyz[i][axis] < mins ? xyz[i][axis] : mins;
		maxs = xyz[i][axis] > maxs ? xyz[i][axis] : maxs;
	}
	cut = (float)( floor( mins / SUBDIVIDE_SIZE ) + 1 ) * SUBDIVIDE_SIZE;
	if ( cut >= maxs - 1.0f || cut <= mins + 1.0f || *n + 2 > SUBDIVIDE_VERTS ) {
		return 0;
	}
	*nb = 0;
	for ( i = 0 ; i < *n ; i++ ) {
		int		j = ( i + 1 ) % *n;
		float	di = xyz[i][axis] - cut, dj = xyz[j][axis] - cut;

		if ( di >= 0 ) {
			memcpy( front[nf], xyz[i], sizeof( front[0] ) );
			memcpy( frontSt[nf++], st[i], sizeof( frontSt[0] ) );
		}
		if ( di <= 0 ) {
			memcpy( back[*nb], xyz[i], sizeof( back[0] ) );
			memcpy( backSt[( *nb )++], st[i], sizeof( backSt[0] ) );
		}
		if ( ( di > 0 && dj < 0 ) || ( di < 0 && dj > 0 ) ) {
			float t = di / ( di - dj );

			for ( k = 0 ; k < 3 ; k++ ) {
				front[nf][k] = back[*nb][k] = xyz[i][k] + t * ( xyz[j][k] - xyz[i][k] );
			}
			front[nf][axis] = back[*nb][axis] = cut;
			for ( k = 0 ; k < 2 ; k++ ) {
				frontSt[nf][k] = backSt[*nb][k] = st[i][k] + t * ( st[j][k] - st[i][k] );
			}
			nf++;
			( *nb )++;
		}
	}
	memcpy( xyz, front, nf * sizeof( front[0] ) );
	memcpy( st, frontSt, nf * sizeof( frontSt[0] ) );
	*n = nf;
	return 1;
}

static void SurfacePolygon( builder_t *b, surfbuilder_t *sb, int n, const float (*xyz)[3], const float (*st)[2],
	const float normal[3], int lightSector, int lightDelta ) {
	float	rest[SUBDIVIDE_VERTS][3], restSt[SUBDIVIDE_VERTS][2], strip[SUBDIVIDE_VERTS][3], stripSt[SUBDIVIDE_VERTS][2];
	float	cell[SUBDIVIDE_VERTS][3], cellSt[SUBDIVIDE_VERTS][2];
	int		axes[2], dominant = 0, i, nStrip, nCell;

	if ( n < 3 || n > SUBDIVIDE_VERTS ) {
		EmitPolygon( b, sb, n, xyz, st, normal, lightSector, lightDelta );
		return;
	}
	for ( i = 1 ; i < 3 ; i++ ) {
		if ( fabs( normal[i] ) > fabs( normal[dominant] ) ) {
			dominant = i;
		}
	}
	axes[0] = dominant == 0 ? 1 : 0;
	axes[1] = dominant == 2 ? 1 : 2;

	// strips along the first axis, then cells along the second
	memcpy( rest, xyz, n * sizeof( rest[0] ) );
	memcpy( restSt, st, n * sizeof( restSt[0] ) );
	for ( ;; ) {
		int more = CutPolygon( &n, rest, restSt, axes[0], &nStrip, strip, stripSt );

		if ( !more ) {
			nStrip = n;
			memcpy( strip, rest, n * sizeof( strip[0] ) );
			memcpy( stripSt, restSt, n * sizeof( stripSt[0] ) );
		}
		for ( ;; ) {
			int moreCells = CutPolygon( &nStrip, strip, stripSt, axes[1], &nCell, cell, cellSt );

			if ( !moreCells ) {
				EmitPolygon( b, sb, nStrip, (const float (*)[3])strip, (const float (*)[2])stripSt, normal, lightSector, lightDelta );
				break;
			}
			EmitPolygon( b, sb, nCell, (const float (*)[3])cell, (const float (*)[2])cellSt, normal, lightSector, lightDelta );
		}
		if ( !more ) {
			break;
		}
	}
}

/*
=====================================================================

POINT LIGHTS (Quake Visuals)

Colored point lights at Doom's light sources, baked into the vertex
colors: lamps, candles and torches, and lit flats (light panels,
teleporter gates, lava, nukage) on a grid over their sector. A light
reaches a vertex unless a wall stands between them in the 2D map, or an
opening between two sectors is too low or too high for the straight line
(the starting heights: closed doors block). Linear falloff, Lambert
shading. The renderer adds them to the sector light only with
r_quakeVisuals (CGEN_DOOM_QUAKE); the other renderings ignore the colors.

=====================================================================
*/

typedef struct {
	float	x, y, z;
	float	color[3];		// 0..1
	float	radius;
} blight_t;

#define MAX_BAKED_LIGHTS		2048
#define LIGHT_GRID				160		// spacing of the lights over a lit flat

typedef struct {
	int		ednum;
	float	r, g, b, radius, z;
} lightthing_t;

static const lightthing_t lightThings[] = {
	{ 2028,	1.0f, 0.95f, 0.8f,	288, 48 },		// floor lamp
	{ 85,	0.8f, 0.9f, 1.0f,	320, 72 },		// tall techno lamp
	{ 86,	0.8f, 0.9f, 1.0f,	240, 48 },		// short techno lamp
	{ 34,	1.0f, 0.75f, 0.45f,	112, 12 },		// candle
	{ 35,	1.0f, 0.75f, 0.45f,	224, 48 },		// candelabra
	{ 44,	0.35f, 0.45f, 1.0f,	288, 64 },		// tall blue torch
	{ 45,	0.35f, 1.0f, 0.4f,	288, 64 },		// tall green torch
	{ 46,	1.0f, 0.4f, 0.25f,	288, 64 },		// tall red torch
	{ 55,	0.35f, 0.45f, 1.0f,	208, 40 },		// short blue torch
	{ 56,	0.35f, 1.0f, 0.4f,	208, 40 },		// short green torch
	{ 57,	1.0f, 0.4f, 0.25f,	208, 40 },		// short red torch
	{ 70,	1.0f, 0.6f, 0.3f,	224, 40 }		// burning barrel
};

typedef struct {
	const char	*flat;
	float		r, g, b, radius;
	int			floor;		// lit from the floor (liquids, gates), else the ceiling
} lightflat_t;

static const lightflat_t lightFlats[] = {
	{ "TLITE6_",	1.0f, 0.95f, 0.85f,	256, 0 },
	{ "CEIL1_2",	1.0f, 1.0f, 0.95f,	224, 0 },
	{ "CEIL1_3",	1.0f, 1.0f, 0.95f,	224, 0 },
	{ "CEIL3_4",	1.0f, 0.95f, 0.85f,	224, 0 },
	{ "CEIL3_6",	1.0f, 0.95f, 0.85f,	224, 0 },
	{ "FLAT2",		1.0f, 1.0f, 1.0f,	224, 0 },
	{ "FLAT17",		1.0f, 1.0f, 1.0f,	224, 0 },
	{ "FLAT22",		0.4f, 0.5f, 1.0f,	224, 0 },
	{ "FLOOR1_7",	1.0f, 0.3f, 0.25f,	192, 1 },
	{ "GATE1",		1.0f, 0.3f, 0.2f,	160, 1 },
	{ "GATE2",		1.0f, 0.3f, 0.2f,	160, 1 },
	{ "GATE3",		1.0f, 0.3f, 0.2f,	160, 1 },
	{ "GATE4",		0.9f, 0.9f, 1.0f,	160, 1 },
	{ "LAVA",		1.0f, 0.45f, 0.15f,	224, 1 },
	{ "NUKAGE",		0.35f, 0.9f, 0.3f,	176, 1 }
};

static void AddLight( blight_t *lights, int *count, float x, float y, float z, const float color[3], float radius ) {
	blight_t *l;

	if ( *count >= MAX_BAKED_LIGHTS ) {
		return;
	}
	l = &lights[( *count )++];
	l->x = x;
	l->y = y;
	l->z = z;
	l->color[0] = color[0];
	l->color[1] = color[1];
	l->color[2] = color[2];
	l->radius = radius;
}

static int CollectLights( builder_t *b, blight_t *lights ) {
	const doom_map_t	*map = b->map;
	int					count = 0, i, j;

	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t *t = &map->things[i];

		for ( j = 0 ; j < (int)( sizeof( lightThings ) / sizeof( lightThings[0] ) ) ; j++ ) {
			const lightthing_t	*lt = &lightThings[j];
			int					sector;
			float				color[3];

			if ( t->type != lt->ednum ) {
				continue;
			}
			sector = doom_map_sector_at( map, t->x + 0.25, t->y + 0.25 );
			if ( sector < 0 ) {
				break;
			}
			color[0] = lt->r;
			color[1] = lt->g;
			color[2] = lt->b;
			AddLight( lights, &count, t->x, t->y, map->sectors[sector].floor_height + lt->z, color, lt->radius );
			break;
		}
	}

	// lit flats: a grid of lights over the sector, under its ceiling or over its floor
	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		const doom_sector_t	*sec = &map->sectors[i];
		double				mins[2] = { 1e9, 1e9 }, maxs[2] = { -1e9, -1e9 }, x, y;
		int					k, lit = -1, side, placed = 0;

		for ( side = 0 ; side < 2 && lit < 0 ; side++ ) {
			const char *flat = side ? sec->floor_flat : sec->ceiling_flat;

			for ( j = 0 ; j < (int)( sizeof( lightFlats ) / sizeof( lightFlats[0] ) ) ; j++ ) {
				if ( lightFlats[j].floor == side && !strncmp( flat, lightFlats[j].flat, strlen( lightFlats[j].flat ) ) ) {
					lit = j;
					break;
				}
			}
		}
		if ( lit < 0 ) {
			continue;
		}
		// the sector's extent, from its lines
		for ( k = 0 ; k < map->num_linedefs ; k++ ) {
			const doom_linedef_t *ld = &map->linedefs[k];
			int v;

			if ( map->sidedefs[ld->side[0]].sector != i && ( ld->side[1] == DOOM_NO_SIDE || map->sidedefs[ld->side[1]].sector != i ) ) {
				continue;
			}
			for ( v = 0 ; v < 2 ; v++ ) {
				const doom_vertex_t *vx = &map->vertexes[v ? ld->v2 : ld->v1];

				mins[0] = vx->x < mins[0] ? vx->x : mins[0];
				mins[1] = vx->y < mins[1] ? vx->y : mins[1];
				maxs[0] = vx->x > maxs[0] ? vx->x : maxs[0];
				maxs[1] = vx->y > maxs[1] ? vx->y : maxs[1];
			}
		}
		if ( mins[0] > maxs[0] ) {
			continue;
		}
		{
			const lightflat_t	*lf = &lightFlats[lit];
			float				color[3] = { lf->r, lf->g, lf->b };
			float				z = lf->floor ? sec->floor_height + 16 : sec->ceiling_height - 8;
			double				stepx = ( maxs[0] - mins[0] ) / ( floor( ( maxs[0] - mins[0] ) / LIGHT_GRID ) + 1 );
			double				stepy = ( maxs[1] - mins[1] ) / ( floor( ( maxs[1] - mins[1] ) / LIGHT_GRID ) + 1 );

			for ( x = mins[0] + stepx * 0.5 ; x < maxs[0] && placed < 64 ; x += stepx ) {
				for ( y = mins[1] + stepy * 0.5 ; y < maxs[1] && placed < 64 ; y += stepy ) {
					if ( doom_map_sector_at( map, x, y ) == i ) {
						AddLight( lights, &count, (float)x, (float)y, z, color, lf->radius );
						placed++;
					}
				}
			}
			// small or odd sectors: at least one light, in the middle of a line's inner side
			if ( !placed ) {
				AddLight( lights, &count, (float)( ( mins[0] + maxs[0] ) * 0.5 ), (float)( ( mins[1] + maxs[1] ) * 0.5 ), z, color, lf->radius );
			}
		}
	}
	return count;
}

// true if the 2D map blocks the straight line from the light to the point
static int LightBlocked( const doom_map_t *map, const int *lines, int numLines, const blight_t *l, const float p[3] ) {
	double	dx = p[0] - l->x, dy = p[1] - l->y, len = sqrt( dx * dx + dy * dy );
	int		i;

	if ( len < 1 ) {
		return 0;
	}
	for ( i = 0 ; i < numLines ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[lines[i]];
		const doom_vertex_t		*a = &map->vertexes[ld->v1], *c = &map->vertexes[ld->v2];
		double					ex = c->x - a->x, ey = c->y - a->y, den = dx * ey - dy * ex, u, t;

		if ( fabs( den ) < 1e-9 ) {
			continue;
		}
		u = ( ( a->x - l->x ) * ey - ( a->y - l->y ) * ex ) / den;		// along light -> point
		t = ( ( a->x - l->x ) * dy - ( a->y - l->y ) * dx ) / den;		// along the line
		if ( u <= 0 || t < 0 || t > 1 || ( 1 - u ) * len < 1.0 ) {
			continue;		// not between them, or the point is on this line
		}
		if ( ld->side[1] == DOOM_NO_SIDE ) {
			return 1;
		}
		{
			const doom_sector_t	*s0 = &map->sectors[map->sidedefs[ld->side[0]].sector];
			const doom_sector_t	*s1 = &map->sectors[map->sidedefs[ld->side[1]].sector];
			double				z = l->z + u * ( p[2] - l->z );
			double				bottom = s0->floor_height > s1->floor_height ? s0->floor_height : s1->floor_height;
			double				top = s0->ceiling_height < s1->ceiling_height ? s0->ceiling_height : s1->ceiling_height;

			if ( z < bottom - 0.5 || z > top + 0.5 ) {
				return 1;
			}
		}
	}
	return 0;
}

static void BakeLights( builder_t *b ) {
	const doom_map_t	*map = b->map;
	blight_t			*lights = malloc( sizeof( blight_t ) * MAX_BAKED_LIGHTS );
	float				*sum;
	int					*lines = malloc( sizeof( int ) * map->num_linedefs );
	int					numLights, s, i, v, total = 0, base;

	numLights = CollectLights( b, lights );
	for ( s = 0 ; s < b->surfVerts.count ; s++ ) {
		total += BufAt( &b->surfVerts, dbuf_t, s )->count;
	}
	sum = calloc( total * 3 + 1, sizeof( float ) );

	for ( i = 0 ; i < numLights ; i++ ) {
		const blight_t	*l = &lights[i];
		int				numLines = 0, k;

		// the lines in reach of the light
		for ( k = 0 ; k < map->num_linedefs ; k++ ) {
			const doom_linedef_t	*ld = &map->linedefs[k];
			const doom_vertex_t		*a = &map->vertexes[ld->v1], *c = &map->vertexes[ld->v2];
			double					minx = a->x < c->x ? a->x : c->x, maxx = a->x > c->x ? a->x : c->x;
			double					miny = a->y < c->y ? a->y : c->y, maxy = a->y > c->y ? a->y : c->y;

			if ( minx > l->x + l->radius || maxx < l->x - l->radius || miny > l->y + l->radius || maxy < l->y - l->radius ) {
				continue;
			}
			lines[numLines++] = k;
		}

		for ( s = 0, base = 0 ; s < b->surfVerts.count ; s++ ) {
			dbuf_t *verts = BufAt( &b->surfVerts, dbuf_t, s );

			for ( v = 0 ; v < verts->count ; v++ ) {
				const overt_t	*vt = BufAt( verts, overt_t, v );
				float			dir[3], d, att, lambert;

				dir[0] = l->x - vt->xyz[0];
				dir[1] = l->y - vt->xyz[1];
				dir[2] = l->z - vt->xyz[2];
				d = (float)sqrt( dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2] );
				if ( d >= l->radius ) {
					continue;
				}
				lambert = d > 0.01f ? ( dir[0] * vt->normal[0] + dir[1] * vt->normal[1] + dir[2] * vt->normal[2] ) / d : 1;
				if ( lambert <= 0 ) {
					continue;
				}
				if ( LightBlocked( map, lines, numLines, l, vt->xyz ) ) {
					continue;
				}
				att = ( 1 - d / l->radius ) * lambert;
				sum[( base + v ) * 3 + 0] += l->color[0] * att;
				sum[( base + v ) * 3 + 1] += l->color[1] * att;
				sum[( base + v ) * 3 + 2] += l->color[2] * att;
			}
			base += verts->count;
		}
	}

	// into the vertex colors (LightByte scale: the renderer shifts them back up)
	for ( s = 0, base = 0 ; s < b->surfVerts.count ; s++ ) {
		dbuf_t *verts = BufAt( &b->surfVerts, dbuf_t, s );

		for ( v = 0 ; v < verts->count ; v++ ) {
			overt_t *vt = BufAt( verts, overt_t, v );
			int c;

			for ( c = 0 ; c < 3 ; c++ ) {
				vt->color[c] = LightByte( (int)( sum[( base + v ) * 3 + c] * 255 ) );
			}
		}
		base += verts->count;
	}
	free( sum );
	free( lines );
	free( lights );
}

/*
=================
PackSurfaces

Lays every surface's vertexes and indexes out contiguously, as the
renderer requires (indexes are relative to the surface's first vertex).
=================
*/
static void PackSurfaces( builder_t *b ) {
	int		i, j;

	for ( i = 0 ; i < b->surfaces.count ; i++ ) {
		osurface_t	*surf = BufAt( &b->surfaces, osurface_t, i );
		dbuf_t		*verts = BufAt( &b->surfVerts, dbuf_t, i );
		dbuf_t		*indexes = BufAt( &b->surfIndexes, dbuf_t, i );

		surf->firstVert = b->verts.count;
		surf->firstIndex = b->indexes.count;
		for ( j = 0 ; j < verts->count ; j++ ) {
			*(overt_t *)BufPush( &b->verts ) = *BufAt( verts, overt_t, j );
		}
		for ( j = 0 ; j < indexes->count ; j++ ) {
			*(int *)BufPush( &b->indexes ) = *BufAt( indexes, int, j );
		}
		BufFree( verts );
		BufFree( indexes );
	}
}

static int TextureSize( builder_t *b, const char *name, int *w, int *h ) {
	const doom_texture_t *tex = doom_texture( b->wad, doom_find_texture( b->wad, name ) );

	if ( !tex || tex->width <= 0 || tex->height <= 0 ) {
		*w = *h = 64;
		return 0;
	}
	*w = tex->width;
	*h = tex->height;
	return 1;
}

static int IsSky( const char *flat ) {
	return !strcmp( flat, "F_SKY1" );
}

/*
=================
WallShaderName

Switch textures get one shader per line, so the game can swap them line by
line (shader remapping); the front side of a scrolling line scrolls.
=================
*/
static void WallShaderName( builder_t *b, int line, int side, const char *texture, char *out, size_t outlen ) {
	const doom_special_t	*sp = doom_line_special( b->map->linedefs[line].special );
	char					other[DOOM_LUMP_NAME_LEN + 1];

	if ( sp && ( sp->trigger == DOOM_TRIG_SWITCH || sp->trigger == DOOM_TRIG_GUN ) && doom_switch_texture( b->wad, texture, other ) ) {
		snprintf( out, outlen, "doom/sw%d/%s", line, texture );
	} else if ( sp && sp->kind == DOOM_KIND_SCROLL && side == 0 ) {
		snprintf( out, outlen, "doom/scroll/%s", texture );
	} else {
		snprintf( out, outlen, "doom/wall/%s", texture );
	}
}

// Doom's "fake contrast": east-west walls darker, north-south walls lighter
static int FakeContrast( pt_t v1, pt_t v2 ) {
	if ( v1.y == v2.y ) {
		return -16;
	}
	if ( v1.x == v2.x ) {
		return 16;
	}
	return 0;
}

/*
=================
WallQuad

One wall piece of sidedef 'side' of 'line' between z0 and z1, facing the
right of a->c. 'texTop' is the height at which texture row 0 sits before
the sidedef y offset (Doom pegging), in world heights; 'zShift' converts
the quad's heights to world heights (movers are built at a canonical
position). 'inset' pulls the quad back against its normal.
=================
*/
static void WallQuad( builder_t *b, int line, int side, const char *texture, int surfSector, int lightSector, int lightDelta,
	pt_t a, pt_t c, double z0, double z1, double zShift, double texTop, double xoff, double yoff, double inset ) {
	surfbuilder_t	*sb = NULL;
	float			xyz[4][3], st[4][2], normal[3];
	char			name[64];
	int				tw, th, shader, i;
	double			len;
	line_t			l;

	if ( z1 - z0 < 0.01 || !strcmp( texture, "-" ) ) {
		return;
	}
	TextureSize( b, texture, &tw, &th );
	len = sqrt( ( c.x - a.x ) * ( c.x - a.x ) + ( c.y - a.y ) * ( c.y - a.y ) );
	l = LineFromPoints( a, c );
	normal[0] = (float)l.nx;
	normal[1] = (float)l.ny;
	normal[2] = 0;
	a.x -= l.nx * inset; a.y -= l.ny * inset;
	c.x -= l.nx * inset; c.y -= l.ny * inset;

	// clockwise seen from the front: start top, then end top, end bottom, start bottom
	xyz[0][0] = (float)a.x; xyz[0][1] = (float)a.y; xyz[0][2] = (float)z1;
	xyz[1][0] = (float)c.x; xyz[1][1] = (float)c.y; xyz[1][2] = (float)z1;
	xyz[2][0] = (float)c.x; xyz[2][1] = (float)c.y; xyz[2][2] = (float)z0;
	xyz[3][0] = (float)a.x; xyz[3][1] = (float)a.y; xyz[3][2] = (float)z0;
	st[0][0] = st[3][0] = (float)( xoff / tw );
	st[1][0] = st[2][0] = (float)( ( xoff + len ) / tw );
	st[0][1] = st[1][1] = (float)( ( texTop - ( z1 + zShift ) + yoff ) / th );
	st[2][1] = st[3][1] = (float)( ( texTop - ( z0 + zShift ) + yoff ) / th );

	// walls with the same shader facing the same sector in the same model share surfaces
	WallShaderName( b, line, side, texture, name, sizeof( name ) );
	shader = FindShader( b, name, 0, 0 );
	for ( i = 0 ; i < b->wallBuilders.count ; i++ ) {
		surfbuilder_t *w = BufAt( &b->wallBuilders, surfbuilder_t, i );
		if ( w->shader == shader && w->sector == surfSector && w->model == b->currentModel ) {
			sb = w;
			break;
		}
	}
	if ( !sb ) {
		sb = BufPush( &b->wallBuilders );
		BeginSurface( b, sb, shader, surfSector );
	}
	SurfacePolygon( b, sb, 4, (const float (*)[3])xyz, (const float (*)[2])st, normal, lightSector, lightDelta );
}

/*
=================
BuildWalls

Doom's wall rules (r_segs.c): one-sided middle, two-sided upper and lower,
masked two-sided middle, the sky hack, and the fake contrast. Only the
parts that never move are world walls: heights use each sector's extreme
plane positions (floors at their lowest, ceilings at their highest); the
moving parts are faces of the movers. Texture alignment uses the initial
heights.
=================
*/
static void BuildWalls( builder_t *b ) {
	const doom_map_t	*map = b->map;
	int					i, s;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		pt_t					v1 = Vertex( map, ld->v1 ), v2 = Vertex( map, ld->v2 );
		int						contrast = FakeContrast( v1, v2 );

		if ( fabs( v1.x - v2.x ) < 0.001 && fabs( v1.y - v2.y ) < 0.001 ) {
			continue;
		}

		for ( s = 0 ; s < 2 ; s++ ) {
			const doom_sidedef_t		*sd, *other;
			const doom_sector_t			*front, *back;
			const doom_plane_range_t	*rf, *rb;
			// a->c runs from the sidedef's own start vertex (v1 for the front,
			// v2 for the back), so its sector is on the right, where it faces
			pt_t						a = s ? v2 : v1, c = s ? v1 : v2;
			int							tw, th, sector;

			if ( ld->side[s] == DOOM_NO_SIDE ) {
				continue;
			}
			sd = &map->sidedefs[ld->side[s]];
			sector = sd->sector;
			front = &map->sectors[sector];
			rf = &b->ranges[sector];
			other = ld->side[s ^ 1] == DOOM_NO_SIDE ? NULL : &map->sidedefs[ld->side[s ^ 1]];
			back = other ? &map->sectors[other->sector] : NULL;
			rb = other ? &b->ranges[other->sector] : NULL;

			if ( !back ) {
				// one-sided: middle texture from floor to ceiling
				TextureSize( b, sd->mid, &tw, &th );
				WallQuad( b, i, s, sd->mid, sector, sector, contrast, a, c, rf->floor_min, rf->ceiling_max, 0,
					( ld->flags & DOOM_ML_DONTPEGBOTTOM ) ? front->floor_height + th : front->ceiling_height,
					sd->x_offset, sd->y_offset, 0 );
				continue;
			}

			// upper: front ceiling above back ceiling, unless both are sky
			if ( !( IsSky( front->ceiling_flat ) && IsSky( back->ceiling_flat ) ) ) {
				TextureSize( b, sd->top, &tw, &th );
				WallQuad( b, i, s, sd->top, sector, sector, contrast, a, c, rb->ceiling_max, rf->ceiling_max, 0,
					( ld->flags & DOOM_ML_DONTPEGTOP ) ? front->ceiling_height : back->ceiling_height + th,
					sd->x_offset, sd->y_offset, 0 );
			}

			// lower: back floor above front floor
			WallQuad( b, i, s, sd->bottom, sector, sector, contrast, a, c, rf->floor_min, rb->floor_min, 0,
				( ld->flags & DOOM_ML_DONTPEGBOTTOM ) ? front->ceiling_height : back->floor_height,
				sd->x_offset, sd->y_offset, 0 );

			// masked middle texture: one texture height, not tiled vertically
			if ( strcmp( sd->mid, "-" ) ) {
				double	lo = front->floor_height > back->floor_height ? front->floor_height : back->floor_height;
				double	hi = front->ceiling_height < back->ceiling_height ? front->ceiling_height : back->ceiling_height;
				double	top, z0, z1;

				TextureSize( b, sd->mid, &tw, &th );
				top = ( ld->flags & DOOM_ML_DONTPEGBOTTOM ) ? lo + th : hi;
				z1 = top + sd->y_offset;
				z0 = z1 - th;
				if ( z1 > hi ) z1 = hi;
				if ( z0 < lo ) z0 = lo;
				WallQuad( b, i, s, sd->mid, sector, sector, contrast, a, c, z0, z1, 0, top, sd->x_offset, sd->y_offset, 0 );
			}
		}
	}
}

/*
=================
SectorFlat

The floor or ceiling of every leaf of a sector at height z, merged in one
surface builder. Doom draws flats aligned to the world grid with y flipped.
=================
*/
static void SectorFlat( builder_t *b, int sector, int ceiling, double z ) {
	const doom_sector_t	*sec = &b->map->sectors[sector];
	const char			*flat = ceiling ? sec->ceiling_flat : sec->floor_flat;
	surfbuilder_t		sb;
	float				normal[3] = { 0, 0, ceiling ? -1.0f : 1.0f };
	float				(*xyz)[3] = NULL, (*st)[2] = NULL;
	int					i, j, maxPoints = 0, shader;

	// moving planes can change texture ("...AndChange" specials): one shader
	// per sector so the game can remap it alone
	if ( b->currentModel && !IsSky( flat ) ) {
		char name[64];

		snprintf( name, sizeof( name ), "doom/fl%d/%s", sector, flat );
		shader = FindShader( b, name, 0, 0 );
	} else {
		shader = FlatShader( b, flat, 0 );
	}
	BeginSurface( b, &sb, shader, sector );

	for ( i = 0 ; i < b->bsp.leafs.count ; i++ ) {
		const bspleaf2_t *leaf = BufAt( &b->bsp.leafs, bspleaf2_t, i );

		if ( leaf->sector != sector || leaf->degenerate ) {
			continue;
		}
		if ( leaf->poly.n > maxPoints ) {
			maxPoints = leaf->poly.n;
			xyz = realloc( xyz, sizeof( *xyz ) * maxPoints );
			st = realloc( st, sizeof( *st ) * maxPoints );
		}
		for ( j = 0 ; j < leaf->poly.n ; j++ ) {
			// clockwise from the viewer: reversed (CW) from above for floors,
			// the CCW polygon order for ceilings seen from below
			const pt_t *p = &leaf->poly.p[ceiling ? j : leaf->poly.n - 1 - j];

			xyz[j][0] = (float)p->x;
			xyz[j][1] = (float)p->y;
			xyz[j][2] = (float)z;
			st[j][0] = (float)( p->x / 64.0 );
			st[j][1] = (float)( -p->y / 64.0 );
		}
		SurfacePolygon( b, &sb, leaf->poly.n, (const float (*)[3])xyz, (const float (*)[2])st, normal, sector, 0 );
	}
	free( xyz );
	free( st );
}

// world floors and ceilings: only the planes that never move
static void BuildFlats( builder_t *b ) {
	int		sector;

	for ( sector = 0 ; sector < b->map->num_sectors ; sector++ ) {
		if ( !FLOOR_MOVES( b, sector ) ) {
			SectorFlat( b, sector, 0, b->map->sectors[sector].floor_height );
		}
		if ( !CEILING_MOVES( b, sector ) ) {
			SectorFlat( b, sector, 1, b->map->sectors[sector].ceiling_height );
		}
	}
}

/*
=================
BuildMover

A moving floor or ceiling of a sector, as inline model 'model', built at a
canonical position the game offsets by entity origin:
- floor: a block from floor_min to floor_max (the top is the floor), so
  that wherever the floor is, the space below it down to the world floor
  (at floor_min) is filled; entity origin z = floor - floor_max
- ceiling: a block from ceiling_min to ceiling_max (the bottom is the
  ceiling), the world ceiling being at ceiling_max; origin z = ceiling -
  ceiling_min
Its faces on two-sided lines show the neighbor's lower (floor) or upper
(ceiling) texture, inset into the sector.
=================
*/
static void BuildMover( builder_t *b, int sector, int ceiling, int model ) {
	const doom_map_t			*map = b->map;
	const doom_sector_t			*sec = &map->sectors[sector];
	const doom_plane_range_t	*r = &b->ranges[sector];
	mover_t						*m;
	double						z0 = ceiling ? r->ceiling_min : r->floor_min;
	double						z1 = ceiling ? r->ceiling_max : r->floor_max;
	double						zShift = ceiling ? sec->ceiling_height - r->ceiling_min : sec->floor_height - r->floor_max;
	int							i, firstSurface = b->surfaces.count, firstBrush = b->brushes.count;
	float						mins[3] = { 1e30f, 1e30f, (float)z0 }, maxs[3] = { -1e30f, -1e30f, (float)z1 };

	b->currentModel = model;

	// brushes over every leaf of the sector
	for ( i = 0 ; i < b->bsp.leafs.count ; i++ ) {
		const bspleaf2_t	*leaf = BufAt( &b->bsp.leafs, bspleaf2_t, i );
		int					j;

		if ( leaf->sector != sector || leaf->degenerate ) {
			continue;
		}
		if ( ceiling ) {
			AddPrismBrush( b, &leaf->poly, z0, z1, b->shaderSolid, FlatShader( b, sec->ceiling_flat, Q3_CONTENTS_SOLID ), b->shaderNodraw );
		} else {
			AddPrismBrush( b, &leaf->poly, z0, z1, b->shaderSolid, b->shaderNodraw, FlatShader( b, sec->floor_flat, Q3_CONTENTS_SOLID ) );
		}
		for ( j = 0 ; j < leaf->poly.n ; j++ ) {
			if ( leaf->poly.p[j].x < mins[0] ) mins[0] = (float)leaf->poly.p[j].x;
			if ( leaf->poly.p[j].y < mins[1] ) mins[1] = (float)leaf->poly.p[j].y;
			if ( leaf->poly.p[j].x > maxs[0] ) maxs[0] = (float)leaf->poly.p[j].x;
			if ( leaf->poly.p[j].y > maxs[1] ) maxs[1] = (float)leaf->poly.p[j].y;
		}
	}

	// the moving flat
	SectorFlat( b, sector, ceiling, ceiling ? z0 : z1 );

	// faces towards the neighbors
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t		*ld = &map->linedefs[i];
		const doom_sidedef_t		*nsd;
		const doom_sector_t			*nsec;
		const doom_plane_range_t	*rn;
		pt_t						v1 = Vertex( map, ld->v1 ), v2 = Vertex( map, ld->v2 ), a, c;
		int							nside, neighbor, tw, th;

		if ( ld->side[1] == DOOM_NO_SIDE ) {
			continue;
		}
		if ( map->sidedefs[ld->side[0]].sector == sector ) {
			nside = 1;
		} else if ( map->sidedefs[ld->side[1]].sector == sector ) {
			nside = 0;
		} else {
			continue;
		}
		nsd = &map->sidedefs[ld->side[nside]];
		neighbor = nsd->sector;
		if ( neighbor == sector ) {
			continue;
		}
		nsec = &map->sectors[neighbor];
		rn = &b->ranges[neighbor];
		// the neighbor is on the right of a->c
		a = nside ? v2 : v1;
		c = nside ? v1 : v2;

		if ( ceiling ) {
			if ( r->ceiling_min >= rn->ceiling_max || ( IsSky( sec->ceiling_flat ) && IsSky( nsec->ceiling_flat ) ) ) {
				continue;	// never below the neighbor's ceiling
			}
			TextureSize( b, nsd->top, &tw, &th );
			WallQuad( b, i, nside, nsd->top, neighbor, neighbor, FakeContrast( v1, v2 ), a, c, z0, z1, zShift,
				( ld->flags & DOOM_ML_DONTPEGTOP ) ? nsec->ceiling_height : sec->ceiling_height + th,
				nsd->x_offset, nsd->y_offset, MOVER_INSET );
		} else {
			if ( r->floor_max <= rn->floor_min ) {
				continue;	// never above the neighbor's floor
			}
			WallQuad( b, i, nside, nsd->bottom, neighbor, neighbor, FakeContrast( v1, v2 ), a, c, z0, z1, zShift,
				( ld->flags & DOOM_ML_DONTPEGBOTTOM ) ? nsec->ceiling_height : sec->floor_height,
				nsd->x_offset, nsd->y_offset, MOVER_INSET );
		}
	}

	m = BufPush( &b->movers );
	m->sector = sector;
	m->ceiling = ceiling;
	m->model = model;
	m->firstSurface = firstSurface;
	m->numSurfaces = b->surfaces.count - firstSurface;
	m->firstBrush = firstBrush;
	m->numBrushes = b->brushes.count - firstBrush;
	memcpy( m->mins, mins, sizeof( mins ) );
	memcpy( m->maxs, maxs, sizeof( maxs ) );
	b->currentModel = 0;
}

// every moving plane, in sector order (floor before ceiling): models 1..n
static void BuildMovers( builder_t *b ) {
	int		sector;

	for ( sector = 0 ; sector < b->map->num_sectors ; sector++ ) {
		if ( FLOOR_MOVES( b, sector ) ) {
			BuildMover( b, sector, 0, b->movers.count + 1 );
		}
		if ( CEILING_MOVES( b, sector ) ) {
			BuildMover( b, sector, 1, b->movers.count + 1 );
		}
	}
}

/*
=====================================================================

TREE, BRUSHES, ENTITIES, LIGHT GRID

=====================================================================
*/

static void SetBounds3( int mins[3], int maxs[3], const double m2[2], const double x2[2], double zmin, double zmax ) {
	mins[0] = (int)floor( m2[0] ); mins[1] = (int)floor( m2[1] ); mins[2] = (int)floor( zmin );
	maxs[0] = (int)ceil( x2[0] ); maxs[1] = (int)ceil( x2[1] ); maxs[2] = (int)ceil( zmax );
}

/*
=================
EmitLeafs

One Quake 3 leaf per 2D leaf, with its brushes and the surfaces of its
sector. Sector leafs are cluster 0 / area 0 (everything is visible from
everywhere: Doom maps are small); void leafs are opaque (cluster -1).
=================
*/
static void EmitLeafs( builder_t *b ) {
	const doom_map_t	*map = b->map;
	int					i, j;

	for ( i = 0 ; i < b->bsp.leafs.count ; i++ ) {
		bspleaf2_t	*leaf = BufAt( &b->bsp.leafs, bspleaf2_t, i );
		oleaf_t		*out = BufPush( &b->leafs );
		double		m2[2], x2[2];
		int			brush;

		NodeBounds( &b->bsp, -( i + 1 ), m2, x2 );
		SetBounds3( out->mins, out->maxs, m2, x2, b->zmin, b->zmax );
		out->firstLeafBrush = b->leafBrushes.count;
		out->firstLeafSurface = b->leafSurfaces.count;

		if ( leaf->degenerate ) {
			out->cluster = -1;
			out->area = -1;
		} else if ( leaf->sector < 0 ) {
			out->cluster = -1;
			out->area = -1;
			brush = AddPrismBrush( b, &leaf->poly, b->zmin, b->zmax, b->shaderSolid, b->shaderNodraw, b->shaderNodraw );
			if ( brush >= 0 ) {
				*(int *)BufPush( &b->leafBrushes ) = brush;
			}
		} else {
			const doom_sector_t *sec = &map->sectors[leaf->sector];

			out->cluster = 0;
			out->area = 0;
			// moving planes: the world stops at their extreme position, movers fill the rest
			brush = AddPrismBrush( b, &leaf->poly, b->zmin, b->ranges[leaf->sector].floor_min, b->shaderSolid,
				b->shaderNodraw, FlatShader( b, sec->floor_flat, Q3_CONTENTS_SOLID ) );
			if ( brush >= 0 ) {
				*(int *)BufPush( &b->leafBrushes ) = brush;
			}
			brush = AddPrismBrush( b, &leaf->poly, b->ranges[leaf->sector].ceiling_max, b->zmax, b->shaderSolid,
				FlatShader( b, sec->ceiling_flat, Q3_CONTENTS_SOLID ), b->shaderNodraw );
			if ( brush >= 0 ) {
				*(int *)BufPush( &b->leafBrushes ) = brush;
			}
			for ( j = 0 ; j < b->sectorSurfaces[leaf->sector].count ; j++ ) {
				*(int *)BufPush( &b->leafSurfaces ) = *BufAt( &b->sectorSurfaces[leaf->sector], int, j );
			}
		}

		for ( j = 0 ; j < b->leafExtraBrushes[i].count ; j++ ) {
			*(int *)BufPush( &b->leafBrushes ) = *BufAt( &b->leafExtraBrushes[i], int, j );
		}

		out = BufAt( &b->leafs, oleaf_t, i );
		out->numLeafBrushes = b->leafBrushes.count - out->firstLeafBrush;
		out->numLeafSurfaces = b->leafSurfaces.count - out->firstLeafSurface;
	}
}

static void EmitNodes( builder_t *b ) {
	int		i;

	for ( i = 0 ; i < b->bsp.nodes.count ; i++ ) {
		const bspnode2_t	*n = BufAt( &b->bsp.nodes, bspnode2_t, i );
		onode_t				*out = BufPush( &b->nodes );

		out->planeNum = FindPlane( b, n->line.nx, n->line.ny, 0, n->line.d );
		out->children[0] = n->children[0];
		out->children[1] = n->children[1];
		SetBounds3( out->mins, out->maxs, n->mins, n->maxs, b->zmin, b->zmax );
	}
}

/*
=================
BuildClipBrushes

Impassable two-sided lines (ML_BLOCKING) block players and monsters at
any height but let shots through; ML_BLOCKMONSTERS only blocks monsters.
One-sided lines normally lie on the void boundary, which is already
solid; those standing inside open space (map tricks) get a thin solid
wall, since Doom blocks every one-sided line.
=================
*/
static void BuildClipBrushes( builder_t *b, int root ) {
	const doom_map_t	*map = b->map;
	int					i;

	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t	*ld = &map->linedefs[i];
		pt_t					v1 = Vertex( map, ld->v1 ), v2 = Vertex( map, ld->v2 );
		line_t					l;
		poly_t					slab;
		pt_t					pts[4];
		int						shader, brush;

		if ( fabs( v1.x - v2.x ) < 0.001 && fabs( v1.y - v2.y ) < 0.001 ) {
			continue;
		}
		l = LineFromPoints( v1, v2 );

		if ( ld->side[1] == DOOM_NO_SIDE ) {
			// behind the middle of the line: void is already solid
			double mx = ( v1.x + v2.x ) * 0.5 - l.nx * 2, my = ( v1.y + v2.y ) * 0.5 - l.ny * 2;

			if ( doom_map_sector_at( map, mx + 0.01, my + 0.01 ) < 0 ) {
				continue;
			}
			shader = b->shaderSolid;
		} else if ( ld->flags & ( DOOM_ML_BLOCKING | DOOM_ML_BLOCKMONSTERS ) ) {
			shader = ( ld->flags & DOOM_ML_BLOCKING ) ? b->shaderPlayerClip : b->shaderMonsterClip;
			if ( doom_linedef_is_jumpable_window( map, ld ) ) {
				shader = b->shaderMonsterClip;		// players can jump through, monsters can't
			}
		} else {
			continue;
		}

		// counter-clockwise rectangle around the line
		pts[0].x = v1.x - l.nx * CLIP_THICKNESS; pts[0].y = v1.y - l.ny * CLIP_THICKNESS;
		pts[1].x = v1.x + l.nx * CLIP_THICKNESS; pts[1].y = v1.y + l.ny * CLIP_THICKNESS;
		pts[2].x = v2.x + l.nx * CLIP_THICKNESS; pts[2].y = v2.y + l.ny * CLIP_THICKNESS;
		pts[3].x = v2.x - l.nx * CLIP_THICKNESS; pts[3].y = v2.y - l.ny * CLIP_THICKNESS;
		slab.n = 4;
		slab.p = pts;
		if ( PolyArea( &slab ) < 0 ) {
			pt_t t = pts[1]; pts[1] = pts[3]; pts[3] = t;
		}

		brush = AddPrismBrush( b, &slab, b->zmin, b->zmax, shader, b->shaderNodraw, b->shaderNodraw );
		if ( brush >= 0 ) {
			FilterBrushIntoLeafs( b, root, &slab, brush );
		}
	}
}

static void EntPrintf( builder_t *b, const char *fmt, ... ) {
	char	text[512];
	va_list	ap;
	int		i, n;

	va_start( ap, fmt );
	n = vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	for ( i = 0 ; i < n && i < (int)sizeof( text ) - 1 ; i++ ) {
		*(char *)BufPush( &b->entities ) = text[i];
	}
}

/*
=================
BuildEntities

worldspawn plus player starts (1-4 coop, 11 deathmatch). Other things
come with the game logic in later milestones.
=================
*/
static void BuildEntities( builder_t *b ) {
	const doom_map_t	*map = b->map;
	int					i;

	EntPrintf( b, "{\n\"classname\" \"worldspawn\"\n\"message\" \"%s\"\n\"doom_map\" \"%s\"\n}\n", map->name, map->name );

	for ( i = 0 ; i < map->num_things ; i++ ) {
		const doom_thing_t	*t = &map->things[i];
		int					sector;
		double				z;

		if ( !( ( t->type >= 1 && t->type <= 4 ) || t->type == 11 ) ) {
			continue;
		}
		sector = doom_map_sector_at( map, t->x + 0.25, t->y + 0.25 );	// off the integer grid of vertexes
		z = sector >= 0 ? map->sectors[sector].floor_height : 0;
		// origin of the 56 unit tall player box (mins z -24) resting on the floor
		EntPrintf( b, "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"%d %d %d\"\n\"angle\" \"%d\"\n\"doom_type\" \"%d\"\n}\n",
			t->x, t->y, (int)z + 25, t->angle, t->type );
	}

	// moving floors and ceilings; the game positions them from the sector heights
	for ( i = 0 ; i < b->movers.count ; i++ ) {
		const mover_t *m = BufAt( &b->movers, mover_t, i );

		EntPrintf( b, "{\n\"classname\" \"func_doom_plane\"\n\"model\" \"*%d\"\n\"doom_sector\" \"%d\"\n\"doom_plane\" \"%s\"\n}\n",
			m->model, m->sector, m->ceiling ? "ceiling" : "floor" );
	}
	*(char *)BufPush( &b->entities ) = 0;
}

/*
=================
BuildLightGrid

Same grid layout the renderer computes from the world bounds
(R_LoadLightGrid). Each point gets the light of the sector it is in,
as ambient plus a softer directed light from above; points in the void
stay black, which the renderer skips when interpolating.
=================
*/
static void BuildLightGrid( builder_t *b, const float wmins[3], const float wmaxs[3], dbuf_t *grid ) {
	const float	size[3] = { LIGHTGRID_X, LIGHTGRID_Y, LIGHTGRID_Z };
	float		origin[3], maxs[3];
	int			bounds[3], x, y, z, i;

	for ( i = 0 ; i < 3 ; i++ ) {
		origin[i] = size[i] * (float)ceil( wmins[i] / size[i] );
		maxs[i] = size[i] * (float)floor( wmaxs[i] / size[i] );
		bounds[i] = (int)( ( maxs[i] - origin[i] ) / size[i] + 1 );
	}

	for ( z = 0 ; z < bounds[2] ; z++ ) {
		for ( y = 0 ; y < bounds[1] ; y++ ) {
			for ( x = 0 ; x < bounds[0] ; x++ ) {
				uint8_t	*p;
				int		sector, k;
				double	px = origin[0] + x * size[0], py = origin[1] + y * size[1];

				for ( k = 0 ; k < 8 ; k++ ) {
					BufPush( grid );	// zeroed: black, ignored by the renderer
				}
				// grid points sit on integer coordinates, where map vertexes are too
				sector = doom_map_sector_at( b->map, px + 0.25, py + 0.25 );
				if ( sector >= 0 ) {
					p = BufAt( grid, uint8_t, grid->count - 8 );
					p[0] = p[1] = p[2] = LightByte( b->map->sectors[sector].light * 3 / 4 );
					p[3] = p[4] = p[5] = LightByte( b->map->sectors[sector].light / 2 );
					p[6] = p[7] = 0;	// light straight from above
				}
			}
		}
	}
}

/*
=====================================================================

FILE

=====================================================================
*/

#define NUM_LUMPS	17

static void Put32( uint8_t *p, uint32_t v ) {
	p[0] = (uint8_t)v; p[1] = (uint8_t)( v >> 8 ); p[2] = (uint8_t)( v >> 16 ); p[3] = (uint8_t)( v >> 24 );
}

/*
=================
WriteBsp

All Quake 3 BSP structures are 32-bit ints and floats, little-endian on
every platform we target, so the arrays are written as they are.
=================
*/
static uint8_t *WriteBsp( const dbuf_t *lumps[NUM_LUMPS], size_t *outSize ) {
	size_t		size = 8 + NUM_LUMPS * 8, ofs;
	uint8_t		*data;
	int			i;

	for ( i = 0 ; i < NUM_LUMPS ; i++ ) {
		size += ( (size_t)lumps[i]->count * lumps[i]->elemsize + 3 ) & ~(size_t)3;
	}
	data = calloc( 1, size );
	memcpy( data, "IBSP", 4 );
	Put32( data + 4, 46 );

	ofs = 8 + NUM_LUMPS * 8;
	for ( i = 0 ; i < NUM_LUMPS ; i++ ) {
		size_t len = (size_t)lumps[i]->count * lumps[i]->elemsize;

		Put32( data + 8 + i * 8, (uint32_t)ofs );
		Put32( data + 8 + i * 8 + 4, (uint32_t)len );
		if ( len ) {
			memcpy( data + ofs, lumps[i]->data, len );
		}
		ofs += ( len + 3 ) & ~(size_t)3;
	}
	*outSize = size;
	return data;
}

static void SkyShaderName( const doom_map_t *map, char *out, size_t outlen ) {
	int		sky = 1;

	if ( map->name[0] == 'E' && map->name[2] == 'M' ) {
		sky = map->name[1] - '0';				// Doom: one sky per episode
		if ( sky < 1 || sky > 4 ) {
			sky = 1;
		}
	} else if ( !strncmp( map->name, "MAP", 3 ) ) {
		int level = atoi( map->name + 3 );		// Doom II: maps 12-20 and 21+
		sky = level < 12 ? 1 : level < 21 ? 2 : 3;
	}
	snprintf( out, outlen, "doom/sky/SKY%d", sky );
}

int doom_build_bsp( const doom_wad_t *wad, const doom_map_t *map, uint8_t **outData, size_t *outSize,
	doom_build_stats_t *stats, char *err, size_t errlen ) {
	builder_t	b;
	seg_t		*segs;
	poly_t		cell;
	int			i, nsegs = 0, root, worldSurfaces, worldBrushes;
	double		minFloor = 1e30, maxCeiling = -1e30;
	dbuf_t		vis, empty, grid;
	const dbuf_t *lumps[NUM_LUMPS];

	*outData = NULL;
	*outSize = 0;
	if ( doom_map_validate( map, err, errlen ) ) {
		return -1;
	}
	if ( !map->num_sectors || !map->num_linedefs ) {
		doom_set_error( err, errlen, "map has no geometry" );
		return -1;
	}

	memset( &b, 0, sizeof( b ) );
	b.map = map;
	b.wad = wad;
	b.bsp.map = map;
	b.bsp.wad = wad;
	BufInit( &b.bsp.nodes, sizeof( bspnode2_t ) );
	BufInit( &b.bsp.leafs, sizeof( bspleaf2_t ) );
	BufInit( &b.shaders, sizeof( oshader_t ) );
	BufInit( &b.planes, sizeof( oplane_t ) );
	BufInit( &b.planeNext, sizeof( int ) );
	BufInit( &b.nodes, sizeof( onode_t ) );
	BufInit( &b.leafs, sizeof( oleaf_t ) );
	BufInit( &b.leafSurfaces, sizeof( int ) );
	BufInit( &b.leafBrushes, sizeof( int ) );
	BufInit( &b.brushes, sizeof( obrush_t ) );
	BufInit( &b.sides, sizeof( obrushside_t ) );
	BufInit( &b.verts, sizeof( overt_t ) );
	BufInit( &b.indexes, sizeof( int ) );
	BufInit( &b.surfaces, sizeof( osurface_t ) );
	BufInit( &b.entities, 1 );
	BufInit( &b.wallBuilders, sizeof( surfbuilder_t ) );
	BufInit( &b.surfVerts, sizeof( dbuf_t ) );
	BufInit( &b.surfIndexes, sizeof( dbuf_t ) );
	BufInit( &vis, 1 );
	BufInit( &empty, 1 );
	BufInit( &grid, 1 );
	for ( i = 0 ; i < PLANE_HASH ; i++ ) {
		b.planeHash[i] = -1;
	}
	SkyShaderName( map, b.skyShader, sizeof( b.skyShader ) );

	// how far every floor and ceiling can move
	b.ranges = malloc( sizeof( doom_plane_range_t ) * map->num_sectors );
	doom_sector_ranges( wad, map, b.ranges );
	BufInit( &b.movers, sizeof( mover_t ) );

	// world extents
	b.bmins[0] = b.bmins[1] = 1e30;
	b.bmaxs[0] = b.bmaxs[1] = -1e30;
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		pt_t p[2];
		int k;

		p[0] = Vertex( map, map->linedefs[i].v1 );
		p[1] = Vertex( map, map->linedefs[i].v2 );
		for ( k = 0 ; k < 2 ; k++ ) {
			if ( p[k].x < b.bmins[0] ) b.bmins[0] = p[k].x;
			if ( p[k].y < b.bmins[1] ) b.bmins[1] = p[k].y;
			if ( p[k].x > b.bmaxs[0] ) b.bmaxs[0] = p[k].x;
			if ( p[k].y > b.bmaxs[1] ) b.bmaxs[1] = p[k].y;
		}
	}
	b.bmins[0] -= WORLD_MARGIN; b.bmins[1] -= WORLD_MARGIN;
	b.bmaxs[0] += WORLD_MARGIN; b.bmaxs[1] += WORLD_MARGIN;
	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		if ( b.ranges[i].floor_min < minFloor ) minFloor = b.ranges[i].floor_min;
		if ( b.ranges[i].ceiling_max > maxCeiling ) maxCeiling = b.ranges[i].ceiling_max;
	}
	b.zmin = minFloor - SLAB_DEPTH;
	b.zmax = maxCeiling + SLAB_DEPTH;

	// shaders used for collision only
	b.shaderSolid = FindShader( &b, "doom/solid", Q3_SURF_NODRAW, Q3_CONTENTS_SOLID );
	b.shaderNodraw = FindShader( &b, "doom/solid", Q3_SURF_NODRAW, Q3_CONTENTS_SOLID );
	b.shaderPlayerClip = FindShader( &b, "doom/clip", Q3_SURF_NODRAW, Q3_CONTENTS_PLAYERCLIP | Q3_CONTENTS_MONSTERCLIP );
	b.shaderMonsterClip = FindShader( &b, "doom/monsterclip", Q3_SURF_NODRAW, Q3_CONTENTS_MONSTERCLIP );

	// 2D BSP over every linedef
	segs = malloc( sizeof( seg_t ) * map->num_linedefs );
	for ( i = 0 ; i < map->num_linedefs ; i++ ) {
		const doom_linedef_t *ld = &map->linedefs[i];
		pt_t a = Vertex( map, ld->v1 ), c = Vertex( map, ld->v2 );

		if ( fabs( a.x - c.x ) < 0.001 && fabs( a.y - c.y ) < 0.001 ) {
			continue;
		}
		segs[nsegs].a = a;
		segs[nsegs].b = c;
		segs[nsegs].linedef = i;
		nsegs++;
	}
	cell.n = 4;
	cell.p = malloc( sizeof( pt_t ) * 4 );
	cell.p[0].x = b.bmins[0]; cell.p[0].y = b.bmins[1];
	cell.p[1].x = b.bmaxs[0]; cell.p[1].y = b.bmins[1];
	cell.p[2].x = b.bmaxs[0]; cell.p[2].y = b.bmaxs[1];
	cell.p[3].x = b.bmins[0]; cell.p[3].y = b.bmaxs[1];
	root = BuildNode( &b.bsp, segs, nsegs, &cell );
	if ( root < 0 ) {
		// a single leaf: Quake 3 needs at least one node, split it with a dummy plane
		doom_set_error( err, errlen, "map too simple" );
		goto fail;
	}
	{
		double m[2], x[2];
		NodeBounds( &b.bsp, root, m, x );
	}

	b.sectorSurfaces = calloc( map->num_sectors, sizeof( dbuf_t ) );
	for ( i = 0 ; i < map->num_sectors ; i++ ) {
		BufInit( &b.sectorSurfaces[i], sizeof( int ) );
	}
	b.leafExtraBrushes = calloc( b.bsp.leafs.count, sizeof( dbuf_t ) );
	for ( i = 0 ; i < b.bsp.leafs.count ; i++ ) {
		BufInit( &b.leafExtraBrushes[i], sizeof( int ) );
	}

	// the world (model 0) first: its surfaces and brushes must be contiguous,
	// then each mover's
	BuildFlats( &b );
	BuildWalls( &b );
	BuildClipBrushes( &b, root );
	EmitNodes( &b );
	EmitLeafs( &b );
	worldSurfaces = b.surfaces.count;
	worldBrushes = b.brushes.count;
	BuildMovers( &b );
	BakeLights( &b );
	PackSurfaces( &b );
	BuildEntities( &b );

	{
		dbuf_t		models;
		omodel_t	*m;

		BufInit( &models, sizeof( omodel_t ) );
		m = BufPush( &models );
		m->mins[0] = (float)b.bmins[0]; m->mins[1] = (float)b.bmins[1]; m->mins[2] = (float)b.zmin;
		m->maxs[0] = (float)b.bmaxs[0]; m->maxs[1] = (float)b.bmaxs[1]; m->maxs[2] = (float)b.zmax;
		m->firstSurface = 0;
		m->numSurfaces = worldSurfaces;
		m->firstBrush = 0;
		m->numBrushes = worldBrushes;

		BuildLightGrid( &b, m->mins, m->maxs, &grid );

		for ( i = 0 ; i < b.movers.count ; i++ ) {
			const mover_t *mv = BufAt( &b.movers, mover_t, i );

			m = BufPush( &models );
			memcpy( m->mins, mv->mins, sizeof( m->mins ) );
			memcpy( m->maxs, mv->maxs, sizeof( m->maxs ) );
			m->firstSurface = mv->firstSurface;
			m->numSurfaces = mv->numSurfaces;
			m->firstBrush = mv->firstBrush;
			m->numBrushes = mv->numBrushes;
		}

		lumps[0] = &b.entities;
		lumps[1] = &b.shaders;
		lumps[2] = &b.planes;
		lumps[3] = &b.nodes;
		lumps[4] = &b.leafs;
		lumps[5] = &b.leafSurfaces;
		lumps[6] = &b.leafBrushes;
		lumps[7] = &models;
		lumps[8] = &b.brushes;
		lumps[9] = &b.sides;
		lumps[10] = &b.verts;
		lumps[11] = &b.indexes;
		lumps[12] = &empty;		// fogs
		lumps[13] = &b.surfaces;
		lumps[14] = &empty;		// lightmaps
		lumps[15] = &grid;
		lumps[16] = &vis;		// none: everything is visible
		*outData = WriteBsp( lumps, outSize );
		BufFree( &models );
	}

	if ( stats ) {
		double area = 0;

		memset( stats, 0, sizeof( *stats ) );
		for ( i = 0 ; i < b.bsp.leafs.count ; i++ ) {
			const bspleaf2_t *leaf = BufAt( &b.bsp.leafs, bspleaf2_t, i );

			area += leaf->area > 0 ? leaf->area : 0;
			if ( leaf->degenerate ) {
				stats->degenerate_leafs++;
			} else if ( leaf->sector < 0 ) {
				stats->void_leafs++;
			} else {
				stats->sector_leafs++;
			}
		}
		stats->nodes = b.bsp.nodes.count;
		stats->brushes = b.brushes.count;
		stats->surfaces = b.surfaces.count;
		stats->planes = b.planes.count;
		stats->verts = b.verts.count;
		stats->movers = b.movers.count;
		stats->covered_area = area;
		stats->bounds_area = ( b.bmaxs[0] - b.bmins[0] ) * ( b.bmaxs[1] - b.bmins[1] );
	}

fail:
	for ( i = 0 ; i < b.bsp.leafs.count ; i++ ) {
		PolyFree( &BufAt( &b.bsp.leafs, bspleaf2_t, i )->poly );
	}
	if ( b.sectorSurfaces ) {
		for ( i = 0 ; i < map->num_sectors ; i++ ) {
			BufFree( &b.sectorSurfaces[i] );
		}
		free( b.sectorSurfaces );
	}
	if ( b.leafExtraBrushes ) {
		for ( i = 0 ; i < b.bsp.leafs.count ; i++ ) {
			BufFree( &b.leafExtraBrushes[i] );
		}
		free( b.leafExtraBrushes );
	}
	BufFree( &b.bsp.nodes );
	BufFree( &b.bsp.leafs );
	BufFree( &b.shaders );
	BufFree( &b.planes );
	BufFree( &b.planeNext );
	BufFree( &b.nodes );
	BufFree( &b.leafs );
	BufFree( &b.leafSurfaces );
	BufFree( &b.leafBrushes );
	BufFree( &b.brushes );
	BufFree( &b.sides );
	BufFree( &b.verts );
	BufFree( &b.indexes );
	BufFree( &b.surfaces );
	BufFree( &b.entities );
	BufFree( &b.wallBuilders );
	for ( i = 0 ; i < b.surfVerts.count ; i++ ) {
		BufFree( BufAt( &b.surfVerts, dbuf_t, i ) );	// already empty after PackSurfaces
		BufFree( BufAt( &b.surfIndexes, dbuf_t, i ) );
	}
	BufFree( &b.surfVerts );
	BufFree( &b.surfIndexes );
	BufFree( &b.movers );
	free( b.ranges );
	BufFree( &grid );
	return *outData ? 0 : -1;
}
