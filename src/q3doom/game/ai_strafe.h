// Q3Doom: the bots' strafe-jumping (ai_strafe.c)

void		BotStrafeInit( void );
void		BotStrafeRecordMove( bot_state_t *bs, bot_goal_t *goal, bot_moveresult_t *result );
void		BotStrafeThink( bot_state_t *bs );
qboolean	BotStrafeInput( bot_state_t *bs, bot_input_t *bi, float frametime );
qboolean	BotAirSafety( bot_state_t *bs, bot_input_t *bi );
qboolean	BotUnstick( bot_state_t *bs, bot_input_t *bi );
