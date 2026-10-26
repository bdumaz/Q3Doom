/*
===========================================================================
Q3Doom - the bots' strafe-jumping (VQ3 travel movement)

On a straight, open and safe stretch of its route a bot runs up to speed,
jumps, and in the air holds forward and a strafe key with its view turned
so that the wish direction makes the angle with its velocity that gives the
most speed, as worked out from Pmove (PM_Accelerate): in the air a bot gains
accel = pm_airaccelerate * 320 * frametime when its velocity projected on
the wish direction is 320 - accel, so

	cos( angle ) = ( 320 - accel ) / speed

It turns toward the route by strafing on that side, otherwise changes side
on every jump. Both keys go full down (a part-pressed diagonal gets a lower
wish speed) and the jump key only on the frame it lands: Pmove checks the
jump before the ground friction, and a key held in the air would also lower
the wish speed.

Only where it is safe: no turn, wall, door, ledge or hazard (lava, slime,
Doom's damaging floors) on the stretch ahead, nothing to aim at, VQ3
physics. Skilled bots only (skill 3 and up), with an error on the angle for
the less skilled. bot_strafejump 0 turns it off.
===========================================================================
*/

#include "g_local.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_ea.h"
#include "be_ai_char.h"
#include "be_ai_chat.h"
#include "be_ai_gen.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"
#include "be_ai_weap.h"
//
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_strafe.h"

vmCvar_t	bot_strafejump;
extern vmCvar_t	bot_stats;
int			strafeReasons[16];	// why the stretches were turned down (printed with the bot stats)

// travel types, from botlib's aasfile.h
#define TRAVEL_WALK			2
#define TRAVELTYPE_MASK		0xFFFFFF

#define STRAFE_LOOKAHEAD	600		// how far the route must run straight
#define STRAFE_MINDIST		400		// and at least
#define STRAFE_MARGIN		64		// the zigzag's room either side
#define STRAFE_STARTSPEED	250		// the speed to jump at
#define STRAFE_VALID		0.25f	// seconds a decision lasts

void BotStrafeInit( void ) {
	trap_Cvar_Register( &bot_strafejump, "bot_strafejump", "1", 0 );
}

/*
==================
BotStrafeRecordMove

The goal the bot moves to and how (from the nodes' BotMoveToGoal)
==================
*/
void BotStrafeRecordMove( bot_state_t *bs, bot_goal_t *goal, bot_moveresult_t *result ) {
	botStrafe_t *sj = &bs->strafe;

	sj->goal = *goal;
	sj->goalTime = FloatTime();
	sj->moveFlags = result->flags;
	sj->travelType = result->traveltype;
}

// lava, slime, or a Doom floor that hurts
static qboolean HazardAt( vec3_t point ) {
	vec3_t	p;
	int		sector, special;

	VectorCopy( point, p );
	p[2] += 1;
	if ( trap_PointContents( p, -1 ) & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) {
		return qtrue;
	}
	if ( DOOM_LEVEL() ) {
		sector = doom_map_sector_at( doomLevel.map, p[0], p[1] );
		if ( sector >= 0 ) {
			special = doomLevel.special[sector];
			return special == 4 || special == 5 || special == 7 || special == 11 || special == 16;
		}
	}
	return qfalse;
}

/*
==================
StretchSafe

Room for the bot all the way to the target (no wall, closed door or step
too high), and a floor under it all the way, on the line and either side of
it, where its zigzag and its momentum may take it (no drop, no hazard)
==================
*/
static qboolean StretchSafe( bot_state_t *bs, vec3_t target ) {
	vec3_t		start, end, dir, side, p, q, down;
	vec3_t		mins = { -20, -20, 0 }, maxs = { 20, 20, 32 }, up = { 0, 0, 1 };
	bsp_trace_t	trace;
	float		dist, d;
	int			k;

	VectorCopy( bs->origin, start );
	VectorCopy( target, end );
	end[2] = start[2];
	BotAI_Trace( &trace, start, mins, maxs, end, bs->client, MASK_PLAYERSOLID );
	if ( trace.fraction < 1 || trace.startsolid ) {
		return qfalse;
	}
	VectorSubtract( end, start, dir );
	dist = VectorNormalize( dir );
	if ( !( dist <= STRAFE_LOOKAHEAD * 2 ) ) {
		return qfalse;		// not the target asked for (and not a NaN)
	}
	CrossProduct( dir, up, side );
	for ( d = 48 ; d <= dist + STRAFE_MARGIN ; d += 48 ) {
		VectorMA( start, d, dir, p );
		for ( k = -1 ; k <= 1 ; k++ ) {
			VectorMA( p, k * STRAFE_MARGIN, side, q );
			VectorCopy( q, down );
			down[2] -= 24 + 40;		// feet, and a little below
			BotAI_Trace( &trace, q, NULL, NULL, down, bs->client, MASK_PLAYERSOLID );
			if ( k && trace.startsolid ) {
				continue;			// a wall beside: nothing to fall into
			}
			if ( trace.fraction >= 1 || HazardAt( trace.endpos ) ) {
				return qfalse;
			}
		}
		// room above for a jump (46 units high): Doom's low ceilings stop it
		VectorCopy( p, down );
		down[2] += 32 + 48;
		BotAI_Trace( &trace, p, NULL, NULL, down, bs->client, MASK_PLAYERSOLID );
		if ( trace.fraction < 1 ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
==================
BotStrafeThink

Whether the stretch ahead suits strafe-jumping, and where it goes
==================
*/
void BotStrafeThink( bot_state_t *bs ) {
	botStrafe_t	*sj = &bs->strafe;
	vec3_t		target, mid, dir, dir2;

	sj->allowed = qfalse;
	trap_Cvar_Update( &bot_strafejump );
	if ( !bot_strafejump.integer || bs->settings.skill < 3 ) {
		{ strafeReasons[1]++; return; }
	}
	// travelling (walking) to a goal, nothing special about the move
	if ( sj->goalTime < FloatTime() - 0.3f || sj->goalTime > FloatTime() ) {
		{ strafeReasons[2]++; return; }
	}
	if ( ( sj->travelType & TRAVELTYPE_MASK ) != TRAVEL_WALK && sj->travelType ) {
		{ strafeReasons[3]++; return; }
	}
	if ( sj->moveFlags & ( MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW | MOVERESULT_WAITING |
		MOVERESULT_ONTOPOFOBSTACLE | MOVERESULT_ONTOPOF_FUNCBOB | MOVERESULT_ONTOPOF_ELEVATOR |
		MOVERESULT_BLOCKEDBYAVOIDSPOT ) ) {
		{ strafeReasons[4]++; return; }
	}
	// VQ3 physics, on its feet, out of the water
	if ( DOOM_LEVEL() && DOOM_CPM( &bs->cur_ps ) ) {
		{ strafeReasons[5]++; return; }
	}
	if ( ( bs->cur_ps.pm_flags & PMF_DUCKED ) || bs->cur_ps.pm_type != PM_NORMAL ) {
		{ strafeReasons[6]++; return; }
	}
	if ( trap_AAS_Swimming( bs->origin ) ) {
		{ strafeReasons[7]++; return; }
	}
	// aiming at an enemy: the view is busy
	if ( bs->enemy >= 0 && bs->aim_update_time > FloatTime() - 0.3f ) {
		{ strafeReasons[8]++; return; }
	}
	// a straight route ahead
	if ( !trap_BotMovementViewTarget( bs->ms, &sj->goal, bs->tfl, STRAFE_LOOKAHEAD, target ) ||
		!trap_BotMovementViewTarget( bs->ms, &sj->goal, bs->tfl, STRAFE_LOOKAHEAD / 2, mid ) ) {
		{ strafeReasons[9]++; return; }
	}
	VectorSubtract( target, bs->origin, dir );
	dir[2] = 0;
	if ( VectorNormalize( dir ) < STRAFE_MINDIST ) {
		{ strafeReasons[10]++; return; }
	}
	VectorSubtract( mid, bs->origin, dir2 );
	dir2[2] = 0;
	VectorNormalize( dir2 );
	if ( DotProduct( dir, dir2 ) < 0.978f ) {	// 12 degrees
		{ strafeReasons[11]++; return; }
	}
	if ( !StretchSafe( bs, target ) ) {
		{ strafeReasons[12]++; return; }
	}
	strafeReasons[0]++;
	sj->allowed = qtrue;
	sj->allowedTime = FloatTime();
	VectorCopy( target, sj->target );
}

/*
==================
BotStrafeInput

Every frame, before the bot's input becomes a usercmd: the jump, the keys
and the view of a strafe-jump. qtrue when it took over
==================
*/
qboolean BotStrafeInput( bot_state_t *bs, bot_input_t *bi, float frametime ) {
	botStrafe_t		*sj = &bs->strafe;
	playerState_t	ps;
	vec3_t			vel, todir;
	float			speed, velYaw, targetYaw, diff, accel, c, angle, wishYaw, viewYaw, step;
	int				side;

	if ( !sj->allowed || sj->allowedTime < FloatTime() - STRAFE_VALID || frametime <= 0 ) {
		sj->wasOnGround = qtrue;
		return qfalse;
	}
	// the bot wants something else from its keys
	if ( bi->actionflags & ( ACTION_JUMP | ACTION_DELAYEDJUMP | ACTION_CROUCH | ACTION_ATTACK | ACTION_MOVEUP | ACTION_MOVEDOWN ) ) {
		return qfalse;
	}
	BotAI_GetClientState( bs->client, &ps );
	VectorCopy( ps.velocity, vel );
	vel[2] = 0;
	speed = VectorLength( vel );
	VectorSubtract( sj->target, ps.origin, todir );
	todir[2] = 0;
	if ( VectorNormalize( todir ) < 64 ) {
		sj->allowed = qfalse;
		return qfalse;
	}
	targetYaw = vectoyaw( todir );
	velYaw = speed > 1 ? vectoyaw( vel ) : targetYaw;
	diff = AngleDifference( targetYaw, velYaw );
	if ( fabs( diff ) > 45 ) {
		return qfalse;		// off course: walking takes over
	}

	if ( ps.groundEntityNum != ENTITYNUM_NONE ) {
		// landed: the other side this time, and a new error on the angle
		if ( !sj->wasOnGround ) {
			sj->side = -sj->side;
			sj->noise = ( 5 - bs->settings.skill ) * 3 * crandom();
		}
		sj->wasOnGround = qtrue;
		if ( speed < STRAFE_STARTSPEED || fabs( diff ) > 15 ) {
			return qfalse;	// running up to speed, and on course, first
		}
		// jump at once, on toward the target
		bi->actionflags |= ACTION_JUMP;
		VectorCopy( todir, bi->dir );
		bi->speed = 400;
		return qtrue;
	}
	sj->wasOnGround = qfalse;
	if ( !sj->side ) {
		sj->side = 1;
	}

	// in the air: turn toward the route by strafing on its side
	side = fabs( diff ) > 2 ? ( diff > 0 ? 1 : -1 ) : sj->side;
	step = pmove_fixed.integer ? pmove_msec.integer * 0.001f : frametime;
	accel = 320 * step;		// pm_airaccelerate 1
	c = ( 320 - accel ) / ( speed > 1 ? speed : 1 );
	angle = c >= 1 ? 0 : RAD2DEG( acos( c ) );
	// off course: a wider angle turns faster, for less speed
	if ( fabs( diff ) > 8 ) {
		angle += fabs( diff ) * 0.5f;
		if ( angle > 80 ) angle = 80;
	}
	angle += sj->noise;
	if ( angle < 0 ) angle = 0;
	wishYaw = velYaw + side * angle;
	// forward + left moves 45 degrees left of the view, forward + right 45 right
	viewYaw = AngleMod( wishYaw - side * 45 );
	bi->dir[0] = cos( DEG2RAD( wishYaw ) );
	bi->dir[1] = sin( DEG2RAD( wishYaw ) );
	bi->dir[2] = 0;
	// both keys full down (127 each): at 45 degrees that is 400 * sqrt( 2 ) in
	// the bot's input; anything less makes Pmove scale the wish speed below 320
	bi->speed = 400 * 1.4143f;
	bi->viewangles[YAW] = viewYaw;
	bs->viewangles[YAW] = viewYaw;
	bs->viewanglespeed[YAW] = 0;
	if ( bot_stats.integer >= 3 ) {		// the strafe-jumps, frame by frame
		G_Printf( "SJ %d t %d speed %.0f vz %.0f diff %.1f side %d angle %.1f velYaw %.0f view %.0f ft %.3f\n", bs->client, level.time,
			speed, ps.velocity[2], diff, side, angle, velYaw, viewYaw, step );
	}
	if ( speed > sj->speedPeak ) {
		sj->speedPeak = speed;
	}
	return qtrue;
}
