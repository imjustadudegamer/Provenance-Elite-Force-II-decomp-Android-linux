

int BotLoadCharacter(char *charfile, float skill);
void BotFreeCharacter(int character);
float Characteristic_Float(int character, int index);
float Characteristic_BFloat(int character, int index, float min, float max);
int Characteristic_Integer(int character, int index);
int Characteristic_BInteger(int character, int index, int min, int max);
void Characteristic_String(int character, int index, char *buf, int size);
void BotShutdownCharacters(void);
