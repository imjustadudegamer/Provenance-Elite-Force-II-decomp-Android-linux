

#include "g_phys.h"
#include "worldspawn.h"
#include "level.h"
#include "scriptmaster.h"
#include "scriptexception.h"
#include "player.h"
#include "groupcoordinator.h"  

extern Event EV_World_SetSoundtrack;



#ifdef GAME_DLL
#    include "../fgame/soundman.h"
#endif

#define DEFAULT_ENTITY_FADE_DIST 3000


Event EV_World_SetNextMap (    "nextmap",
    EV_SCRIPTONLY,
    "s",
    "nextMap",
    "Set the next map to change to");
extern Event EV_Trigger_SetMessage;

Event EV_World_SetScript (    "script",
    EV_SCRIPTONLY,
    "s",
    "worldScript",
    "Set the script for this map");
Event EV_World_SetWaterColor (    "watercolor",
    EV_SCRIPTONLY,
    "v",
    "color_water",
    "Set the watercolor screen blend");
Event EV_World_SetWaterAlpha (    "wateralpha",
    EV_SCRIPTONLY,
    "f",
    "waterAlpha",
    "Set the alpha of the water screen blend");
Event EV_World_SetSlimeColor (    "slimecolor",
    EV_SCRIPTONLY,
    "v",
    "color_slime",
    "Set the slimecolor screen blend");
Event EV_World_SetSlimeAlpha (    "slimealpha",
    EV_SCRIPTONLY,
    "f",
    "slimeAlpha",
    "Set the alpha of the slime screen blend");
Event EV_World_SetLavaColor (    "lavacolor",
    EV_SCRIPTONLY,
    "v",
    "color_lava",
    "Set the color of lava screen blend");
Event EV_World_SetLavaAlpha (    "lavaalpha",
    EV_SCRIPTONLY,
    "f",
    "lavaAlpha",
    "Set the alpha of lava screen blend");
Event EV_World_SetFarPlane_Color (    "farplane_color",
    EV_SCRIPTONLY,
    "v",
    "color_farplane",
    "Set the color of the far clipping plane fog"
);
Event EV_World_SetFarPlane_Cull (    "farplane_cull",
    EV_SCRIPTONLY,
    "b",
    "farplaneCull",
    "Whether or not the far clipping plane should cull things out of the world");
Event EV_World_SetFarPlane_Fog (    "farplane_fog",
    EV_SCRIPTONLY,
    "b",
    "farplaneFog",
    "Whether or not to use fog with far plane");
Event EV_World_SetFarPlane (    "farplane",
    EV_SCRIPTONLY,
    "f",
    "farplaneDistance",
    "Set the distance of the far clipping plane"
);
Event EV_World_SetTerrainGlobal (    "terrainglobal",
    EV_SCRIPTONLY,
    "b",
    "terrain_global_bool",
    "Turns the terrain drawing as global on or off");
Event EV_World_SetTerrainGlobalMin (    "terrainglobalmin",
    EV_SCRIPTONLY,
    "f",
    "terrain_global_min",
    "Turns the terrain drawing as global on and sets the minimum");
Event EV_World_SetEntityFadeDist (    "entity_fade_dist",
    EV_SCRIPTONLY,
    "f",
    "entity_fade_dist",
    "Sets the default distance to fade out entities");
Event EV_World_SetAmbientLight (    "ambientlight",
    EV_SCRIPTONLY,
    "b",
    "ambientLight",
    "Set whether or not ambient light should be used");
Event EV_World_SetAmbientIntensity (    "ambient",
    EV_SCRIPTONLY,
    "f",
    "ambientIntensity",
    "Set the intensity of the ambient light");
Event EV_World_SetSunColor (    "suncolor",
    EV_SCRIPTONLY,
    "v",
    "sunColor",
    "Set the color of the sun");
Event EV_World_SetSunLight (    "sunlight",
    EV_SCRIPTONLY,
    "b",
    "sunlight",
    "Set whether or not there should be sunlight");
Event EV_World_SetSunDirection (    "sundirection",
    EV_SCRIPTONLY,
    "v",
    "sunlightDirection",
    "Set the direction of the sunlight");
Event EV_World_LightmapDensity (    "lightmapdensity",
    EV_SCRIPTONLY,
    "f",
    "density",
    "Set the default lightmap density for all world surfaces");
Event EV_World_SunFlare (    "sunflare",
    EV_SCRIPTONLY,
    "v",
    "position_of_sun",
    "Set the position of the sun for the purposes of the sunflare");
Event EV_World_SunFlareInPortalSky (    "sunflare_inportalsky",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "Let the renderer know that the sun is in the portal sky");
Event EV_World_SetSkyAlpha (    "skyalpha",
    EV_SCRIPTONLY,
    "f",
    "newAlphaForPortalSky",
    "Set the alpha on the sky");
Event EV_World_SetSkyPortal (    "skyportal",
    EV_SCRIPTONLY,
    "b",
    "newSkyPortalState",
    "Whether or not to use the sky portal at all");
Event EV_World_AddBroken (    "addbroken",
    EV_SCRIPTONLY,
    "s",
    "brokenThingName",
    "Adds this thing to the broken list");
Event EV_World_RemoveBroken (    "removebroken",
    EV_SCRIPTONLY,
    "s",
    "brokenThingName",
    "Removes this thing to the broken list");
// registered strings must be retail's exact camelCase spellings ("addAvailableViewMode" /
Event EV_World_AddAvailableViewMode (    "addAvailableViewMode",
    EV_SCRIPTONLY,
    "s",
    "modeName",
    "Adds this view mode as being available");
Event EV_World_RemoveAvailableViewMode (    "removeAvailableViewMode",
    EV_SCRIPTONLY,
    "s",
    "modeName",
    "Removes this view mode as being available");
Event EV_World_ClearAvailableViewModes (    "clearAvailableViewModes",
    EV_SCRIPTONLY,
    NULL,
    NULL,
    "Clears all available modes"
);
Event EV_World_CanShakeCamera (    "canShakeCamera",
    EV_SCRIPTONLY,
    "b",
    "canShakeCamera",
    "Specifies whether or not cinematic camera's can shake from earthquakes.");

Event EV_World_SetLightIntensity (    "light_intensity",
    EV_SCRIPTONLY,
    "sf",
    "light_group_name light_intensity",
    "Sets the intensity of a dynamic light");
Event EV_World_SetLightDefaultIntensity (    "light_default_intensity",
    EV_SCRIPTONLY,
    "sf",
    "light_group_name light_intensity",
    "Sets the default intensity of a dynamic light.  This is used only when r_dynamiclightmaps is 0.\n"
    "A value below 0 specifies that this light has no default value and will always be dynamic");
Event EV_World_SetLightFade (    "light_fade",
    EV_SCRIPTONLY,
    "sff",
    "light_group_name end_light_intensity fade_time",
    "Sets a up a dynamic light to fade");
Event EV_World_SetLightLightstyle (    "light_lightstyle",
    EV_SCRIPTONLY,
    "ssB",
    "light_group_name light_style stop_at_end",
    "Sets the lightstyle of a dynamic light");
Event EV_World_GetLightIntensity (    "getlightintensity",
    EV_SCRIPTONLY,
    "@is",
    "light_intensity light_group_name",
    "Gets the intensity of a dynamic light");
Event EV_World_SetWindDirection (    "wind_direction",
    EV_SCRIPTONLY,
    "v",
    "wind_direction",
    "Sets the direction of the wind");
Event EV_World_SetWindIntensity (    "wind_intensity",
    EV_SCRIPTONLY,
    "f",
    "wind_intensity",
    "Sets the intensity of the wind");
Event EV_World_SetWindGust (    "wind_gust",
    EV_SCRIPTONLY,
    "fff",
    "wind_intensity fade_time gust_time",
    "Creates a gust of wind");
Event EV_World_SetWindFade (    "wind_fade",
    EV_SCRIPTONLY,
    "ff",
    "end_wind_intensity fade_time",
    "Fades the wind intensity to the new value");
Event EV_World_SetWindMinMax (    "wind_minmax",
    EV_SCRIPTONLY,
    "ffF",
    "min_wind_intensity min_wind_intensity max_change_per_frame",
    "Sets the min & max intensity of the wind");
Event EV_World_SetWeather (    "weather",
    EV_SCRIPTONLY,
    "sf",
    "type intensity",
    "Sets the current weather");
Event EV_World_SetMessage (    "message",
    EV_SCRIPTONLY,
    "s",
    "worldMessage",
    "Set a message for the world");
Event EV_World_SetPhysicsVar (    "physicsVar",
    EV_SCRIPTONLY,
    "sf",
    "physicsVarName physicsVerValue",
    "Sets one of the world's physics variables");

Event EV_World_SetSkipThread
(
	"skipthread",
	EV_SCRIPTONLY,
	"s",
	"skipThread",
	"Set the thread for skipping cinematics"
);

Event EV_World_SetTimeScale (    "time_scale",
    EV_SCRIPTONLY,
    "f",
    "time_scale",
    "Sets the time scale for the world");

WorldPtr world;

Event EV_World_SetSoundtrack (	"soundtrack",
	EV_SCRIPTONLY,
	"s",
	"MusicFile",
	"Set music soundtrack for this level.");

CLASS_DECLARATION(Entity, World, "worldspawn") {
    {&EV_World_SetSoundtrack,            &World::SetSoundtrack           },
    {&EV_World_SetSkipThread,                   &World::SetSkipThread           },
    {&EV_World_SetNextMap,               &World::SetNextMap              },
    {&EV_World_SetMessage,               &World::SetMessage              },
    {&EV_World_SetScript,                &World::SetScript               },
    {&EV_World_SetWaterColor,            &World::SetWaterColor           },
    {&EV_World_SetWaterAlpha,            &World::SetWaterAlpha           },
    {&EV_World_SetSlimeColor,            &World::SetSlimeColor           },
    {&EV_World_SetSlimeAlpha,            &World::SetSlimeAlpha           },
    {&EV_World_SetLavaColor,             &World::SetLavaColor            },
    {&EV_World_SetLavaAlpha,             &World::SetLavaAlpha            },
    {&EV_World_SetFarPlane_Color,        &World::SetFarPlane_Color       },
    {&EV_World_SetFarPlane_Cull,         &World::SetFarPlane_Cull        },
    {&EV_World_SetFarPlane_Fog,          &World::SetFarPlane_Fog         },
    {&EV_World_SetFarPlane,              &World::SetFarPlane             },
    {&EV_World_SetTerrainGlobal,         &World::SetTerrainGlobal        },
    {&EV_World_SetTerrainGlobalMin,      &World::SetTerrainGlobalMin     },
    {&EV_World_SetEntityFadeDist,        &World::SetEntityFadeDist       },
    {&EV_World_SetSkyAlpha,              &World::SetSkyAlpha             },
    {&EV_World_SetSkyPortal,             &World::SetSkyPortal            },
    {&EV_World_SetPhysicsVar,            &World::setPhysicsVar           },
    {&EV_World_SetAmbientLight,          NULL                            },
    {&EV_World_SetAmbientIntensity,      NULL                            },
    {&EV_World_SetSunColor,              NULL                            },
    {&EV_World_SetSunLight,              NULL                            },
    {&EV_World_SetSunDirection,          NULL                            },
    {&EV_World_LightmapDensity,          NULL                            },
    {&EV_World_SunFlare,                 NULL                            },
    {&EV_World_SunFlareInPortalSky,      NULL                            },
    {&EV_World_AddBroken,                &World::addBrokenThing          },
    {&EV_World_RemoveBroken,             &World::removeBrokenThing       },
    {&EV_World_AddAvailableViewMode,     &World::addAvailableViewMode    },
    {&EV_World_RemoveAvailableViewMode,  &World::removeAvailableViewMode },
    {&EV_World_ClearAvailableViewModes,  &World::clearAvailableViewModes },
    {&EV_World_CanShakeCamera,           &World::setCanShakeCamera       },
    {&EV_World_SetLightIntensity,        &World::SetLightIntensity       },
    {&EV_World_SetLightDefaultIntensity, &World::SetLightDefaultIntensity},
    {&EV_World_SetLightFade,             &World::SetLightFade            },
    {&EV_World_SetLightLightstyle,       &World::SetLightLightstyle      },
    {&EV_World_GetLightIntensity,        &World::GetLightIntensity       },
    {&EV_World_SetWindDirection,         &World::SetWindDirection        },
    {&EV_World_SetWindIntensity,         &World::SetWindIntensity        },
    {&EV_World_SetWindGust,              &World::SetWindGust             },
    {&EV_World_SetWindFade,              &World::SetWindFade             },
    {&EV_World_SetWindMinMax,            &World::SetWindMinMax           },
    {&EV_World_SetWeather,               &World::SetWeather              },
    {&EV_World_SetTimeScale,             &World::SetTimeScale            },
    {NULL,                               NULL                            }
};

World::World()
{
    world       = this;
    world_dying = qfalse;

    flags |= FL_THINK;

    groupcoordinator = NULL;

    if (LoadingSavegame) {
        return;
    }

    assert(entnum == ENTITYNUM_WORLD);

    setMoveType(MOVETYPE_NONE);
    setSolidType(SOLID_BSP);

    edict->s.modelindex = 1;
    model               = "*0";

    UpdateConfigStrings();

    ChangeSoundtrack("");

    farplane_distance = 0;
    farplane_color    = vec_zero;
    farplane_cull     = qtrue;
    farplane_fog      = qtrue;
    UpdateFog();

    terrain_global     = qfalse;
    terrain_global_min = MIN_WORLD_COORD;
    UpdateTerrain();

    entity_fade_dist = DEFAULT_ENTITY_FADE_DIST;
    UpdateEntityFadeDist();

    UpdateDynamicLights();
    UpdateWeather();

    time_scale = 1.0f;

    sky_alpha  = 1.0f;
    sky_portal = qtrue;
    UpdateSky();

    m_fAIVisionDistance = 2048.0f;

    level.cinematic = spawnflags & WORLD_CINEMATIC;

    if (level.cinematic) {
        gi.cvar_set("sv_cinematic", "1");
    } else {
        gi.cvar_set("sv_cinematic", "0");
    }

    level.nextmap    = "";
    level.level_name = level.mapname;

    {
        str mapname;
        int i;

        mapname = "maps/";
        mapname += level.mapname;
        for (i = mapname.length() - 1; i >= 0; i--) {
            if (mapname[i] == '.') {
                mapname[i] = 0;
                break;
            }
        }

        mapname += ".scr";

        if (gi.FS_ReadFile(mapname.c_str(), NULL, true) != -1) {
            gi.DPrintf("Adding script: '%s'\n", mapname.c_str());
            level.SetGameScript(mapname.c_str());
        } else {
            level.SetGameScript("");
        }
    }

    level.consoleThread = Director.CreateThread();

    SoundMan.Load();

    level.water_color = Vector(0.0f, 0.0f, 0.5f);
    level.water_alpha = 0.4f;

    slime_color = Vector(0.2f, 0.4f, 0.2f);
    slime_alpha = 0.6f;

    level.lava_color = Vector(0.5f, 0.15f, 0.0f);
    level.lava_alpha = 0.6f;

    SetTargetName("world");

    groupcoordinator = new GroupCoordinator;

    m_fNorth = 0;

    for (int j = 0; j < WORLD_PHYSICS_TOTAL_NUMBER; j++) {
        _physicsInfo[j] = -1.0f;
    }

    _canShakeCamera = false;

}

World::~World()
{
    world_dying = true;
    FreeTargetList();

    if (groupcoordinator) {
        delete groupcoordinator;
    }

    groupcoordinator = NULL;

    freeAllBrokenThings();
}

void World::UpdateConfigStrings(void)
{
    gi.setConfigstring(CS_GAME_VERSION, GAME_VERSION);
    // SDK worldspawn.cpp:642 -- retail publishes Level::startTime here, not the
    gi.setConfigstring(CS_LEVEL_START_TIME, va("%i", level.startTime));

    gi.setConfigstring(CS_NAME, level.level_name.c_str());
};

void World::UpdateFog(void)
{
    gi.SetFarPlane(farplane_distance, farplane_cull);
    gi.setConfigstring(
        CS_FOGINFO,
        va("%d %d %.0f %.4f %.4f %.4f",
           farplane_cull,
           farplane_fog,
           farplane_distance,
           farplane_color.x,
           farplane_color.y,
           farplane_color.z)
    );
}

void World::UpdateTerrain(void)
{
    gi.setConfigstring(CS_TERRAININFO, va("%d %.0f", terrain_global, terrain_global_min));
}

void World::UpdateEntityFadeDist(void)
{
    gi.setConfigstring(CS_ENTITYFADEINFO, va("%f", entity_fade_dist));
}

void World::UpdateSky(void)
{
    gi.SetSkyPortal(sky_portal);
    gi.setConfigstring(CS_SKYINFO, va("%.4f %d", sky_alpha, sky_portal));
}


void World::UpdateDynamicLights(void)
{
    int i;

    for (i = 0; i < MAX_LIGHTING_GROUPS; i++) {
        gi.SetDynamicLight(i, dynamic_lights[i].intensity);
        gi.SetDynamicLightDefault(i, dynamic_lights[i].defaultIntensity);
    }
}

void World::UpdateWindDirection(void)
{
    gi.SetWindDirection(wind.direction);
}

void World::UpdateWindIntensity(void)
{
    gi.SetWindIntensity(wind.intensity);
}

void World::UpdateTimeScale(void)
{
    gi.SetTimeScale(time_scale);
}

void World::UpdateWeather(void)
{
    gi.SetWeatherInfo(weather.type, weather.intensity);
}

void World::SetLightIntensity(Event *ev)
{
    str   light_group_name;
    int   light_group_num;
    float light_intensity;


    light_group_name = ev->GetString(1);
    light_intensity  = ev->GetFloat(2);

    light_group_num = gi.GetLightingGroup(light_group_name.c_str());


    if (light_group_num == -1) {
        gi.WDPrintf("Unknown light group %s\n", light_group_name.c_str());
        return;
    }


    dynamic_lights[light_group_num].intensity  = light_intensity;
    dynamic_lights[light_group_num].fade_time  = 0;
    dynamic_lights[light_group_num].lightstyle = "";
}

void World::SetLightDefaultIntensity(Event *ev)
{
    str   light_group_name;
    int   light_group_num;
    float light_intensity;


    light_group_name = ev->GetString(1);
    light_intensity  = ev->GetFloat(2);

    light_group_num = gi.GetLightingGroup(light_group_name.c_str());


    if (light_group_num == -1) {
        gi.WDPrintf("Unknown light group %s\n", light_group_name.c_str());
        return;
    }


    dynamic_lights[light_group_num].defaultIntensity = light_intensity;
}

void World::SetLightFade(Event *ev)
{
    str   light_group_name;
    int   light_group_num;
    float light_end_intensity;
    float fade_time;


    light_group_name    = ev->GetString(1);
    light_end_intensity = ev->GetFloat(2);
    fade_time           = ev->GetFloat(3);

    light_group_num = gi.GetLightingGroup(light_group_name.c_str());


    if (light_group_num == -1) {
        gi.WDPrintf("Unknown light group %s\n", light_group_name.c_str());
        return;
    }


    dynamic_lights[light_group_num].start_intensity = dynamic_lights[light_group_num].intensity;
    dynamic_lights[light_group_num].end_intensity   = light_end_intensity;

    dynamic_lights[light_group_num].start_fade_time = level.time;
    dynamic_lights[light_group_num].fade_time       = fade_time;
}

void World::SetLightLightstyle(Event *ev)
{
    str      light_group_name;
    int      light_group_num;
    str      lightstyle;
    qboolean stop_at_end;


    light_group_name = ev->GetString(1);
    lightstyle       = ev->GetString(2);

    if (ev->NumArgs() > 2) {
        stop_at_end = ev->GetBoolean(3);
    } else {
        stop_at_end = qfalse;
    }

    light_group_num = gi.GetLightingGroup(light_group_name.c_str());


    if (light_group_num == -1) {
        gi.WDPrintf("Unknown light group %s\n", light_group_name.c_str());
        return;
    }


    dynamic_lights[light_group_num].lightstyle                  = lightstyle;
    dynamic_lights[light_group_num].current_lightstyle_position = -1;
    dynamic_lights[light_group_num].stop_at_end                 = stop_at_end;
}

void World::GetLightIntensity(Event *ev)
{
    str light_group_name;
    int light_group_num;


    light_group_name = ev->GetString(1);

    light_group_num = gi.GetLightingGroup(light_group_name.c_str());


    if (light_group_num == -1) {
        gi.WDPrintf("Unknown light group %s\n", light_group_name.c_str());
        return;
    }

    ev->ReturnFloat(dynamic_lights[light_group_num].intensity);
}

void World::SetWindDirection(Event *ev)
{
    wind.direction = ev->GetVector(1);
}

void World::SetWindIntensity(Event *ev)
{
    wind.intensity = ev->GetFloat(1);

    wind.gust_time     = 0;
    wind.fade_time     = 0;
    wind.min_intensity = -1;
    wind.max_intensity = -1;
}

void World::SetWindGust(Event *ev)
{
    wind.start_intensity = wind.intensity;
    wind.end_intensity   = ev->GetFloat(1);

    wind.start_fade_time = level.time;
    wind.fade_time       = ev->GetFloat(2);

    wind.gust_time = ev->GetFloat(3);

    wind.min_intensity = -1;
    wind.max_intensity = -1;
}

void World::SetWindFade(Event *ev)
{
    wind.start_intensity = wind.intensity;
    wind.end_intensity   = ev->GetFloat(1);

    wind.start_fade_time = level.time;
    wind.fade_time       = ev->GetFloat(2);

    wind.gust_time = 0;

    wind.min_intensity = -1;
    wind.max_intensity = -1;
}

void World::SetWindMinMax(Event *ev)
{
    wind.fade_time = 0;

    wind.gust_time = 0;

    wind.min_intensity = ev->GetFloat(1);
    wind.max_intensity = ev->GetFloat(2);

    if (ev->NumArgs() > 2) {
        wind.max_change = ev->GetFloat(3);
    } else {
        wind.max_change = (wind.max_intensity - wind.min_intensity) * 0.05f;
    }
}

void World::SetWeather(Event *ev)
{
    str weather_name;

    weather_name = ev->GetString(1);

    if (weather_name == "rain") {
        weather.type = WEATHER_RAIN;
    } else if (weather_name == "rain_plain") {
        weather.type = WEATHER_RAIN_PLAIN;
    } else if (weather_name == "snow") {
        weather.type = WEATHER_SNOW;
    } else {
        weather.type = WEATHER_NONE;
    }

    weather.intensity = ev->GetFloat(2);

    UpdateWeather();
}

void World::SetTimeScale(Event *ev)
{
    time_scale = ev->GetFloat(1);
}

void World::SetSoundtrack(Event *ev)
{
    str text;

    text = ev->GetString(1);
    ChangeSoundtrack(text);
}


void World::SetFarPlane(Event *ev)
{
    farplane_distance = ev->GetFloat(1);
    UpdateFog();
}


void World::SetFarPlane_Color(Event *ev)
{
    farplane_color = ev->GetVector(1);
    UpdateFog();
}

void World::SetFarPlane_Cull(Event *ev)
{
    farplane_cull = ev->GetBoolean(1);
    UpdateFog();
}

void World::SetFarPlane_Fog(Event *ev)
{
    farplane_fog = ev->GetBoolean(1);
    UpdateFog();
}

void World::SetTerrainGlobal(Event *ev)
{
    terrain_global     = ev->GetBoolean(1);
    terrain_global_min = MIN_WORLD_COORD;
    UpdateTerrain();
}

void World::SetTerrainGlobalMin(Event *ev)
{
    terrain_global     = qtrue;
    terrain_global_min = ev->GetFloat(1);
    UpdateTerrain();
}

void World::SetEntityFadeDist(Event *ev)
{
    entity_fade_dist = ev->GetFloat(1);
    UpdateEntityFadeDist();
}

void World::SetSkyAlpha(Event *ev)
{
    sky_alpha = ev->GetFloat(1);
    UpdateSky();
}

void World::SetSkyPortal(Event *ev)
{
    sky_portal = ev->GetBoolean(1);
    UpdateSky();
}

void World::SetSkipThread(Event *ev)
{
    str label;

    label = ev->GetString(1);
    if (label.length() && label.icmp("null")) {
        skipthread = label;
    } else {
        skipthread = "";
    }
}

void World::SetNextMap(Event *ev)
{
    level.nextmap = ev->GetString(1);
}

void World::SetScript(Event *ev)
{
    str text;
    str scriptname;

    text = ev->GetString(1);

    gi.DPrintf("Adding script: '%s'\n", text.c_str());
    scriptname = "maps/" + text;

    level.SetGameScript(scriptname.c_str());
}

void World::SetMessage(Event *ev)
{
    str text;

    text             = ev->GetString(1);
    level.level_name = text;
    gi.setConfigstring(CS_NAME, text);
}

void World::SetWaterColor(Event *ev)
{
    level.water_color = ev->GetVector(1);
}

void World::SetWaterAlpha(Event *ev)
{
    level.water_alpha = ev->GetFloat(1);
}

void World::SetLavaColor(Event *ev)
{
    level.lava_color = ev->GetVector(1);
}

void World::SetLavaAlpha(Event *ev)
{
    level.lava_alpha = ev->GetFloat(1);
}

void World::SetSlimeColor(Event *ev)
{
    slime_color = ev->GetVector(1);
}

void World::SetSlimeAlpha(Event *ev)
{
    slime_alpha = ev->GetFloat(1);
}


static char worldPhysicsVarNames[WORLD_PHYSICS_TOTAL_NUMBER][32] = {
    "maxSpeed",
    "airAccelerate",
    "gravity"
};

int World::worldPhysicsVarNameToIndex(const char *varName)
{
    int i;

    for (i = 0; i < WORLD_PHYSICS_TOTAL_NUMBER; i++) {
        if (Q_stricmp(varName, worldPhysicsVarNames[i]) == 0) {
            return i;
        }
    }

    gi.DPrintf("Unknown physics variable %s\n", varName);
    return -1;
}

void World::setPhysicsVar(Event *ev)
{
    setPhysicsVar(ev->GetString(1), ev->GetFloat(2));
}

void World::setPhysicsVar(const char *varName, float varValue)
{
    int index;

    index = worldPhysicsVarNameToIndex(varName);

    if (index != -1) {
        _physicsInfo[index] = varValue;
    }
}

float World::getPhysicsVar(int index)
{
    return _physicsInfo[index];
}

float World::getPhysicsVar(const char *varName)
{
    int index;

    index = worldPhysicsVarNameToIndex(varName);

    if (index != -1) {
        return getPhysicsVar(index);
    } else {
        return -1.0f;
    }
}


int World::findBrokenThing(const char *name)
{
    int  i;
    str *brokenThingName;

    for (i = 1; i <= _brokenThings.NumObjects(); i++) {
        brokenThingName = &_brokenThings.ObjectAt(i);

        if (Q_stricmp(brokenThingName->c_str(), name) == 0) {
            return i;
        }
    }

    return 0;
}

void World::addBrokenThing(Event *ev)
{
    addBrokenThing(ev->GetString(1));
}

void World::removeBrokenThing(Event *ev)
{
    removeBrokenThing(ev->GetString(1));
}

void World::addBrokenThing(const char *name)
{
    if (!findBrokenThing(name)) {
        _brokenThings.AddObject(str(name));
    }
}

void World::removeBrokenThing(const char *name)
{
    int index;

    index = findBrokenThing(name);

    if (index) {
        _brokenThings.RemoveObjectAt(index);
    }
}

void World::freeAllBrokenThings(void)
{
    _brokenThings.FreeObjectList();
}

bool World::isThingBroken(const char *name)
{
    return findBrokenThing(name) != 0;
}

void World::setCanShakeCamera(Event *ev)
{
    _canShakeCamera = ev->GetBoolean(1);
}

bool World::canShakeCamera(void)
{
    return _canShakeCamera;
}










TargetList *World::GetTargetList(const str &targetName, qboolean createnew)
{
    TargetList *targetList;
    int         i;

    if (!targetName.length()) {
        return NULL;
    }

    for (i = m_targetListContainer.NumObjects(); i > 0; i--) {
        targetList = m_targetListContainer.ObjectAt(i);
        if (targetName == targetList->targetname) {
            return targetList;
        }
    }

    if (!createnew) {
        return NULL;
    }

    // ⚠ THE INDEX WAS NEVER ASSIGNED HERE, and TargetList's constructor leaves it 0.  It
    targetList       = new TargetList(targetName);
    targetList->index = m_targetListContainer.AddObject(targetList);

    return targetList;
}

TargetList *World::GetTargetList(int index)
{
    if (index <= 0 || index > m_targetListContainer.NumObjects()) {
        return NULL;
    }

    return m_targetListContainer.ObjectAt(index);
}

void World::AddTargetEntity(const str &targetname_shadow1, Entity *ent)
{
    TargetList *targetlist;

    targetlist = GetTargetList(targetname_shadow1);
    targetlist->AddEntity(ent);
}



void World::RemoveTargetEntity(const str& targetname_shadow1, Entity *ent) 
{
    TargetList *targetlist;

    if (world_dying) {
        return;
    }

    targetlist = GetTargetList(targetname_shadow1);
    targetlist->RemoveEntity(ent);
}

void World::RemoveTargetEntity(Entity *ent)
{
    if (world->world_dying) {
        return;
    }

    TargetList *targetList = GetTargetList(ent->TargetName(), qfalse);
    if (!targetList) {
        return;
    }

    targetList->RemoveEntity(ent);
}

Entity *World::GetNextEntity(const str& targetName, Entity *ent)  
{
    TargetList *targetList = GetTargetList(targetName, qfalse);
    if (!targetList) {
        return NULL;
    }

    return targetList->GetNextEntity(ent);
}

void World::FreeTargetList()
{
    int i;

    for (i = 1; i <= m_targetListContainer.NumObjects(); i++) {
        delete m_targetListContainer.ObjectAt(i);
    }

    m_targetListContainer.FreeObjectList();
}

void World::Archive(Archiver& arc)
{
    int         num;
    int         i;
    TargetList *targetList;

    Entity::Archive(arc);

    if (arc.Loading()) {
        for (i = 1; i <= m_targetListContainer.NumObjects(); i++) {
            delete m_targetListContainer.ObjectAt(i);
        }
        m_targetListContainer.FreeObjectList();
    } else {
        num = m_targetListContainer.NumObjects();
    }

    arc.ArchiveInteger(&num);

    if (arc.Loading() && (num < 0 || num > 8192)) {
        arc.FileError(
            "World: target-list count %d is out of range -- Entity::Archive has consumed a "
            "different number of bytes than this file holds",
            num
        );
        return;
    }

    for (i = 1; i <= num; i++) {
        if (arc.Saving()) {
            targetList = m_targetListContainer.ObjectAt(i);
        } else {
            targetList = new TargetList;
            m_targetListContainer.AddObject(targetList);
        }

        arc.ArchiveObject((Class *)targetList);
    }

    _brokenThings.Archive(arc);
    _availableViewModes.Archive(arc);

    arc.ArchiveBoolean(&world_dying);

    arc.ArchiveString(&skipthread);

    arc.ArchiveFloat(&farplane_distance);
    arc.ArchiveVector(&farplane_color);
    arc.ArchiveBoolean(&farplane_cull);
    arc.ArchiveBoolean(&farplane_fog);
    arc.ArchiveBoolean(&terrain_global);
    arc.ArchiveFloat(&terrain_global_min);
    arc.ArchiveFloat(&entity_fade_dist);


    for (i = 0; i < MAX_LIGHTING_GROUPS; i++) {
        dynamic_lights[i].Archive(arc);
    }

    wind.Archive(arc);
    weather.Archive(arc);

    arc.ArchiveFloat(&time_scale);

    arc.ArchiveFloat(&sky_alpha);
    arc.ArchiveBoolean(&sky_portal);

    for (i = 0; i < WORLD_PHYSICS_TOTAL_NUMBER; i++) {
        arc.ArchiveFloat(&_physicsInfo[i]);
    }

    arc.ArchiveBool(&_canShakeCamera);

    if (arc.Loading()) {
        UpdateConfigStrings();
        UpdateFog();
        UpdateTerrain();
        UpdateEntityFadeDist();
        UpdateSky();
        UpdateDynamicLights();
        UpdateWindDirection();
        UpdateWindIntensity();
        UpdateWeather();
        UpdateTimeScale();
    }

    if (arc.Loading()) {
        if (groupcoordinator) {
            delete groupcoordinator;
        }

        groupcoordinator = new GroupCoordinator;
    }

    groupcoordinator->Archive(arc);

    UpdateConfigStrings();
}


void World::Think(void)
{
    int               i;
    DynamicLightInfo *dlight;


    for (i = 0; i < MAX_LIGHTING_GROUPS; i++) {
        dlight = &dynamic_lights[i];

        if (dlight->fade_time) {
            if (level.time > (dlight->start_fade_time + dlight->fade_time)) {
                dlight->intensity = dlight->end_intensity;
                dlight->fade_time = 0;
            } else {
                dlight->intensity =
                    dlight->start_intensity
                    + ((dlight->end_intensity - dlight->start_intensity)
                       * ((level.time - dlight->start_fade_time) / dlight->fade_time));
            }
        } else if (dlight->lightstyle.length()) {
            dlight->current_lightstyle_position++;

            if (dlight->current_lightstyle_position >= (int)dlight->lightstyle.length()) {
                if (dlight->stop_at_end) {
                    dlight->lightstyle                  = "";
                    dlight->current_lightstyle_position = -1;
                } else {
                    dlight->current_lightstyle_position = 0;
                }
            }

            if (dlight->current_lightstyle_position >= 0) {
                dlight->intensity = (dlight->lightstyle[dlight->current_lightstyle_position] - 'a') / 25.0f;
            }
        }
    }

    UpdateDynamicLights();


    if (wind.gust_time) {
        if (level.time < (wind.start_fade_time + wind.fade_time)) {
            wind.intensity =
                wind.start_intensity
                + ((wind.end_intensity - wind.start_intensity)
                   * ((level.time - wind.start_fade_time) / wind.fade_time));
        } else if (level.time < (wind.start_fade_time + wind.fade_time + wind.gust_time)) {
            wind.intensity = wind.end_intensity;
        } else {
            wind.intensity =
                wind.end_intensity
                + ((wind.start_intensity - wind.end_intensity)
                   * ((level.time - wind.start_fade_time - wind.fade_time - wind.gust_time) / wind.fade_time));

            if (level.time > (wind.start_fade_time + wind.fade_time + wind.gust_time + wind.fade_time)) {
                wind.intensity = wind.start_intensity;
                wind.fade_time = 0;
                wind.gust_time = 0;
            }
        }
    } else if (wind.fade_time) {
        if (level.time > (wind.start_fade_time + wind.fade_time)) {
            wind.intensity = wind.end_intensity;
            wind.fade_time = 0;
        } else {
            wind.intensity =
                wind.start_intensity
                + ((wind.end_intensity - wind.start_intensity)
                   * ((level.time - wind.start_fade_time) / wind.fade_time));
        }
    } else if ((wind.min_intensity > 0.0f) && (wind.max_intensity > 0.0f)) {
        wind.intensity += G_CRandom(wind.max_change);

        if (wind.intensity > wind.max_intensity) {
            wind.intensity = wind.max_intensity;
        }

        if (wind.intensity < wind.min_intensity) {
            wind.intensity = wind.min_intensity;
        }
    }

    UpdateWindDirection();
    UpdateWindIntensity();
    UpdateTimeScale();

    float worldGravity;


    worldGravity = getPhysicsVar(WORLD_PHYSICS_GRAVITY);


    if (worldGravity == -1) {
        worldGravity = sv_gravity->value;
    }


    if (sv_currentGravity->value != worldGravity) {
        gi.cvar_set("sv_currentGravity", va("%f", worldGravity));
    }
}

DynamicLightInfo::DynamicLightInfo()
{
    defaultIntensity = 0.5f;

    intensity = 1.0f;
    fade_time = 0.0f;

    start_intensity = 0.0f;
    end_intensity   = 0.0f;

    start_fade_time = 0.0f;

    current_lightstyle_position = 0;
    stop_at_end                 = qfalse;
}

void DynamicLightInfo::Archive(Archiver& arc)
{
    arc.ArchiveFloat(&defaultIntensity);

    arc.ArchiveFloat(&intensity);
    arc.ArchiveFloat(&start_intensity);
    arc.ArchiveFloat(&end_intensity);

    arc.ArchiveFloat(&start_fade_time);
    arc.ArchiveFloat(&fade_time);

    arc.ArchiveString(&lightstyle);
    arc.ArchiveInteger(&current_lightstyle_position);
    arc.ArchiveBoolean(&stop_at_end);
}

WindInfo::WindInfo()
{
    intensity       = 0.0f;
    fade_time       = 0.0f;
    gust_time       = 0.0f;
    start_fade_time = 0.0f;
    max_change      = 0.0f;

    min_intensity = -1.0f;
    max_intensity = -1.0f;

    start_intensity = 0.0f;
    end_intensity   = 0.0f;
    direction       = Vector(1, 1, 0);
}


WeatherInfo::WeatherInfo()
{
    type      = WEATHER_NONE;
    intensity = 0;
}



CLASS_DECLARATION(Class, TargetList, NULL) {
    {NULL, NULL}
};

TargetList::TargetList()
    : index(0)
{}

TargetList::TargetList(const str& tname)
    : targetname(tname)
    , index(0)
{}

void TargetList::Archive(Archiver& arc)
{
    int num = 0;
    int i;

    Class::Archive(arc);

    if (arc.Saving()) {
        num = list.NumObjects();
    }

    arc.ArchiveInteger(&num);

    if (arc.Loading()) {
        list.ClearObjectList();
        list.Resize(num);
    }

    for (i = 1; i <= num; i++) {
        if (arc.Loading()) {
            list.AddObject(SafePtr<Entity>(NULL));
        }
        arc.ArchiveSafePointer(list.AddressOfObjectAt(i));
    }

    arc.ArchiveString(&targetname);
    arc.ArchiveInteger(&index);
}

void TargetList::AddEntity(Listener *ent)
{
    if (!list.ObjectInList(static_cast<Entity *>(ent))) {
#pragma GCC diagnostic push
#if !defined(__clang__) 
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
        list.AddObject(static_cast<Entity *>(ent));
#pragma GCC diagnostic pop
    }
}

// i.e. once per map, which retail never prints.
void TargetList::RemoveEntity(Listener *ent)
{
    if (list.ObjectInList(static_cast<Entity *>(ent))) {
        list.RemoveObject(static_cast<Entity *>(ent));
    }
}

Entity *TargetList::GetNextEntity(Entity *ent)
{
    int entIndex;

    if (ent) {
        entIndex = list.IndexOfObject(ent);
    } else {
        entIndex = 0;
    }

    entIndex++;

    if (entIndex <= list.NumObjects()) {
        return list.ObjectAt(entIndex);
    }

    return NULL;
}









TargetList::~TargetList()
{
}

