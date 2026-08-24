

#include "g_local.h"
#include "g_spawn.h"
#include "../script/scriptvariable.h"
#include "object.h"
#include "../corepp/tiki.h"

#include "../fgame/entity.h"
#include "../fgame/actor.h"

CLASS_DECLARATION(Class, SpawnArgs, NULL) {
    {NULL, NULL}
};

Container<SafePtr<Listener>> g_spawnlist;

SpawnArgs::SpawnArgs() {}

SpawnArgs::SpawnArgs(SpawnArgs& otherlist)
{
    int num;
    int i;

    num = otherlist.NumArgs();
    keyList.Resize(num);
    valueList.Resize(num);
    for (i = 1; i <= num; i++) {
        keyList.AddObject(otherlist.keyList.ObjectAt(i));
        valueList.AddObject(otherlist.valueList.ObjectAt(i));
    }
}

void SpawnArgs::Clear(void)
{
    keyList.FreeObjectList();
    valueList.FreeObjectList();
}

char *SpawnArgs::Parse(char *data, bool bAllowUtils)
{
    str         keyname;
    const char *com_token;

    Clear();

    com_token = COM_Parse(&data);
    if (!data) {
        return NULL;
    }

    if (com_token[0] != '{') {
        gi.Error(ERR_DROP, "SpawnArgs::Parse : found %s when expecting {", com_token);
    }

    while (1) {
        com_token = COM_Parse(&data);
        if (com_token[0] == '}') {
            break;
        }

        if (!data) {
            gi.Error(ERR_DROP, "SpawnArgs::Parse : EOF without closing brace");
        }

        keyname = com_token;

        com_token = COM_Parse(&data);
        if (!data) {
            gi.Error(ERR_DROP, "SpawnArgs::Parse : EOF without closing brace");
        }

        if (com_token[0] == '}') {
            gi.Error(ERR_DROP, "SpawnArgs::Parse : closing brace without data");
        }

        if (!bAllowUtils && keyname[0] == '_') {
            continue;
        }

        setArg(keyname.c_str(), com_token);
    }

    return data;
}

const char *SpawnArgs::getArg(const char *key, const char *defaultValue)
{
    int i;
    int num;

    num = keyList.NumObjects();
    for (i = 1; i <= num; i++) {
        if (keyList.ObjectAt(i) == key) {
            return valueList.ObjectAt(i);
        }
    }

    return defaultValue;
}

void SpawnArgs::setArg(const char *key, const char *value)
{
    int i;
    int num;

    num = keyList.NumObjects();
    for (i = 1; i <= num; i++) {
        if (keyList.ObjectAt(i) == key) {
            valueList.ObjectAt(i) = value;
            return;
        }
    }

    keyList.AddObject(str(key));
    valueList.AddObject(str(value));
}

void SpawnArgs::operator=(SpawnArgs& otherlist)
{
    int num;
    int i;

    Clear();

    num = otherlist.NumArgs();
    keyList.Resize(num);
    valueList.Resize(num);

    for (i = 1; i <= num; i++) {
        keyList.AddObject(otherlist.keyList.ObjectAt(i));
        valueList.AddObject(otherlist.valueList.ObjectAt(i));
    }
}

int SpawnArgs::NumArgs(void)
{
    return keyList.NumObjects();
}

const char *SpawnArgs::getKey(int index)
{
    return keyList.ObjectAt(index + 1);
}

const char *SpawnArgs::getValue(int index)
{
    return valueList.ObjectAt(index + 1);
}

void SpawnArgs::Archive(Archiver& arc)
{
    Class::Archive(arc);

    keyList.Archive(arc);
    valueList.Archive(arc);
}


ClassDef *SpawnArgs::getClassDef(qboolean *tikiWasStatic)
{
    const char  *classname;
    ClassDef    *cls  = NULL;
    [[maybe_unused]] dtiki_t     *tiki = NULL;
    [[maybe_unused]] dtikianim_t *a    = NULL;

    classname = getArg("classname");

    if (tikiWasStatic) {
        *tikiWasStatic = false;
    }

    if (classname) {
        if (!Q_stricmp(classname, "light")) {
            if (tikiWasStatic) {
                *tikiWasStatic = true;
            }
            return NULL;
        }

        cls = getClassForID(classname);
        if (!cls) {
            cls = getClass(classname);
        }
    }

    if (!cls) {
        const char *model;

        cls   = &Object::ClassInfo;
        model = getArg("model");
        if (model) {
            int i;

            int modelindex;

            if ((strlen(model) >= 3) && (!Q_stricmp(&model[strlen(model) - 3], "tik"))) {
                modelindex = modelIndex(model);

                if (modelindex == -1) {
                    return NULL;
                }

                if (gi.IsModel(modelindex)) {
                    const char *s;

                    s = getArg("make_static");
                    if (s && atoi(s)) {
                        if (tikiWasStatic) {
                            *tikiWasStatic = true;
                        }

                        return NULL;
                    }

                    tiki_cmd_t cmds;
                    if (gi.InitCommands(modelindex, &cmds)) {
                        for (i = 0; i < cmds.num_cmds; i++) {
                            if (!Q_stricmp(cmds.cmds[i].args[0], "classname")) {
                                cls = getClass(cmds.cmds[i].args[1]);
                                break;
                            }
                        }

                        if (i == cmds.num_cmds) {
                            if (developer->integer == 2) {
                                gi.WDPrintf("Classname %s used, but 'classname' was not found in Initialization commands, using Object.\n", classname);
                            }
                        }
                    } else {
                        gi.WDPrintf("Classname %s used, but TIKI had no Initialization commands, using Object.\n", classname);
                    }
                } else {
                    gi.WDPrintf("Classname %s used, but TIKI was not valid, using Object.\n", classname);
                }
            } else {
                gi.WDPrintf("Classname %s used, but model was not a TIKI, using Object.\n", classname);
            }
        } else {
            gi.WDPrintf("Classname %s' used, but no model was set, using Object.\n", classname);
        }
    }

    return cls;
}

Entity *SpawnArgs::Spawn( void )
{
	str      classname;
	ClassDef	*cls;
	Entity	*obj;
	Event    *ev;
	int      i;
	qboolean tikiWasStatic; 
	
	classname = getArg( "classname", "Unspecified" );
	cls = getClassDef( &tikiWasStatic );
	if ( !cls )
	{
		if ( !tikiWasStatic )
		{
			gi.DPrintf( "%s doesn't have a spawn function\n", classname.c_str() );
		}
		
		return NULL;
	}
	
	obj = ( Entity * )cls->newInstance();
	
	for( i = 0; i < NumArgs(); i++ )
	{
		if ( !Q_stricmp( getKey( i ), "script" ) )
		{
			char *ptr;
			char * token;
			
			ptr = const_cast< char * >( getValue( i ) );
			while ( 1 )
            {
				token = COM_ParseExt( &ptr, true );
				if ( !token[ 0 ] )
					break;
				if ( strchr( token, ':' ) )
				{
					gi.WDPrintf( "Label %s imbedded inside editor script for %s.\n", token, classname.c_str() );
				}
				else
				{
					ev = new Event( token );
					while ( 1 )
					{
						token = COM_ParseExt( &ptr, false );
						if ( !token[ 0 ] )
							break;
						ev->AddToken( token );
					}
					
					obj->PostEvent( ev, EV_SPAWNARG );
				}
            }
		}
		else
		{
			ev = new Event( getKey( i ) );
			ev->AddToken( getValue( i ) );
			
			if ( Q_stricmp( getKey( i ), "model" ) == 0 )
				obj->PostEvent( ev, EV_PRIORITY_SPAWNARG );
			else
				obj->PostEvent( ev, EV_SPAWNARG );
		}
	}
	
	if ( !obj )
	{
		gi.WDPrintf( "%s failed on newInstance\n", classname.c_str() );
		return NULL;
	}
	
	return obj;
}


ClassDef *FindClass(const char *name, qboolean *isModel)
{
    ClassDef *cls;

    *isModel = false;

    cls = getClass(name);
    if (!cls) {
        SpawnArgs args;

        args.setArg("model", name);

        cls = args.getClassDef();
        if (cls) {
            *isModel = true;
        }
    }
    return cls;
}

void G_InitClientPersistant(gclient_t *client)
{
    memset(&client->pers, 0, sizeof(client->pers));
}

