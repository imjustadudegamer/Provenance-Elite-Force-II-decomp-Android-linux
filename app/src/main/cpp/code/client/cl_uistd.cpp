
#include "cl_ui.h"
#include "../qcommon/gameplaymanager.h"

#define R120_PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define R120_PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_r120.h"
#include "../qcommon/stringresource.h"
#include "../corepp/tiki.h" 

// task: EF2 stat-system conversion).  Retail does NOT bound the read: a name the
static const char *const ef2PlayerStatNames[] = {
    "health",          "dead_yaw",           "ammo_left",             "clipammo_left",
    "num_shots_left",  "max_num_shots_left", "ammo_right",            "clipammo_right",
    "num_shots_right", "armor_level",        "maxammo_left",          "maxammo_right",
    "maxclipammo_left","maxclipammo_right",  "ammo_type1",            "ammo_type2",
    "ammo_type3",      "ammo_type4",         "last_pain",             "accumulated_pain",
    "bosshealth",      "bossnameIndex",      "cinematic",             "addfade",
    "letterbox",       "poweruptime",        "weapon_generic1",       "weapon_generic2",
    "itemicon",        "itemtext",           "votetext",              "enemies_killed",
    "teammates_killed","shots_fired",        "shots_hit",             "accuracy",
    "mission_duration","generic",            "num_objectives",        "complete_objectives",
    "failed_objectives","incomplete_objectives","specialmovetimer",   "points",
    "secretstotal",    "secretsfound",       "itemstotal",            "itemsfound",
    "redTeamScore",    "blueTeamScore",      "team",                  "score",
    "kills",           "deaths",             "timeleft_seconds",      "won_matches",
    "lost_matches",    "mp_generic1",        "mp_generic2",           "mp_generic3",
    "mp_generic4",     "mp_generic5",        "mp_generic6",           "mp_generic7",
    "mp_generic8",     "mp_spectating_entnum","mp_mode_icon",         "mp_team_icon",
    "mp_teamhud_icon", "mp_otherteam_icon",  "mp_specialty_icon",     "mp_holdableitem_icon",
    "mp_rune_icon",    "mp_powerup_icon",    "mp_award_icon",         "mp_award_count",
    "mp_state"
};
#define EF2_NUM_PLAYER_STATS ((int)(sizeof(ef2PlayerStatNames) / sizeof(ef2PlayerStatNames[0])))

int CL_GetStatByName(const char *name)
{
    if (!name) {
        return -1;
    }

    for (int i = 0; i < EF2_NUM_PLAYER_STATS; i++) {
        if (!Q_stricmp(name, ef2PlayerStatNames[i])) {
            return i;
        }
    }

    return -1;
}

// bounds test: a name PlayerStat_NameToNum does not know returns -1 and retail reads
// does not own that tree; the body is retail's.)
int CL_SafePlayerStat(int idx)
{
    return cl.snap.ps.stats[idx];
}

static qboolean CL_UIPaused(void)
{
    return paused && paused->integer;
}

Event EV_Layout_PlayerStat
(
    "playerstat",
    EV_DEFAULT,
    "s",
    "playerStatName",
    "set playerstat for layout\n"
);
Event EV_Layout_PlayerStatConfigstring
(
    "playerstatconfigstring",
    EV_DEFAULT,
    "i",
    "playerstatValue",
    "set playerstat for layout - will print out the corresponding configstring\n"
);
Event EV_Layout_MaxPlayerStat
(
    "maxplayerstat",
    EV_DEFAULT,
    "s",
    "maxPlayerstatName",
    "set the playerstat that holds the max value for that stat.  Used for statbars\n"
);
Event EV_Layout_StatbarRepeatShader
(
    "statbar_repeatshader",
    EV_DEFAULT,
    "s",
    "Shader",
    "set the repeat shader for the statbar"
);
Event EV_Layout_StatbarDirection
(
    "statbardirection",
    EV_DEFAULT,
    "s",
    "direction",
    "Sets the direction for that statbar to move. Choices are up, down, left, and right"
);
Event EV_Layout_StatMultiplier
(
    "statmultiplier",
    EV_DEFAULT,
    "ff",
    "max multiplier",
    "Sets the stat multiplier, if a stat exceeds the max, then the multiplier is used."   
);
Event EV_ColorStat_WarningLevel
(
    "warningLevel",
    EV_DEFAULT,
    "i",
    "warningLevel",
    "Sets the value at which the hud will start blinking"   
);
Event EV_ColorStat_WarningSound
(
    "warningSound",
    EV_DEFAULT,
    "si",
    "warningSound warningSoundFreq",
    "Sets up a warning sound to play when stat is too low."
);
Event EV_ColorStat_WarningSoundWhenChanged
(
    "warningSoundWhenChanged",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes a warning noise when changed (works with warningLevel also)."
);
Event EV_FakkLabel_DisplayStatAsTime
(
    "displaystatastime",
    EV_DEFAULT,
    NULL,
    NULL,
    "Displays the stat in hours:minutes:seconds. Title must be in seconds."
);
Event EV_FakkLabel_InvalidPlayerStatValue
(
    "invalidPlayerStatValue",
    EV_DEFAULT,
    "i",
    "invalidPlayerStatValue",
    "Hides the display of the player stat if it is equal to this\n"
);
// `statVisibility` is NOT registered on UIFakkLabel in retail -- it lives in
Event EV_FakkLabel_StatisticProperty
(
    "statisticProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Hooks this widget up to the specified gameplay object property, which is treatedlike a statistic.  A graphical bar is used to present it.  Use statbar as wellto define the stat bar properties."
);
Event EV_Layout_ItemIndex
(
    "itemstat",
    EV_DEFAULT,
    "i",
    "itemstatValue",
    "set itemstat for layout\n"
);
Event EV_Layout_InventoryRenderModelIndex
(
    "invmodelrender",
    EV_DEFAULT,
    "i",
    "handindex",
    "set the specified hand index to render a 3d model\n"
);
Event EV_Layout_Statbar
(
    "statbar",
    EV_DEFAULT,
    "sII",
    "orientation min max",
    "make this label present the stat using a graphical bar.  Optionally specify a min and max value"
);
Event EV_Layout_StatbarTileShader
(
    "statbar_tileshader",
    EV_DEFAULT,
    "s",
    "shader",
    "set the tile shader for this statbar"
);
Event EV_Layout_StatbarTileShader_Flash
(
    "statbar_tileshader_flash",
    EV_DEFAULT,
    "s",
    "shader",
    "set the flash tile shader for this statbar"
);
Event EV_Layout_StatbarShader
(
    "statbar_shader",
    EV_DEFAULT,
    "s",
    "shader",
    "set the shader for this statbar"
);
Event EV_Layout_StatbarShader_Flash
(
    "statbar_shader_flash",
    EV_DEFAULT,
    "s",
    "shader",
    "set the flash shader for this statbar"
);
Event EV_Layout_RenderModel
(
    "rendermodel",
    EV_DEFAULT,
    "b",
    "bool",
    "Render the model specified by the cvar."
);
Event EV_Layout_RenderModelOffset
(
    "modeloffset",
    EV_DEFAULT,
    "v",
    "offset",
    "Render model offset"
);
Event EV_Layout_RenderModelRotateOffset
(
    "modelrotateoffset",
    EV_DEFAULT,
    "v",
    "offset",
    "Render model rotation offset"
);
Event EV_Layout_RenderModelAngles
(
    "modelangles",
    EV_DEFAULT,
    "v",
    "angles",
    "Render model angles"
);
Event EV_Layout_RenderModelScale
(
    "modelscale",
    EV_DEFAULT,
    "f",
    "scale",
    "Render model scale"
);
Event EV_Layout_RenderModelAnim
(
    "modelanim",
    EV_DEFAULT,
    "s",
    "anim",
    "Render model anim"
);

CLASS_DECLARATION(UILabel, UIFakkLabel, NULL) {
    {&EV_Layout_PlayerStat,                &UIFakkLabel::LayoutPlayerStat             },
    {&EV_FakkLabel_InvalidPlayerStatValue, &UIFakkLabel::LayoutInvalidPlayerStatValue },
    {&EV_Layout_PlayerStatConfigstring,    &UIFakkLabel::LayoutPlayerStatConfigstring },
    {&EV_Layout_MaxPlayerStat,             &UIFakkLabel::LayoutMaxPlayerStat          },
    {&EV_Layout_ItemIndex,                 &UIFakkLabel::LayoutItemIndex              },
    {&EV_Layout_InventoryRenderModelIndex, &UIFakkLabel::InventoryRenderModelIndex    },
    {&EV_Layout_Statbar,                   &UIFakkLabel::LayoutStatbar                },
    {&EV_Layout_StatbarRepeatShader,       &UIFakkLabel::LayoutStatbarRepeatShader    },
    {&EV_Layout_StatbarTileShader,         &UIFakkLabel::LayoutStatbarTileShader      },
    {&EV_Layout_StatbarTileShader_Flash,   &UIFakkLabel::LayoutStatbarTileShader_Flash},
    {&EV_Layout_StatbarShader,             &UIFakkLabel::LayoutStatbarShader          },
    {&EV_Layout_StatbarShader_Flash,       &UIFakkLabel::LayoutStatbarShader_Flash    },
    {&EV_Layout_RenderModel,               &UIFakkLabel::LayoutRenderModel            },
    {&EV_Layout_RenderModelOffset,         &UIFakkLabel::LayoutRenderModelOffset      },
    {&EV_Layout_RenderModelRotateOffset,   &UIFakkLabel::LayoutRenderModelRotateOffset},
    {&EV_Layout_RenderModelAngles,         &UIFakkLabel::LayoutRenderModelAngles      },
    {&EV_Layout_RenderModelScale,          &UIFakkLabel::LayoutRenderModelScale       },
    {&EV_Layout_RenderModelAnim,           &UIFakkLabel::LayoutRenderModelAnim        },
    {&EV_FakkLabel_StatisticProperty,      &UIFakkLabel::LayoutGameplayStatisticProperty},
    {&EV_FakkLabel_DisplayStatAsTime,      &UIFakkLabel::displayStatAsTime            },
    {&EV_ColorStat_WarningLevel,           &UIFakkLabel::setWarningLevel              },
    {&EV_ColorStat_WarningSound,           &UIFakkLabel::setWarningSound              },
    {&EV_ColorStat_WarningSoundWhenChanged,&UIFakkLabel::setWarningSoundWhenChanged   },
    {&EV_Layout_StatMultiplier,            &UIFakkLabel::LayoutStatMultiplier         },
    {&EV_Layout_StatbarDirection,          &UIFakkLabel::LayoutStatbarDirection       },
    {NULL,                                 NULL                                       }
};

UIFakkLabel::UIFakkLabel()
{
    m_statbar_min                = -1.0f;
    m_statbar_max                = -1.0f;
    m_stat                       = -1;
    m_stat_configstring          = -1;
    m_maxstat                    = -1;
    m_itemindex                  = -1;
    m_inventoryrendermodelindex  = -1;
    m_rendermodel                = false;
    m_statbar_or                 = L_STATBAR_NONE;
    m_statbar_material           = NULL;
    m_statbar_material_flash     = NULL;
    m_lastitemindex              = -1;
    m_statbarDir                 = L_STATBARDIR_NONE;
    m_displayStatAsTime          = false;
    m_invalidPlayerStatValue     = 0;
    m_hasInvalidPlayerStatValue  = false;
    m_useGameplayProperty        = false;
    m_warningLevel               = 0;
    m_warningSoundFreq             = 0;
    m_lastWarnSoundTime          = 0;
    m_warnSoundWhenChanged       = 0;
    m_scale                      = 1.0f;
    VectorClear(m_offset);
    VectorClear(m_rotateoffset);
    VectorClear(m_angles);
}

void UIFakkLabel::displayStatAsTime(Event *ev)
{
    m_displayStatAsTime = true;
}

void UIFakkLabel::LayoutInvalidPlayerStatValue(Event *ev)
{
    m_invalidPlayerStatValue     = ev->GetInteger(1);
    m_hasInvalidPlayerStatValue  = true;
}

void UIFakkLabel::LayoutGameplayStatisticProperty(Event *ev)
{
    str group;
    str name;

    if (ev->NumArgs() < 1) {
        ev->Error("Index %d out of range.", 1);
    } else {
        group = ev->GetString(1);
    }
    if (ev->NumArgs() < 2) {
        ev->Error("Index %d out of range.", 2);
    } else {
        name = ev->GetString(2);
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm) {
        Com_Printf(
            "ERROR: Couldn't find GameplayManager to look up object property: %s.%s\n",
            group.c_str(),
            name.c_str()
        );
        return;
    }

    m_stringPropertyGroup = group;
    m_stringPropertyName  = name;
    m_useGameplayProperty = true;

    if (!gpm->hasProperty(m_stringPropertyGroup, m_stringPropertyName)) {
        Com_Printf("WARNING: Couldn't find gameplay property %s.%s\n", group.c_str(), name.c_str());
    }
}

void UIFakkLabel::updateGameplayProperty(void)
{
    GameplayManager *gpm;

    UIWidget::updateGameplayProperty();

    if (!m_useGameplayProperty) {
        return;
    }

    gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) {
        return;
    }

    DrawStatbar(gpm->getFloatValue(m_stringPropertyGroup, m_stringPropertyName));
}

void UIFakkLabel::LayoutPlayerStat(Event *ev)
{
    m_stat = CL_GetStatByName(ev->GetString(1));
    if (m_stat < 0) {
        Com_Printf("Couldn't find player stat %s\n", ev->GetString(1));
        m_stat = 0;
    }
}

void UIFakkLabel::LayoutStatbarDirection(Event *ev)
{
    str dir = ev->GetString(1);

    m_statbarDir = L_STATBARDIR_NONE;

    if (!str::icmp(dir, "right")) {
        m_statbarDir = L_STATBARDIR_RIGHT;
    } else if (!str::icmp(dir, "up")) {
        m_statbarDir = L_STATBARDIR_UP;
    } else if (!str::icmp(dir, "down")) {
        m_statbarDir = L_STATBARDIR_DOWN;
    }
}

void UIFakkLabel::LayoutStatMultiplier(Event *ev)
{
    statmultiplier_t sm;

    sm.max        = ev->GetFloat(1);
    sm.multiplier = ev->GetFloat(2);
    m_statMultipliers.AddObject(sm);
}

void UIFakkLabel::setWarningLevel(Event *ev)
{
    m_warningLevel = ev->GetInteger(1);
}

void UIFakkLabel::setWarningSound(Event *ev)
{
    m_warningSound   = ev->GetString(1);
    m_warningSoundFreq = ev->GetInteger(2);
}

void UIFakkLabel::setWarningSoundWhenChanged(Event *ev)
{
    m_warnSoundWhenChanged = 1;
}

void UIFakkLabel::LayoutPlayerStatConfigstring(Event *ev)
{
    m_stat_configstring = CL_GetStatByName(ev->GetString(1));
    if (m_stat_configstring < 0) {
        Com_Printf("Couldn't find player stat %s\n", ev->GetString(1));
        m_stat_configstring = 0;
    }
}

void UIFakkLabel::LayoutMaxPlayerStat(Event *ev)
{
    m_maxstat = CL_GetStatByName(ev->GetString(1));
    if (m_maxstat < 0) {
        Com_Printf("Couldn't find player stat %s\n", ev->GetString(1));
        m_maxstat = 0;
    }
}

void UIFakkLabel::LayoutItemIndex(Event *ev)
{
    m_itemindex = ev->GetInteger(1);
}

void UIFakkLabel::InventoryRenderModelIndex(Event *ev)
{
    m_inventoryrendermodelindex = ev->GetInteger(1);
}

void UIFakkLabel::LayoutStatbar(Event *ev)
{
    str _or = ev->GetString(1);

    if (ev->NumArgs() > 1) {
        m_statbar_min = ev->GetFloat(2);
        m_statbar_max = ev->GetFloat(3);
    }

    if (!str::icmp(_or, "horizontal")) {
        m_statbar_or = L_STATBAR_HORIZONTAL;
    } else if (!str::icmp(_or, "vertical")) {
        m_statbar_or = L_STATBAR_VERTICAL;
    } else {
        warning("UIFakkLabel::LayoutPlayerStat", "Invalid statbar orientation: \"%s\"", _or.c_str());
    }
}

void UIFakkLabel::LayoutStatbarShader(Event *ev)
{
    m_statbar_material = uWinMan.RegisterShader(ev->GetString(1));
}

void UIFakkLabel::LayoutStatbarTileShader(Event *ev)
{
    m_statbar_material = uWinMan.RegisterShader(ev->GetString(1));
    m_flags |= WF_TILESHADER;
}

void UIFakkLabel::LayoutStatbarShader_Flash(Event *ev)
{
    m_statbar_material_flash = uWinMan.RegisterShader(ev->GetString(1));
}

void UIFakkLabel::LayoutStatbarRepeatShader(Event *ev)
{
    m_statbar_material = uWinMan.RegisterShader(ev->GetString(1));
    m_flags = WF_STATBAR_REPEATSHADER;
}

void UIFakkLabel::LayoutStatbarTileShader_Flash(Event *ev)
{
    m_statbar_material_flash = uWinMan.RegisterShader(ev->GetString(1));
    m_flags |= WF_TILESHADER;
}

void UIFakkLabel::LayoutRenderModel(Event *ev)
{
    m_rendermodel = ev->GetBoolean(1);
}

void UIFakkLabel::LayoutRenderModelOffset(Event *ev)
{
    Vector vec = ev->GetVector(1);
    VectorCopy(vec, m_offset);
}

void UIFakkLabel::LayoutRenderModelRotateOffset(Event *ev)
{
    Vector vec = ev->GetVector(1);
    VectorCopy(vec, m_rotateoffset);
}

void UIFakkLabel::LayoutRenderModelAngles(Event *ev)
{
    Vector vec = ev->GetVector(1);
    VectorCopy(vec, m_angles);
}

void UIFakkLabel::LayoutRenderModelScale(Event *ev)
{
    m_scale = ev->GetFloat(1);
}

void UIFakkLabel::LayoutRenderModelAnim(Event *ev)
{
    m_anim = ev->GetString(1);
}

void UIFakkLabel::DrawStatbar(float frac)
{
    UColor fgcolor;
    UColor flashcolor;
    float  x, y;
    float  t;
    int    ms;

    if (frac == 0.0f) {
        return;
    }

    ms = Sys_Milliseconds();

    if (m_warningLevel > 0 && 100.0f * frac <= (float)m_warningLevel) {
        if (ms > m_lastWarnSoundTime + 1000) {
            m_lastWarnSoundTime = ms;
            return;
        }
        if (ms <= m_lastWarnSoundTime + 500) {
            return;
        }
    }

    if (frac != m_lastfrac) {
        m_flashtime = (float)ms;
    }
    m_lastfrac = frac;

    t = 1.0f - ((float)ms - m_flashtime) / 750.0f;
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    flashcolor = UColor(t, t, t, t);

    x = 0.0f;
    y = 0.0f;
    if (m_statbar_or == L_STATBAR_HORIZONTAL && m_statbarDir == L_STATBARDIR_RIGHT) {
        x = (1.0f - frac) * m_frame.size.width;
    } else if (m_statbar_or == L_STATBAR_VERTICAL && m_statbarDir == L_STATBARDIR_DOWN) {
        y = (1.0f - frac) * m_frame.size.height;
    }

    if (!m_statbar_material) {
        DrawBox(x, y, m_frame.size.width * frac, m_frame.size.height, getForegroundColor(), 1.0f);
        return;
    }

    fgcolor = getForegroundColor();
    re.SetColor(fgcolor);

    if (m_flags & WF_TILESHADER) {
        // The 0x8 path never writes s1/t1/s2/t2 in retail (they keep whatever the
        if (m_statbar_or == L_STATBAR_HORIZONTAL) {
            re.DrawStretchPic(
                x,
                y,
                m_frame.size.width * frac,
                m_frame.size.height,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                m_statbar_material->GetMaterial()
            );

            if (t != 0.0f && m_statbar_material_flash) {
                re.SetColor(flashcolor);
                re.DrawStretchPic(
                    x,
                    y,
                    m_frame.size.width * frac,
                    m_frame.size.height,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f,
                    m_statbar_material_flash->GetMaterial()
                );
            }
        } else if (m_statbar_or == L_STATBAR_VERTICAL) {
            re.DrawStretchPic(
                x, y, m_frame.size.width, m_frame.size.height, 0.0f, 0.0f, 1.0f, 1.0f, m_statbar_material->GetMaterial()
            );

            if (t != 0.0f && m_statbar_material_flash) {
                re.SetColor(flashcolor);
                re.DrawStretchPic(
                    x,
                    y,
                    m_frame.size.width,
                    m_frame.size.height,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f,
                    m_statbar_material_flash->GetMaterial()
                );
            }
        }
        return;
    }

    if (m_flags & WF_STATBAR_REPEATSHADER) {
        float multiplier = 1.0f;
        float tile;
        float s1, s2, t1, t2;
        int   i;

        for (i = 1; i <= m_statMultipliers.NumObjects(); i++) {
            const statmultiplier_t sm = m_statMultipliers.ObjectAt(i);

            if ((float)CL_SafePlayerStat(m_maxstat) >= sm.max) {
                multiplier = sm.multiplier;
            }
        }

        tile = (float)CL_SafePlayerStat(m_stat) / multiplier - 0.125f;

        if (m_statbar_or == L_STATBAR_HORIZONTAL) {
            s1 = (m_statbarDir == L_STATBARDIR_RIGHT) ? tile : 0.0f;
            s2 = (m_statbarDir == L_STATBARDIR_RIGHT) ? 0.0f : tile;
            t1 = 0.0f;
            t2 = 1.0f;

            re.DrawStretchPic(
                x, y, m_frame.size.width * frac, m_frame.size.height, s1, t1, s2, t2, m_statbar_material->GetMaterial()
            );

            if (t != 0.0f && m_statbar_material_flash) {
                re.SetColor(flashcolor);
                re.DrawStretchPic(
                    x,
                    y,
                    m_frame.size.width * frac,
                    m_frame.size.height,
                    s1,
                    t1,
                    s2,
                    t2,
                    m_statbar_material_flash->GetMaterial()
                );
            }
        } else if (m_statbar_or == L_STATBAR_VERTICAL) {
            s1 = 0.0f;
            s2 = 1.0f;
            t1 = (m_statbarDir == L_STATBARDIR_DOWN) ? tile : 0.0f;
            t2 = (m_statbarDir == L_STATBARDIR_DOWN) ? 0.0f : tile;

            re.DrawStretchPic(x, y, m_frame.size.width, m_frame.size.height * frac, s1, t1, s2, t2, m_statbar_material->GetMaterial());

            if (t != 0.0f && m_statbar_material_flash) {
                re.SetColor(flashcolor);
                re.DrawStretchPic(
                    x,
                    y,
                    m_frame.size.width,
                    m_frame.size.height * frac,
                    s1,
                    t1,
                    s2,
                    t2,
                    m_statbar_material_flash->GetMaterial()
                );
            }
        }
        return;
    }

    if (m_statbar_or == L_STATBAR_HORIZONTAL) {
        re.DrawStretchPic(
            x, y, m_frame.size.width * frac, m_frame.size.height, 0.0f, 0.0f, 1.0f, 1.0f, m_statbar_material->GetMaterial()
        );

        if (t != 0.0f && m_statbar_material_flash) {
            re.SetColor(flashcolor);
            re.DrawStretchPic(
                x,
                y,
                m_frame.size.width * frac,
                m_frame.size.height,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                m_statbar_material_flash->GetMaterial()
            );
        }
    } else if (m_statbar_or == L_STATBAR_VERTICAL) {
        re.DrawStretchPic(
            x, y, m_frame.size.width, m_frame.size.height * frac, 0.0f, 0.0f, 1.0f, 1.0f, m_statbar_material->GetMaterial()
        );

        if (t != 0.0f && m_statbar_material_flash) {
            re.SetColor(flashcolor);
            re.DrawStretchPic(
                x,
                y,
                m_frame.size.width,
                m_frame.size.height * frac,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                m_statbar_material_flash->GetMaterial()
            );
        }
    }
}

void UIFakkLabel::Draw(void)
{
    // NOTE: `stringProperty "RadarDialog" "RadarString"` is NOT handled here.  Retail

    if (m_stat == -1 && m_itemindex == -1 && m_inventoryrendermodelindex == -1 && m_stat_configstring == -1
        && !m_rendermodel && m_statbar_or == L_STATBAR_NONE) {
        UILabel::Draw();
        return;
    }

    if (m_stat_configstring != -1) {
        const int csIndex = CL_SafePlayerStat(m_stat_configstring);

        if (csIndex >= 0 && CL_ConfigString(csIndex)) {
            setTitle(CL_ConfigString(csIndex));
            UILabel::Draw();
        }
        return;
    }

    if (m_stat != -1) {
        const float value = (float)CL_SafePlayerStat(m_stat);

        if (m_statbar_or != L_STATBAR_NONE) {
            float frac;

            if (m_maxstat >= 0) {
                frac = value / (float)CL_SafePlayerStat(m_maxstat);
            } else {
                frac = value / (m_statbar_max - m_statbar_min);
            }

            if (frac > 1.0f) {
                frac = 1.0f;
            } else if (frac < 0.0f) {
                frac = 0.0f;
            }

            DrawStatbar(frac);
        } else {
            if (m_hasInvalidPlayerStatValue && value == (float)m_invalidPlayerStatValue) {
                return;
            }

            m_font->setColor(getForegroundColor());

            if (m_displayStatAsTime) {
                int   secs = (int)value;
                str   stamp = va("%02d:%02d:%02d", secs / 3600, (secs % 3600) / 60, secs % 60);

                setTitle(stamp);
            } else {
                if (m_warnSoundWhenChanged) {
                    str valueStr = va("%d", (int)value);
                    str title(getTitle());

                    if (title != valueStr || strcmp(title.c_str(), valueStr.c_str()) != 0) {
                        if (!m_warningLevel || value < (float)m_warningLevel) {
                            uii.Snd_PlaySound(m_warningSound, CHAN_MENU);
                        }
                    }
                }

                setTitle(va("%d", (int)value));
            }

            UILabel::Draw();
        }

        return;
    }


    float     scale;
    qhandle_t handle;
    vec3_t    origin;
    vec3_t    mins;
    vec3_t    maxs;
    vec3_t    angles;
    vec3_t    offset;
    vec3_t    rotateoffset;
    str       sAnimName;
    float     height;

    if (m_itemindex != -1) {
        if (m_itemindex < 0) {
            return;
        }

        if (cl.snap.ps.activeItems[m_itemindex] < 0 || cl.snap.ps.activeItems[m_itemindex] >= MAX_WEAPONS) {
            return;
        }

        m_font->setColor(m_foreground_color);

        m_font->PrintJustified(
            getClientFrame(),
            m_iFontAlignmentHorizontal,
            m_iFontAlignmentVertical,
            CL_ConfigString(CS_ITEMS + cl.snap.ps.activeItems[m_itemindex]), UColor(0.0f, 0.0f, 0.0f, 0.0f), m_bVirtual);

        return;
    }

    if (m_inventoryrendermodelindex == -1 && !m_rendermodel) {
        float frac = 0.0;

        if (m_statbar_or == L_STATBAR_NONE) {
            return;
        }

        if (m_cvarname.length()) {
            frac = Cvar_VariableValue(m_cvarname) / (m_statbar_max - m_statbar_min);
        }

        if (frac > 1.0) {
            frac = 1.0;
        }
        if (frac < 0.0) {
            frac = 0.0;
        }

        DrawStatbar(frac);
        return;
    }

    if (m_rendermodel) {
        if (!m_cvarname.length()) {
            return;
        }

        handle = re.RegisterModel(Cvar_VariableString(m_cvarname));
        if (!handle) {
            return;
        }

        VectorCopy(m_angles, angles);
        VectorCopy(m_rotateoffset, rotateoffset);
        VectorCopy(m_offset, offset);
        scale     = m_scale;
        sAnimName = m_anim;

    } else {
        if (m_inventoryrendermodelindex >= 0 && m_lastitemindex != cl.snap.ps.activeItems[m_inventoryrendermodelindex]) {
            m_lastitem = CL_GetInvItemByName(
                &client_inv, CL_ConfigString(CS_ITEMS + cl.snap.ps.activeItems[m_inventoryrendermodelindex])
            );
            m_lastitemindex = cl.snap.ps.activeItems[m_inventoryrendermodelindex];
        }

        if (!m_lastitem) {
            return;
        }

        VectorCopy(m_lastitem->hudprops.rotateoffset, rotateoffset);
        VectorCopy(m_lastitem->hudprops.offset, offset);
        scale  = m_lastitem->hudprops.scale;
        handle = re.RegisterModel(m_lastitem->hudprops.model);
        VectorCopy(m_lastitem->hudprops.angledeltas, angles);

        if (m_lastitem->hudprops.move == INV_MOVE_BOB) {
            float frac = (float)uid.time / 333.0f;
            angles[0] += sin(frac) * m_lastitem->hudprops.angledeltas[0];
            angles[1] += sin(frac) * m_lastitem->hudprops.angledeltas[1];
            angles[2] += sin(frac) * m_lastitem->hudprops.angledeltas[2];
        } else if (m_lastitem->hudprops.move == INV_MOVE_SPIN) {
            float frac = (float)uid.time / 333.0f;
            angles[0] += frac * m_lastitem->hudprops.angledeltas[0];
            angles[1] += frac * m_lastitem->hudprops.angledeltas[1];
            angles[2] += frac * m_lastitem->hudprops.angledeltas[2];
        }
    }

    re.ModelBounds(handle, mins, maxs);

    origin[1] = (mins[1] + maxs[1]) * 0.5f;
    origin[2] = 0.0f;

    height = maxs[2] - mins[2];

    if (height < maxs[1] - mins[1]) {
        height = maxs[1] - mins[1];
    }

    if (height < maxs[0] - mins[0]) {
        height = maxs[0] - mins[0];
    }

    origin[0] = 0.5f * scale * height / 0.268f;

    CL_Draw3DModel(
        m_screenframe.pos.x,
        m_screenframe.pos.y,
        m_screenframe.size.width,
        m_screenframe.size.height,
        handle,
        origin,
        rotateoffset,
        offset,
        angles,
        sAnimName,
        m_rendermodel,
        NULL,
        30.0f,
        30.0f
    );

    set2D();
}


Event EV_ColorStat_IncreaseColor(
    "increasecolor", EV_DEFAULT, "ffff", "red green blue alpha", "Increase color"
);
Event EV_ColorStat_DecreaseColor(
    "decreasecolor", EV_DEFAULT, "ffff", "red green blue alpha", "Decrease color"
);

CLASS_DECLARATION(UIFakkLabel, UIColorStatLabel, NULL) {
    {&EV_ColorStat_IncreaseColor, &UIColorStatLabel::setIncreaseColor},
    {&EV_ColorStat_DecreaseColor, &UIColorStatLabel::setDecreaseColor},
    {NULL,                        NULL                               }
};

UIColorStatLabel::UIColorStatLabel()
{
    m_lastStatValue  = -1;
    m_flashTime      = 0.0f;
    m_increaseColor  = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_decreaseColor  = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_foregroundSave = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_flashColor     = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_warningCount   = 0;
}

void UIColorStatLabel::setIncreaseColor(Event *ev)
{
    m_increaseColor.r = ev->GetFloat(1);
    m_increaseColor.g = ev->GetFloat(2);
    m_increaseColor.b = ev->GetFloat(3);
    m_increaseColor.a = ev->GetFloat(4);
}

void UIColorStatLabel::setDecreaseColor(Event *ev)
{
    m_decreaseColor.r = ev->GetFloat(1);
    m_decreaseColor.g = ev->GetFloat(2);
    m_decreaseColor.b = ev->GetFloat(3);
    m_decreaseColor.a = ev->GetFloat(4);
}

void UIColorStatLabel::UpdateUIElement(void)
{
    float now = (float)(cls.realtime / 1000.0);

    if (m_stat < 0) {
        return;
    }

    int value = CL_SafePlayerStat(m_stat);

    if (m_lastStatValue == -1) {
        m_foregroundSave = getForegroundColor();
        m_flashColor     = getForegroundColor();
        m_flashTime      = now;
        m_lastStatValue  = value;
    }

    int delta = value - m_lastStatValue;
    m_lastStatValue = value;

    if (delta < -1) {
        setForegroundColor(m_decreaseColor);
        m_flashTime = now;
    }

    if (delta > 0) {
        setForegroundColor(m_increaseColor);
        m_flashTime = now;
    }

    if (now - m_flashTime > 0.25f) {
        setForegroundColor(m_flashColor);
    }

    if (m_warningLevel > 0 && value <= m_warningLevel) {
        float band;
        if (value > m_warningLevel / 4) {
            band = (value > m_warningLevel / 2) ? 0.75f : 0.5f;
        } else {
            band = 0.25f;
        }

        if (now - m_flashTime > 0.25f + band && !CL_UIPaused()) {
            setForegroundColor(m_decreaseColor);
            m_flashTime = now;
            m_warningCount++;

            if (m_warningSoundFreq > 0 && m_warningCount >= m_warningSoundFreq
                && m_warningSound.length() > 0) {
                S_StartLocalSound(m_warningSound.c_str(), false);
                m_warningCount = 0;
            }
        }
    }
}

CLASS_DECLARATION(UIColorStatLabel, UIAmmoColorLabel, NULL) {
    {NULL, NULL}
};

UIAmmoColorLabel::UIAmmoColorLabel()
{
    m_lastAmmoIndex = 0;
}

void UIAmmoColorLabel::UpdateUIElement(void)
{
    UIColorStatLabel::UpdateUIElement();

    int left = cl.snap.ps.activeItems[3];
    if (left != -1 && left != m_lastAmmoIndex) {
        m_lastAmmoIndex = left;
        setForegroundColor(m_foregroundSave);
    }

    int right = cl.snap.ps.activeItems[4];
    if (right != -1 && right != m_lastAmmoIndex) {
        m_lastAmmoIndex = right;
        setForegroundColor(m_foregroundSave);
    }
}

Event EV_Objective_Index
(
    "objective",
    EV_DEFAULT,
    "i",
    "index",
    "Sets this label to display information from the mission objectives list"
);
Event EV_Objective_IncompleteColor
(
    "incompletetextcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Sets the text color of an incomplete objective"
);
Event EV_Objective_CompleteColor
(
    "completetextcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Sets the text color of an complete objective"
);
Event EV_Objective_FailedColor
(
    "failedtextcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Sets the text color of an failed objective"
);
Event EV_Objective_DisplayType
(
    "displaytype",
    EV_DEFAULT,
    "s",
    "displayType",
    "Sets the display type of the objective."
);
Event EV_Objective_TextBox
(
    "textbox",
    EV_DEFAULT,
    "ffff",
    "x y width height",
    "Set the dimensions for the text box to display"
);
Event EV_Objective_IncompleteShader
(
    "incompleteshader", EV_DEFAULT, "s", "shader", "Sets the incomplete shader for the objective"
);
Event EV_Objective_CompleteShader
(
    "completeshader", EV_DEFAULT, "s", "shader", "Sets complete shader"
);
Event EV_Objective_FailedShader
(
    "failedshader", EV_DEFAULT, "s", "shader", "Sets the failed shader for the objective"
);

// SetObjectiveIndex (0x001BB370) that I could not recover.  It is wired here to
CLASS_DECLARATION(UILabel, UIObjectiveLabel, NULL) {
    {&EV_Objective_Index,             &UIObjectiveLabel::SetObjectiveIndex       },
    {&EV_Objective_TextBox,           &UIObjectiveLabel::LayoutTextBox         },
    {&EV_Objective_IncompleteColor,   &UIObjectiveLabel::SetIncompleteTextColor },
    {&EV_Objective_CompleteColor,     &UIObjectiveLabel::SetCompleteTextColor   },
    {&EV_Objective_FailedColor,       &UIObjectiveLabel::SetFailedTextColor     },
    {&EV_Objective_DisplayType,       &UIObjectiveLabel::SetDisplayType     },
    {&EV_Objective_IncompleteShader,  &UIObjectiveLabel::SetIncompleteShader},
    {&EV_Objective_CompleteShader,    &UIObjectiveLabel::SetCompleteShader  },
    {&EV_Objective_FailedShader,      &UIObjectiveLabel::SetFailedShader    },
    {NULL,                            NULL                                     }
};

UIObjectiveLabel::UIObjectiveLabel()
{
    m_objectiveIndex  = 0;
    m_incompleteColor = UColor(1, 1, 1, 1);
    m_completeColor   = UColor(0, 1, 0, 1);
    m_failedColor     = UColor(1, 0, 0, 1);
    m_displayType     = OBJDISP_TITLE;
    m_objectiveState  = 0;
    m_incompleteMat   = NULL;
    m_completeMat     = NULL;
    m_failedMat       = NULL;
}

void UIObjectiveLabel::SetObjectiveIndex(Event *ev)
{
    m_objectiveIndex = ev->GetInteger(1);
}

void UIObjectiveLabel::LayoutTextBox(Event *ev)
{
    m_textBox = UIRect2D(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4));
}

void UIObjectiveLabel::SetIncompleteTextColor(Event *ev)
{
    m_incompleteColor = UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4));
}

void UIObjectiveLabel::SetCompleteTextColor(Event *ev)
{
    m_completeColor = UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4));
}

void UIObjectiveLabel::SetFailedTextColor(Event *ev)
{
    m_failedColor = UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4));
}

void UIObjectiveLabel::SetDisplayType(Event *ev)
{
    // with a 0 "title" mode retail does not have, so the retail values are
    str type = ev->GetString(1);

    if (!str::icmp(type, "shader")) {
        m_displayType = 0;
    } else if (!str::icmp(type, "description")) {
        m_displayType = 1;
    } else if (!str::icmp(type, "status")) {
        m_displayType = 2;
    }
}

void UIObjectiveLabel::SetIncompleteShader(Event *ev)
{
    m_incompleteMat = uWinMan.RegisterShader(ev->GetString(1));
}

void UIObjectiveLabel::SetCompleteShader(Event *ev)
{
    m_completeMat = uWinMan.RegisterShader(ev->GetString(1));
}

void UIObjectiveLabel::SetFailedShader(Event *ev)
{
    m_failedMat = uWinMan.RegisterShader(ev->GetString(1));
}

int UIObjectiveLabel::HandleObjective(void)
{
    unsigned int states = (unsigned int)cl.snap.ps.objectiveStates;

    if (m_objectiveIndex < 1 || m_objectiveIndex > 8) {
        setShow(qfalse);
        return 0;
    }

    unsigned int showBit     = 1u << (3 * (m_objectiveIndex - 1) + 0);
    unsigned int completeBit = 1u << (3 * (m_objectiveIndex - 1) + 1);
    unsigned int failedBit   = 1u << (3 * (m_objectiveIndex - 1) + 2);

    if (!(states & showBit)) {
        setShow(qfalse);
        return 0;
    }

    m_objectiveState = (states & completeBit) ? 1 : 0;
    if (states & failedBit) {
        m_objectiveState = 2;
    }

    setShow(qtrue);
    return 1;
}

const char *UIObjectiveLabel::ObjectiveText(void)
{
    return MissionObjective_GetDescription(MissionObjective_GetNameFromIndex(m_objectiveIndex));
}

void UIObjectiveLabel::Draw(void)
{
    if (!HandleObjective()) {
        return;
    }

    if (m_objectiveState == 2) {
        m_foreground_color = m_failedColor;
    } else if (m_objectiveState == 1) {
        m_foreground_color = m_completeColor;
    } else {
        m_foreground_color = m_incompleteColor;
    }

    if (m_displayType == 0) {
        UIReggedMaterial *mat = m_incompleteMat;
        if (m_objectiveState == 2) {
            mat = m_failedMat;
        } else if (m_objectiveState == 1) {
            mat = m_completeMat;
        }

        if (mat && mat->GetMaterial()) {
            vec4_t col;
            col[0] = m_foreground_color.r;
            col[1] = m_foreground_color.g;
            col[2] = m_foreground_color.b;
            col[3] = m_foreground_color.a * m_local_alpha;
            uii.Rend_SetColor(col);
            uii.Rend_DrawPicStretched(
                0, 0, m_frame.size.width, m_frame.size.height, 0, 0, 1, 1, mat->GetMaterial()
            );
        }
        return;
    }

    const char *text;

    if (m_displayType == 2) {
        switch (m_objectiveState) {
        case 1:
            text = "$$ObjectiveComplete$$";
            break;
        case 2:
            text = "$$ObjectiveFailed$$";
            break;
        default:
            text = "$$ObjectiveIncomplete$$";
            break;
        }
    } else {
        text = ObjectiveText();
    }

    if (text && text[0]) {
        setTitle(text);
    }

    UILabel::Draw();
}

CLASS_DECLARATION(UIWidget, UIIcon, NULL) {
    {&EV_Layout_PlayerStat, &UIIcon::iconPlayerStat},
    {NULL,                  NULL                   }
};

UIIcon::UIIcon()
{
    m_stat    = 0;
    m_lastCs  = -1;
    m_iconMat = NULL;
}

void UIIcon::iconPlayerStat(Event *ev)
{
    m_stat = CL_GetStatByName(ev->GetString(1));
    if (m_stat < 0) {
        Com_Printf("Couldn't find player stat %s\n", ev->GetString(1));
        m_stat = 0;
    }
}

void UIIcon::Draw(void)
{
    if (m_stat < 0) {
        return;
    }

    int cs = CL_SafePlayerStat(m_stat);
    if (cs < 0) {
        return;
    }

    cs += CS_IMAGES;

    if (cs != m_lastCs) {
        m_lastCs  = cs;
        m_iconMat = uWinMan.RegisterShader(CL_ConfigString(cs));
    }

    if (!m_iconMat || !m_iconMat->GetMaterial()) {
        return;
    }

    vec4_t col;
    col[0] = m_foreground_color.r;
    col[1] = m_foreground_color.g;
    col[2] = m_foreground_color.b;
    col[3] = m_foreground_color.a * m_local_alpha;
    re.SetColor(col);
    re.DrawStretchPic(0, 0, m_frame.size.width, m_frame.size.height, 0, 0, 1, 1, m_iconMat->GetMaterial());
}

Event EV_Information_Index
(
    "information",
    EV_DEFAULT,
    "i",
    "index",
    "Sets this label to display information from the mission information list"
);
Event EV_Information_TextBox
(
    "textbox",
    EV_DEFAULT,
    "ffff",
    "x y width height",
    "Set the dimensions for the text box to display"
);
Event EV_Information_TextColor
(
    "textcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Sets the text color of an complete objective"
);

CLASS_DECLARATION(UILabel, UIInformationLabel, NULL) {
    {&EV_Information_Index,    &UIInformationLabel::SetInformationIndex},
    {&EV_Information_TextBox,  &UIInformationLabel::SetTextBoxFrame    },
    {&EV_Information_TextColor,&UIInformationLabel::SetTextColor       },
    {NULL,                     NULL                                    }
};

UIInformationLabel::UIInformationLabel()
{
    m_informationIndex = 0;
    m_textColor        = UColor(0.0f, 0.0f, 0.0f, 1.0f);
    m_textBoxFrame     = UIRect2D(0.0f, 0.0f, 0.0f, 0.0f);
    m_screenFrame      = UIRect2D(0.0f, 0.0f, 0.0f, 0.0f);
    AllowActivate(false);
}

void UIInformationLabel::SetInformationIndex(Event *ev)
{
    m_informationIndex = ev->GetInteger(1);
}

void UIInformationLabel::SetTextBoxFrame(Event *ev)
{
    const float pad = 0.0f;   

    m_textBoxFrame.pos.x = (float)ev->GetInteger(1) - pad - m_frame.pos.x;
    m_textBoxFrame.pos.y = (float)ev->GetInteger(2) - pad - m_frame.pos.y;
    m_textBoxFrame.size.width = (float)ev->GetInteger(3) - pad;
    m_textBoxFrame.size.height = (float)ev->GetInteger(4) - pad;

    m_screenFrame = m_textBoxFrame;
}

void UIInformationLabel::SetTextColor(Event *ev)
{
    m_textColor.r = ev->GetFloat(1);
    m_textColor.g = ev->GetFloat(2);
    m_textColor.b = ev->GetFloat(3);
    m_textColor.a = ev->GetFloat(4);
}

int UIInformationLabel::HandleInformation(void)
{
    unsigned int states = (unsigned int)cl.snap.ps.informationStates;
    bool         show;

    if (m_informationIndex < 1 || m_informationIndex > 8) {
        // retail's switch default arm only returns 0 -- it never hides.
        return 0;
    }

    unsigned int bit = 1u << (m_informationIndex - 1);

    show = (states & bit) != 0;

    if (show) {
        SetShader();
        ShowMe();
        return 1;
    }

    HideMe();
    return 0;
}

void UIInformationLabel::ShowMe(void)
{
    setShow(true);
}

void UIInformationLabel::HideMe(void)
{
    setShow(false);
}

void UIInformationLabel::SetShader(void)
{
    if (m_informationIndex < 1) {
        return;
    }

    const char *text = CL_ConfigString(CS_GENERAL_STRINGS + (m_informationIndex - 1));

    if (!text || !text[0]) {
        return;
    }

    setTitle(text);
}

void UIInformationLabel::UpdateData(void)
{
    HandleInformation();
}

void UIInformationLabel::ChangeResolution(void)
{
    UIWidget::ChangeResolution();

    const vec2_t& vs  = uid.scaleRes;
    const float   pad = 0.0f;   

    float x = (float)(int)(0.5f + vs[0] * m_textBoxFrame.pos.x);
    float y = (float)(int)(0.5f + vs[1] * m_textBoxFrame.pos.y);

    float w = vs[0] * (m_textBoxFrame.size.width + m_textBoxFrame.pos.x) - x;
    float h = vs[1] * (m_textBoxFrame.size.height + m_textBoxFrame.pos.y) - y;

    m_screenFrame.pos.x = x - pad;
    m_screenFrame.pos.y = y - pad;
    m_screenFrame.size.width  = (float)(int)(0.5f + w) - pad;
    m_screenFrame.size.height = (float)(int)(0.5f + h) - pad;
}

void UIInformationLabel::Draw(void)
{
    if (!m_informationIndex) {
        return;
    }

    if (!HandleInformation()) {
        return;
    }

    if (!m_font) {
        return;
    }

    m_font->setColor(m_textColor);
    m_font->PrintJustified(
        getClientFrame(),
        m_iFontAlignmentHorizontal,
        m_iFontAlignmentVertical,
        getTitle(), UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
}


static const float UIMOVE_MSEC_PER_SEC = 1000.0f; 
static const float UIMOVE_FRACTION_MIN = 0.0f;    
static const float UIMOVE_FRACTION_MAX = 1.0f;    

UIMoveCommand::UIMoveCommand()
{
    commandtype = 0;
    x           = UIMOVE_FRACTION_MIN;
    y           = UIMOVE_FRACTION_MIN;
    startx      = UIMOVE_FRACTION_MIN;
    starty      = UIMOVE_FRACTION_MIN;
    time        = UIMOVE_FRACTION_MIN;
    xcvarscale  = UIMOVE_FRACTION_MIN;
    ycvarscale  = UIMOVE_FRACTION_MIN;
    xcvarbase   = UIMOVE_FRACTION_MIN;
    ycvarbase   = UIMOVE_FRACTION_MIN;
}

UIMove::UIMove()
{
    AllowActivate(false);
    m_moveActive      = true;
    m_currentCommand  = 0;
    m_startTime       = UIMOVE_FRACTION_MIN;
    m_endTime         = UIMOVE_FRACTION_MIN;
    m_consumedTime    = UIMOVE_FRACTION_MIN;
    m_currentTime     = UIMOVE_FRACTION_MIN;
    m_bCommandStarted = false;
    restartCommands();
}

UIMove::~UIMove()
{
    deleteCommands();
}

void UIMove::restartCommands(void)
{
    m_currentCommand = 0;
    m_startTime      = UIMOVE_FRACTION_MIN;
    m_endTime        = UIMOVE_FRACTION_MIN;
    m_consumedTime   = UIMOVE_FRACTION_MIN;
}

void UIMove::addCommand(UIMoveCommand *cmd)
{
    m_commands.AddObject(cmd);
}

void UIMove::deleteCommands(void)
{
    int i;

    for (i = m_commands.NumObjects(); i > 0; i--) {
        UIMoveCommand *cmd = m_commands.ObjectAt(i);

        if (cmd) {
            delete cmd;
        }
    }

    m_commands.ClearObjectList();
}

void UIMove::clearCommands(void)
{
    m_commands.ClearObjectList();
    restartCommands();
}

UIMoveCommand *UIMove::getCommand(void)
{
    int prev = m_currentCommand;

    if (m_currentCommand) {
        if (m_consumedTime >= m_endTime) {
            m_currentCommand++;
        }
    } else {
        m_currentCommand = 1;
        m_consumedTime   = m_currentTime;
    }

    if (m_currentCommand > m_commands.NumObjects()) {
        m_currentCommand = m_commands.NumObjects();
        return NULL;
    }

    UIMoveCommand *cmd = m_commands.ObjectAt(m_currentCommand);

    if (m_currentCommand != prev) {
        m_startTime       = m_consumedTime;
        m_endTime         = m_consumedTime + cmd->time;
        m_bCommandStarted = false;
    }

    return cmd;
}

float UIMove::getFraction(UIMoveCommand *cmd)
{
    float frac;

    if (cmd->time <= UIMOVE_FRACTION_MIN) {
        frac = UIMOVE_FRACTION_MIN;
    } else {
        frac = (m_currentTime - m_startTime) / cmd->time;
    }

    if (frac < UIMOVE_FRACTION_MIN) {
        frac = UIMOVE_FRACTION_MIN;
    }

    if (frac > UIMOVE_FRACTION_MAX) {
        return UIMOVE_FRACTION_MAX;
    }

    return frac;
}

void UIMove::move(UIMoveCommand *cmd)
{
    if (!m_bCommandStarted) {
        m_startX          = m_frame.pos.x;
        m_startY          = m_frame.pos.y;
        m_endX            = m_frame.pos.x + cmd->x;
        m_endY            = m_frame.pos.y + cmd->y;
        m_bCommandStarted = true;
    }

    float frac = getFraction(cmd);

    UIRect2D frame(
        frac * (m_endX - m_startX) + m_startX,
        frac * (m_endY - m_startY) + m_startY,
        m_frame.size.width,
        m_frame.size.height
        );

    setFrame(frame);
}

void UIMove::moveTo(UIMoveCommand *cmd)
{
    if (!m_bCommandStarted) {
        m_startX          = m_frame.pos.x;
        m_startY          = m_frame.pos.y;
        m_endX            = cmd->x;
        m_endY            = cmd->y;
        m_bCommandStarted = true;
    }

    float frac = getFraction(cmd);
    float x    = frac * (m_endX - m_startX) + m_startX;
    float y    = frac * (m_endY - m_startY) + m_startY;

    if (frac == 0.0f) {
        x = m_endX;
        y = m_endY;
    }

    UIRect2D frame(x, y, m_frame.size.width, m_frame.size.height);

    setFrame(frame);
}

void UIMove::moveToCvar(UIMoveCommand *cmd)
{
    if (!m_bCommandStarted) {
        m_startX = m_frame.pos.x;
        m_startY = m_frame.pos.y;
        m_endX   = cmd->xcvarscale * Cvar_VariableValue(cmd->xcvarname) + cmd->x;
        m_endY   = cmd->ycvarscale * Cvar_VariableValue(cmd->ycvarname) + cmd->y;
        m_bCommandStarted = true;
    }

    float frac = getFraction(cmd);

    UIRect2D frame(
        frac * (m_endX - m_startX) + m_startX,
        frac * (m_endY - m_startY) + m_startY,
        m_frame.size.width,
        m_frame.size.height
        );

    setFrame(frame);
}

void UIMove::Motion(void)
{
    m_currentTime = (float)uid.time / UIMOVE_MSEC_PER_SEC;

    while (m_consumedTime < m_currentTime) {
        UIMoveCommand *cmd = getCommand();

        if (!cmd) {
            break;
        }

        switch (cmd->commandtype) {
        case 1:
            move(cmd);
            break;
        case 2:
            moveTo(cmd);
            break;
        case 3:
            moveToCvar(cmd);
            break;
        case 5:
            restartCommands();
            break;
        default:
            break;
        }

        if (m_currentTime <= m_endTime) {
            m_consumedTime = m_currentTime;
        } else {
            m_consumedTime = m_endTime;
        }
    }
}

void UIMove::turnOn(void)
{
    float now;

    UIWidget::turnOn();

    now = (float)uid.time / UIMOVE_MSEC_PER_SEC;

    if (m_endTime > UIMOVE_FRACTION_MIN) {
        float delta = now - m_consumedTime;

        m_startTime += delta;
        m_endTime += delta;
    }

    m_currentTime = now;
}

void UIMove::turnOff(void)
{
    UIWidget::turnOff();
}

void UIMove::ChangeResolution(void)
{
    int i;

    UIWidget::ChangeResolution();

    for (i = 1; i <= m_commands.NumObjects(); i++) {
        UIMoveCommand *cmd = m_commands.ObjectAt(i);

        cmd->x          = cmd->startx * uid.scaleRes[0];
        cmd->y          = cmd->starty * uid.scaleRes[1];
        cmd->xcvarscale = cmd->xcvarbase * uid.scaleRes[0];
        cmd->ycvarscale = cmd->ycvarbase * uid.scaleRes[1];
    }
}

void UIMove::Move(Event *ev)
{
    UIMoveCommand *cmd = new UIMoveCommand;

    cmd->commandtype = 1;
    cmd->x           = ev->GetFloat(1);
    cmd->y           = ev->GetFloat(2);
    cmd->time        = ev->GetFloat(3);
    cmd->startx      = ev->GetFloat(1);
    cmd->starty      = ev->GetFloat(2);
    addCommand(cmd);
}

void UIMove::MoveTo(Event *ev)
{
    UIMoveCommand *cmd = new UIMoveCommand;

    cmd->commandtype = 2;
    cmd->x           = ev->GetFloat(1);
    cmd->y           = ev->GetFloat(2);
    cmd->time        = ev->GetFloat(3);
    cmd->startx      = ev->GetFloat(1);
    cmd->starty      = ev->GetFloat(2);
    addCommand(cmd);
}

void UIMove::Wait(Event *ev)
{
    UIMoveCommand *cmd = new UIMoveCommand;

    cmd->time = ev->GetFloat(1);
    addCommand(cmd);
}

void UIMove::Restart(Event *ev)
{
    UIMoveCommand *cmd = new UIMoveCommand;

    cmd->commandtype = 5;
    addCommand(cmd);
}

void UIMove::RestartCommands(Event *ev)
{
    restartCommands();
}

void UIMove::ClearCommands(Event *ev)
{
    clearCommands();
}

void ParseEquationString(const str& equation, float& value, str& remainder, float& sign, float defaultValue)
{
    static const float kDigitOffset  = 0.0f;   
    static const float kFractionUnit = 1.0f;   
    static const float kRadix        = 10.0f;  

    float integerPart  = 0.0f;
    float fractionPart = 0.0f;
    int   i;
    int   len = equation.length();

    for (i = 0; i < len; i++) {
        char c = equation[i];

        if (c < '0' || c > '9') {
            break;
        }

        integerPart = kRadix * integerPart + (float)(c - kDigitOffset);
    }

    if (i < len && equation[i] == '.') {
        i++;
    }

    for (; i < len; i++) {
        char c = equation[i];

        if (c < '0' || c > '9') {
            break;
        }

        fractionPart = kRadix * fractionPart + (float)(c - kDigitOffset);
    }

    while (fractionPart > kFractionUnit) {
        fractionPart = fractionPart / kRadix;
    }

    sign = defaultValue;
    if (i < len && equation[i] == '+') {
        i++;
    } else if (i < len && equation[i] == '-') {
        i++;
        sign = -defaultValue;
    }

    remainder = "";
    for (; i < len; i++) {
        remainder += equation[i];
    }

    value = integerPart + fractionPart;
}

void UIMove::MoveToCvar(Event *ev)
{
    str            eq;
    UIMoveCommand *cmd;
    float          scale;

    if (ev->NumArgs() < 2) {
        return;
    }

    scale = ev->NumArgs() >= 3 ? ev->GetFloat(3) : 1.0f;

    cmd              = new UIMoveCommand;
    cmd->commandtype = 3;

    eq = ev->GetString(1);
    ParseEquationString(eq, cmd->x, cmd->xcvarname, cmd->xcvarscale, scale);

    eq = ev->GetString(2);
    ParseEquationString(eq, cmd->y, cmd->ycvarname, cmd->ycvarscale, scale);

    cmd->time = ev->NumArgs() >= 4 ? ev->GetFloat(4) : 0.0f;

    cmd->startx    = cmd->x;
    cmd->starty    = cmd->y;
    cmd->xcvarbase = cmd->xcvarscale;
    cmd->ycvarbase = cmd->ycvarscale;

    addCommand(cmd);
}

static Event EV_UIMove_Move(
    "mwc_move",
    EV_DEFAULT,
    "fff",
    "x y time",
    "Moves the widget."
);

static Event EV_UIMove_MoveTo(
    "mwc_moveto",
    EV_DEFAULT,
    "fff",
    "x y time",
    "Moves the widget to the specified location over the time period specified."
);

static Event EV_UIMove_MoveToCvar(
    "mwc_movetocvar",
    EV_DEFAULT,
    "ssFF",
    "x y scale time",
    "Moves the widget to (x+cvar1*scale,y+cvar2*scale) over the specified time."
);

static Event EV_UIMove_Wait(
    "mwc_wait",
    EV_DEFAULT,
    "f",
    "time",
    "Waits the specified amount of time."
);

static Event EV_UIMove_Restart(
    "mwc_restart",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes the widget commands start from the beginning."
);

static Event EV_UIMove_ClearCommands(
    "clearcommands",
    EV_DEFAULT,
    NULL,
    NULL,
    "Clears all of the commands."
);

static Event EV_UIMove_RestartCommands(
    "restartcommands",
    EV_DEFAULT,
    NULL,
    NULL,
    "Restarts all of the commands."
);

CLASS_DECLARATION(UIWidget, UIMove, NULL) {
    {&EV_UIMove_Move,           &UIMove::Move},
    {&EV_UIMove_MoveTo,         &UIMove::MoveTo},
    {&EV_UIMove_MoveToCvar,     &UIMove::MoveToCvar},
    {&EV_UIMove_Wait,           &UIMove::Wait},
    {&EV_UIMove_Restart,        &UIMove::Restart},
    {&EV_UIMove_ClearCommands,  &UIMove::ClearCommands},
    {&EV_UIMove_RestartCommands, &UIMove::RestartCommands},
    {NULL, NULL}
};


UIInventoryItem::UIInventoryItem()
{
    m_item      = NULL;
    m_bValid    = false;
    m_bSelected = false;
}

UIInventoryItem::~UIInventoryItem()
{
    m_items.ClearObjectList();
}

bool UIInventoryItem::isInventoryItemValid(void)
{
    if (!m_item) {
        return false;
    }

    return CL_HasInventoryItem(m_item->name) != qfalse;
}

str UIInventoryItem::getShowName(void)
{
    if (m_item) {
        return m_item->name;
    }

    return "0";
}

bool UIInventoryItem::sendEquipItemCommand(void)
{
    if (!isInventoryItemValid()) {
        return false;
    }

    if (m_item->command.length()) {
        Cbuf_AddText(va("%s\n", m_item->command.c_str()));
        return true;
    }

    if (m_item->equip & 4) {
        Cbuf_AddText(va("use \"%s\" dualhand\n", m_item->name.c_str()));
    } else if (m_item->equip & 1) {
        Cbuf_AddText(va("use \"%s\" lefthand\n", m_item->name.c_str()));
    } else if (m_item->equip & 2) {
        Cbuf_AddText(va("use \"%s\" righthand\n", m_item->name.c_str()));
    } else if (m_item->equip & 8) {
        Cbuf_AddText(va("use \"%s\"\n", m_item->name.c_str()));
    }

    return true;
}

void UIInventoryItem::render3dModel(void)
{
    vec3_t    mins, maxs;
    vec3_t    angles;
    vec3_t    rotateoffset;
    vec3_t    offset;
    vec3_t    origin;
    qhandle_t handle;

    if (!m_item) {
        return;
    }

    if (!m_item->hudprops.model.length()) {
        return;
    }

    handle = re.RegisterModel(m_item->hudprops.model);
    if (!handle) {
        return;
    }

    VectorCopy(m_item->hudprops.angles, angles);

    if (m_item->hudprops.move == INV_MOVE_BOB) {
        float frac = (float)uid.time / 333.0f;
        int   i;

        for (i = 0; i < 3; i++) {
            angles[i] += sin(frac) * m_item->hudprops.angledeltas[i];
        }
    } else if (m_item->hudprops.move == INV_MOVE_SPIN) {
        float frac = (float)uid.time / 333.0f;
        int   i;

        for (i = 0; i < 3; i++) {
            angles[i] += frac * m_item->hudprops.angledeltas[i];
        }
    }

    VectorCopy(m_item->hudprops.rotateoffset, rotateoffset);
    VectorCopy(m_item->hudprops.offset, offset);

    re.ModelBounds(handle, mins, maxs);

    float height = maxs[2] - mins[2];

    if (height < maxs[1] - mins[1]) {
        height = maxs[1] - mins[1];
    }

    if (height < maxs[0] - mins[0]) {
        height = maxs[0] - mins[0];
    }

    origin[1] = (mins[1] + maxs[1]) * 0.5f;
    origin[2] = (mins[2] + maxs[2]) * -0.5f;
    origin[0] = (0.5f * m_item->hudprops.scale * height) / 0.268f;

    CL_Draw3DModel(
        m_screenframe.pos.x,
        m_screenframe.pos.y,
        m_frame.size.width,
        m_frame.size.height,
        handle,
        origin,
        rotateoffset,
        offset,
        angles,
        m_item->hudprops.anim,
        0,       
        NULL,    
        m_item->fovx,
        m_item->fovy
        );

    set2D();
}

void UIInventoryItem::Draw(void)
{
    if (m_item && m_bValid) {
        render3dModel();
    }
}

CLASS_DECLARATION(UIMove, UIInventoryItem, NULL) {
    {NULL, NULL}
};

UIInventoryType::UIInventoryType()
{
    m_type        = NULL;
    m_currentItem = 0;
}

UIInventoryType::~UIInventoryType()
{
    m_visibleItems.ClearObjectList();
    m_items.ClearObjectList();
}

void UIInventoryType::Enable(void)
{
    UIWidget::Enable();
}

void UIInventoryType::Disable(void)
{
    UIWidget::Disable();
}

void UIInventoryType::setShow(bool visible)
{
    UIWidget::setShow(visible);
}

void UIInventoryType::Draw(void)
{
    UILabel::Draw();
}

static inventory_item_t *UIInv_PickAlternate(UIInventoryItem *widget)
{
    inventory_item_t *fallback = NULL;
    int               j;

    if (!widget->m_items.NumObjects()) {
        return NULL;
    }

    for (j = 1; j <= widget->m_items.NumObjects(); j++) {
        inventory_item_t *alt = widget->m_items.ObjectAt(j);

        if (!alt) {
            continue;
        }

        if (alt->hasShowOnlyIfValid) {
            fallback = alt;
        }

        if (CL_HasInventoryItem(alt->name)) {
            return alt;
        }
    }

    return fallback;
}

bool UIInventoryType::switchToNextItem(bool bAllowEmpty)
{
    if (!m_items.NumObjects()) {
        return false;
    }

    for (int i = m_currentItem + 1;; i++) {
        if (i > m_items.NumObjects() && m_currentItem != i) {
            m_currentItem = 0;
            return false;
        }

        UIInventoryItem *widget = m_items.ObjectAt(i);

        if (!widget) {
            return false;
        }

        inventory_item_t *alt = UIInv_PickAlternate(widget);

        if (alt) {
            widget->m_item = alt;
            widget->setMaterial(alt->availshader, WIDGET_STATE_NORMAL);
            widget->setMaterial(alt->selshader, WIDGET_STATE_SELECTED);
        }

        if (widget->m_item && CL_HasInventoryItem(widget->m_item->name)) {
            if (CL_DoesWeaponHaveAmmo(widget->m_item->name) || bAllowEmpty) {
                m_currentItem = i;
                return true;
            }
        }
    }
}

bool UIInventoryType::switchToPreviousItem(void)
{
    if (!m_items.NumObjects()) {
        return false;
    }

    for (int i = m_currentItem - 1;; i--) {
        if (i < 1) {
            m_currentItem = 0;
            return false;
        }

        UIInventoryItem *widget = m_items.ObjectAt(i);

        if (!widget) {
            return false;
        }

        inventory_item_t *alt = UIInv_PickAlternate(widget);

        if (alt) {
            widget->m_item = alt;
            widget->setMaterial(alt->availshader, WIDGET_STATE_NORMAL);
            widget->setMaterial(alt->selshader, WIDGET_STATE_SELECTED);
        }

        if (widget->m_item && CL_HasInventoryItem(widget->m_item->name)) {
            if (CL_DoesWeaponHaveAmmo(widget->m_item->name)) {
                m_currentItem = i;
                return true;
            }
        }
    }
}

void UIInventoryType::addUIInventoryItem(inventory_item_t *item, int width, int height)
{
    if (item && item->name.length()) {
        for (int i = 1; i <= m_items.NumObjects(); i++) {
            UIInventoryItem *existing = m_items.ObjectAt(i);
            str              name     = existing->getShowName();

            if (name.length() == item->name.length() && !strcmp(name, item->name)) {
                existing->m_items.AddObject(item);
                return;
            }
        }
    }

    UIInventoryItem *widget = new UIInventoryItem;

    widget->InitFrame(m_parent, 0.0f, 0.0f, (float)width, (float)height, 0, "tahoma-10");

    if (item) {
        widget->m_item = item;
        widget->setMaterial(item->availshader, WIDGET_STATE_NORMAL);
        widget->setMaterial(item->selshader, WIDGET_STATE_SELECTED);

        if (item->name.length()) {
            widget->m_items.AddObject(item);
        }

        widget->setForegroundColor(getForegroundColor());
        widget->setBackgroundColor(getBackgroundColor(), WIDGET_STATE_NORMAL, false);
        widget->setMaterial(item->availshader, WIDGET_STATE_NORMAL);
        widget->setMaterial(item->selshader, WIDGET_STATE_SELECTED);
    }

    m_items.AddObject(widget);
}

void UIInventoryType::updateItems(
    const UIRect2D& frame,
    float           width,
    float           height,
    float           scale,
    UIWidget       *widget
    )
{
    float step = scale + height;
    float y    = frame.pos.y - step;
    float x    = width * 0.25f + frame.pos.x + (frame.size.width - width) * 0.5f;
    int   i;

    for (i = m_items.NumObjects(); i >= 1; i--) {
        UIInventoryItem *item = m_items.ObjectAt(i);

        if (!item || !item->m_item) {
            continue;
        }

        if (!CL_HasInventoryItem(item->m_item->name)) {
            continue;
        }

        item->setFrame(UIRect2D(x, y, width, height));
        item->setNormalState();

        item->Enable();

        if (CL_DoesWeaponHaveAmmo(item->m_item->name)) {
            item->setMaterial(item->m_item->availshader, WIDGET_STATE_NORMAL);
            item->setMaterial(item->m_item->selshader, WIDGET_STATE_SELECTED);
        } else {
            item->setMaterial(item->m_item->noammoshader, WIDGET_STATE_NORMAL);
            item->setMaterial(item->m_item->noammoshader, WIDGET_STATE_SELECTED);
        }

        y -= step;
    }

    if (m_currentItem > 0 && m_currentItem <= m_items.NumObjects()) {
        UIInventoryItem *sel = m_items.ObjectAt(m_currentItem);

        sel->setSelectedState();
        widget->setFrame(UIRect2D(frame.pos.x, sel->getFrame().pos.y, frame.size.width, height));
    }

    m_visibleItems.ClearObjectList();
}


CLASS_DECLARATION(UILabel, UIInventoryType, NULL) {
    {NULL, NULL}
};

Event EV_Inv_ItemSize
(
    "inventoryitemsize", EV_DEFAULT, "ff", "width height", "inventory item size"
);
Event EV_Inv_TypeSize
(
    "inventorytypesize", EV_DEFAULT, "ff", "width height", "inventory type size"
);
Event EV_Inv_SelectSound
(
    "inventoryselectsound", EV_DEFAULT, "s", "selectSound", "The select sound"
);
Event EV_Inv_ChangeSound
(
    "inventorychangesound", EV_DEFAULT, "s", "changeSound", "The change sound"
);
Event EV_Inv_RejectSound
(
    "inventoryrejectsound", EV_DEFAULT, "s", "rejectSound", "The reject sound"
);
Event EV_Inv_EmptyGroupShader
(
    "inventoryemptygroupshader", EV_DEFAULT, "s", "shader", "Sets the empty group shader"
);
Event EV_Inv_SelectionShader
(
    "inventoryselectionshader", EV_DEFAULT, "s", "shader", "Sets the selection shader"
);
Event EV_Inv_NoAmmoSelectionShader
(
    "inventorynoammoselectionshader", EV_DEFAULT, "s", "shader", "Sets the selection shader when the item has no ammo"
);
Event EV_Inv_NotAvailSelectionShader
(
    "inventorynotavailselectionshader", EV_DEFAULT, "s", "shader", "Sets the selection shader when the item is not available."
);
Event EV_Inv_NextItem
(
    "inventorynextitem", EV_DEFAULT, NULL, NULL, "next inventory item"
);
Event EV_Inv_PreviousItem
(
    "inventorypreviousitem", EV_DEFAULT, NULL, NULL, "previous inventory item"
);
Event EV_Inv_WarpToItem
(
    "inventorywarpitem", EV_DEFAULT, "s", "typename", "Warps to a type in the inventory"
);
Event EV_Inv_Hide
(
    "inventoryhide", EV_DEFAULT, NULL, NULL, "Hides the inventory"
);
Event EV_Inv_Show
(
    "inventoryshow", EV_DEFAULT, NULL, NULL, "Shows the inventory"
);
Event EV_Inv_BarEnable
(
    "inventorybarenable", EV_DEFAULT, NULL, NULL, "Enables the inventory bar"
);
Event EV_Inv_BarDisable
(
    "inventorybardisable", EV_DEFAULT, NULL, NULL, "Disables the inventory bar"
);

CLASS_DECLARATION(UIWidget, UIInventoryBar, NULL) {
    {&EV_Inv_ItemSize,                &UIInventoryBar::setItemSize     },
    {&EV_Inv_TypeSize,                &UIInventoryBar::setTypeSize     },
    {&EV_Inv_NextItem,                &UIInventoryBar::nextItem        },
    {&EV_Inv_PreviousItem,            &UIInventoryBar::previousItem    },
    {&EV_Inv_WarpToItem,              &UIInventoryBar::warpToItem      },
    {&EV_Inv_Hide,                    &UIInventoryBar::hideInventory   },
    {&EV_Inv_Show,                    &UIInventoryBar::showInventory   },
    {&EV_Inv_EmptyGroupShader,        &UIInventoryBar::emptyGroupShader},
    {&EV_Inv_SelectionShader,         &UIInventoryBar::selectionShader },
    {&EV_Inv_NoAmmoSelectionShader,   &UIInventoryBar::noAmmoSelectionShader},
    {&EV_Inv_NotAvailSelectionShader, &UIInventoryBar::notAvailSelectionShader},
    {&EV_Inv_SelectSound,             &UIInventoryBar::setSelectSound  },
    {&EV_Inv_ChangeSound,             &UIInventoryBar::setChangeSound  },
    {&EV_Inv_RejectSound,             &UIInventoryBar::setRejectSound  },
    {&EV_Inv_BarEnable,               &UIInventoryBar::setBarEnable    },
    {&EV_Inv_BarDisable,              &UIInventoryBar::setBarDisable   },
    {NULL,                            NULL                                          }
};

UIInventoryBar *inventoryBar = NULL;

static bool s_inventoryEquipPending  = false;  
static int  s_inventoryLastEquipTime = 0;      

UIInventoryBar::UIInventoryBar()
{
    m_noAmmoSelectionShader   = NULL;
    m_notAvailSelectionShader = NULL;
    m_emptyGroupShader        = NULL;
    m_itemSize[0] = m_itemSize[1] = 0.0f;
    m_typeSize[0] = m_typeSize[1] = 0.0f;
    m_authoredItemSize[0] = m_authoredItemSize[1] = 0.0f;
    m_authoredTypeSize[0] = m_authoredTypeSize[1] = 0.0f;
    m_spacing[0]          = m_spacing[1]          = 0.0f;
    m_lastSwitchTime              = 0.0f;
    m_enabled = true;
    AllowActivate(true);
    m_autoswitchCvarName = "ui_autoswitchhudweapon";
    m_autoswitchCvar     = Cvar_Get("ui_autoswitchhudweapon", "1", 1);
    m_selectedWidget     = new UIInventoryItem;
    m_selectedWidget->setParent(this);
    m_selectedWidget->m_bValid = true;
    m_selectedWidget->setForegroundColor(getForegroundColor());
    m_selectedWidget->setBackgroundColor(getBackgroundColor(), WIDGET_STATE_NORMAL, false);
    m_selectedTypeWidget = new UIWidget;
    m_selectedTypeWidget->InitFrame(
        this, 0.0f, 0.0f, m_itemSize[0], m_itemSize[1], 0, "tahoma-10");
    m_selectedTypeWidget->setForegroundColor(getForegroundColor());
    m_selectedTypeWidget->setBackgroundColor(getBackgroundColor(), WIDGET_STATE_NORMAL, false);
    {
        static cvar_t *r208_inv = NULL;
        static int     r208_invRows;

        if (!r208_inv) {
            r208_inv = Cvar_Get("r208_invStats", "1", 0);
        }
        if (r208_inv && r208_inv->integer && r208_invRows < 8) {
            UColor c = getBackgroundColor();
            Com_Printf("[R208:inv] ctor bar.bg=(%.3f,%.3f,%.3f,%.3f) "
                       "selWidget.bg=(%.3f,%.3f,%.3f,%.3f) typeWidget.bg=(%.3f,%.3f,%.3f,%.3f)\n",
                       c.r, c.g, c.b, c.a,
                       m_selectedWidget->getBackgroundColor().r, m_selectedWidget->getBackgroundColor().g,
                       m_selectedWidget->getBackgroundColor().b, m_selectedWidget->getBackgroundColor().a,
                       m_selectedTypeWidget->getBackgroundColor().r, m_selectedTypeWidget->getBackgroundColor().g,
                       m_selectedTypeWidget->getBackgroundColor().b, m_selectedTypeWidget->getBackgroundColor().a);
            r208_invRows++;
        }
    }
    inventoryBar = this;
    loadItems();
    hideInventory();
}


void UIInventoryBar::setBackgroundColor(const UColor& color, WidgetState state, bool setbordercolor)
{
    int i;

    UIWidget::setBackgroundColor(color, state, setbordercolor);

    for (i = 1; i <= m_children.NumObjects(); i++) {
        m_children.ObjectAt(i)->setBackgroundColor(color, state, setbordercolor);
    }
}

void UIInventoryBar::setItemSize(Event *ev)
{
    m_authoredItemSize[0] = ev->GetFloat(1);
    m_authoredItemSize[1] = ev->GetFloat(2);

    updateScaledSizes();
}

void UIInventoryBar::setTypeSize(Event *ev)
{
    m_authoredTypeSize[0] = ev->GetFloat(1);
    m_authoredTypeSize[1] = ev->GetFloat(2);

    updateScaledSizes();
}

void UIInventoryBar::updateScaledSizes(void)
{
    if (isResizeable()) {
        m_typeSize[0] = m_authoredTypeSize[0] * uid.scaleRes[0];
        m_typeSize[1] = m_authoredTypeSize[1] * uid.scaleRes[1];
        m_itemSize[0] = m_authoredItemSize[0] * uid.scaleRes[0];
        m_itemSize[1] = m_authoredItemSize[1] * uid.scaleRes[1];
    } else {
        m_typeSize[0] = m_authoredTypeSize[0];
        m_typeSize[1] = m_authoredTypeSize[1];
        m_itemSize[0] = m_authoredItemSize[0];
        m_itemSize[1] = m_authoredItemSize[1];
    }
}

void UIInventoryBar::update(void)
{
    if (!m_visible) {
        return;
    }

    if ((float)cls.realtime > 2000.0f + m_lastSwitchTime) {
        UI_RemoveHud("inventoryhud");
    }
}



void UIInventoryBar::ChangeResolution(void)
{
    updateScaledSizes();

    UIWidget::ChangeResolution();
}

qboolean UIInventoryBar::KeyEvent(int key, unsigned int time)
{
    char        binding[1024];
    const char *command;

    const char *bind = Key_GetBinding(key);

    if (bind) {
        Q_strncpyz(binding, bind, sizeof(binding));
    } else {
        memset(binding, 0, sizeof(binding));
    }

    if (!binding[0]) {
        CL_Key_ForceCommand(key, time);
        return qtrue;
    }

    command = strtok(binding, " ");

    if (!command) {
        CL_Key_ForceCommand(key, time);
        return qtrue;
    }

    if (!str::icmp(command, "inventory")) {
        UI_RemoveHud("inventoryhud");
        return qtrue;
    }

    if (!str::icmp(command, "nextinv")) {
        if (!m_visible) {
            getCurrentWeaponPosition();
        }

        UI_AddHud("inventoryhud", 0);
        getItem(DIR_NEXT);
        return qtrue;
    }

    if (!str::icmp(command, "previnv")) {
        if (!m_visible) {
            getCurrentWeaponPosition();
        }

        UI_AddHud("inventoryhud", 0);
        getItem(DIR_PREVIOUS);
        return qtrue;
    }

    if (!str::icmp(command, "+attackleft")) {
        equipItem(true);
        UI_RemoveHud("inventoryhud");
        return qtrue;
    }

    CL_Key_ForceCommand(key, time);
    return qtrue;
}

void UIInventoryBar::loadItems(void)
{
    for (int i = 1; i <= client_inv.types.NumObjects(); i++) {
        inventory_type_t *type = client_inv.types.ObjectAt(i);

        if (!type) {
            continue;
        }

        UIInventoryType *widget = new UIInventoryType;

        widget->InitFrame(this, 0.0f, 0.0f, 0.0f, 0.0f, 0, "tahoma-10");
        widget->m_type = type;
        widget->setForegroundColor(getForegroundColor());
        widget->setBackgroundColor(getBackgroundColor(), WIDGET_STATE_NORMAL, false);
        widget->setMaterial(type->texture, WIDGET_STATE_NORMAL);
        widget->setMaterial(type->selTexture, WIDGET_STATE_SELECTED);
        m_types.AddObject(widget);

        for (int j = 1; j <= type->items.NumObjects(); j++) {
            inventory_item_t *item = type->items.ObjectAt(j);

            if (item) {
                widget->addUIInventoryItem(item, (int)m_itemSize[0], (int)m_itemSize[1]);
            }
        }
    }
}


// geometry did not match retail anywhere: the selected widget's x had its two
void UIInventoryBar::updateTypes(void)
{
    int numTypes = m_types.NumObjects();

    if (!numTypes) {
        return;
    }

    float selX = m_frame.pos.x + (m_frame.size.width - m_itemSize[0]) * 0.5f;
    float selY = m_frame.size.height - m_itemSize[1];

    m_selectedWidget->setFrame(UIRect2D(selX, selY, m_itemSize[0], m_itemSize[1]));

    float cursor    = selX - (m_spacing[0] + m_typeSize[0]);
    float fitWidth  = m_typeSize[0];
    float fitHeight = m_typeSize[1];
    float fitTotal  = 0.0f;
    int   radius    = 0;

    while (cursor > m_frame.pos.x) {
        float thisStep = fitWidth + m_spacing[0];

        fitTotal += thisStep;
        fitWidth *= 0.75f;
        fitHeight *= 0.75f;
        radius++;

        if (fitWidth == 16.0f || fitHeight == 16.0f) {
            break;
        }

        cursor -= fitWidth + m_spacing[0];
    }

    int start = m_currentType - radius;

    if (start < 1) {
        start += numTypes;
    }

    int count = numTypes <= 2 * radius + 1 ? numTypes : 2 * radius + 1;

    for (int i = 1; i <= numTypes; i++) {
        UIInventoryType *t = m_types.ObjectAt(i);

        if (!t) {
            continue;
        }

        t->Disable();

        for (int j = 1; j <= t->m_items.NumObjects(); j++) {
            UIInventoryItem *it = t->m_items.ObjectAt(j);

            if (it) {
                it->Disable();
            }
        }
    }

    float ladderWidth  = m_typeSize[0] * powf(0.75f, (float)(radius - 1));
    float ladderHeight = m_typeSize[1] * powf(0.75f, (float)(radius - 1));
    float x = m_frame.size.width * 0.5f - (fitTotal * 2.0f + m_itemSize[0]) * 0.5f;

    int run = start;
    bool beforeSelected = true;

    for (int n = 0; n < count; n++) {
        int idx = (start + n) % numTypes;

        if (!idx) {
            idx = numTypes;
        }

        UIInventoryType *t = m_types.ObjectAt(idx);

        if (!t) {
            break;
        }

        if (run == m_currentType) {
            t->setFrame(UIRect2D(selX, selY, m_typeSize[0] * 0.5f, m_typeSize[1] * 0.5f));
            t->Enable();
            t->setSelectedState();
            beforeSelected = false;
            x += m_itemSize[0] + m_spacing[0];
        } else {
            float selY2 = m_selectedWidget->getFrame().pos.y;

            t->setFrame(UIRect2D(
                x, m_itemSize[1] * 0.5f - ladderHeight * 0.5f + selY2, ladderWidth, ladderHeight));
            t->Enable();
            t->setNormalState();
            x += ladderWidth + m_spacing[0];
        }

        if (++run > numTypes) {
            run = 1;
        }

        if (beforeSelected) {
            ladderWidth /= 0.75f;
            ladderHeight /= 0.75f;
        } else {
            ladderWidth *= 0.75f;
            ladderHeight *= 0.75f;
        }
    }

    updateSelectedTypeWidget();

    if (m_autoswitchCvar->value != 0.0f) {
        equipItem(false);
    }
}


void UIInventoryBar::updateSelectedTypeWidget(void)
{
    UIInventoryType *type = m_types.ObjectAt(m_currentType);
    UIInventoryItem  *sel = NULL;

    if (!type) {
        return;
    }

    if (type->m_currentItem >= 1 && type->m_currentItem <= type->m_items.NumObjects()) {
        sel = type->m_items.ObjectAt(type->m_currentItem);
    }

    if (sel) {
        if (sel->m_item) {
            m_selectedWidget->m_item = sel->m_item;
            m_selectedWidget->setMaterial(sel->m_item->availshader, WIDGET_STATE_NORMAL);
            m_selectedWidget->setMaterial(sel->m_item->selshader, WIDGET_STATE_SELECTED);
        }

        // @0x0017A8F0): the STORED m_bValid is computed from **sel->m_item**, not the
        // (possibly stale) m_item -- retail's v25 -- which is why the two must not be
        m_selectedWidget->m_bValid = sel->m_item != NULL
                                     && CL_HasInventoryItem(sel->m_item->name) != qfalse;
        m_selectedWidget->setSelectedState();

        // never held an item faults in retail too; the port now does the same.
        if (!m_selectedWidget->m_bValid) {
            m_selectedWidget->setMaterial(m_selectedWidget->m_item->notavailshader, WIDGET_STATE_NORMAL);
            m_selectedWidget->setMaterial(m_selectedWidget->m_item->notavailshader, WIDGET_STATE_SELECTED);
        } else if (!CL_DoesWeaponHaveAmmo(m_selectedWidget->m_item->name)) {
            m_selectedWidget->setMaterial(m_selectedWidget->m_item->noammoshader, WIDGET_STATE_NORMAL);
            m_selectedWidget->setMaterial(m_selectedWidget->m_item->noammoshader, WIDGET_STATE_SELECTED);
        } else {
            m_selectedWidget->setMaterial(m_selectedWidget->m_item->availshader, WIDGET_STATE_NORMAL);
            m_selectedWidget->setMaterial(m_selectedWidget->m_item->selshader, WIDGET_STATE_SELECTED);
        }
    }

    type->setSelectedState();

    bool anyCarried = false;

    for (int i = 1; i <= type->m_items.NumObjects(); i++) {
        UIInventoryItem *it = type->m_items.ObjectAt(i);

        if (it && it->m_item && CL_HasInventoryItem(it->m_item->name)) {
            anyCarried = true;
            break;
        }
    }

    if (!anyCarried) {
        m_selectedWidget->m_bValid = false;
        m_selectedWidget->setMaterial(m_emptyGroupShader, WIDGET_STATE_NORMAL);
        m_selectedWidget->setMaterial(m_emptyGroupShader, WIDGET_STATE_SELECTED);
        m_selectedTypeWidget->Disable();
        return;
    }

    m_selectedTypeWidget->Enable();

    UIRect2D r = m_selectedWidget->getFrame();

    type->updateItems(r, m_itemSize[0] * 0.5f, m_itemSize[1] * 0.5f, m_spacing[1], m_selectedTypeWidget);
}


void UIInventoryBar::equipItem(bool force)
{
    UIInventoryType *type = m_types.ObjectAt(m_currentType);
    UIInventoryItem  *sel = NULL;

    if (!type) {
        return;
    }

    if (type->m_currentItem >= 1 && type->m_currentItem <= type->m_items.NumObjects()) {
        sel = type->m_items.ObjectAt(type->m_currentItem);
    }

    bool valid = sel && sel->m_item && CL_HasInventoryItem(sel->m_item->name) != qfalse;
    bool hasAmmo = valid && sel->m_item
                   && CL_DoesWeaponHaveAmmo(sel->m_item->name) != 0;

    if (!valid || !hasAmmo) {
        S_StartLocalSound(m_rejectSound, false);
        s_inventoryEquipPending = false;
        return;
    }

    bool equipped = false;

    if (cls.realtime > s_inventoryLastEquipTime + 300 || force) {
        if (sel->sendEquipItemCommand()) {
            s_inventoryEquipPending = false;
            s_inventoryLastEquipTime = cls.realtime;
            equipped = true;
        } else {
            s_inventoryEquipPending = true;
        }
    } else {
        s_inventoryEquipPending = true;
    }

    if (equipped) {
        S_StartLocalSound(m_selectSound, false);
    }
}

int UIInventoryBar::getItem(Direction dir)
{
    if (!m_types.NumObjects()) {
        return 0;
    }

    if (m_currentType < 1 || m_currentType > m_types.NumObjects()) {
        m_currentType = 1;
    }

    UIInventoryType *type = m_types.ObjectAt(m_currentType);

    if (!type) {
        return 0;
    }

    bool found;

    if (dir) {
        found = type->switchToPreviousItem();
    } else {
        found = type->switchToNextItem(false);
    }

    int cur = m_currentType;

    while (!found) {
        cur = dir ? getPreviousType(cur) : getNextType(cur);
        UIInventoryType *t = m_types.ObjectAt(cur);

        if (dir) {
            t->m_currentItem = t->m_items.NumObjects() + 1;
        } else {
            t->m_currentItem = 0;
        }

        if (dir) {
            found = t->switchToPreviousItem();
        } else {
            found = t->switchToNextItem(false);
        }

        if (m_currentType == cur) {
            break;
        }
    }

    m_currentType = cur;
    updateTypes();
    S_StartLocalSound(m_changeSound, false);
    m_lastSwitchTime = (float)(cls.realtime);

    return m_currentType;
}

int UIInventoryBar::getNextType(int type)
{
    int next = type + 1;

    if (next > m_types.NumObjects()) {
        next = 1;
    }

    return next;
}

int UIInventoryBar::getPreviousType(int type)
{
    int prev = type - 1;

    if (prev < 1) {
        return m_types.NumObjects();
    }

    return prev;
}

int UIInventoryBar::getTypeIndex(const str& name)
{
    for (int i = 1; i <= m_types.NumObjects(); i++) {
        UIInventoryType *t = m_types.ObjectAt(i);

        if (t && t->m_type && !str::icmp(t->m_type->name, name)) {
            return i;
        }
    }

    return 0;
}

int UIInventoryBar::getCurrentWeaponPosition(void)
{
    const char *weapon = CL_GetCurrentWeapon();

    if (!weapon || !strlen(weapon)) {
        m_currentType = 0;
        return m_currentType;
    }

    for (int i = 1; i <= m_types.NumObjects(); i++) {
        UIInventoryType *t = m_types.ObjectAt(i);

        if (!t) {
            continue;
        }

        str name = weapon;
        bool found = false;

        for (int j = 1; j <= t->m_items.NumObjects(); j++) {
            UIInventoryItem *it = t->m_items.ObjectAt(j);

            if (it && it->m_item && !str::icmp(it->m_item->name, name)) {
                t->m_currentItem = j;
                found = true;
                break;
            }
        }

        if (found) {
            m_currentType = i;
            return m_currentType;
        }
    }

    return m_currentType;
}

void UIInventoryBar::showInventory(void)
{
    UI_AddHud("inventoryhud", 0);
}

void UIInventoryBar::hideInventory(void)
{
    UI_RemoveHud("inventoryhud");
}

void UIInventoryBar::setShow(bool visible)
{
    UIWidget::setShow(visible);

    if (!visible) {
        m_autoswitchCvarName = "";
    }

    m_autoswitchCvar = Cvar_Get("ui_autoswitchhudweapon", "1", 1);
}

void UIInventoryBar::Enable(void)
{
    UIWidget::Enable();
}

void UIInventoryBar::Draw(void)
{
    if (s_inventoryEquipPending) {
        equipItem(false);
    }
}



void UIInventoryBar::hideInventory(Event *ev)
{
    hideInventory();
}

void UIInventoryBar::showInventory(Event *ev)
{
    showInventory();
}

void UIInventoryBar::setBarEnable(Event *ev)
{
    m_enabled = true;
}

void UIInventoryBar::setBarDisable(Event *ev)
{
    m_enabled = false;
}

void UIInventoryBar::nextItem(void)
{
    if (!m_visible) {
        getCurrentWeaponPosition();
    }

    UI_AddHud("inventoryhud", 0);
    getItem(DIR_NEXT);
}

void UIInventoryBar::previousItem(void)
{
    if (!m_visible) {
        getCurrentWeaponPosition();
    }

    UI_AddHud("inventoryhud", 0);
    getItem(DIR_PREVIOUS);
}

void UIInventoryBar::nextItem(Event *ev)
{
    if (!m_enabled) {
        return;
    }

    if (!UI_IsView3DActive() && !IsActive()) {
        return;
    }

    nextItem();
}

void UIInventoryBar::previousItem(Event *ev)
{
    if (!m_enabled) {
        return;
    }

    if (!UI_IsView3DActive() && !IsActive()) {
        return;
    }

    previousItem();
}

void UIInventoryBar::warpToItem(Event *ev)
{

    R120_PROBE(
        "inv:bar",
        "warpToItem: event='%s' enabled=%d view3d=%d isActive=%d types=%d",
        ev->getName(),
        m_enabled ? 1 : 0,
        UI_IsView3DActive() ? 1 : 0,
        IsActive() ? 1 : 0,
        m_types.NumObjects()
    );

    if (!m_enabled) {
        R120_PROBE("inv:bar", "GATE m_enabled==0 -- bar dead, nothing will happen");
        return;
    }

    if (!UI_IsView3DActive() && !IsActive()) {
        R120_PROBE("inv:bar", "GATE view3d+focus -- event dropped, nothing will happen");
        return;
    }

    showInventory();
    s_inventoryLastEquipTime = cls.realtime;
    m_lastSwitchTime = (float)cls.realtime;

    str name = ev->GetString(1);
    int idx = getTypeIndex(name);

    if (idx < 1 || idx > m_types.NumObjects()) {
        R120_PROBE(
            "inv:bar",
            "REJECT type '%s' not found (types=%d) -- client inventory mirror empty",
            name.c_str(),
            m_types.NumObjects()
        );
        S_StartLocalSound(m_rejectSound, false);
        return;
    }

    m_currentType = idx;
    updateTypes();

    UIInventoryType *type = m_types.ObjectAt(m_currentType);
    bool anyUsable = false;

    for (int j = 1; j <= type->m_items.NumObjects(); j++) {
        UIInventoryItem *it = type->m_items.ObjectAt(j);

        if (it && it->m_item && CL_HasInventoryItem(it->m_item->name)
            && CL_DoesWeaponHaveAmmo(it->m_item->name)) {
            anyUsable = true;
            break;
        }
    }

    if (anyUsable) {
        if (!type->switchToNextItem(false)) {
            type->m_currentItem = 0;
            type->switchToNextItem(false);
        }
    } else if (!type->switchToNextItem(true)) {
        type->switchToNextItem(true);
    }

    S_StartLocalSound(m_changeSound, false);
    updateTypes();
}

void UIInventoryBar::setSelectSound(Event *ev)
{
    m_selectSound = ev->GetString(1);
}

void UIInventoryBar::setChangeSound(Event *ev)
{
    m_changeSound = ev->GetString(1);
}

void UIInventoryBar::setRejectSound(Event *ev)
{
    m_rejectSound = ev->GetString(1);
}

void UIInventoryBar::emptyGroupShader(Event *ev)
{
    m_emptyGroupShader = uWinMan.RegisterShader(ev->GetString(1));
}

void UIInventoryBar::selectionShader(Event *ev)
{
    m_selectedTypeWidget->setMaterial(uWinMan.RegisterShader(ev->GetString(1)), WIDGET_STATE_NORMAL);
}

void UIInventoryBar::noAmmoSelectionShader(Event *ev)
{
    m_noAmmoSelectionShader = uWinMan.RegisterShader(ev->GetString(1));
}

void UIInventoryBar::notAvailSelectionShader(Event *ev)
{
    m_notAvailSelectionShader = uWinMan.RegisterShader(ev->GetString(1));
}



Event EV_Widget_PrintDialog(
    "dialog_printtext",
    EV_DEFAULT,
    "ssf",
    "dialogName dialogText dialogLength",
    "Adds dialog text to the dialogConsole");
Event EV_Widget_ResetDialog(
    "dialog_resetdialog",
    EV_DEFAULT,
    NULL,
    NULL,
    "Resets the text dialog");

CLASS_DECLARATION(UIWidget, UIDialogTextConsole, NULL) {
    {&W_SizeChanged,          &UIDialogTextConsole::OnSizeChanged},
    {&EV_Layout_Rect,         &UIDialogTextConsole::LayoutRect   },
    {&EV_Widget_PrintDialog,  &UIDialogTextConsole::PrintDialog  },
    {&EV_Widget_ResetDialog,  &UIDialogTextConsole::ResetDialog  },
    {NULL,                    NULL                               }
};

UIDialogTextConsole::UIDialogTextConsole()
{
    AllowActivate(false);

    m_fadeInTime     = 200.0f;
    m_holdTime       = 5000.0f;
    m_fadeOutTime    = 1000.0f;
    m_alpha          = 1.0f;
    m_lastPrintTime  = -99999;
    m_scrollDelay    = 0;
    m_firstLine      = -1;
    m_useAlpha       = true;
    m_displayLevel   = 0;
    m_maxLines       = 0;
    m_maxLineWidth   = 0;
    m_lastPrintDelta = 0;
}

void UIDialogTextConsole::OnSizeChanged(void)
{
    m_maxLineWidth = (int)(m_frame.size.width - 30.0f);
    m_maxLines     = (int)((m_frame.size.height - 10.0f) / (float)(m_font->getHeight(false) + 2));
}

void UIDialogTextConsole::OnSizeChanged(Event *ev)
{
    OnSizeChanged();
}

void UIDialogTextConsole::LayoutRect(Event *ev)
{
    UIWidget::LayoutRect(ev);
    OnSizeChanged(ev);
}

void UIDialogTextConsole::ChangeResolution(void)
{
    UIWidget::ChangeResolution();
    OnSizeChanged();
}

void UIDialogTextConsole::update(void)
{
    if (CL_UIPaused()) {
        m_lastPrintTime = cls.realtime - m_lastPrintDelta;
    } else {
        m_lastPrintDelta = cls.realtime - m_lastPrintTime;
    }
}

void UIDialogTextConsole::PrintDialog(Event *ev)
{
    str  name;
    str  text;
    float lip;

    name = ev->GetString(1);
    text = ev->GetString(2);
    lip  = ev->GetFloat(3);

    PrintDialog(name, text, lip);
}

void UIDialogTextConsole::PrintDialog(const str& name, const str& text, float lip)
{
    unsigned char level = 0;

    if (!strcmp(m_currentName.c_str(), name.c_str()) && !strcmp(m_currentText.c_str(), text.c_str())) {
        return;
    }

    DialogEntry *entry = GetDialogEntry(name.c_str());
    if (entry) {
        level = entry->_flags;
        if (m_displayLevel > level) {
            return;
        }
    }

    if (text.length()) {
        m_currentText  = text;
        m_currentName  = name;
        m_holdTime     = 1000.0f * lip;
        m_displayLevel = level;

        PrintDialog(level, text, true);
    } else {
        m_displayLevel = 0;
        m_holdTime     = (float)(cls.realtime - m_lastPrintTime);
    }
}

static int s_r201PrintDialogCalls;
static int s_r201ResetDialogCalls;

void UIDialogTextConsole::PrintDialog(unsigned char level, const str& text, bool useAlpha)
{
    s_r201PrintDialogCalls++;

    str       line;
    int       lineWidth;
    int       lastSpace;
    size_t    i;

    m_useAlpha = useAlpha;

    m_lines.ClearObjectList();

    if (m_maxLines < 0) {
        m_maxLines = 1;
    }

    str  work = text;
    lineWidth = 0;
    lastSpace = -1;
    for (i = 0; i < work.length(); i++) {
        char c = work[i];

        if (c == ' ') {
            lastSpace = i;
        }

        if (c != ' ' || lineWidth != 0) {
            if (lineWidth + m_font->getCharWidth(c, 0) <= m_maxLineWidth) {
                if (c == '\n') {
                    lineWidth = 0;
                    lastSpace = -1;
                } else {
                    lineWidth += m_font->getCharWidth(c, 0);
                }
            } else {
                lineWidth = 0;
                if (c == ' ') {
                    work[i] = '\n';
                } else if (lastSpace > -1) {
                    i = lastSpace;
                    lastSpace = -1;
                    work[i] = '\n';
                }
            }
        }
    }

    line = "";
    for (i = 0; i < work.length(); i++) {
        char c = work[i];

        if (c == '\n') {
            m_lines.AddObject(line);
            line = "";
        } else {
            line += c;
        }
    }
    if (line.length()) {
        m_lines.AddObject(line);
    }

    if (m_maxLines < 0) {
        m_maxLines = 1;
    }
    m_lastPrintTime = cls.realtime;
    m_visible       = true;
}

void UIDialogTextConsole::ResetDialog(Event *ev)
{
    s_r201ResetDialogCalls++;
    m_currentText = "";
    m_currentName = "";
}

static const float kDialogTextX = 15.0f;

void UIDialogTextConsole::Draw(void)
{
    int   elapsed;
    float maxAlpha;
    float lineHeight;

    update();

    if (server_loading) {
        return;
    }

    UIWidget::Draw();

    elapsed  = cls.realtime - m_lastPrintTime;
    maxAlpha = m_displayLevel > 55 ? 1.0f : 0.75f;

    if ((float)elapsed < m_fadeInTime) {
        m_alpha = (float)elapsed / m_fadeInTime;
    } else if ((float)elapsed < m_holdTime) {
        m_alpha = 1.0f;
    } else if ((float)elapsed < m_holdTime + m_fadeOutTime) {
        m_alpha = 1.0f - ((float)elapsed - m_holdTime) / m_fadeOutTime;
    } else {
        m_alpha        = 0.0f;
        m_visible      = false;
        m_firstLine    = -1;
        m_displayLevel = 0;
        m_currentText  = "";
        m_currentName  = "";
        return;
    }

    m_alpha *= maxAlpha;
    lineHeight = (float)m_font->getHeight(false);

    if (m_lines.NumObjects()) {
        m_font->setColor(m_foreground_color);
        if (m_useAlpha) {
            m_font->setAlpha(m_alpha);
        } else {
            m_font->setAlpha(1.0f);
        }

        if (m_maxLines < m_lines.NumObjects()) {
            // line's own clock -- it is NOT pinned to the last page.  Raw PPC
            // NOT reset m_firstLine (retail @0x001A5670 stores only +1740 and +1820),
            int   numLines     = m_lines.NumObjects();
            int   nominalFirst = numLines - m_maxLines + 1;
            int   firstLine;
            int   lastLine;
            float y;

            if (m_holdTime > 0.0f) {
                firstLine = (int)(1.0f + floorf(((float)elapsed / m_holdTime) * (float)nominalFirst));
            } else {
                firstLine = nominalFirst;
            }

            lastLine = m_maxLines + firstLine - 1;

            if (lastLine > numLines) {
                lastLine  = numLines;
                firstLine = nominalFirst;
            }

            y = 5.0f - (float)(firstLine - 1) * lineHeight;
            if (firstLine != 1 && firstLine != m_firstLine) {
                m_scrollDelay = 500;
            }
            if (m_scrollDelay > 0) {
                y = ((float)m_scrollDelay / 500.0f) * (2.0f + lineHeight) + y;
            }

            for (int n = 1; n <= lastLine; n++) {
                if (y >= -lineHeight && y <= m_frame.size.height + lineHeight) {
                    m_font->Print(kDialogTextX, y, m_lines.ObjectAt(n).c_str(), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
                }
                y += 2.0f + lineHeight;
            }
            m_firstLine = firstLine;
            m_scrollDelay -= cls.frametime;

#define R201_DIALOG_MAX 48
            {
                static int s_r201DialogRows;
                static int s_r201DialogLastMs = -100000;

                if (s_r201DialogRows < R201_DIALOG_MAX
                    && cls.realtime - s_r201DialogLastMs >= 500) {
                    s_r201DialogLastMs = cls.realtime;
                    s_r201DialogRows++;

                    int longest = 0;
                    for (int k = 1; k <= numLines; k++) {
                        int L = (int)strlen(m_lines.ObjectAt(k).c_str());
                        if (L > longest) longest = L;
                    }
                    Com_Printf("[R201:dialog] page el=%d hold=%.0f alpha=%.3f lines=%d "
                               "maxLines=%d maxWidth=%d first=%d last=%d nominal=%d "
                               "lastFirst=%d delay=%d level=%d useAlpha=%d "
                               "cwSpace=%d cwA=%d h=%d longest=%d "
                               "printCalls=%d resetCalls=%d\n",
                               elapsed, m_holdTime, m_alpha, numLines, m_maxLines,
                               m_maxLineWidth, firstLine, lastLine, nominalFirst,
                               m_firstLine, m_scrollDelay, (int)m_displayLevel,
                               m_useAlpha ? 1 : 0,
                               m_font->getCharWidth(' ', 0), m_font->getCharWidth('a', 0),
                               m_font->getHeight(false), longest,
                               s_r201PrintDialogCalls,
                               s_r201ResetDialogCalls);
                }
            }
        } else {
            float y = (m_frame.size.height - 5.0f) - lineHeight;
            int   n = m_lines.NumObjects();

            {
                static int s_r201DialogFitRows;
                static int s_r201DialogFitLastMs = -100000;

                if (s_r201DialogFitRows < R201_DIALOG_MAX
                    && cls.realtime - s_r201DialogFitLastMs >= 500) {
                    s_r201DialogFitLastMs = cls.realtime;
                    s_r201DialogFitRows++;

                    int longest = 0;
                    for (int k = 1; k <= n; k++) {
                        int L = (int)strlen(m_lines.ObjectAt(k).c_str());
                        if (L > longest) longest = L;
                    }
                    Com_Printf("[R201:dialog] fits el=%d hold=%.0f alpha=%.3f lines=%d "
                               "maxLines=%d maxWidth=%d nominal=%d delay=%d level=%d useAlpha=%d "
                               "cwSpace=%d cwA=%d h=%d longest=%d "
                               "printCalls=%d resetCalls=%d\n",
                               elapsed, m_holdTime, m_alpha, n, m_maxLines,
                               m_maxLineWidth, n - m_maxLines + 1, m_scrollDelay,
                               (int)m_displayLevel, m_useAlpha ? 1 : 0,
                               m_font->getCharWidth(' ', 0), m_font->getCharWidth('a', 0),
                               m_font->getHeight(false), longest,
                               s_r201PrintDialogCalls,
                               s_r201ResetDialogCalls);
                }
            }

            while (y > -lineHeight && n >= 1) {
                m_font->Print(kDialogTextX, y, m_lines.ObjectAt(n).c_str(), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
                y -= 2.0f + lineHeight;
                n--;
            }
        }
    }
    m_visible = true;
}

Event EV_Widget_DialogHeadEntity(
    "dialogheadentity",
    EV_DEFAULT,
    "i",
    "entnum",
    "The entity reference number to render");

CLASS_DECLARATION(UIWidget, UIDialogHead, NULL) {
    {&EV_Widget_DialogHeadEntity, &UIDialogHead::SetEntityNum},
    {NULL,                        NULL                     }
};

static const entityState_t *UIDialogHead_FindState(int entnum)
{
    int i;

    if (entnum < 0 || !cl.snap.valid) {
        return NULL;
    }
    for (i = 0; i < cl.snap.numEntities; i++) {
        entityState_t *es = &cl.parseEntities[(cl.snap.parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1)];
        if (es->number == entnum) {
            return es;
        }
    }
    return NULL;
}

UIDialogHead::UIDialogHead()
{
    AllowActivate(false);
    m_state    = NULL;
    m_tiki     = -1;
    m_entnum   = 1023;
    m_sequence = -1;
}

void UIDialogHead::SetEntityNum(Event *ev)
{
    int oldnum = m_entnum;

    m_entnum = ev->GetInteger(1);

    if (oldnum == m_entnum && m_sequence == re.GetRegistrationSequence()) {
        return;
    }

    const entityState_t *st = UIDialogHead_FindState(m_entnum);
    if (st) {
        m_state = st;
    }

    const char *name = m_state ? CL_ConfigString(CS_MODELS + m_state->modelindex) : NULL;

    m_tiki     = (name && name[0]) ? re.RegisterModel(name) : -1;
    m_sequence = re.GetRegistrationSequence();
}

void UIDialogHead::Draw(void)
{
    if (m_entnum == 1023 || m_entnum == 1022) {
        return;
    }

    UIWidget::Draw();

    const entityState_t *headState = UIDialogHead_FindState(m_entnum);
    if (headState) {
        m_state = headState;
    }
    headState = m_state;

    if (!headState || m_tiki == -1) {
        return;
    }

    dtiki_t *tiki = re.R_Model_GetHandle(m_tiki);
    if (!tiki) {
        return;
    }

    // (`mr r7, r26', 0x001A4B28) -- NOT the m_tiki cached by setEntityNum.  setEntityNum
    // 0x001A4720 whose two outputs are never read; a pure lookup with no side effect,
    qhandle_t hModel = re.RegisterModel(CL_ConfigString(CS_MODELS + headState->modelindex));

    vec3_t origin, angles, rotateoffset, offset;
    int    i;

    refEntity_t ent {};

    for (i = 0; i < NUM_MORPH_CONTROLLERS; i++) {
        ent.morph_controllers[i] = headState->morph_controllers[i];
    }

    for (i = 0; i < NUM_BONE_CONTROLLERS; i++) {
        ent.bone_tag[i] = headState->bone_tag[i];
        if (ent.bone_tag[i] >= 0) {
            ent.boneTagGate = 1;
            VectorCopy4(headState->bone_quat[i], ent.bone_quat[i]);
        }
    }

    for (i = 0; i < tiki->num_surfaces && i < (int)sizeof(ent.surfaces); i++) {
        ent.surfaces[i] = headState->surfaces[i];
    }

    {
        qboolean bHasHead = qfalse;

        for (i = 0; i < tiki->num_surfaces; i++) {
            const char *name = TIKI_Surface_NumToName(tiki, i);

            if (name && strstr(name, "head")) {
                bHasHead = qtrue;
                break;
            }
        }

        if (bHasHead) {
            for (i = 0; i < tiki->num_surfaces && i < (int)sizeof(ent.surfaces); i++) {
                const char *name = TIKI_Surface_NumToName(tiki, i);

                if (!name || !strstr(name, "head")) {
                    ent.surfaces[i] |= MDL_SURFACE_NODRAW;
                }
            }
        }
    }

    ent.renderfx = (ent.renderfx | RF_FULLBRIGHT) & ~RF_DONTDRAW;

    VectorClear(rotateoffset);
    VectorSet(angles, 0, 191, 0);
    VectorClear(origin);
    VectorCopy(tiki->head_offset, offset);

    str anim = "idle";
    if (TIKI_Anim_NumForName(tiki, "idle_still") != -1) {
        anim = "idle_still";
    }

    CL_Draw3DModel(
        m_screenframe.pos.x,
        m_screenframe.pos.y,
        m_frame.size.width,
        m_frame.size.height,
        hModel,
        origin,
        rotateoffset,
        offset,
        angles,
        anim,
        0,
        &ent,
        30.0f,
        30.0f
    );
}


#define RADAR_DIST_SCALE 2.0f
#define RADAR_TOPDOWN_PITCH 90.0f
#define RADAR_FOV           30.0f
#define RADAR_PITCH_CLAMP   40.0f
#define RADAR_BOUNDS_SCALE 0.5f
#define RADAR_COLOR_SCALE 255.0f
#define RADAR_MSEC 1000.0f
#define RADAR_RENDERFX 0x40000

static UIRadarDialog *s_radarDialog = NULL;

Event EV_Radar_SimpleGroundModel
(
    "simplegroundmodel", EV_DEFAULT, "s", "path", "Sets the simple radar ground model to use"
);
Event EV_Radar_ComplexGroundModel
(
    "complexgroundmodel", EV_DEFAULT, "s", "path", "Sets the radar gound model to use"
);
Event EV_Radar_SimpleBackgroundColor
(
    "simplebackgroundcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the simple background color"
);
Event EV_Radar_ComplexBackgroundColor
(
    "complexbackgroundcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the complex background color"
);
Event EV_Radar_FovModel
(
    "fovmodel", EV_DEFAULT, "s", "path", "Sets the radar fov model"
);
Event EV_Radar_BlipModel
(
    "blipmodel", EV_DEFAULT, "s", "blip", "Sets the blip shader to use"
);
Event EV_Radar_SelectedBlipModel
(
    "selectedblipmodel", EV_DEFAULT, "s", "selectedblip", "Sets the selected blip shader"
);
Event EV_Radar_ObjectiveBlipModel
(
    "objectiveblipmodel", EV_DEFAULT, "s", "selectedblip", "Sets the mission objective blip model"
);
Event EV_Radar_ObjectiveSelectedBlipModel
(
    "objectiveselectedblipmodel", EV_DEFAULT, "s", "selectedblip", "Sets the mission objective selected blip model"
);
Event EV_Radar_ObjectiveArrowModel
(
    "objectivearrowmodel", EV_DEFAULT, "s", "objectivearrow", "Sets the mission objective arrow"
);
Event EV_Radar_Range
(
    "range", EV_DEFAULT, "f", "range", "Sets the range on the radar"
);
Event EV_Radar_BlipScale
(
    "blipscale", EV_DEFAULT, "f", "scale", "Sets the scale on the blip sprite"
);
Event EV_Radar_GroundModelBoundingSize
(
    "groundmodelboundingsize", EV_DEFAULT, "f", "size", "Sets the bounding size of the ground model."
);

CLASS_DECLARATION(UIWidget, UIRadarDialog, NULL) {
    {&EV_Radar_SimpleGroundModel,          &UIRadarDialog::SetSimpleGroundModel         },
    {&EV_Radar_ComplexGroundModel,         &UIRadarDialog::SetComplexGroundModel        },
    {&EV_Radar_SimpleBackgroundColor,      &UIRadarDialog::SetSimpleBackgroundColor     },
    {&EV_Radar_ComplexBackgroundColor,     &UIRadarDialog::SetComplexBackgroundColor    },
    {&EV_Radar_FovModel,                   &UIRadarDialog::SetFovModel                  },
    {&EV_Radar_BlipModel,                  &UIRadarDialog::SetBlipModel                 },
    {&EV_Radar_SelectedBlipModel,          &UIRadarDialog::SetSelectedBlipModel         },
    {&EV_Radar_ObjectiveBlipModel,         &UIRadarDialog::SetObjectiveBlipModel        },
    {&EV_Radar_ObjectiveSelectedBlipModel, &UIRadarDialog::SetObjectiveSelectedBlipModel},
    {&EV_Radar_ObjectiveArrowModel,        &UIRadarDialog::SetObjectiveArrow       },
    {&EV_Radar_Range,                      &UIRadarDialog::SetRadarRange                     },
    {&EV_Radar_BlipScale,                  &UIRadarDialog::SetBlipScale                 },
    {&EV_Radar_GroundModelBoundingSize,    &UIRadarDialog::SetGroundModelBoundingSize   },
    {NULL,                                 NULL                                            }
};

UIRadarDialog::UIRadarDialog()
{
    m_groundModel                = 0;
    m_simpleGroundModel          = NULL;
    m_complexGroundModel         = NULL;
    m_blipModel                  = NULL;
    m_selectedBlipModel          = NULL;
    m_objectiveBlipModel         = NULL;
    m_objectiveSelectedBlipModel = NULL;
    m_objectiveArrowModel        = NULL;
    m_fovModel                   = NULL;

    m_numBlips          = 0;
    m_numObjectiveBlips = 0;
    memset(m_blips, 0, sizeof(m_blips));
    memset(m_objectiveBlips, 0, sizeof(m_objectiveBlips));

    m_blipScale = 1.0f;
    VectorClear(m_viewAngles);
    m_viewAnglesFromGame = false;
    memset(m_matrix, 0, sizeof(m_matrix));
    m_range                      = 16.0f;
    m_groundModelBoundingSize    = 0.0f;
    m_groundModelBoundingSizeSet = false;
    m_radarString                = "";
    m_complexMode                = 1;
    m_complexBackgroundColor     = UColor(0, 0, 0, 0);
    m_simpleBackgroundColor      = UColor(0, 0, 0, 0);

    if (!s_radarDialog) {
        s_radarDialog = this;
    }

    AllowActivate(false);
}

UIRadarDialog::~UIRadarDialog()
{
    if (s_radarDialog == this) {
        s_radarDialog = NULL;
    }
}

// Registering here rather than at first Draw is not just closer to retail, it is the
void UIRadarDialog::SetSimpleGroundModel(Event *ev)
{
    m_simpleGroundModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetComplexGroundModel(Event *ev)
{
    m_complexGroundModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetFovModel(Event *ev)
{
    m_fovModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetBlipModel(Event *ev)
{
    m_blipModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetSelectedBlipModel(Event *ev)
{
    m_selectedBlipModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetObjectiveBlipModel(Event *ev)
{
    m_objectiveBlipModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetObjectiveSelectedBlipModel(Event *ev)
{
    m_objectiveSelectedBlipModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetObjectiveArrow(Event *ev)
{
    m_objectiveArrowModel = uWinMan.RegisterModel(ev->GetString(1));
}

void UIRadarDialog::SetBlipScale(Event *ev)
{
    m_blipScale = ev->GetFloat(1);
}

void UIRadarDialog::SetRadarRange(Event *ev)
{
    m_range = ev->GetFloat(1);
}

void UIRadarDialog::SetGroundModelBoundingSize(Event *ev)
{
    m_groundModelBoundingSize    = ev->GetFloat(1);
    m_groundModelBoundingSizeSet = true;
}

void UIRadarDialog::SetComplexBackgroundColor(Event *ev)
{
    m_complexBackgroundColor = UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4));
}

void UIRadarDialog::SetSimpleBackgroundColor(Event *ev)
{
    m_simpleBackgroundColor = UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4));
}


void UIRadarDialog::AddSpriteToScene(
    qhandle_t hModel, const vec3_t origin, const vec3_t rotatedOffset, const vec3_t offset, const vec3_t angles,
    float scale, const vec4_t color
)
{
    refEntity_t ent;
    vec3_t      rotated;
    int         i;

    memset(&ent, 0, sizeof(ent));
    AnglesToAxis(angles, ent.axis);

    for (i = 0; i < 3; i++) {
        rotated[i] = ent.axis[0][i] * rotatedOffset[0] + ent.axis[1][i] * rotatedOffset[1]
                   + ent.axis[2][i] * rotatedOffset[2];
        ent.origin[i] = origin[i] + offset[i] + rotated[i];
    }

    ent.reType = RT_SPRITE;
    ent.hModel = hModel;
    ent.scale  = scale;
    ent.renderfx |= RADAR_RENDERFX;
    for (i = 0; i < 4; i++) {
        ent.shaderRGBA[i] = (byte)(int)(RADAR_COLOR_SCALE * color[i]);
    }
    ent.entityNumber = ENTITYNUM_NONE;

    re.AddRefSpriteToScene(&ent);
}

// zero and R_AddSkelSurfaces' blend branch never engages.  Retail also has no
// handle guard is also a behaviour fix, not just a de-guard: retail stores
void UIRadarDialog::AddModelToScene(
    qhandle_t hModel, const vec3_t origin, const vec3_t rotatedOffset, const vec3_t offset, const vec3_t angles,
    const str& anim, float scale, bool morphGate
)
{
    refEntity_t ent;
    vec3_t      rotated;
    dtiki_t    *tiki;
    int         i;

    memset(&ent, 0, sizeof(ent));
    AnglesToAxis(angles, ent.axis);

    for (i = 0; i < 3; i++) {
        rotated[i] = ent.axis[0][i] * rotatedOffset[0] + ent.axis[1][i] * rotatedOffset[1]
                   + ent.axis[2][i] * rotatedOffset[2];
        ent.origin[i] = origin[i] + offset[i] + rotated[i];
    }

    ent.reType = RT_MODEL;
    ent.hModel = hModel;
    ent.scale  = scale;
    ent.renderfx |= RADAR_RENDERFX;
    ent.entityNumber = ENTITYNUM_NONE;

    ent.legsLive        = qtrue;
    ent.crossblend_lerp = -1.0f; 

    tiki     = re.R_Model_GetHandle(hModel);
    ent.tiki = tiki;

    if (anim.length()) {
        int animIndex = TIKI_Anim_NumForName(tiki, anim.c_str());

        ent.anim    = animIndex;
        ent.oldanim = animIndex;

        int   numFrames = TIKI_Anim_NumFrames(tiki, animIndex);
        float animTime  = TIKI_Anim_Time(tiki, animIndex);

        if (animTime != 0.0f) {
            ent.frame = (short)((int)((double)(unsigned int)(cls.realtime * numFrames) / (RADAR_MSEC * animTime))
                                % numFrames);
        }
    }

    // Port-only tail field (no retail offset -- see tr_types.h): skeletor's
    ent.actionWeight = 1.0f;

    if (morphGate) {
        ent.morph_controllers[0].index   = 0;
        ent.morph_controllers[0].percent = dm_morph_c1->value;
    }

    ent.fade = 1.0f;

    re.AddRefEntityToScene(&ent);
}

void UIRadarDialog::DrawRadar(void)
{
    vec3_t origin, rotatedOffset, offset, angles;

    if (!m_groundModel) {
        return;
    }

    VectorClear(origin);
    VectorClear(rotatedOffset);
    VectorClear(offset);
    origin[0] = RADAR_DIST_SCALE * m_range;

    MatrixToEulerAngles(m_matrix, angles);

    AddModelToScene(m_groundModel, origin, rotatedOffset, offset, angles, "idle", 1.0f, true);
}

void UIRadarDialog::DrawFov(void)
{
    vec3_t origin, rotatedOffset, offset, angles, yawAngles;
    float  quat[4];
    float  yawMatrix[3][3];
    float  combined[3][3];

    if (!m_fovModel) {
        return;
    }

    VectorClear(origin);
    VectorClear(rotatedOffset);
    VectorClear(offset);
    origin[0] = RADAR_DIST_SCALE * m_range;

    VectorClear(yawAngles);
    yawAngles[1] = m_viewAngles[1];

    EulerToQuat(yawAngles, quat);
    QuatToMat(quat, yawMatrix);
    MatrixMultiply(yawMatrix, m_matrix, combined);
    MatrixToEulerAngles(combined, angles);

    AddModelToScene(m_fovModel->GetHandle(), origin, rotatedOffset, offset, angles, "idle", 1.0f, true);
}

void UIRadarDialog::DrawBlips(void)
{
    vec3_t origin, rotatedOffset, offset, angles, scaled, mins, maxs;
    vec4_t color;
    float  scale, fade;
    int    i;

    if (!m_blipModel || !m_selectedBlipModel) {
        return;
    }

    VectorClear(origin);
    VectorClear(rotatedOffset);
    VectorClear(offset);
    VectorClear(angles);
    origin[0] = RADAR_DIST_SCALE * m_range;

    if (m_groundModelBoundingSizeSet) {
        scale = m_groundModelBoundingSize;
    } else {
        re.ModelBounds(m_groundModel, mins, maxs);
        scale = RADAR_BOUNDS_SCALE * (maxs[0] - mins[0]);
    }

    for (i = 0; i < m_numBlips; i++) {
        const radarBlip_t *blip = &m_blips[i];
        qhandle_t          hModel;

        VectorScale(blip->pos, scale, scaled);
        hModel = blip->selected ? m_selectedBlipModel->GetHandle() : m_blipModel->GetHandle();

        MatrixTransformVector(scaled, m_matrix, offset);

        fade     = 1.0f - fabs(blip->pos[2]);
        color[0] = blip->color[0];
        color[1] = blip->color[1];
        color[2] = blip->color[2];
        color[3] = fade * fade;

        AddSpriteToScene(hModel, origin, rotatedOffset, offset, angles, m_blipScale, color);
    }
}

void UIRadarDialog::DrawMissionObjectiveBlips(void)
{
    vec3_t origin, rotatedOffset, offset, angles, scaled, mins, maxs;
    vec4_t color;
    float  scale;
    int    i;

    if (!m_objectiveArrowModel || !m_objectiveSelectedBlipModel || !m_objectiveBlipModel) {
        return;
    }

    VectorClear(origin);
    VectorClear(rotatedOffset);
    VectorClear(offset);
    VectorClear(angles);
    origin[0] = RADAR_DIST_SCALE * m_range;

    if (m_groundModelBoundingSizeSet) {
        scale = m_groundModelBoundingSize;
    } else {
        re.ModelBounds(m_groundModel, mins, maxs);
        scale = RADAR_BOUNDS_SCALE * (maxs[0] - mins[0]);
    }

    for (i = 0; i < m_numObjectiveBlips; i++) {
        const radarObjectiveBlip_t *blip = &m_objectiveBlips[i];

        VectorClear(offset);

        if (blip->inRange) {
            qhandle_t hModel =
                blip->selected ? m_objectiveSelectedBlipModel->GetHandle() : m_objectiveBlipModel->GetHandle();

            VectorScale(blip->pos, scale, scaled);
            MatrixTransformVector(scaled, m_matrix, offset);

            color[0] = blip->color[0];
            color[1] = blip->color[1];
            color[2] = blip->color[2];
            color[3] = 1.0f;

            AddSpriteToScene(hModel, origin, rotatedOffset, offset, angles, m_blipScale, color);
        } else {
            vec3_t dirAngles;
            float  quat[4];
            float  dirMatrix[3][3];
            float  combined[3][3];

            vectoangles(blip->pos, dirAngles);
            EulerToQuat(dirAngles, quat);
            QuatToMat(quat, dirMatrix);
            MatrixMultiply(dirMatrix, m_matrix, combined);
            MatrixToEulerAngles(combined, angles);

            AddModelToScene(
                m_objectiveArrowModel->GetHandle(), origin, rotatedOffset, offset, angles, "idle", 1.0f, true
            );
        }
    }
}

void UIRadarDialog::AddBlip(const vec3_t pos, const vec3_t color, const char *name, int selected, int alwaysShow)
{
    radarBlip_t *blip;

    if (!m_complexMode && !alwaysShow) {
        return;
    }

    if (m_numBlips >= RADAR_MAX_BLIPS) {
        return;
    }

    blip = &m_blips[m_numBlips];
    VectorCopy(pos, blip->pos);
    VectorCopy(color, blip->color);
    blip->selected = selected;

    if (selected && name && strlen(name)) {
        m_radarString = getStringResource(name);
    }

    m_numBlips++;
}

void UIRadarDialog::AddMissionObjectiveBlip(
    const vec3_t pos, const vec3_t color, const char *name, int selected, int inRange
)
{
    radarObjectiveBlip_t *blip;

    if (m_numObjectiveBlips >= RADAR_MAX_OBJECTIVE_BLIPS) {
        return;
    }

    blip = &m_objectiveBlips[m_numObjectiveBlips];
    VectorCopy(pos, blip->pos);
    VectorCopy(color, blip->color);
    blip->selected = selected;

    if (selected) {
        m_radarString = name ? name : "";
    }

    blip->inRange = inRange;
    m_numObjectiveBlips++;
}

void UIRadarDialog::SetViewAngles(const vec3_t angles)
{
    VectorCopy(angles, m_viewAngles);
    m_viewAnglesFromGame = true;
}

void UIRadarDialog::setShow(bool visible)
{
    UIWidget::setShow(visible);
    m_complexMode = Cvar_Get("ui_radarmode", "0", 0)->integer;
}

void UIRadarDialog::Draw(void)
{
    vec3_t   angles;
    float    quat[4];
    float    mat[3][3];
    refdef_t refdef;
    int      viewMode;

    VectorCopy(m_viewAngles, angles);

    if (m_complexMode) {
        viewMode      = Cvar_Get("ui_radarviewmode", "0", CVAR_ARCHIVE)->integer;
        m_groundModel = m_complexGroundModel ? m_complexGroundModel->GetHandle() : 0;
        setBackgroundColor(m_complexBackgroundColor, WIDGET_STATE_NORMAL, true);
    } else {
        viewMode      = 2;
        m_groundModel = m_simpleGroundModel ? m_simpleGroundModel->GetHandle() : 0;
        setBackgroundColor(m_simpleBackgroundColor, WIDGET_STATE_NORMAL, true);
    }


    if (viewMode == 2) {
        angles[0] = RADAR_TOPDOWN_PITCH;
    } else if (viewMode == 3) {
        angles[0] = RADAR_FOV;
    } else if (viewMode == 1) {
        if (angles[0] < 0.0f) {
            angles[0] = 0.0f;
        }
        if (angles[0] > RADAR_PITCH_CLAMP) {
            angles[0] = RADAR_PITCH_CLAMP;
        }
        angles[0] = RADAR_TOPDOWN_PITCH - RADAR_DIST_SCALE * angles[0];
    }

    EulerToQuat(angles, quat);
    QuatToMat(quat, mat);
    TransposeMatrix(mat, m_matrix);

    re.ClearScene();

    DrawRadar();
    DrawFov();
    DrawBlips();
    DrawMissionObjectiveBlips();

    memset(&refdef, 0, sizeof(refdef));
    refdef.rdflags = RDF_NOWORLDMODEL;
    AxisClear(refdef.viewaxis);
    refdef.fov_x  = RADAR_FOV;
    refdef.fov_y  = RADAR_FOV;
    refdef.x      = uid.originX + (int)m_screenframe.pos.x;
    refdef.y      = uid.originY + (int)m_screenframe.pos.y;
    refdef.width  = (int)m_screenframe.size.width;
    refdef.height = (int)m_screenframe.size.height;
    refdef.time   = cl.serverTime;

    {
        static int r138uivp3d_radar;
        if ((uid.originX || uid.originY) && R120_Throttle(&r138uivp3d_radar, 2, 120)) {
            Com_Printf(
                "[R138:uivp3d] radar refdef %d,%d %dx%d | origin %d,%d rect %dx%d\n",
                refdef.x, refdef.y, refdef.width, refdef.height,
                uid.originX, uid.originY, uid.vidWidth, uid.vidHeight
            );
        }
    }

    re.RenderScene(&refdef);

    set2D();

    memset(m_blips, 0, sizeof(m_blips));
    m_numBlips = 0;
    memset(m_objectiveBlips, 0, sizeof(m_objectiveBlips));
    m_numObjectiveBlips = 0;

    // Order matters and is retail's: publish, THEN clear.  The Label therefore shows the
    SetGameplayStringProperty("RadarDialog", "RadarString", m_radarString.c_str(), qfalse);
    m_radarString = "";
}

void Radar_AddBlip(const vec3_t pos, const vec3_t color, const char *name, int selected, int alwaysShow)
{
    if (s_radarDialog) {
        s_radarDialog->AddBlip(pos, color, name, selected, alwaysShow);
    }
}

void Radar_AddMissionObjectiveBlip(
    const vec3_t pos, const vec3_t color, const char *name, int selected, int inRange
)
{
    if (s_radarDialog) {
        s_radarDialog->AddMissionObjectiveBlip(pos, color, name, selected, inRange);
    }
}

void Radar_SetViewAngles(const vec3_t angles)
{
    if (s_radarDialog) {
        s_radarDialog->SetViewAngles(angles);
    }
}


float UIFakkLabel::getGameplayFloat(const char *groupName, const char *propertyName)
{
    str             group;
    str             name;
    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm) {
        return 0.0f;
    }

    group = groupName;
    name  = propertyName;

    if (!gpm->hasProperty(group, name)) {
        return 0.0f;
    }

    return gpm->getFloatValue(group, name);
}

float UIFakkLabel::getPlayerstatFloat(const char *statName)
{
    return (float)CL_SafePlayerStat(CL_GetStatByName(statName));
}
