/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction


#include "ui_local.h"
#include "../qcommon/gameplaymanager.h" 
#include "../corepp/tiki.h"

// MENU3D MENU3D TEMP PROBE (change menu3d-menu3d, tree-only, NEVER merge): one
#define PROBE_GET(name, def, flags) ((cvar_t *)0)
#define PROBE_PRINT                 Com_Printf
#define PROBE_ON_OVERRIDE                 (atoi(CvarGetForUI("qa_probe", "0")) != 0)
#include "../qcommon/qa_probe.h"


Event EV_RMWidget_RenderModel
(
    "rendermodel",
    EV_DEFAULT,
    "s",
    "modelname",
    "The name of the model to render"
);
Event EV_RMWidget_RenderModelPrecache
(
    "rendermodelprecache",
    EV_DEFAULT,
    NULL,
    NULL,
    "Makes the model get cached immediately"
);
Event EV_RMWidget_RenderModelOffset
(
    "rendermodeloffset",
    EV_DEFAULT,
    "v",
    "offsetVector",
    "The offset vector of the render model"
);
Event EV_RMWidget_RenderModelRotateOffset
(
    "rendermodelrotateoffset",
    EV_DEFAULT,
    "v",
    "rotationVector",
    "The rotation offset to rotate the model."
);
Event EV_RMWidget_RenderModelAngles
(
    "rendermodelangles",
    EV_DEFAULT,
    "v",
    "angleVector",
    "The vector for the model angles"
);
Event EV_RMWidget_RenderModelPitch
(
    "rendermodelpitch",
    EV_DEFAULT,
    "f",
    "pitchangle",
    "Set the model's pitch"
);
Event EV_RMWidget_RenderModelYaw
(
    "rendermodelyaw",
    EV_DEFAULT,
    "f",
    "yawangle",
    "Set the model's yaw"
);
Event EV_RMWidget_RenderModelRoll
(
    "rendermodelroll",
    EV_DEFAULT,
    "f",
    "rollangle",
    "Set the model's roll"
);
Event EV_RMWidget_RenderModelAnimation
(
    "rendermodelanimation",
    EV_DEFAULT,
    "s",
    "animations",
    "The default model animation to play"
);
Event EV_RMWidget_RenderModelHoverAnimation
(
    "rendermodelhoveranimation",
    EV_DEFAULT,
    "s",
    "animations",
    "The model animation to play during mouse hover"
);
Event EV_RMWidget_RenderModelArmedAnimation
(
    "rendermodelarmedanimation",
    EV_DEFAULT,
    "s",
    "animations",
    "The model animation to play when armed"
);
Event EV_RMWidget_RenderModelSelectedAnimation
(
    "rendermodelselectedanimation",
    EV_DEFAULT,
    "s",
    "animations",
    "The model animation to play when selected"
);
Event EV_RMWidget_RenderModelDisabledAnimation
(
    "rendermodeldisabledanimation",
    EV_DEFAULT,
    "s",
    "animations",
    "The model animation to play when disabled"
);
Event EV_RMWidget_RenderModelFov
(
    "rendermodelfov",
    EV_DEFAULT,
    "ii",
    "fovX fovY",
    "The fov to render the model at"
);
Event EV_RMWidget_RenderModelSpin
(
    "rendermodelspin",
    EV_DEFAULT,
    "v",
    "spin_angles",
    "Causes the model to spin."   
);
Event EV_RMWidget_ModelProperty
(
    "modelProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Hooks this widget up to the specfied gameplay object property, which is treatedas a model to render.  In addition to fetching the model from the gameplaydatabase, the other rendering properties are pulled from the database as well."
);
Event EV_RMWidget_NormalSpin
(
    "normalspin",
    EV_DEFAULT,
    "vF",
    "_normalspin _normalspintime",
    "causes model to spin with a specified direction and speed"   
);
Event EV_RMWidget_HoverSpin
(
    "hoverspin",
    EV_DEFAULT,
    "vF",
    "_hoverspin _hoverspintime",
    "causes model to spin with a specified direction and speed on hover"   
);
Event EV_RMWidget_NormalPivot
(
    "normalpivot",
    EV_DEFAULT,
    "vF",
    "_normalpivot _normalpivottime",
    "causes model to pivot back and forth with a specified angle and period"   
);
Event EV_RMWidget_HoverPivot
(
    "hoverpivot",
    EV_DEFAULT,
    "vF",
    "_hoverpivot _hoverpivottime",
    "causes model to pivot back and forth with a specified angle and period on hover"   
);
Event EV_RMWidget_NormalBob
(
    "normalbob",
    EV_DEFAULT,
    "vF",
    "_normalbob _normalbobtime",
    "causes model to bob up and down a specified vector and period"   
);
Event EV_RMWidget_HoverBob
(
    "hoverbob",
    EV_DEFAULT,
    "vF",
    "_hoverbob _hoverbobtime",
    "causes model to bob up and down a specified vector and period on hover"   
);
Event EV_RMWidget_ClickSoundProperty
(
    "clicksoundproperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Uses the specified object's property as the sound to play whenthe widget is left clicked."
);
Event EV_RMWidget_AltClickSoundProperty
(
    "altclicksoundproperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Uses the specified object's property as the sound to play whenthe widget is right clicked."
);
CLASS_DECLARATION(UIWidget, UIRenderModel, NULL) {
    {&EV_RMWidget_RenderModel,                 &UIRenderModel::setModel                },
    {&EV_RMWidget_RenderModelPrecache,         &UIRenderModel::setModelPrecache        },
    {&EV_RMWidget_RenderModelOffset,           &UIRenderModel::setModelOffset          },
    {&EV_RMWidget_RenderModelRotateOffset,     &UIRenderModel::setModelRotateOffset    },
    {&EV_RMWidget_RenderModelAngles,           &UIRenderModel::setModelAngles          },
    {&EV_RMWidget_RenderModelPitch,            &UIRenderModel::setModelPitch           },
    {&EV_RMWidget_RenderModelYaw,              &UIRenderModel::setModelYaw             },
    {&EV_RMWidget_RenderModelRoll,             &UIRenderModel::setModelRoll            },
    {&EV_RMWidget_RenderModelAnimation,        &UIRenderModel::setModelAnimation       },
    {&EV_RMWidget_RenderModelHoverAnimation,   &UIRenderModel::setModelHoverAnimation  },
    {&EV_RMWidget_RenderModelArmedAnimation,   &UIRenderModel::setModelArmedAnimation  },
    {&EV_RMWidget_RenderModelSelectedAnimation,&UIRenderModel::setModelSelectedAnimation},
    {&EV_RMWidget_RenderModelDisabledAnimation,&UIRenderModel::setModelDisabledAnimation},
    {&EV_RMWidget_RenderModelFov,              &UIRenderModel::setModelFov             },
    {&EV_RMWidget_RenderModelSpin,             &UIRenderModel::LayoutRenderModelSpin            },
    {&EV_RMWidget_ModelProperty,               &UIRenderModel::setModelGameplayProperty              },
    {&EV_RMWidget_NormalSpin,                  &UIRenderModel::setNormalSpin                 },
    {&EV_RMWidget_HoverSpin,                   &UIRenderModel::setHoverSpin                  },
    {&EV_RMWidget_NormalPivot,                 &UIRenderModel::setNormalPivot                },
    {&EV_RMWidget_HoverPivot,                  &UIRenderModel::setHoverPivot                 },
    {&EV_RMWidget_NormalBob,                   &UIRenderModel::setNormalBob                  },
    {&EV_RMWidget_HoverBob,                    &UIRenderModel::setHoverBob                   },
    {&EV_RMWidget_ClickSoundProperty,          &UIRenderModel::setClickSoundProperty         },
    {&EV_RMWidget_AltClickSoundProperty,       &UIRenderModel::setAltClickSoundProperty      },
    {NULL,                                     NULL                                             }
};

UIRenderModel::UIRenderModel()
{
    m_fovX     = 60.0f;
    m_fovY     = 60.0f;
    m_fitScale = 1.0f;
    m_precache = false;

    m_modelHandle   = 0;
    m_modelSequence = -1;

    VectorClear(m_offset);
    VectorClear(m_rotateOffset);
    VectorClear(m_angles);
    VectorClear(m_spin);

    VectorClear(m_normalSpin);
    m_normalSpinPeriod = 0.0f;
    VectorClear(m_hoverSpin);
    m_hoverSpinPeriod = 0.0f;
    VectorClear(m_normalPivot);
    m_normalPivotPeriod = 0.0f;
    VectorClear(m_hoverPivot);
    m_hoverPivotPeriod = 0.0f;
    VectorClear(m_normalBob);
    m_normalBobPeriod = 0.0f;
    VectorClear(m_hoverBob);
    m_hoverBobPeriod = 0.0f;

    m_hModel       = 0;

    Cvar_Get("ui_mainMenuModel", "", 0);
}

void UIRenderModel::PrecacheModel(void)
{
    if (m_modelName.length()) {
        m_hModel = re.RegisterModel(m_modelName.c_str());
    }
}

void UIRenderModel::Pressed(Event *ev)
{
    if (m_modelName.length()) {
        if (m_command.length()) {
            UIWidget::Pressed(ev);

            str stuff = m_command + "\n";
            uii.Cmd_Stuff(stuff.c_str());
        }

        if (m_clicksound.length()) {
            uii.Snd_PlaySound(m_clicksound.c_str(), CHAN_MENU);  
        }
    }
}

void UIRenderModel::RightMouseButtonPressed(Event *ev)
{
    if (m_modelName.length()) {
        UIWidget::RightMouseButtonPressed(ev);
    }
}

void UIRenderModel::Draw(void)
{
    qhandle_t   handle;
    dtiki_t    *tiki;
    const char *modelName;
    vec3_t      origin;
    vec3_t      angles;
    vec3_t      offset;
    float       tsec;
    int         i;

    UIWidget::Draw();

    {
        /* One row per model per frame for the first frames of a screen, then
           sparsely.  The shared PROBE_Throttle counter this replaced sampled
           whichever widget happened to land on the throttle's phase and hid the
           other: the CONTROLS boot carried 17 rows for its munro preview and
           none at all for the 0,0,640x480 stardome, so the composition the blue
           background depends on could not be read.  This site answers per
           widget, and it comes after UIWidget::Draw -- the row is the fill that
           was just painted, which is what a flat background wash is. */
        /* Keyed on the widget, not on a global draw counter: CONTROLS is the one
           front-end screen that draws two UIRenderModels every frame, and a
           single counter sampled every 240 rows aliases exactly with the
           two-per-frame pattern, so every sample landed on the same widget and
           the second model never appeared in a boot. */
        static const void *s_menu3dWidget[16];
        static int         s_menu3dRows[16];
        static int         s_menu3dWidgets;
        int                slot = -1;

        for (i = 0; i < s_menu3dWidgets; i++) {
            if (s_menu3dWidget[i] == (const void *)this) {
                slot = i;
                break;
            }
        }
        if (slot < 0 && s_menu3dWidgets < (int)(sizeof(s_menu3dWidget) / sizeof(s_menu3dWidget[0]))) {
            slot                     = s_menu3dWidgets;
            s_menu3dWidget[slot]     = (const void *)this;
            s_menu3dRows[slot]       = 0;
            s_menu3dWidgets++;
        }

        if (slot >= 0) {
            s_menu3dRows[slot]++;
            if (s_menu3dRows[slot] <= 8 || (s_menu3dRows[slot] % 240) == 0) {
                UIVK_CENSUS_PROBE(
                    "menu3d",
                    "rm_draw widget=%p seq=%d state=%d vis=%d show=%d alpha=%.3f screen=%.0f,%.0f %.0fx%.0f "
                    "static='%s' cvar='%s' cv='%s'",
                    (const void *)this,
                    s_menu3dRows[slot],
                    (int)m_widgetState,
                    m_visible ? 1 : 0,
                    getShow() ? 1 : 0,
                    (double)m_local_alpha,
                    (double)m_screenframe.pos.x,
                    (double)m_screenframe.pos.y,
                    (double)m_screenframe.size.width,
                    (double)m_screenframe.size.height,
                    m_modelName.c_str(),
                    m_cvarname.c_str(),
                    m_cvarname.length() ? Cvar_VariableString(m_cvarname) : ""
                );
            }
        }
    }

    // empty, i.e. it drew a model retail does not draw.
    if (m_cvarname.length()) {
        modelName = Cvar_VariableString(m_cvarname);
        if (!modelName) {
            modelName = "";
        }
    } else if (m_modelName.length()) {
        modelName = m_modelName.c_str();
    } else {
        {
            static int menu3dmvk_noname;
            if (PROBE_Throttle(&menu3dmvk_noname, 4, 300)) {
                UIVK_PROBE("menu3d", "rm_early return=noname static='%s' cvar='%s'",
                           m_modelName.c_str(), m_cvarname.c_str());
            }
        }
        return;
    }

    handle = m_modelHandle;

    /* The memo also dies with the registration that produced it: a registration keeps TIKI
       models, so both tests above pass while the tiki's surface hShader[] are indices into the
       tr.shaders[] R_SetupShaders just rebuilt from zero. */
    if (!handle || m_resolvedModel != modelName || m_modelSequence != re.GetRegistrationSequence()) {
        m_resolvedModel = modelName;
        handle          = re.RegisterModel(modelName);
        m_modelHandle   = handle;
        m_modelSequence = re.GetRegistrationSequence();
    }

    if (!handle) {
        {
            static int menu3dmvk_nohandle;
            if (PROBE_Throttle(&menu3dmvk_nohandle, 4, 300)) {
                UIVK_PROBE("menu3d", "rm_early return=nohandle model='%s'", modelName);
            }
        }
        return;
    }

    tiki = re.R_Model_GetHandle(handle);
    if (!tiki) {
        /* A stale handle -- a renderer restart emptied the model table, and
           GetModelByHandle answered with the default BAD model.  Drop the memo
           so the next draw re-registers against the live table. */
        m_modelHandle   = 0;
        m_resolvedModel = str();
        m_modelSequence = -1;
        {
            static int menu3dmvk_notiki;
            if (PROBE_Throttle(&menu3dmvk_notiki, 4, 300)) {
                UIVK_PROBE("menu3d", "rm_early return=notiki model='%s' handle=%d", modelName, handle);
            }
        }
        return;
    }

    tsec  = cls.realtime * 0.001f;

    // STATE-DRIVEN ANIMATION.  Retail Draw @0x001B2C50 does not read a single
    str animName;
    switch (m_widgetState) {
    case WIDGET_STATE_HOVER:    animName = m_hoverAnim;    break;
    case WIDGET_STATE_SELECTED: animName = m_selectedAnim; break;
    case WIDGET_STATE_ARMED:    animName = m_armedAnim;    break;
    case WIDGET_STATE_DISABLED: animName = m_disabledAnim; break;
    default:                    animName = m_animName;     break;
    }
    if (!animName.length()) {
        animName = m_animName;
    }

    {
        static int menu3dmvk_submit;
        if (PROBE_Throttle(&menu3dmvk_submit, 4, 120)) {
            UIVK_PROBE(
                "menu3d",
                "rm_submit model='%s' handle=%d tiki=%p state=%d anim='%s' fov=%.0fx%.0f rect=%.0f,%.0f %.0fx%.0f realtime=%d",
                modelName,
                handle,
                (void *)tiki,
                (int)m_widgetState,
                animName.c_str(),
                (double)m_fovX,
                (double)m_fovY,
                (double)m_screenframe.pos.x,
                (double)m_screenframe.pos.y,
                (double)m_screenframe.size.width,
                (double)m_screenframe.size.height,
                cls.realtime
            );
        }
    }

    // invention; it reproduced the retail main-menu framing by construction --
    VectorClear(origin);

    VectorCopy(m_angles, angles);
    VectorCopy(m_offset, offset);

    for (i = 0; i < 3; i++) {
        angles[i] += m_spin[i] * tsec;
    }

    if (m_widgetState == WIDGET_STATE_NORMAL) {
        if (m_normalSpinPeriod > 0.0f) {
            float rev = tsec / m_normalSpinPeriod;
            for (i = 0; i < 3; i++) {
                angles[i] += m_normalSpin[i] * rev;
            }
        }

        if (m_normalPivotPeriod > 0.0f) {
            float s = sin(2.0f * M_PI * tsec / m_normalPivotPeriod);
            for (i = 0; i < 3; i++) {
                angles[i] += m_normalPivot[i] * s;
            }
        }

        if (m_normalBobPeriod > 0.0f) {
            float s = sin(2.0f * M_PI * tsec / m_normalBobPeriod);
            for (i = 0; i < 3; i++) {
                offset[i] += m_normalBob[i] * s;
            }
        }
    } else if (m_widgetState == WIDGET_STATE_HOVER) {
        if (m_hoverSpinPeriod > 0.0f) {
            float rev = tsec / m_hoverSpinPeriod;
            for (i = 0; i < 3; i++) {
                angles[i] += m_hoverSpin[i] * rev;
            }
        }

        if (m_hoverPivotPeriod > 0.0f) {
            float s = sin(2.0f * M_PI * tsec / m_hoverPivotPeriod);
            for (i = 0; i < 3; i++) {
                angles[i] += m_hoverPivot[i] * s;
            }
        }

        if (m_hoverBobPeriod > 0.0f) {
            float s = sin(2.0f * M_PI * tsec / m_hoverBobPeriod);
            for (i = 0; i < 3; i++) {
                offset[i] += m_hoverBob[i] * s;
            }
        }
    }

    // Retail UIRenderModel::Draw @0x001B2C50 does NOT build the scene itself: it
    CL_Draw3DModel(
        m_screenframe.pos.x,
        m_screenframe.pos.y,
        m_screenframe.size.width,
        m_screenframe.size.height,
        handle,
        origin,
        m_rotateOffset,
        offset,
        angles,
        animName,
        1,
        NULL,
        (float)(int)m_fovX,
        (float)(int)m_fovY
    );

    set2D();
}

void UIRenderModel::setModel(Event *ev)
{
    m_modelName = ev->GetString(1);

    if (m_precache) {
        PrecacheModel();
    }
}

void UIRenderModel::setModelPrecache(Event *ev)
{
    m_precache = true;
}

void UIRenderModel::setModelOffset(Event *ev)
{
    ev->GetVector(1).copyTo(m_offset);
}

void UIRenderModel::setModelRotateOffset(Event *ev)
{
    ev->GetVector(1).copyTo(m_rotateOffset);
}

void UIRenderModel::setModelAngles(Event *ev)
{
    ev->GetVector(1).copyTo(m_angles);
}

void UIRenderModel::setModelPitch(Event *ev)
{
    m_angles[0] = ev->GetFloat(1);
}

void UIRenderModel::setModelYaw(Event *ev)
{
    m_angles[1] = ev->GetFloat(1);
}

void UIRenderModel::setModelRoll(Event *ev)
{
    m_angles[2] = ev->GetFloat(1);
}

void UIRenderModel::setModelAnimation(Event *ev)
{
    m_animName = ev->GetString(1);
}

void UIRenderModel::setModelHoverAnimation(Event *ev)
{
    m_hoverAnim = ev->GetString(1);
}

void UIRenderModel::setModelArmedAnimation(Event *ev)
{
    m_armedAnim = ev->GetString(1);
}

void UIRenderModel::setModelSelectedAnimation(Event *ev)
{
    m_selectedAnim = ev->GetString(1);
}

void UIRenderModel::setModelDisabledAnimation(Event *ev)
{
    m_disabledAnim = ev->GetString(1);
}

void UIRenderModel::setModelFov(Event *ev)
{
    // retail @0x001B20F0 reads BOTH args with GetInteger, not GetFloat, then runs each
    m_fovX = (float)ev->GetInteger(1);
    m_fovY = (float)ev->GetInteger(2);
}

// engine_EF2exe_symbols.txt.  The retail binding is therefore not recoverable
void UIRenderModel::LayoutRenderModelSpin(Event *ev)
{
    ev->GetVector(1).copyTo(m_spin);
}

void UIRenderModel::setModelGameplayProperty(Event *ev)
{
    str group;
    str name;

    if (ev->NumArgs() >= 1) {
        group = ev->GetString(1);
    } else {
        ev->Error("Index %d out of range.", 1);
        group = "";
    }

    if (ev->NumArgs() >= 2) {
        name = ev->GetString(2);
    } else {
        ev->Error("Index %d out of range.", 2);
        name = "";
    }

    if (GameplayManager::getTheGameplayManager()) {
        m_modelPropertyGroup = group;
        m_modelPropertyName  = name;
        m_modelPropertyInUse = true;
    } else {
        Com_Printf("ERROR: Couldn't find GameplayManager to look up object property: %s.%s\n",
                   group.c_str(),
                   name.c_str());
    }
}

void UIRenderModel::setNormalSpin(Event *ev)
{
    ev->GetVector(1).copyTo(m_normalSpin);
    m_normalSpinPeriod = ev->GetFloat(2);
}

void UIRenderModel::setHoverSpin(Event *ev)
{
    ev->GetVector(1).copyTo(m_hoverSpin);
    m_hoverSpinPeriod = ev->GetFloat(2);
}

void UIRenderModel::setNormalPivot(Event *ev)
{
    ev->GetVector(1).copyTo(m_normalPivot);
    m_normalPivotPeriod = ev->GetFloat(2);
}

void UIRenderModel::setHoverPivot(Event *ev)
{
    ev->GetVector(1).copyTo(m_hoverPivot);
    m_hoverPivotPeriod = ev->GetFloat(2);
}

void UIRenderModel::setNormalBob(Event *ev)
{
    ev->GetVector(1).copyTo(m_normalBob);
    m_normalBobPeriod = ev->GetFloat(2);
}

void UIRenderModel::setHoverBob(Event *ev)
{
    ev->GetVector(1).copyTo(m_hoverBob);
    m_hoverBobPeriod = ev->GetFloat(2);
}

static void UIRenderModel_ResolveSoundProperty(UIWidget *self, Event *ev, Event &soundEventDef)
{
    str group;
    str name;
    str value;

    if (ev->NumArgs() >= 1) {
        group = ev->GetString(1);
    } else {
        ev->Error("Index %d out of range.", 1);
        group = "";
    }

    if (ev->NumArgs() >= 2) {
        name = ev->GetString(2);
    } else {
        ev->Error("Index %d out of range.", 2);
        name = "";
    }

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) {
        Com_Printf("ERROR: Couldn't find the gameplayManager to look up object property: %s.%s\n",
                   group.c_str(),
                   name.c_str());
        return;
    }

    if (gpm->hasProperty(group, name)) {
        value = gpm->getStringValue(group, name);
    }

    Event *soundEvent = new Event(soundEventDef);
    soundEvent->AddString(value);
    self->ProcessEvent(soundEvent);
}

void UIRenderModel::setClickSoundProperty(Event *ev)
{
    static Event soundEventDef(EV_Layout_ClickSound);

    if (ev->NumArgs() != 2) {
        Com_Printf("Usage: clicksoundproperty <objectname> <propertyname>\n");
        return;
    }

    UIRenderModel_ResolveSoundProperty(this, ev, soundEventDef);
}

void UIRenderModel::setAltClickSoundProperty(Event *ev)
{
    static Event soundEventDef(EV_Layout_AltClickSound);

    if (ev->NumArgs() != 2) {
        Com_Printf("Usage: altclicksoundproperty <objectname> <propertyname>\n");
        return;
    }

    UIRenderModel_ResolveSoundProperty(this, ev, soundEventDef);
}

