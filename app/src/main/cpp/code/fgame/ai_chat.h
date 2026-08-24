

int BotChat_EnterGame(bot_state_t *bs);
int BotChat_ExitGame(bot_state_t *bs);
int BotChat_StartLevel(bot_state_t *bs);
int BotChat_EndLevel(bot_state_t *bs);
int BotChat_HitTalking(bot_state_t *bs);
int BotChat_HitNoDeath(bot_state_t *bs);
int BotChat_HitNoKill(bot_state_t *bs);
int BotChat_Death(bot_state_t *bs);
int BotChat_Kill(bot_state_t *bs);
int BotChat_EnemySuicide(bot_state_t *bs);
int BotChat_Random(bot_state_t *bs);
float BotChatTime(bot_state_t *bs);
int BotValidChatPosition(bot_state_t *bs);
void BotChatTest(bot_state_t *bs);

