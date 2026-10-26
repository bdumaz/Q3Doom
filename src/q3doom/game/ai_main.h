/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//

/*****************************************************************************
 * name:		ai_main.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_chat.c $
 *
 *****************************************************************************/

//#define DEBUG
#define CTF

#define MAX_ITEMS					256
//bot flags
#define BFL_STRAFERIGHT				1	//strafe to the right
#define BFL_ATTACKED				2	//bot has attacked last ai frame
#define BFL_ATTACKJUMPED			4	//bot jumped during attack last frame
#define BFL_AIMATENEMY				8	//bot aimed at the enemy this frame
#define BFL_AVOIDRIGHT				16	//avoid obstacles by going to the right
#define BFL_IDEALVIEWSET			32	//bot has ideal view angles set
#define BFL_FIGHTSUICIDAL			64	//bot is in a suicidal fight
//long term goal types
#define LTG_TEAMHELP				1	//help a team mate
#define LTG_TEAMACCOMPANY			2	//accompany a team mate
#define LTG_DEFENDKEYAREA			3	//defend a key area
#define LTG_GETFLAG					4	//get the enemy flag
#define LTG_RUSHBASE				5	//rush to the base
#define LTG_RETURNFLAG				6	//return the flag
#define LTG_CAMP					7	//camp somewhere
#define LTG_CAMPORDER				8	//ordered to camp somewhere
#define LTG_PATROL					9	//patrol
#define LTG_GETITEM					10	//get an item
#define LTG_KILL					11	//kill someone
#define LTG_HARVEST					12	//harvest skulls
#define LTG_ATTACKENEMYBASE			13	//attack the enemy base
#define LTG_MAKELOVE_UNDER			14
#define LTG_MAKELOVE_ONTOP			15
//some goal dedication times
#define TEAM_HELP_TIME				60	//1 minute teamplay help time
#define TEAM_ACCOMPANY_TIME			600	//10 minutes teamplay accompany time
#define TEAM_DEFENDKEYAREA_TIME		600	//10 minutes ctf defend base time
#define TEAM_CAMP_TIME				600	//10 minutes camping time
#define TEAM_PATROL_TIME			600	//10 minutes patrolling time
#define TEAM_LEAD_TIME				600	//10 minutes taking the lead
#define TEAM_GETITEM_TIME			60	//1 minute
#define	TEAM_KILL_SOMEONE			180	//3 minute to kill someone
#define TEAM_ATTACKENEMYBASE_TIME	600	//10 minutes
#define TEAM_HARVEST_TIME			120	//2 minutes
#define CTF_GETFLAG_TIME			600	//10 minutes ctf get flag time
#define CTF_RUSHBASE_TIME			120	//2 minutes ctf rush base time
#define CTF_RETURNFLAG_TIME			180	//3 minutes to return the flag
#define CTF_ROAM_TIME				60	//1 minute ctf roam time
//patrol flags
#define PATROL_LOOP					1
#define PATROL_REVERSE				2
#define PATROL_BACK					4
//teamplay task preference
#define TEAMTP_DEFENDER				1
#define TEAMTP_ATTACKER				2
//CTF strategy
#define CTFS_AGRESSIVE				1
//copied from the aas file header
#define PRESENCE_NONE				1
#define PRESENCE_NORMAL				2
#define PRESENCE_CROUCH				4
//
#define MAX_PROXMINES				64

//check points
typedef struct bot_waypoint_s
{
	int			inuse;
	char		name[32];
	bot_goal_t	goal;
	struct		bot_waypoint_s *next, *prev;
} bot_waypoint_t;

#define MAX_ACTIVATESTACK		8
#define MAX_ACTIVATEAREAS		32

typedef struct bot_activategoal_s
{
	int inuse;
	bot_goal_t goal;						//goal to activate (buttons etc.)
	float time;								//time to activate something
	float start_time;						//time starting to activate something
	float justused_time;					//time the goal was used
	int shoot;								//true if bot has to shoot to activate
	int weapon;								//weapon to be used for activation
	vec3_t target;							//target to shoot at to activate something
	vec3_t origin;							//origin of the blocking entity to activate
	int areas[MAX_ACTIVATEAREAS];			//routing areas disabled by blocking entity
	int numareas;							//number of disabled routing areas
	int areasdisabled;						//true if the areas are disabled for the routing
	struct bot_activategoal_s *next;		//next activate goal on stack
} bot_activategoal_t;

// Q3Doom: the bot's planner (ai_goap.c): what it knows of the world (the
// facts), the goal it pursues and the plan that gets there
#define GOAP_MAX_PLAN		4

typedef enum {
	GA_NONE,
	GA_TRAVEL,				// go to the objective, shooting what is in the way
	GA_ENGAGE_PLAYER,		// fight a player at the weapon's best range
	GA_CLEAR_MONSTER,		// kill a monster before going on
	GA_PICKUP,				// detour to an item
	GA_NUM_ACTIONS
} goapAction_t;

typedef enum {
	GG_NONE,
	GG_STAY_ALIVE,
	GG_STOP_RIVAL,			// a player engaging me (or, in deathmatch, one in sight)
	GG_REACH_OBJECTIVE,
	GG_STOCK_UP,
	GG_NUM_GOALS
} goapGoal_t;

typedef struct botFacts_s {
	float		time;				// when they were last worked out
	qboolean	hasObjective;		// a place to get to (Doom Duel)
	int			objectiveArea;
	float		objectiveTime;		// seconds of travel to it
	int			health, armor;
	float		weaponPower;		// 0 to 1: the best weapon held, with ammo
	float		dps;				// the damage per second the bot deals with it
	int			playerThreat;		// an enemy player engaging the bot, -1 none
	int			visiblePlayer;		// the nearest enemy player in sight, -1 none
	int			monsterBlocking;	// a monster on the route, a few seconds ahead, -1 none
	float		blockPenalty;		// seconds it costs to push past it
	float		monsterCost;		// damage expected from the monsters if the bot goes on
	float		monsterDps;			// what the monsters in sight deal per second, all together
	int			worstMonster;		// the monster expected to do most of it, -1 none
	float		worstMonsterCost;
	float		blockingDps, blockingTtk;	// the blocking monster's damage per second, and time to kill it
	float		worstDps, worstTtk;
	int			resourceType;		// IT_HEALTH, IT_ARMOR, IT_WEAPON, IT_AMMO of an item worth a detour, 0 none
	float		resourceTime;		// seconds of detour to it
	float		resourceNeed;		// 0 to 1
	bot_goal_t	resourceGoal;		// the item itself
} botFacts_t;

typedef struct botGoap_s {
	botFacts_t	facts;
	goapGoal_t	goal;
	int			plan[GOAP_MAX_PLAN];
	int			planLength;
	float		planCost;
	int			target;				// what the first action shoots at, -1 nothing
	float		replanTime;
	int			lastState;			// the world state the plan was made from
	float		hurtTime;			// when the bot was last hurt, and by whom
	int			hurtBy;
	float		dodgeTime;			// M19: stepping aside from a missile until then
	vec3_t		dodgeDir;
	int			dodgeMissile;		// the last missile seen coming
	qboolean	dodgeSkip;			// and it is not dodged
	float		debugTime;
	float		trappedTime;		// since when the bot can't get anywhere, 0 when it can
	float		trapCheckTime;
	float		strafeFlipTime;		// M19: when the circle strafe turns about
	float		jumpTime;
} botGoap_t;

// Q3Doom: the bot's strafe-jumping (ai_strafe.c)
typedef struct botStrafe_s {
	bot_goal_t	goal;			// what the bot moves to, and how
	float		goalTime;
	int			moveFlags, travelType;
	qboolean	allowed;		// a straight, open, safe stretch ahead
	float		allowedTime;
	vec3_t		target;			// where it goes
	int			side;			// 1 strafing left, -1 right
	float		noise;			// this jump's error on the angle, degrees
	qboolean	wasOnGround;
	float		speedPeak;
} botStrafe_t;

//bot state
typedef struct bot_state_s
{
	int inuse;										//true if this state is used by a bot client
	int botthink_residual;							//residual for the bot thinks
	int client;										//client number of the bot
	int entitynum;									//entity number of the bot
	playerState_t cur_ps;							//current player state
	int last_eFlags;								//last ps flags
	usercmd_t lastucmd;								//usercmd from last frame
	int entityeventTime[1024];						//last entity event time
	//
	bot_settings_t settings;						//several bot settings
	int (*ainode)(struct bot_state_s *bs);			//current AI node
	float thinktime;								//time the bot thinks this frame
	vec3_t origin;									//origin of the bot
	vec3_t velocity;								//velocity of the bot
	int presencetype;								//presence type of the bot
	vec3_t eye;										//eye coordinates of the bot
	int areanum;									//the number of the area the bot is in
	int inventory[MAX_ITEMS];						//string with items amounts the bot has
	int tfl;										//the travel flags the bot uses
	int flags;										//several flags
	int respawn_wait;								//wait until respawned
	int lasthealth;									//health value previous frame
	int lastkilledplayer;							//last killed player
	int lastkilledby;								//player that last killed this bot
	int botdeathtype;								//the death type of the bot
	int enemydeathtype;								//the death type of the enemy
	int botsuicide;									//true when the bot suicides
	int enemysuicide;								//true when the enemy of the bot suicides
	int setupcount;									//true when the bot has just been setup
	int map_restart;									//true when the map is being restarted
	int entergamechat;								//true when the bot used an enter game chat
	int num_deaths;									//number of time this bot died
	int num_kills;									//number of kills of this bot
	int revenge_enemy;								//the revenge enemy
	int revenge_kills;								//number of kills the enemy made
	int lastframe_health;							//health value the last frame
	int lasthitcount;								//number of hits last frame
	int chatto;										//chat to all or team
	float walker;									//walker charactertic
	float ltime;									//local bot time
	float entergame_time;							//time the bot entered the game
	float ltg_time;									//long term goal time
	float nbg_time;									//nearby goal time
	float respawn_time;								//time the bot takes to respawn
	float respawnchat_time;							//time the bot started a chat during respawn
	float chase_time;								//time the bot will chase the enemy
	float enemyvisible_time;						//time the enemy was last visible
	float check_time;								//time to check for nearby items
	float stand_time;								//time the bot is standing still
	float lastchat_time;							//time the bot last selected a chat
	float kamikaze_time;							//time to check for kamikaze usage
	float invulnerability_time;						//time to check for invulnerability usage
	float standfindenemy_time;						//time to find enemy while standing
	float attackstrafe_time;						//time the bot is strafing in one dir
	float attackcrouch_time;						//time the bot will stop crouching
	float attackchase_time;							//time the bot chases during actual attack
	float attackjump_time;							//time the bot jumped during attack
	float enemysight_time;							//time before reacting to enemy
	float enemydeath_time;							//time the enemy died
	float enemyposition_time;						//time the position and velocity of the enemy were stored
	float defendaway_time;							//time away while defending
	float defendaway_range;							//max travel time away from defend area
	float rushbaseaway_time;						//time away from rushing to the base
	float attackaway_time;							//time away from attacking the enemy base
	float harvestaway_time;							//time away from harvesting
	float ctfroam_time;								//time the bot is roaming in ctf
	float killedenemy_time;							//time the bot killed the enemy
	float arrive_time;								//time arrived (at companion)
	float lastair_time;								//last time the bot had air
	float teleport_time;							//last time the bot teleported
	float camp_time;								//last time camped
	float camp_range;								//camp range
	float weaponchange_time;						//time the bot started changing weapons
	float firethrottlewait_time;					//amount of time to wait
	float firethrottleshoot_time;					//amount of time to shoot
	float notblocked_time;							//last time the bot was not blocked
	float blockedbyavoidspot_time;					//time blocked by an avoid spot
	float predictobstacles_time;					//last time the bot predicted obstacles
	int predictobstacles_goalareanum;				//last goal areanum the bot predicted obstacles for
	vec3_t aimtarget;
	vec3_t enemyvelocity;							//enemy velocity 0.5 secs ago during battle
	vec3_t enemyorigin;								//enemy origin 0.5 secs ago during battle
	//
	int kamikazebody;								//kamikaze body
	int proxmines[MAX_PROXMINES];
	int numproxmines;
	//
	int character;									//the bot character
	int ms;											//move state of the bot
	int gs;											//goal state of the bot
	int cs;											//chat state of the bot
	int ws;											//weapon state of the bot
	//
	int enemy;										//enemy entity number
	int lastenemyareanum;							//last reachability area the enemy was in
	vec3_t lastenemyorigin;							//last origin of the enemy in the reachability area
	int weaponnum;									//current weapon number
	vec3_t viewangles;								//current view angles
	vec3_t ideal_viewangles;						//ideal view angles
	vec3_t viewanglespeed;
	// Q3Doom: human aim (see BotAimAtEnemy and BotChangeViewAngles)
	int aim_enemy;									//enemy the aim is tracking
	float aim_track_time;							//time the bot started tracking it
	float aim_update_time;							//last time the aim was updated
	vec3_t aim_error;								//angular error that shrinks while tracking
	vec3_t aim_drift;								//slow wander of the aim around the target
	vec3_t aim_lastideal;							//ideal view angles last frame
	float lastattack_time;							//last time the bot pressed fire
	int noattack;									//line where BotCheckAttack last gave up (bot_stats 2)
	float runaway_time;								//retreating: face the way until then
	botGoap_t goap;									// Q3Doom: the planner
	botStrafe_t strafe;								// Q3Doom: strafe-jumping
	//
	int ltgtype;									//long term goal type
	// team goals
	int teammate;									//team mate involved in this team goal
	int decisionmaker;								//player who decided to go for this goal
	int ordered;									//true if ordered to do something
	float order_time;								//time ordered to do something
	int owndecision_time;							//time the bot made it's own decision
	bot_goal_t teamgoal;							//the team goal
	bot_goal_t altroutegoal;						//alternative route goal
	float reachedaltroutegoal_time;					//time the bot reached the alt route goal
	float teammessage_time;							//time to message team mates what the bot is doing
	float teamgoal_time;							//time to stop helping team mate
	float teammatevisible_time;						//last time the team mate was NOT visible
	int teamtaskpreference;							//team task preference
	// last ordered team goal
	int lastgoal_decisionmaker;
	int lastgoal_ltgtype;
	int lastgoal_teammate;
	bot_goal_t lastgoal_teamgoal;
	// for leading team mates
	int lead_teammate;								//team mate the bot is leading
	bot_goal_t lead_teamgoal;						//team goal while leading
	float lead_time;								//time leading someone
	float leadvisible_time;							//last time the team mate was visible
	float leadmessage_time;							//last time a messaged was sent to the team mate
	float leadbackup_time;							//time backing up towards team mate
	//
	char teamleader[32];							//netname of the team leader
	float askteamleader_time;						//time asked for team leader
	float becometeamleader_time;					//time the bot will become the team leader
	float teamgiveorders_time;						//time to give team orders
	float lastflagcapture_time;						//last time a flag was captured
	int numteammates;								//number of team mates
	int redflagstatus;								//0 = at base, 1 = not at base
	int blueflagstatus;								//0 = at base, 1 = not at base
	int neutralflagstatus;							//0 = at base, 1 = our team has flag, 2 = enemy team has flag, 3 = enemy team dropped the flag
	int flagstatuschanged;							//flag status changed
	int forceorders;								//true if forced to give orders
	int flagcarrier;								//team mate carrying the enemy flag
	int ctfstrategy;								//ctf strategy
	char subteam[32];								//sub team name
	float formation_dist;							//formation team mate intervening space
	char formation_teammate[16];					//netname of the team mate the bot uses for relative positioning
	float formation_angle;							//angle relative to the formation team mate
	vec3_t formation_dir;							//the direction the formation is moving in
	vec3_t formation_origin;						//origin the bot uses for relative positioning
	bot_goal_t formation_goal;						//formation goal

	bot_activategoal_t *activatestack;				//first activate goal on the stack
	bot_activategoal_t activategoalheap[MAX_ACTIVATESTACK];	//activate goal heap

	bot_waypoint_t *checkpoints;					//check points
	bot_waypoint_t *patrolpoints;					//patrol points
	bot_waypoint_t *curpatrolpoint;					//current patrol point the bot is going for
	int patrolflags;								//patrol flags
} bot_state_t;

//resets the whole bot state
void BotResetState(bot_state_t *bs);
//returns the number of bots in the game
int NumBots(void);
//returns info about the entity
void BotEntityInfo(int entnum, aas_entityinfo_t *info);

extern float floattime;
#define FloatTime() floattime

// from the game source
void	QDECL BotAI_Print(int type, char *fmt, ...);
void	QDECL QDECL BotAI_BotInitialChat( bot_state_t *bs, char *type, ... );
void	BotAI_Trace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
int		BotAI_GetClientState( int clientNum, playerState_t *state );
int		BotAI_GetEntityState( int entityNum, entityState_t *state );
int		BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state );
int		BotTeamLeader(bot_state_t *bs);
void	BotStats_MissileThreat(int client);
void	BotStats_MissileHit(int client);
float	AngleDifference(float ang1, float ang2);
// Q3Doom: bot statistics
void	BotStats_Frame(bot_state_t *bs, float thinktime);
