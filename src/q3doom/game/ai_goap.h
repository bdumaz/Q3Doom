// Q3Doom: the bots' planner (ai_goap.c)

void		BotGoapInit( void );
qboolean	BotGoapActive( bot_state_t *bs );
qboolean	BotGoapSteers( bot_state_t *bs );
int			BotGoapAction( bot_state_t *bs );
void		BotGoapUpdate( bot_state_t *bs );
qboolean	BotGoapFindEnemy( bot_state_t *bs, int curenemy );
qboolean	BotGoapWantsToRetreat( bot_state_t *bs );
qboolean	BotGoapWantsToChase( bot_state_t *bs );
float		BotGoapItemRange( bot_state_t *bs, float range );
qboolean	BotGoapPickUp( bot_state_t *bs, bot_goal_t *goal );
qboolean	BotGoapTopGoalIsTrap( bot_state_t *bs );
qboolean	BotGoapDodge( bot_state_t *bs, vec3_t dodgedir );
