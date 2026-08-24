

#pragma once

#include "../corepp/class.h"
#include "../corepp/container.h"

#define SPAWNFLAG_NOT_EASY       0x00000100
#define SPAWNFLAG_NOT_MEDIUM     0x00000200
#define SPAWNFLAG_NOT_HARD       0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH 0x00000800
#define SPAWNFLAG_DETAIL         0x00001000
#define SPAWNFLAG_DEVELOPMENT    0x00002000
#define SPAWNFLAG_NOCONSOLE      0x00008000
#define SPAWNFLAG_NOPC           0x00010000

class Listener;

class SpawnArgs : public Class
{
private:
    Container<str> keyList;
    Container<str> valueList;

public:
    CLASS_PROTOTYPE(SpawnArgs);

    SpawnArgs();
    SpawnArgs(SpawnArgs& arglist);

    void Clear(void);

    char       *Parse(char *data, bool bAllowUtils = false);
    const char *getArg(const char *key, const char *defaultValue = NULL);
    void        setArg(const char *key, const char *value);

    int         NumArgs(void);
    const char *getKey(int index);
    const char *getValue(int index);
    void        operator=(SpawnArgs& a);

    ClassDef *getClassDef(qboolean *tikiWasStatic = NULL);
    Entity *Spawn(void);  
    Listener *SpawnInternal(void); 

    void Archive(Archiver& arc) override;
};

extern Container<SafePtr<Listener>> g_spawnlist;

ClassDef *FindClass(const char *name, qboolean *isModel);

#ifdef GAME_DLL
void G_InitClientPersistant(gclient_t *client);  
#endif
