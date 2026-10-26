/*
===========================================================================
Q3Doom - the bots' planner (Goal-Oriented Action Planning)

Several times a second a bot reads the world into a few facts, from what it
can see and hear: the travel time to its objective, its health, armor and
weapons, a player engaging it, a monster in its way, the damage it expects
from the monsters if it goes on, an item worth a detour.

From the facts come a world state (a few yes/no answers: safe, route clear,
rival stopped, armed, at the objective) and the goal that matters most.
The planner looks for the cheapest series of actions that reaches the goal
(travel, engage a player, clear a monster, pick up an item), each with a
cost in seconds: travel time plus the damage expected on the way.

The plan's first action steers the Quake 3 state machine through the
decisions its nodes already ask for: which enemy to fight (BotFindEnemy),
whether to fight it or move on while shooting (BotWantsToRetreat), whether
to chase (BotWantsToChase), how far to go for an item (BotNearbyGoal). The
nodes do the moving and the shooting.

Active on the Doom levels (bot_goap 1). bot_debug 1 prints the facts,
bot_plan 1 each new plan.

Also here (M19): dodging the missiles coming at a bot.
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
#include "ai_goap.h"
//
#include "chars.h"
#include "inv.h"

extern bot_state_t	*botstates[MAX_CLIENTS];

vmCvar_t	bot_goap;
vmCvar_t	bot_debug;
vmCvar_t	bot_plan;

#define FACTS_INTERVAL		0.1f	// seconds between two readings of the world
#define RESOURCE_INTERVAL	0.5f
#define REPLAN_INTERVAL		0.5f
#define MONSTER_RANGE		1500	// beyond it, monsters are left out
#define HEARING_RANGE		300		// a monster this close is heard, seen or not
#define BLOCK_AHEAD			400		// a blocking monster is at most this far ahead (1/100 s)
#define BLOCK_DETOUR		150		// and this little off the route (1/100 s)
#define DAMAGE_SECONDS		25		// losing all of one's health and armor costs this much time

// world state
#define WS_AT_OBJECTIVE		1
#define WS_RIVAL_STOPPED	2
#define WS_ROUTE_CLEAR		4
#define WS_SAFE				8
#define WS_ARMED			16
#define WS_CLEARED			32		// the worst monster is dead

static const char *goapActionNames[GA_NUM_ACTIONS] = {
	"none", "travel", "engage player", "clear monster", "pick up"
};
static const char *goapGoalNames[GG_NUM_GOALS] = {
	"none", "stay alive", "stop rival", "reach objective", "stock up"
};

void BotGoapInit( void ) {
	trap_Cvar_Register( &bot_goap, "bot_goap", "1", 0 );
	trap_Cvar_Register( &bot_debug, "bot_debug", "0", 0 );
	trap_Cvar_Register( &bot_plan, "bot_plan", "0", 0 );
}

/*
==================
BotGoapActive

The planner steers the bots on the Doom levels
==================
*/
qboolean BotGoapActive( bot_state_t *bs ) {
	trap_Cvar_Update( &bot_goap );
	return bot_goap.integer && DOOM_LEVEL();
}

// the plan steers the nodes (no plan: Quake 3's own decisions)
qboolean BotGoapSteers( bot_state_t *bs ) {
	return BotGoapActive( bs ) && bs->goap.planLength > 0;
}

int BotGoapAction( bot_state_t *bs ) {
	return BotGoapSteers( bs ) ? bs->goap.plan[0] : GA_NONE;
}

/*
=====================================================================

FACTS

=====================================================================
*/

/*
==================
MonsterDps

The damage per second a Doom monster deals to a player it attacks from
this far, from its attack (hitscan, missile or melee); more at nightmare
==================
*/
static float MonsterDps( gentity_t *ent, float dist ) {
	float	dps, range;
	int		kind;		// 0 hitscan, 1 missile, 2 melee

	switch ( ent->doomMobj->type ) {
	case MT_POSSESSED:	dps = 6; kind = 0; break;
	case MT_SHOTGUY:	dps = 12; kind = 0; break;
	case MT_CHAINGUY:	dps = 22; kind = 0; break;
	case MT_TROOP:		dps = 7; kind = 1; break;
	case MT_SERGEANT:
	case MT_SHADOWS:	dps = 14; kind = 2; break;
	case MT_SKULL:		dps = 6; kind = 2; break;
	case MT_HEAD:		dps = 11; kind = 1; break;
	case MT_BRUISER:	dps = 18; kind = 1; break;
	case MT_SPIDER:		dps = 60; kind = 0; break;
	case MT_CYBORG:		dps = 45; kind = 1; break;
	default:			dps = 5; kind = 1; break;
	}
	if ( g_doomSkill.integer >= DOOM_SK_NIGHTMARE ) {
		dps *= 1.4f;
	}
	// hitscan spreads with distance, missiles can be dodged from afar, and
	// a melee monster first has to come close
	if ( kind == 0 ) {
		range = 300 / ( dist + 1 );
		dps *= range > 1 ? 1 : range < 0.25f ? 0.25f : range;
	} else if ( kind == 1 ) {
		range = 500 / ( dist + 1 );
		dps *= range > 1 ? 1 : range < 0.35f ? 0.35f : range;
	} else {
		range = 500 / ( dist + 1 );
		dps *= range > 1 ? 1 : range < 0.2f ? 0.2f : range;
	}
	return dps;
}

static qboolean MonsterIsMelee( gentity_t *ent ) {
	int type = ent->doomMobj->type;

	return type == MT_SERGEANT || type == MT_SHADOWS || type == MT_SKULL;
}

/*
==================
BotWeaponPower

The best weapon the bot holds with ammo: how strong (0 to 1) and how much
damage per second it deals in the hands of a bot
==================
*/
static float BotWeaponPower( bot_state_t *bs, float *dps ) {
	int *inv = bs->inventory;

	if ( inv[INVENTORY_BFG10K] > 0 && inv[INVENTORY_BFGAMMO] > 7 ) {
		*dps = 120; return 1;
	}
	if ( inv[INVENTORY_ROCKETLAUNCHER] > 0 && inv[INVENTORY_ROCKETS] > 5 ) {
		*dps = 80; return 0.95f;
	}
	if ( inv[INVENTORY_LIGHTNING] > 0 && inv[INVENTORY_LIGHTNINGAMMO] > 50 ) {
		*dps = 70; return 0.9f;
	}
	if ( inv[INVENTORY_PLASMAGUN] > 0 && inv[INVENTORY_CELLS] > 40 ) {
		*dps = 70; return 0.85f;
	}
	if ( inv[INVENTORY_RAILGUN] > 0 && inv[INVENTORY_SLUGS] > 5 ) {
		*dps = 55; return 0.85f;
	}
	if ( inv[INVENTORY_SHOTGUN] > 0 && inv[INVENTORY_SHELLS] > 10 ) {
		*dps = 55; return 0.6f;
	}
	if ( inv[INVENTORY_GRENADELAUNCHER] > 0 && inv[INVENTORY_GRENADES] > 10 ) {
		*dps = 40; return 0.55f;
	}
	if ( inv[INVENTORY_MACHINEGUN] > 0 && inv[INVENTORY_BULLETS] > 20 ) {
		*dps = 35; return 0.35f;
	}
	*dps = 20;
	return 0.1f;
}

// seconds of travel from the bot to an area, -1 when it can't get there
static float TravelTime( bot_state_t *bs, int area ) {
	int t;

	if ( !area || !bs->areanum ) {
		return -1;
	}
	t = trap_AAS_AreaTravelTimeToGoalArea( bs->areanum, bs->origin, area, TFL_DEFAULT );
	return t > 0 ? t * 0.01f : ( area == bs->areanum ? 0 : -1 );
}

// a clear line from the bot's eye to a point, or to that entity
static qboolean LineOfSight( bot_state_t *bs, vec3_t point, int ent ) {
	bsp_trace_t trace;

	BotAI_Trace( &trace, bs->eye, NULL, NULL, point, bs->client, CONTENTS_SOLID );
	return trace.fraction >= 1 || trace.ent == ent;
}

/*
==================
FactsMonsters

The monsters the bot sees or hears: the one in its way, the damage it
expects from them all if it goes on, and the worst of them
==================
*/
static void FactsMonsters( bot_state_t *bs, botFacts_t *f ) {
	int			i, area, toMonster, monsterToGoal, meToGoal = 0;
	float		dist, dps, exposure, cost, ahead;
	vec3_t		centre, dir;
	gentity_t	*ent;
	qboolean	visible, heard, blocking;

	f->monsterBlocking = f->worstMonster = -1;
	f->monsterCost = f->worstMonsterCost = f->monsterDps = 0;
	f->blockPenalty = f->blockingDps = f->blockingTtk = f->worstDps = f->worstTtk = 0;
	if ( f->hasObjective ) {
		meToGoal = trap_AAS_AreaTravelTimeToGoalArea( bs->areanum, bs->origin, f->objectiveArea, TFL_DEFAULT );
	}
	ahead = BLOCK_AHEAD;
	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse || !ent->doomMobj || ent->health <= 0 || !( ent->doomMobj->flags & MF_COUNTKILL ) ) {
			continue;
		}
		VectorCopy( ent->r.currentOrigin, centre );
		centre[2] += ( ent->r.mins[2] + ent->r.maxs[2] ) * 0.5f;
		VectorSubtract( centre, bs->eye, dir );
		dist = VectorLength( dir );
		if ( dist > MONSTER_RANGE ) {
			continue;
		}
		visible = LineOfSight( bs, centre, i );
		heard = dist < HEARING_RANGE || ( bs->goap.hurtBy == i && bs->goap.hurtTime > FloatTime() - 2 );
		if ( !visible && !heard ) {
			continue;
		}
		dps = MonsterDps( ent, dist );
		if ( !ent->doomMobj->target ) {
			dps *= 0.5f;	// asleep, for now
		}
		if ( !visible ) {
			dps *= 0.5f;	// behind something: it has to come out first
		}
		f->monsterDps += dps;

		// on the route: little out of the way, and soon
		blocking = qfalse;
		toMonster = -1;
		if ( f->hasObjective && meToGoal > 0 ) {
			area = BotPointAreaNum( centre );
			if ( area ) {
				toMonster = trap_AAS_AreaTravelTimeToGoalArea( bs->areanum, bs->origin, area, TFL_DEFAULT );
				monsterToGoal = trap_AAS_AreaTravelTimeToGoalArea( area, centre, f->objectiveArea, TFL_DEFAULT );
				if ( area == bs->areanum ) {
					toMonster = 1;
				}
				if ( toMonster > 0 && monsterToGoal > 0 && toMonster < ahead &&
					toMonster + monsterToGoal - meToGoal < BLOCK_DETOUR ) {
					blocking = qtrue;
				}
			}
		}
		// how long it will have the bot in sight if the bot goes on
		if ( blocking ) {
			exposure = toMonster * 0.01f + 1;
		} else if ( f->hasObjective ) {
			exposure = f->objectiveTime < 3 ? f->objectiveTime : 3;
		} else {
			exposure = 2.5f;
		}
		cost = dps * exposure;
		f->monsterCost += cost;
		if ( cost > f->worstMonsterCost ) {
			f->worstMonsterCost = cost;
			f->worstMonster = i;
			f->worstDps = dps;
			f->worstTtk = ent->health / ( f->dps > 10 ? f->dps : 10 ) + 0.5f;
		}
		if ( blocking ) {
			ahead = toMonster;		// the nearest one in the way
			f->monsterBlocking = i;
			f->blockingDps = dps;
			f->blockingTtk = ent->health / ( f->dps > 10 ? f->dps : 10 ) + 0.5f;
			// a melee monster stands in the way; a shooter can be run past
			f->blockPenalty = MonsterIsMelee( ent ) ? 5 : 2;
		}
	}
}

/*
==================
FactsPlayers

The enemy players the bot sees: the nearest, and one engaging it (shooting
at it or hurting it), or, in Doom Duel, close to its own objective
==================
*/
static void FactsPlayers( bot_state_t *bs, botFacts_t *f ) {
	int					i, area;
	float				dist, bestdist = 1e9f, t;
	vec3_t				dir, angles, goal;
	aas_entityinfo_t	entinfo;

	f->playerThreat = f->visiblePlayer = -1;
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( i == bs->client ) {
			continue;
		}
		BotEntityInfo( i, &entinfo );
		if ( !entinfo.valid || EntityIsDead( &entinfo ) || BotSameTeam( bs, i ) ) {
			continue;
		}
		if ( g_entities[i].client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( BotEntityVisible( bs->entitynum, bs->eye, bs->viewangles, 360, i ) <= 0 ) {
			continue;
		}
		VectorSubtract( entinfo.origin, bs->origin, dir );
		dist = VectorLength( dir );
		if ( dist < bestdist ) {
			bestdist = dist;
			f->visiblePlayer = i;
		}
		// shooting my way, or hurting me
		VectorNegate( dir, dir );
		vectoangles( dir, angles );
		if ( ( bs->goap.hurtBy == i && bs->goap.hurtTime > FloatTime() - 2 ) ||
			( EntityIsShooting( &entinfo ) && dist < 1500 && InFieldOfVision( entinfo.angles, 30, angles ) ) ) {
			f->playerThreat = i;
			continue;
		}
		// in a race: about to reach its objective
		if ( Doom_DuelGoal( &g_entities[i], goal ) ) {
			area = BotPointAreaNum( goal );
			if ( area ) {
				t = trap_AAS_AreaTravelTimeToGoalArea( BotPointAreaNum( entinfo.origin ), entinfo.origin, area, TFL_DEFAULT );
				if ( t > 0 && t < 400 ) {
					f->playerThreat = i;
				}
			}
		}
	}
}

/*
==================
FactsResource

An item worth a detour, how long the detour is and how much the bot needs
what it gives. The item chooser pushes the item on the goal stack: it is
popped back off, this is only a question
==================
*/
static void FactsResource( bot_state_t *bs, botFacts_t *f ) {
	float		need, wneed, range, toItem, itemToGoal;
	bot_goal_t	goal;
	gentity_t	*ent;
	int			area;

	f->resourceType = 0;
	f->resourceTime = f->resourceNeed = 0;
	need = ( 100 - f->health ) / 80.0f;
	if ( need < 0 ) need = 0;
	if ( 0.5f * ( 100 - f->armor ) / 100.0f > need ) need = 0.5f * ( 100 - f->armor ) / 100.0f;
	wneed = 1 - f->weaponPower;
	if ( wneed > need ) need = wneed;
	if ( need > 1 ) need = 1;
	if ( need < 0.1f ) {
		return;
	}
	range = 100 + need * 400;		// 1/100 s
	if ( !trap_BotChooseNBGItem( bs->gs, bs->origin, bs->inventory, TFL_DEFAULT, NULL, range ) ) {
		return;
	}
	trap_BotGetTopGoal( bs->gs, &goal );
	trap_BotPopGoal( bs->gs );
	if ( goal.entitynum <= 0 || goal.entitynum >= MAX_GENTITIES ) {
		return;
	}
	ent = &g_entities[goal.entitynum];
	if ( !ent->item ) {
		return;
	}
	toItem = TravelTime( bs, goal.areanum );
	if ( toItem < 0 ) {
		return;
	}
	// no item down a one-way drop the bot can't come back from
	if ( goal.areanum != bs->areanum &&
		trap_AAS_AreaTravelTimeToGoalArea( goal.areanum, goal.origin, bs->areanum, TFL_DEFAULT ) <= 0 ) {
		return;
	}
	f->resourceGoal = goal;
	f->resourceType = ent->item->giType;
	f->resourceTime = toItem;
	if ( f->hasObjective && f->objectiveTime > 0 ) {
		area = goal.areanum;
		itemToGoal = trap_AAS_AreaTravelTimeToGoalArea( area, goal.origin, f->objectiveArea, TFL_DEFAULT ) * 0.01f;
		f->resourceTime = toItem + itemToGoal - f->objectiveTime;
		if ( f->resourceTime < 0.1f ) {
			f->resourceTime = 0.1f;
		}
	}
	switch ( f->resourceType ) {
	case IT_HEALTH:		f->resourceNeed = ( 100 - f->health ) / 80.0f; break;
	case IT_ARMOR:		f->resourceNeed = 0.5f * ( 100 - f->armor ) / 100.0f; break;
	case IT_WEAPON:
	case IT_AMMO:		f->resourceNeed = wneed; break;
	default:			f->resourceNeed = 0.2f; break;
	}
	if ( f->resourceNeed < 0.05f ) {
		f->resourceType = 0;
	}
}

/*
==================
BotGoapFacts
==================
*/
static void BotGoapFacts( bot_state_t *bs ) {
	botFacts_t	*f = &bs->goap.facts;
	vec3_t		goal;
	qboolean	resource = ( (int)( f->time / RESOURCE_INTERVAL ) != (int)( FloatTime() / RESOURCE_INTERVAL ) );

	f->time = FloatTime();
	f->health = bs->inventory[INVENTORY_HEALTH];
	f->armor = bs->inventory[INVENTORY_ARMOR];
	f->weaponPower = BotWeaponPower( bs, &f->dps );
	f->hasObjective = qfalse;
	f->objectiveTime = 0;
	if ( Doom_DuelGoal( &g_entities[bs->client], goal ) ) {
		f->objectiveArea = BotPointAreaNum( goal );
		f->objectiveTime = TravelTime( bs, f->objectiveArea );
		f->hasObjective = f->objectiveTime >= 0;
	}
	FactsMonsters( bs, f );
	FactsPlayers( bs, f );
	if ( resource ) {
		FactsResource( bs, f );
	}
}

/*
=====================================================================

THE PLANNER

=====================================================================
*/

static void BotGoapCheckTrapped( bot_state_t *bs );

// seconds that losing this much health and armor is worth
static float DamageCost( botFacts_t *f, float damage ) {
	float hp = f->health + f->armor;

	return damage / ( hp > 25 ? hp : 25 ) * DAMAGE_SECONDS;
}

static int WorldState( bot_state_t *bs ) {
	botFacts_t	*f = &bs->goap.facts;
	int			state = 0;

	if ( !f->hasObjective ) state |= WS_AT_OBJECTIVE;
	if ( f->playerThreat < 0 && ( Doom_DuelActive() || f->visiblePlayer < 0 ) ) state |= WS_RIVAL_STOPPED;
	if ( f->monsterBlocking < 0 ) state |= WS_ROUTE_CLEAR;
	if ( f->health >= 35 && f->monsterCost <= 0.5f * ( f->health + f->armor ) ) state |= WS_SAFE;
	if ( f->weaponPower >= 0.55f ) state |= WS_ARMED;
	if ( f->worstMonster < 0 ) state |= WS_CLEARED;
	return state;
}

// the player to fight: one engaging the bot, or in deathmatch any in sight
static int RivalTarget( bot_state_t *bs ) {
	botFacts_t *f = &bs->goap.facts;

	if ( f->playerThreat >= 0 ) return f->playerThreat;
	if ( !Doom_DuelActive() ) return f->visiblePlayer;
	return -1;
}

static int MonsterTarget( bot_state_t *bs ) {
	botFacts_t *f = &bs->goap.facts;

	return f->monsterBlocking >= 0 ? f->monsterBlocking : f->worstMonster;
}

/*
==================
ActionApply

Whether an action can be done from a state; if so the state after it and
its cost
==================
*/
static qboolean ActionApply( bot_state_t *bs, int action, int state, int *next, float *cost ) {
	botFacts_t	*f = &bs->goap.facts;
	float		damage, ttk, others;

	*next = state;
	switch ( action ) {
	case GA_TRAVEL:
		if ( !f->hasObjective || ( state & WS_AT_OBJECTIVE ) ) return qfalse;
		*next |= WS_AT_OBJECTIVE;
		damage = f->monsterCost - ( ( state & WS_CLEARED ) ? f->worstMonsterCost : 0 );
		if ( !( state & WS_RIVAL_STOPPED ) ) {
			damage += 40 * ( f->objectiveTime < 3 ? f->objectiveTime : 3 ) * 0.5f;
		}
		*cost = f->objectiveTime + ( ( state & WS_ROUTE_CLEAR ) ? 0 : f->blockPenalty ) + DamageCost( f, damage );
		return qtrue;
	case GA_ENGAGE_PLAYER:
		if ( RivalTarget( bs ) < 0 || ( state & WS_RIVAL_STOPPED ) ) return qfalse;
		*next |= WS_RIVAL_STOPPED;
		ttk = 125 / ( f->dps > 10 ? f->dps : 10 ) + 0.5f;
		*cost = ttk + DamageCost( f, ( 40 + f->monsterDps * 0.5f ) * ttk );
		return qtrue;
	case GA_CLEAR_MONSTER:
		if ( MonsterTarget( bs ) < 0 || ( ( state & WS_ROUTE_CLEAR ) && ( state & WS_CLEARED ) ) ) return qfalse;
		*next |= WS_ROUTE_CLEAR | WS_CLEARED;
		if ( f->monsterBlocking >= 0 ) {
			ttk = f->blockingTtk;
			others = f->monsterDps - f->blockingDps;
			damage = ( f->blockingDps + others * 0.5f ) * ttk;
		} else {
			ttk = f->worstTtk;
			others = f->monsterDps - f->worstDps;
			damage = ( f->worstDps + others * 0.5f ) * ttk;
		}
		if ( f->health >= 35 && f->monsterCost - f->worstMonsterCost <= 0.5f * ( f->health + f->armor ) ) {
			*next |= WS_SAFE;
		}
		*cost = ttk + DamageCost( f, damage );
		return qtrue;
	case GA_PICKUP:
		if ( !f->resourceType ) return qfalse;
		if ( f->resourceType == IT_HEALTH || f->resourceType == IT_ARMOR ) {
			if ( state & WS_SAFE ) return qfalse;
			*next |= WS_SAFE;
		} else {
			if ( state & WS_ARMED ) return qfalse;
			*next |= WS_ARMED;
		}
		*cost = f->resourceTime + DamageCost( f, f->monsterDps * f->resourceTime * 0.5f );
		return qtrue;
	}
	return qfalse;
}

static int GoalBit( int goal ) {
	switch ( goal ) {
	case GG_STAY_ALIVE:			return WS_SAFE;
	case GG_STOP_RIVAL:			return WS_RIVAL_STOPPED;
	case GG_REACH_OBJECTIVE:	return WS_AT_OBJECTIVE;
	case GG_STOCK_UP:			return WS_ARMED;
	}
	return 0;
}

/*
==================
GoalInsistence

How much each goal matters now, 0 when it is met. In Doom Duel the race
comes first unless a fight is on top of the bot; in deathmatch, players in
sight and better weapons
==================
*/
static float GoalInsistence( bot_state_t *bs, int goal, int state ) {
	botFacts_t	*f = &bs->goap.facts;
	qboolean	duel = Doom_DuelActive();

	if ( state & GoalBit( goal ) ) {
		return 0;
	}
	switch ( goal ) {
	case GG_STAY_ALIVE:
		return 90;
	case GG_STOP_RIVAL:
		return duel ? 80 : 70;
	case GG_REACH_OBJECTIVE:
		// almost there: nothing else matters
		return f->objectiveTime < 2 ? 100 : 60;
	case GG_STOCK_UP:
		return duel ? 20 : 50;
	}
	return 0;
}

typedef struct {
	int		state;
	float	cost;
	int		plan[GOAP_MAX_PLAN];
	int		length;
} planNode_t;

#define MAX_PLAN_NODES	256

/*
==================
Plan

The cheapest series of at most GOAP_MAX_PLAN actions from the state to one
where the goal is met (a uniform cost search: the states are few)
==================
*/
static qboolean Plan( bot_state_t *bs, int state, int goal, planNode_t *best ) {
	static planNode_t	open[MAX_PLAN_NODES];
	int					numOpen = 1, i, b, action, next, want = GoalBit( goal );
	float				cost;
	planNode_t			node;

	open[0].state = state;
	open[0].cost = 0;
	open[0].length = 0;
	while ( numOpen > 0 ) {
		b = 0;
		for ( i = 1 ; i < numOpen ; i++ ) {
			if ( open[i].cost < open[b].cost ) b = i;
		}
		node = open[b];
		open[b] = open[--numOpen];
		if ( node.state & want ) {
			*best = node;
			return node.length > 0;
		}
		if ( node.length >= GOAP_MAX_PLAN ) {
			continue;
		}
		for ( action = GA_TRAVEL ; action < GA_NUM_ACTIONS ; action++ ) {
			if ( !ActionApply( bs, action, node.state, &next, &cost ) || next == node.state ) {
				continue;
			}
			if ( numOpen >= MAX_PLAN_NODES ) {
				break;
			}
			open[numOpen] = node;
			open[numOpen].state = next;
			open[numOpen].cost = node.cost + cost;
			open[numOpen].plan[node.length] = action;
			open[numOpen].length = node.length + 1;
			numOpen++;
		}
	}
	return qfalse;
}

/*
==================
ActionTarget

What the first action shoots at: the player or monster it is about, or
while travelling, what threatens most
==================
*/
static int ActionTarget( bot_state_t *bs, int action ) {
	botFacts_t *f = &bs->goap.facts;

	switch ( action ) {
	case GA_ENGAGE_PLAYER:
		return RivalTarget( bs );
	case GA_CLEAR_MONSTER:
		return MonsterTarget( bs );
	}
	if ( f->playerThreat >= 0 ) return f->playerThreat;
	if ( f->monsterBlocking >= 0 ) return f->monsterBlocking;
	if ( f->worstMonster >= 0 && LineOfSight( bs, g_entities[f->worstMonster].r.currentOrigin, f->worstMonster ) ) {
		return f->worstMonster;
	}
	return -1;
}

static void PrintPlan( bot_state_t *bs ) {
	char	name[MAX_NETNAME], buf[256];
	int		i;

	ClientName( bs->client, name, sizeof( name ) );
	buf[0] = 0;
	for ( i = 0 ; i < bs->goap.planLength ; i++ ) {
		Q_strcat( buf, sizeof( buf ), va( "%s%s", i ? ", " : "", goapActionNames[bs->goap.plan[i]] ) );
	}
	BotAI_Print( PRT_MESSAGE, "%s %1.1f: goal %s: plan %s (cost %1.1f s), target %d\n", name, FloatTime(),
		goapGoalNames[bs->goap.goal], bs->goap.planLength ? buf : "none", bs->goap.planCost, bs->goap.target );
}

static void PrintFacts( bot_state_t *bs ) {
	botFacts_t	*f = &bs->goap.facts;
	char		name[MAX_NETNAME];

	ClientName( bs->client, name, sizeof( name ) );
	BotAI_Print( PRT_MESSAGE, "%s %1.1f facts: objective %s %1.1f s; health %d armor %d weapon %1.2f; "
		"playerThreat %d visiblePlayer %d; monsterBlocking %d (%1.0f s); monsterCost %1.0f (worst %d %1.0f); "
		"resource type %d %1.1f s need %1.2f\n", name, FloatTime(),
		f->hasObjective ? "yes" : "no", f->objectiveTime, f->health, f->armor, f->weaponPower,
		f->playerThreat, f->visiblePlayer, f->monsterBlocking, f->blockPenalty, f->monsterCost,
		f->worstMonster, f->worstMonsterCost, f->resourceType, f->resourceTime, f->resourceNeed );
}

/*
==================
BotGoapUpdate

Read the world and, when it changed, plan again
==================
*/
void BotGoapUpdate( bot_state_t *bs ) {
	botGoap_t	*g = &bs->goap;
	int			state, goal, oldfirst, oldtarget;
	float		insistence, best, score;
	planNode_t	plan;

	if ( !BotGoapActive( bs ) || BotIsDead( bs ) || BotIsObserver( bs ) || BotIntermission( bs ) ) {
		g->planLength = 0;
		g->target = -1;
		return;
	}
	if ( g->facts.time > FloatTime() - FACTS_INTERVAL && g->facts.time <= FloatTime() ) {
		return;
	}
	BotGoapFacts( bs );
	BotGoapCheckTrapped( bs );
	state = WorldState( bs );
	trap_Cvar_Update( &bot_debug );
	if ( bot_debug.integer && ( g->debugTime < FloatTime() - 0.5f || g->debugTime > FloatTime() ) ) {
		g->debugTime = FloatTime();
		PrintFacts( bs );
	}
	if ( state == g->lastState && g->replanTime > FloatTime() && g->planLength > 0 ) {
		g->target = ActionTarget( bs, g->plan[0] );
		return;
	}
	g->lastState = state;
	g->replanTime = FloatTime() + REPLAN_INTERVAL;
	oldfirst = g->planLength ? g->plan[0] : GA_NONE;
	oldtarget = g->target;

	// the goal worth most: how much it matters, less what its plan costs
	// beyond the unavoidable (the bare travel time, to reach the objective)
	g->planLength = 0;
	g->goal = GG_NONE;
	best = 0;
	for ( goal = 1 ; goal < GG_NUM_GOALS ; goal++ ) {
		insistence = GoalInsistence( bs, goal, state );
		if ( insistence <= 0 || !Plan( bs, state, goal, &plan ) ) {
			continue;
		}
		score = insistence - plan.cost;
		if ( goal == GG_REACH_OBJECTIVE ) {
			score += g->facts.objectiveTime;
		}
		if ( score > best ) {
			best = score;
			g->goal = goal;
			memcpy( g->plan, plan.plan, sizeof( g->plan ) );
			g->planLength = plan.length;
			g->planCost = plan.cost;
		}
	}
	g->target = g->planLength ? ActionTarget( bs, g->plan[0] ) : -1;
	trap_Cvar_Update( &bot_plan );
	if ( bot_plan.integer && ( ( g->planLength ? g->plan[0] : GA_NONE ) != oldfirst || g->target != oldtarget ) ) {
		PrintPlan( bs );
	}
}

/*
=====================================================================

STEERING THE NODES

=====================================================================
*/

/*
==================
BotGoapFindEnemy

The enemy the plan wants: qtrue when the bot's enemy changed
==================
*/
qboolean BotGoapFindEnemy( bot_state_t *bs, int curenemy ) {
	int t = bs->goap.target;

	if ( t < 0 || t == curenemy ) {
		return qfalse;
	}
	if ( t < MAX_CLIENTS ) {
		aas_entityinfo_t entinfo;

		BotEntityInfo( t, &entinfo );
		if ( !entinfo.valid || EntityIsDead( &entinfo ) ) return qfalse;
	} else if ( !g_entities[t].inuse || g_entities[t].health <= 0 ) {
		return qfalse;
	}
	// only what is in sight: the fight nodes drop an enemy out of sight, and
	// would go round in circles with one that is only heard
	if ( BotEntityVisible( bs->entitynum, bs->eye, bs->viewangles, 360, t ) <= 0 ) {
		return qfalse;
	}
	bs->enemy = t;
	bs->enemysight_time = FloatTime();
	bs->enemysuicide = qfalse;
	bs->enemydeath_time = 0;
	bs->enemyvisible_time = FloatTime();
	return qtrue;
}

// fight the enemy (engage, clear), or move on while shooting
qboolean BotGoapWantsToRetreat( bot_state_t *bs ) {
	int a = bs->goap.plan[0];

	return a != GA_ENGAGE_PLAYER && a != GA_CLEAR_MONSTER;
}

qboolean BotGoapWantsToChase( bot_state_t *bs ) {
	return bs->goap.plan[0] == GA_ENGAGE_PLAYER;
}

/*
==================
BotGoapPickUp

The plan's item, pushed on the goal stack for the nodes to go and get it:
qfalse when the plan is not a pick-up
==================
*/
qboolean BotGoapPickUp( bot_state_t *bs, bot_goal_t *goal ) {
	if ( bs->goap.plan[0] != GA_PICKUP || !bs->goap.facts.resourceType ) {
		return qfalse;
	}
	*goal = bs->goap.facts.resourceGoal;
	trap_BotPushGoal( bs->gs, goal );
	return qtrue;
}

/*
==================
BotGoapItemRange

How far (1/100 s of travel) the bot goes for an item it finds by itself:
in a race only for what is on the way (a planned detour is BotGoapPickUp)
==================
*/
float BotGoapItemRange( bot_state_t *bs, float range ) {
	if ( bs->goap.facts.hasObjective ) {
		return range < 50 ? range : 50;
	}
	return range;
}

/*
==================
BotGoapTopGoalIsTrap

The item just chosen (on top of the goal stack) is down a one-way drop the
bot couldn't come back from (E1M1's walled-in yard): popped, and avoided
for the rest of the level
==================
*/
qboolean BotGoapTopGoalIsTrap( bot_state_t *bs ) {
	bot_goal_t goal;

	if ( !BotGoapActive( bs ) || !trap_BotGetTopGoal( bs->gs, &goal ) ) {
		return qfalse;
	}
	if ( !goal.areanum || !bs->areanum || goal.areanum == bs->areanum ||
		trap_AAS_AreaTravelTimeToGoalArea( goal.areanum, goal.origin, bs->areanum, TFL_DEFAULT ) > 0 ) {
		return qfalse;
	}
	trap_BotPopGoal( bs->gs );
	trap_BotSetAvoidGoalTime( bs->gs, goal.number, 3600 );
	return qtrue;
}

/*
==================
BotGoapCheckTrapped

A bot that has fallen where it can't get out of (no route to any spawn
point) does what a player would: after a while, it kills itself
==================
*/
static void BotGoapCheckTrapped( bot_state_t *bs ) {
	gentity_t	*spot = NULL;
	vec3_t		origin;
	int			area;

	if ( bs->goap.trapCheckTime > FloatTime() && bs->goap.trapCheckTime < FloatTime() + 5 ) {
		return;
	}
	bs->goap.trapCheckTime = FloatTime() + 2;
	if ( !bs->areanum ) {
		return;
	}
	while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != NULL ) {
		VectorCopy( spot->s.origin, origin );
		area = BotPointAreaNum( origin );
		if ( area == bs->areanum || ( area && trap_AAS_AreaTravelTimeToGoalArea( bs->areanum, bs->origin, area, TFL_DEFAULT ) > 0 ) ) {
			bs->goap.trappedTime = 0;
			return;
		}
	}
	if ( !bs->goap.trappedTime || bs->goap.trappedTime > FloatTime() ) {
		bs->goap.trappedTime = FloatTime();
	} else if ( bs->goap.trappedTime < FloatTime() - 8 ) {
		bs->goap.trappedTime = 0;
		trap_EA_Command( bs->client, "kill" );
	}
}

/*
==================
BotGoapDamaged

A bot was hurt: by whom, for the facts
==================
*/
void BotGoapDamaged( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker ) {
	bot_state_t *bs;

	if ( !targ->client || !( targ->r.svFlags & SVF_BOT ) ) {
		return;
	}
	bs = botstates[targ - g_entities];
	if ( !bs || !bs->inuse ) {
		return;
	}
	if ( attacker && attacker != targ && attacker->s.number != ENTITYNUM_WORLD ) {
		bs->goap.hurtBy = attacker->s.number;
		bs->goap.hurtTime = FloatTime();
	}
	if ( inflictor && inflictor->s.eType == ET_MISSILE ) {
		BotStats_MissileHit( targ - g_entities );
	}
}

/*
=====================================================================

DODGING (M19)

=====================================================================
*/

/*
==================
BotGoapDodge

A missile (a Doom fireball, a rocket, plasma) that will pass close to the
bot within a second: the direction to step aside, square to its flight and
away from its line. A bot sees it coming only in front of it, and the less
skilled, the later
==================
*/
qboolean BotGoapDodge( bot_state_t *bs, vec3_t dodgedir ) {
	botGoap_t	*g = &bs->goap;
	int			i;
	float		speed, t, miss, best = 1e9f, skill;
	vec3_t		vel, rel, closest, side, up = { 0, 0, 1 }, angles;
	gentity_t	*ent, *threat = NULL;

	if ( g->dodgeTime > FloatTime() ) {
		VectorCopy( g->dodgeDir, dodgedir );
		return qtrue;
	}
	skill = trap_Characteristic_BFloat( bs->character, CHARACTERISTIC_ATTACK_SKILL, 0, 1 );
	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse || ent->s.eType != ET_MISSILE || ent->s.pos.trType != TR_LINEAR ) {
			continue;
		}
		if ( ent->r.ownerNum == bs->client || ( ent->parent && ent->parent->client && BotSameTeam( bs, ent->parent->s.number ) ) ) {
			continue;
		}
		VectorCopy( ent->s.pos.trDelta, vel );
		speed = VectorNormalize( vel );
		if ( speed < 1 ) {
			continue;
		}
		VectorSubtract( bs->origin, ent->r.currentOrigin, rel );
		t = DotProduct( rel, vel ) / speed;		// seconds to the closest point
		if ( t < 0 || t > 0.4f + skill * 0.6f ) {
			continue;
		}
		VectorMA( ent->r.currentOrigin, t * speed, vel, closest );
		VectorSubtract( bs->origin, closest, rel );
		miss = VectorLength( rel );
		if ( miss > 56 || miss >= best ) {
			continue;
		}
		// seen coming: within the bot's view
		VectorSubtract( ent->r.currentOrigin, bs->eye, rel );
		vectoangles( rel, angles );
		if ( !InFieldOfVision( bs->viewangles, 120, angles ) ) {
			continue;
		}
		best = miss;
		threat = ent;
		VectorSubtract( bs->origin, closest, side );
	}
	if ( !threat ) {
		return qfalse;
	}
	// a new missile: counted for the statistics, and a less skilled bot may miss it
	if ( threat->s.number != g->dodgeMissile ) {
		g->dodgeMissile = threat->s.number;
		g->dodgeSkip = random() > 0.4f + skill * 0.6f;
		BotStats_MissileThreat( bs->client );
	}
	if ( g->dodgeSkip ) {
		return qfalse;
	}
	side[2] = 0;
	if ( VectorNormalize( side ) < 0.1f ) {
		// dead on: either side of its flight
		VectorCopy( threat->s.pos.trDelta, vel );
		vel[2] = 0;
		CrossProduct( vel, up, side );
		if ( random() < 0.5f ) VectorNegate( side, side );
		VectorNormalize( side );
	}
	VectorCopy( side, g->dodgeDir );
	g->dodgeTime = FloatTime() + 0.35f;
	VectorCopy( side, dodgedir );
	return qtrue;
}
