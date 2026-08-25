

#include "animate.h"
#include "viewthing.h"
#include "game.h"
#include "level.h"
#include "player.h"
#include "debuglines.h"
#include "scriptexception.h"

cvar_t *g_vt_droptoggle;      
cvar_t *g_vt_setFilename;     
cvar_t *g_vt_modelIndex;      
cvar_t *g_vt_numModels;       
cvar_t *g_vt_height;          
cvar_t *g_vt_scale;           
cvar_t *g_vt_yaw;             
cvar_t *g_vt_roll;            
cvar_t *g_vt_pitch;           
cvar_t *g_vt_xtrans;          
cvar_t *g_vt_ytrans;          
cvar_t *g_vt_ztrans;          
cvar_t *g_vt_makestatic;      
cvar_t *g_vt_usebox;          
cvar_t *g_vt_viewcount;       
cvar_t *g_vt_viewnumber;      
cvar_t *g_vt_viewrandomize;   
cvar_t *g_vt_viewsrandomize;  
cvar_t *g_vt_viewzrandomize;  
cvar_t *g_vt_viewyrandomize;  
cvar_t *g_vt_viewxrandomize;  
cvar_t *g_vt_viewminrandomize;
cvar_t *g_vt_viewmaxrandomize;
cvar_t *g_vt_viewbellrandomize;

Event EV_ViewThing_Think (	"viewthing_think",
	EV_CODEONLY,
	NULL,
	NULL,
	"Called every frame to process the view thing."
);


Event EV_ViewThing_ToggleAnimate (    "viewanimate",
    EV_CHEAT,
    NULL,
    NULL,
    "Cycle through the animations modes of the current viewthing\n"
    "No Animation\n"
    "Animation with no motion\n"
    "Animation with looping motion\n"
    "Animation with motion\n");
Event EV_ViewThing_SetModel (    "viewmodel",
    EV_CHEAT,
    "s",
    "viewthingModel",
    "Set the model of the current viewthing");
Event EV_ViewThing_NextFrame (    "viewnext",
    EV_CHEAT,
    NULL,
    NULL,
    "Advance to the next frame of animation of the current viewthing");
Event EV_ViewThing_PrevFrame (    "viewprev",
    EV_CHEAT,
    NULL,
    NULL,
    "Advance to the previous frame of animation of the current viewthing");
Event EV_ViewThing_NextAnim (    "viewnextanim",
    EV_CHEAT,
    NULL,
    NULL,
    "Advance to the next animation of the current viewthing");
Event EV_ViewThing_PrevAnim (    "viewprevanim",
    EV_CHEAT,
    NULL,
    NULL,
    "Advance to the previous animation of the current viewthing");
Event EV_ViewThing_ScaleUp (    "viewscaleup",
    EV_CHEAT,
    NULL,
    NULL,
    "Increase the scale of the current viewthing");
Event EV_ViewThing_ScaleDown (    "viewscaledown",
    EV_CHEAT,
    NULL,
    NULL,
    "Decrease the scale of the current viewthing");
Event EV_ViewThing_SetScale (    "viewscale",
    EV_CHEAT,
    "f",
    "scale",
    "Set the scale of the current viewthing");
Event EV_ViewThing_SetYaw (    "viewyaw",
    EV_CHEAT,
    "f",
    "yaw",
    "Set the yaw of the current viewthing");
Event EV_ViewThing_SetPitch (    "viewpitch",
    EV_CHEAT,
    "f",
    "pitch",
    "Set the pitch of the current viewthing");
Event EV_ViewThing_SetRoll (    "viewroll",
    EV_CHEAT,
    "f",
    "roll",
    "Set the roll of the current viewthing");
Event EV_ViewThing_SetAngles
(
	"viewangles",
	EV_CHEAT,
	"f[0,360]f[0,360]f[0,360]",
	"pitch yaw roll",
	"Set the angles of the current viewthing"
);
Event EV_ViewThing_TranslateX
(
	"viewxtranslate",
	EV_CHEAT,
	"f",
	"xtrans",
	"Translate current viewthing in x direction"
);
Event EV_ViewThing_TranslateY
(
	"viewytranslate",
	EV_CHEAT,
	"f",
	"ytrans",
	"Translate current viewthing in y direction"
);
Event EV_ViewThing_TranslateZ
(
	"viewztranslate",
	EV_CHEAT,
	"f",
	"ztrans",
	"Translate current viewthing in z direction"
);
Event EV_ViewThing_Pull
(
	"viewthingpull",
	EV_CHEAT,
	NULL,
	NULL,
	"Pulls the current viewthing to the camera"
);

Event EV_ViewThing_Spawn (    "viewspawn",
    EV_CHEAT,
    "s",
    "model",
    "Create a viewthing with the specified model");
Event EV_ViewThing_Next (    "viewthingnext",
    EV_CHEAT,
    NULL,
    NULL,
    "Change the active viewthing to the next viewthing");
Event EV_ViewThing_Prev (    "viewthingprev",
    EV_CHEAT,
    NULL,
    NULL,
    "Change the active viewthing to the previous viewthing");
Event EV_ViewThing_Attach (    "viewattach",
    EV_CHEAT,
    "ss",
    "tagname model",
    "Attach a model the the specified tagname");
Event EV_ViewThing_Detach (    "viewdetach",
    EV_CHEAT,
    NULL,
    NULL,
    "Detach the current viewthing from its parent");
Event EV_ViewThing_DetachAll (    "viewdetachall",
    EV_CHEAT,
    NULL,
    NULL,
    "Detach all the models attached to the current viewthing");
Event EV_ViewThing_Delete (    "viewdelete",
    EV_CHEAT,
    NULL,
    NULL,
    "Delete the current viewthing");
Event EV_ViewThing_SetOrigin (    "vieworigin",
    EV_CHEAT,
    "fff",
    "x y z",
    "Set the origin of the current viewthing");
Event EV_ViewThing_DeleteAll (    "viewdeleteall",
    EV_CHEAT,
    NULL,
    NULL,
    "Delete all viewthings");
Event EV_ViewThing_LastFrame (    "viewlastframe",
    EV_CODEONLY,
    NULL,
    NULL,
    "Called when the view things last animation frame is displayed.");
Event EV_ViewThing_SaveOffSurfaces (    "viewsavesurfaces",
    EV_CODEONLY,
    NULL,
    NULL,
    "Called after the model is spawned to save off the models original surfaces.");
Event EV_ViewThing_PrintTime (    "_viewthing_printtime",
    EV_CODEONLY,
    NULL,
    NULL,
    "Prints out the current level.time.");
Event EV_ViewThing_SetAnim (    "viewsetanim",
    EV_CHEAT,
    "f",
    "animNum",
    "Set the animation absolutely based off a floating point value");

Event EV_ViewThing_SpawnFromTS
(
	"viewspawnfromts",
	EV_CHEAT,
	"sS",
	"reloadmodel spawnmodel",
	"Create a viewthing from the model specified by the ToolServer"
);

Event EV_ViewThing_SetStatic
(
	"viewsetstatic",
	EV_CHEAT,
	"i",
	"boolean",
	"Makes the current viewthing a static (not dynamically lit) object"
);
Event EV_ViewThing_NextMorph
(
	"viewnextmorph",
	EV_CHEAT,
	NULL,
	NULL,
	"Goes to the next morph for this model"
);
Event EV_ViewThing_PrevMorph
(
	"viewprevmorph",
	EV_CHEAT,
	NULL,
	NULL,
	"Goes to the previous morph for this model"
);
Event EV_ViewThing_Morph
(
	"viewmorph",
	EV_CHEAT,
	NULL,
	NULL,
	"Morphs the current morph"
);
Event EV_ViewThing_Unmorph
(
	"viewunmorph",
	EV_CHEAT,
	NULL,
	NULL,
	"Unmorphs the current morph"
);
Event EV_ViewSet_Load
(
	"viewsetload",
	EV_CHEAT,
	"s",
	"filename",
	"Loads the specified viewset"
);
Event EV_ViewSet_Save
(
	"viewsetsave",
	EV_CHEAT,
	NULL,
	NULL,
	"Saves names and positions of currently spawned things data to disk"
);
Event EV_ViewThing_Copy
(
	"viewcopy",
	EV_CHEAT,
	NULL,
	NULL,
	"Copies the current viewthing"
);
Event EV_ViewThing_Shoot
(
	"viewshoot",
	EV_CHEAT,
	NULL,
	NULL,
	"Shoots current viewthing ahead and sticks it on the wall"
);
Event EV_ViewList_List
(
	"viewlistlist",
	EV_CHEAT,
	NULL,
	NULL,
	"List all TIKIs in the current .lst set"
);
Event EV_ViewList_NextInSet
(
	"viewlistnext",
	EV_CHEAT,
	NULL,
	NULL,
	"Delete current viewthing and load next view thing in view list"
);
Event EV_ViewList_PrevInSet
(
	"viewlistprev",
	EV_CHEAT,
	NULL,
	NULL,
	"Delete current viewthing and load prev view thing in view list"
);
Event EV_ViewList_LiftModel
(
	"viewlistliftmodel",
	EV_CHEAT,
	"f",
	"z",
	"Raise the origin (z) by specified amount"
);
Event EV_ViewList_ToggleDrop
(
	"viewlisttoggledrop",
	EV_CHEAT,
	NULL,
	NULL,
	"Toggle whether objects are dropped upon spawning"
);
Event EV_ViewList_Jump
(
	"viewlistjump",
	EV_CHEAT,
	"i",
	"modelindex",
	"Jumps to a specific model in a view set (.lst file) of models"
);
Event EV_ViewThing_ResetOrigin
(
	"viewresetorigin",
	EV_CHEAT,
	NULL,
	NULL,
	"Sets the origin of current viewspawn to its current origin"
);
Event EV_ViewThing_XTrans
(
	"viewtransx",
	EV_CHEAT,
	"i",
	"offsetamount",
	"translate current viewspawn in x direction by specified amount"
);
Event EV_ViewThing_YTrans
(
	"viewtransy",
	EV_CHEAT,
	"i",
	"offsetamount",
	"translate current viewspawn in y direction by specified amount"
);
Event EV_ViewThing_ZTrans
(
	"viewtransz",
	EV_CHEAT,
	"i",
	"offsetamount",
	"translate current viewspawn in z direction by specified amount"
);
Event EV_ViewThing_Offset
(
	"viewoffset",
	EV_CHEAT,
	"fff",
	"x y z",
	"Offsets the origin of the current viewthing"
);

Event EV_ViewThing_ChangeRoll
(
	"viewchangeroll",
	EV_CHEAT,
	"i",
	"offsetamount",
	"rotate current viewspawn around x axis by specified amount"
);
Event EV_ViewThing_ChangePitch
(
	"viewchangepitch",
	EV_CHEAT,
	"i",
	"offsetamount",
	"rotate current viewspawn around y axis by specified amount"
);
Event EV_ViewThing_ChangeYaw
(
	"viewchangeyaw",
	EV_CHEAT,
	"i",
	"offsetamount",
	"rotate current viewspawn around z axis by specified amount"
);
Event EV_ViewThing_Flash
(
	"viewflash",
	EV_CHEAT,
	NULL,
	NULL,
	"Flashes the currently select viewthing"
);
Event EV_ViewThing_SetFrame
(
	"viewsetframe",
	EV_CHEAT,
	NULL,
	NULL,
	"Set the animation to a specific frame number."
);

CLASS_DECLARATION(Listener, ViewMaster, NULL) {
    {&EV_ViewThing_Spawn,         &ViewMaster::Spawn           },
    {&EV_ViewThing_SpawnFromTS,   &ViewMaster::SpawnFromTS     },
    {&EV_ViewThing_Next,          &ViewMaster::Next            },
    {&EV_ViewThing_Prev,          &ViewMaster::Prev            },
    {&EV_ViewThing_SetModel,      &ViewMaster::SetModelEvent   },
    {&EV_ViewThing_DeleteAll,     &ViewMaster::DeleteAll       },
    {&EV_ViewThing_ToggleAnimate, &ViewMaster::PassEvent       },
    {&EV_ViewThing_NextFrame,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_PrevFrame,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_NextAnim,      &ViewMaster::PassEvent       },
    {&EV_ViewThing_PrevAnim,      &ViewMaster::PassEvent       },
    {&EV_ViewThing_ScaleUp,       &ViewMaster::PassEvent       },
    {&EV_ViewThing_ScaleDown,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetScale,      &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetYaw,        &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetPitch,      &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetRoll,       &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetAngles,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_Attach,        &ViewMaster::PassEvent       },
    {&EV_ViewThing_Detach,        &ViewMaster::PassEvent       },
    {&EV_ViewThing_DetachAll,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetStatic,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetOrigin,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetAnim,       &ViewMaster::PassEvent       },
    {&EV_ViewThing_NextMorph,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_PrevMorph,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_Morph,         &ViewMaster::PassEvent       },
    {&EV_ViewThing_Unmorph,       &ViewMaster::PassEvent       },
    {&EV_ViewThing_ChangeRoll,    &ViewMaster::PassEvent       },
    {&EV_ViewThing_ChangePitch,   &ViewMaster::PassEvent       },
    {&EV_ViewThing_ChangeYaw,     &ViewMaster::PassEvent       },
    {&EV_ViewThing_Flash,         &ViewMaster::PassEvent       },
    {&EV_ViewThing_SetFrame,      &ViewMaster::PassEvent       },
    {&EV_ViewSet_Load,            &ViewMaster::LoadSet         },
    {&EV_ViewSet_Save,            &ViewMaster::Save            },
    {&EV_ViewThing_Copy,          &ViewMaster::Copy            },
    {&EV_ViewThing_Shoot,         &ViewMaster::Shoot           },
    {&EV_ViewThing_Delete,        &ViewMaster::Delete          },
    {&EV_ViewThing_TranslateX,    &ViewMaster::SetXTranslation },
    {&EV_ViewThing_TranslateY,    &ViewMaster::SetYTranslation },
    {&EV_ViewThing_TranslateZ,    &ViewMaster::SetZTranslation },
    {&EV_ViewList_PrevInSet,      &ViewMaster::PrevModelInSet  },
    {&EV_ViewList_NextInSet,      &ViewMaster::NextModelInSet  },
    {&EV_ViewList_LiftModel,      &ViewMaster::SetZTranslation },
    {&EV_ViewList_ToggleDrop,     &ViewMaster::ToggleDrop      },
    {&EV_ViewList_Jump,           &ViewMaster::JumpToModel     },
    {&EV_ViewThing_Pull,          &ViewMaster::PullToCamera    },
    {&EV_ViewThing_ResetOrigin,   &ViewMaster::ResetOrigin     },
    {&EV_ViewThing_XTrans,        &ViewMaster::XTranslate      },
    {&EV_ViewThing_YTrans,        &ViewMaster::YTranslate      },
    {&EV_ViewThing_ZTrans,        &ViewMaster::ZTranslate      },
    {&EV_ViewThing_Offset,        &ViewMaster::Offset          },
    {NULL,                        NULL                         }
};

ViewMaster Viewmodel;

ViewMaster::ViewMaster()
{
    current_viewthing = NULL;
}

void ViewMaster::Init(void)
{
    gi.AddCommand("viewanimate");
    gi.AddCommand("viewmodel");
    gi.AddCommand("viewnext");
    gi.AddCommand("viewprev");
    gi.AddCommand("viewnextanim");
    gi.AddCommand("viewprevanim");
    gi.AddCommand("viewscaleup");
    gi.AddCommand("viewscaledown");
    gi.AddCommand("viewscale");
    gi.AddCommand("viewyaw");
    gi.AddCommand("viewpitch");
    gi.AddCommand("viewroll");
    gi.AddCommand("viewangles");
    gi.AddCommand("viewxtranslate");
    gi.AddCommand("viewytranslate");
    gi.AddCommand("viewztranslate");
    gi.AddCommand("viewoffset");
    gi.AddCommand("viewspawn");
    gi.AddCommand("viewspawnfromts");
    gi.AddCommand("viewthingnext");
    gi.AddCommand("viewthingprev");
    gi.AddCommand("viewthingpull");
    gi.AddCommand("viewattach");
    gi.AddCommand("viewdetach");
    gi.AddCommand("viewdetachall");
    gi.AddCommand("viewdelete");
    gi.AddCommand("viewsetstatic");
    gi.AddCommand("vieworiginlift");
    gi.AddCommand("vieworigin");
    gi.AddCommand("viewdeleteall");
    gi.AddCommand("viewsetanim");
    gi.AddCommand("viewsetload");
    gi.AddCommand("viewsetnext");
    gi.AddCommand("viewsetprev");
    gi.AddCommand("viewsetsave");
    gi.AddCommand("viewcopy");
    gi.AddCommand("viewlistlist");
    gi.AddCommand("viewlistnext");
    gi.AddCommand("viewlistprev");
    gi.AddCommand("viewlistliftmodel");
    gi.AddCommand("viewlisttoggledrop");
    gi.AddCommand("viewlistjump");
    gi.AddCommand("viewresetorigin");
    gi.AddCommand("viewtransx");
    gi.AddCommand("viewtransy");
    gi.AddCommand("viewtransz");
    gi.AddCommand("viewflash");
    gi.AddCommand("viewsetframe");

    g_vt_droptoggle        = gi.cvar("g_vt_droptoggle",       "1",    0);
    g_vt_setFilename       = gi.cvar("g_vt_setFilename",      "",     0);
    g_vt_modelIndex        = gi.cvar("g_vt_modelIndex",       "0",    0);
    g_vt_height            = gi.cvar("g_vt_height",           "0",    0);
    g_vt_makestatic        = gi.cvar("g_vt_makestatic",       "1",    0);
    g_vt_scale             = gi.cvar("g_vt_scale",            "1.0",  0);
    g_vt_numModels         = gi.cvar("g_vt_numModels",        "0",    0);
    g_vt_yaw               = gi.cvar("g_vt_yaw",              "0.0",  0);
    g_vt_pitch             = gi.cvar("g_vt_pitch",            "0.0",  0);
    g_vt_roll              = gi.cvar("g_vt_roll",             "0.0",  0);
    g_vt_xtrans            = gi.cvar("g_vt_xtrans",           "0.0",  0);
    g_vt_ytrans            = gi.cvar("g_vt_ytrans",           "0.0",  0);
    g_vt_ztrans            = gi.cvar("g_vt_ztrans",           "0.0",  0);
    g_vt_usebox            = gi.cvar("g_vt_usebox",           "1",    0);
    g_vt_viewnumber        = gi.cvar("g_vt_viewnumber",       "0",    0);
    g_vt_viewcount         = gi.cvar("g_vt_viewcount",        "0",    0);
    g_vt_viewrandomize     = gi.cvar("g_vt_viewrandomize",    "0",    0);
    g_vt_viewsrandomize    = gi.cvar("g_vt_viewsrandomize",   "1",    0);
    g_vt_viewzrandomize    = gi.cvar("g_vt_viewzrandomize",   "1",    0);
    g_vt_viewyrandomize    = gi.cvar("g_vt_viewyrandomize",   "0",    0);
    g_vt_viewxrandomize    = gi.cvar("g_vt_viewxrandomize",   "0",    0);
    g_vt_viewminrandomize  = gi.cvar("g_vt_viewminrandomize", "25",   0);
    g_vt_viewmaxrandomize  = gi.cvar("g_vt_viewmaxrandomize", "175",  0);
    g_vt_viewbellrandomize = gi.cvar("g_vt_viewminrandomize", "1",    0);
}

void ViewMaster::Next(Event *ev)
{
    _selectNext();
}

void ViewMaster::Prev(Event *ev)
{
    _selectPrevious();
}

void ViewMaster::DeleteAll(Event *ev)
{
    Entity *next;

    for (next = (Entity *)G_FindClass(NULL, "viewthing"); next != NULL;
         next = (Entity *)G_FindClass(next, "viewthing")) {
        next->PostEvent(EV_Remove, 0);
    }

    current_viewthing = NULL;
}

void ViewMaster::Spawn(Event *ev)
{
    Viewthing *viewthing;
    str        mdl;
    Vector     forward;
    Vector     up;
    Vector     delta;
    Event     *event;
    Entity    *ent;

    mdl = ev->GetString(1);
    if (!mdl || !mdl[0]) {
        ScriptError("Must specify a model name");
        return;
    }

    ent = g_entities[0].entity;
    assert(ent);
    if (!ent) {
        return;
    }

    viewthing = new Viewthing;

    current_viewthing = viewthing;

    ent->angles.AngleVectors(&forward, NULL, &up);

    viewthing->baseorigin = ent->origin;
    viewthing->baseorigin += forward * 48;
    viewthing->baseorigin += up * 48;

    viewthing->setOrigin(viewthing->baseorigin);
    viewthing->droptofloor(256);

    viewthing->baseorigin = viewthing->origin;

    delta = ent->origin - viewthing->origin;
    viewthing->setAngles(delta.toAngles());

    event = new Event(EV_ViewThing_SetModel);
    event->AddString(mdl);
    viewthing->ProcessEvent(event);

    if (!gi.IsModel(viewthing->edict->s.modelindex)) {
        ev->Error("model %s not found, viewmodel not spawned.", mdl.c_str());
        delete viewthing;
        current_viewthing = NULL;
        return;
    }
}

void ViewMaster::SetModelEvent(Event *ev)
{
    str        mdl;
    char       str[128];
    Event     *event;
    Viewthing *viewthing;

    mdl = ev->GetString(1);
    if (!mdl || !mdl[0]) {
        ScriptError("Must specify a model name");
        return;
    }

    if (!current_viewthing) {
        current_viewthing = (Entity *)G_FindClass(NULL, "viewthing");
        if (!current_viewthing) {
            ScriptError("No viewmodel");
            return;
        }
    }

    viewthing = (Viewthing *)((Entity *)current_viewthing);

    str[0] = 0;
    if ((mdl[1] != ':') && Q_stricmpn(mdl, "models", 6)) {
        strcpy(str, "models/");
    }
    strcat(str, mdl);

    event = new Event(EV_ViewThing_SetModel);
    event->AddString(str);
    viewthing->ProcessEvent(event);
    viewthing->UpdateCvars();
}

void ViewMaster::PassEvent(Event *ev)
{
    Viewthing *viewthing;
    Event     *event;

    if (!current_viewthing) {
        // retail PassEvent__10ViewMasterFP5Event calls Event::Error here, not ScriptError
        ev->Error("No viewmodel");
        return;
    }

    viewthing = (Viewthing *)((Entity *)current_viewthing);
    if (viewthing) {
        event = new Event(ev);
        viewthing->ProcessEvent(event);
    }
}



CLASS_DECLARATION(Entity, Viewthing, "viewthing") {

	{ &EV_ViewThing_Think,				&Viewthing::ThinkEvent },
	{ &EV_ViewThing_LastFrame,     		&Viewthing::LastFrameEvent },
	{ &EV_ViewThing_ToggleAnimate,		&Viewthing::ToggleAnimateEvent },
	{ &EV_ViewThing_SetModel,			&Viewthing::SetModelEvent },
	{ &EV_ViewThing_NextFrame,			&Viewthing::NextFrameEvent },
	{ &EV_ViewThing_PrevFrame,			&Viewthing::PrevFrameEvent },
	{ &EV_ViewThing_NextAnim,			&Viewthing::NextAnimEvent },
	{ &EV_ViewThing_PrevAnim,			&Viewthing::PrevAnimEvent },
	{ &EV_ViewThing_ScaleUp,			&Viewthing::ScaleUpEvent },
	{ &EV_ViewThing_ScaleDown,			&Viewthing::ScaleDownEvent },
	{ &EV_ViewThing_SetScale,			&Viewthing::SetScaleEvent },
	{ &EV_ViewThing_SetYaw,				&Viewthing::SetYawEvent },
	{ &EV_ViewThing_SetPitch,			&Viewthing::SetPitchEvent },
	{ &EV_ViewThing_SetRoll,			&Viewthing::SetRollEvent },
	{ &EV_ViewThing_SetAngles,			&Viewthing::SetAnglesEvent },
	{ &EV_ViewThing_SetStatic,          &Viewthing::SetStatic },
	{ &EV_ViewThing_Attach,				&Viewthing::AttachModel },
	{ &EV_ViewThing_Detach,				&Viewthing::Delete },
	{ &EV_ViewThing_DetachAll, 			&Viewthing::DetachAll },
	{ &EV_ViewThing_Delete,				&Viewthing::Delete },
	{ &EV_ViewThing_SetOrigin,			&Viewthing::ChangeOrigin },
	{ &EV_ViewThing_SaveOffSurfaces,	&Viewthing::SaveSurfaces },
	{ &EV_ViewThing_PrintTime,			&Viewthing::PrintTime },
	{ &EV_ViewThing_SetAnim,			&Viewthing::SetAnim },
	{ &EV_ViewThing_NextMorph,			&Viewthing::NextMorph },
	{ &EV_ViewThing_PrevMorph,			&Viewthing::PrevMorph },
	{ &EV_ViewThing_Morph,				&Viewthing::Morph },
	{ &EV_ViewThing_Unmorph,			&Viewthing::Unmorph },
	{ &EV_ViewThing_ChangeRoll,         &Viewthing::ChangeRollEvent },
	{ &EV_ViewThing_ChangePitch,        &Viewthing::ChangePitchEvent },
	{ &EV_ViewThing_ChangeYaw,          &Viewthing::ChangeYawEvent },
	{ &EV_ViewThing_Flash,              &Viewthing::Flash },
	{ &EV_ViewThing_SetFrame,           &Viewthing::SetFrame },

	{ NULL, NULL }
}
;

Viewthing::Viewthing(void)
{
    animate = new Animate(this);
    _static     = (g_vt_makestatic->integer != 0) ? qtrue : qfalse;
    _selected   = false;
    _pulseCount = 0;

    frame     = 0;
    animstate = 0;
    setSolidType(SOLID_NOT);
    baseorigin                  = origin;
    Viewmodel.current_viewthing = this;
    edict->s.eType              = ET_MODELANIM;
    edict->s.renderfx |= RF_SHADOW;
    edict->s.renderfx |= RF_SHADOW_PRECISE;
    edict->s.renderfx |= RF_EXTRALIGHT;

    current_morph = 0;

    gi.cvar_set("g_vt_scale", "1.0");
    gi.cvar_set("g_vt_roll", "0.0");
    gi.cvar_set("g_vt_pitch", "0.0");
    gi.cvar_set("g_vt_yaw", "0.0");
    gi.cvar_set("g_vt_xtrans", "0.0");
    gi.cvar_set("g_vt_ytrans", "0.0");
    gi.cvar_set("g_vt_ztrans", "0.0");

    PostEvent(EV_ViewThing_SaveOffSurfaces, FRAMETIME);

    PostEvent(EV_ViewThing_Think, FRAMETIME);
}

void Viewthing::PrintTime(Event *ev)
{
    gi.Printf("ev current frame %d leveltime %.2f\n", ev->GetInteger(1), level.time);
}

void Viewthing::UpdateCvars(qboolean quiet)
{
    gi.cvar_set("viewmodelanim", animate->AnimName());
    gi.cvar_set("viewmodeltotaltime", va("%.3f", animate->AnimTime()));
    gi.cvar_set("viewmodelanimframes", va("%d", animate->NumFrames()));
    gi.cvar_set("viewmodelframe", va("%d", CurrentFrame()));
    gi.cvar_set("viewmodelname", model.c_str());
    gi.cvar_set("viewmodelscale", va("%0.2f", edict->s.scale));
    gi.cvar_set("currentviewthing", model.c_str());
    gi.cvar_set("viewthingorigin",
                va("%0.2f,%0.2f,%0.2f", edict->s.origin[0], edict->s.origin[1],
                   edict->s.origin[2]));
    gi.cvar_set("viewmodelanimnum",
                va("%.3f", (float)CurrentAnim() / (float)animate->NumAnims()));
    gi.cvar_set("g_vt_scale", va("%.2f", edict->s.scale));
    gi.cvar_set("g_vt_roll",
                va("%.2f", (angles.z <= 180.0f) ? angles.z : (angles.z - 360.0f)));
    gi.cvar_set("g_vt_pitch",
                va("%.2f", (angles.x <= 180.0f) ? angles.x : (angles.x - 360.0f)));
    gi.cvar_set("g_vt_yaw", va("%.2f", (angles.y <= 180.0f) ? angles.y : (angles.y - 360.0f)));
    gi.cvar_set("g_vt_xtrans", va("%.2f", (Viewmodel._lastBaseOrigin.x - baseorigin.x)));
    gi.cvar_set("g_vt_ytrans", va("%.2f", (Viewmodel._lastBaseOrigin.y - baseorigin.y)));
    gi.cvar_set("g_vt_ztrans", va("%.2f", (Viewmodel._lastBaseOrigin.z - baseorigin.z)));

    if (gi.Morph_NameForNum(edict->s.modelindex, current_morph)) {
        gi.cvar_set("viewmorphname", gi.Morph_NameForNum(edict->s.modelindex, current_morph));
    } else {
        gi.cvar_set("viewmorphname", "");
    }

    if (!quiet) {
        gi.Printf("%s, frame %3d, scale %4.2f\n", animate->AnimName(), CurrentFrame(),
                  edict->s.scale);
    }
}

void Viewthing::ThinkEvent(Event *ev)
{
    static float startTime = 0;

    if (_selected && (_pulseCount < 20)) {
        if (startTime == 0.0f) {
            startTime = level.time;
        }

        if ((level.time - startTime) > 0.15f) {
            if (this->hidden()) {
                this->showModel();
            } else {
                this->hideModel();
            }
            startTime    = 0.0f;
            _pulseCount++;
        }
    }

    int f;
    if (animstate >= 2) {
        Vector forward;
        Vector left;
        Vector up;
        Vector realmove;

        angles.AngleVectors(&forward, &left, &up);
        // exist in retail Entity and was never written non-zero in this port.
        realmove = left * total_delta[1] + up * total_delta[2] + forward * total_delta[0];
        setOrigin(baseorigin + realmove);
        gi.cvar_set(
            "viewthingorigin", va("%0.2f,%0.2f,%0.2f", edict->s.origin[0], edict->s.origin[1], edict->s.origin[2])
        );
    }
    PostEvent(EV_ViewThing_Think, FRAMETIME);
    if ((animstate > 0) && (Viewmodel.current_viewthing == this)) {
        f = CurrentFrame();
        if (f != lastframe) {
            float time;
            lastframe = f;
            time      = f * animate->AnimTime() / (float)animate->NumFrames();
            gi.Printf("current frame %d time %.2f\n", f, time);
            gi.cvar_set("viewmodeltime", va("%.2f", time));
            gi.cvar_set("viewmodelframe", va("%d", f));
            gi.cvar_set("viewmodelanim", animate->AnimName());
        }
    }
}

void Viewthing::LastFrameEvent(Event *ev)
{
    if (animstate != 3) {
        total_delta = Vector(0, 0, 0);
    }
}

void Viewthing::ToggleAnimateEvent(Event *ev)
{
    animstate = (animstate + 1) % 4;
    setOrigin(baseorigin);
    switch (animstate) {
    case 0:
        animate->SetFrame(frame, legs, -1);
        gi.Printf("Animation stopped.\n");
        gi.cvar_set("viewmodelanimmode", "Stopped");
        break;
    case 1:
        animate->NewAnim(CurrentAnim());
        gi.Printf("Animation no motion.\n");
        gi.cvar_set("viewmodelanimmode", "No Motion");
        break;
    case 2:
        animate->NewAnim(CurrentAnim(), EV_ViewThing_LastFrame);
        gi.Printf("Animation with motion and looping.\n");
        gi.cvar_set("viewmodelanimmode", "Motion and Looping");
        break;
    case 3:
        animate->NewAnim(CurrentAnim(), EV_ViewThing_LastFrame);
        gi.Printf("Animation with motion no looping.\n");
        gi.cvar_set("viewmodelanimmode", "Motion and No Looping");
        break;
    }
    UpdateCvars(qtrue);
}

void Viewthing::SetModelEvent(Event *ev)
{
    str modelname;

    modelname = ev->GetString(1);

    setModel(modelname);

    if (gi.IsModel(edict->s.modelindex)) {
        if (animate->HasAnim("idle")) {
            animate->RandomAnimate("idle");
        } else {
            animate->NewAnim(0);
        }

        frame = 0;
        animate->SetFrame(0);
    }
    UpdateCvars();
}

void Viewthing::NextFrameEvent(Event *ev)
{
    int numframes;

    numframes = animate->NumFrames();
    if (numframes) {
        frame = (frame + 1) % numframes;
        animate->SetFrame(frame, legs, -1);
        animstate = 0;
        UpdateCvars();
    }
}

void Viewthing::PrevFrameEvent(Event *ev)
{
    int numframes;

    numframes = animate->NumFrames();
    if (numframes) {
        frame = (frame - 1) % numframes;
        animate->SetFrame(frame, legs, -1);
        animstate = 0;
        UpdateCvars();
    }
}

void Viewthing::SetAnim(Event *ev)
{
    int numanims, anim;

    numanims = animate->NumAnims();
    if (numanims) {
        memcpy(edict->s.surfaces, origSurfaces, sizeof(origSurfaces));

        anim = ev->GetFloat(1) * numanims;
        if (anim >= numanims) {
            anim = numanims - 1;
        }
        animate->NewAnim(anim % numanims);
        frame = 0;
        animate->SetFrame(0, legs, -1);
        animstate = 0;
        UpdateCvars();
    }
}

void Viewthing::NextAnimEvent(Event *ev)
{
    int numanims;

    numanims = animate->NumAnims();
    if (numanims) {
        memcpy(edict->s.surfaces, origSurfaces, sizeof(origSurfaces));

        animate->NewAnim((CurrentAnim() + 1) % numanims);
        frame = 0;
        animate->SetFrame(0, legs, -1);
        animstate = 0;
        gi.Printf("Animation stopped.\n");
        gi.cvar_set("viewmodelanimmode", "Stopped");
        UpdateCvars();
    }
}

void Viewthing::PrevAnimEvent(Event *ev)
{
    int anim;
    int numanims;

    numanims = animate->NumAnims();
    if (numanims) {
        memcpy(edict->s.surfaces, origSurfaces, sizeof(origSurfaces));

        anim = CurrentAnim() - 1;
        while (anim < 0) {
            anim += numanims;
        }
        animate->NewAnim(anim);
        frame = 0;
        animate->SetFrame(0, legs, -1);
        animstate = 0;
        gi.Printf("Animation stopped.\n");
        gi.cvar_set("viewmodelanimmode", "Stopped");
        UpdateCvars();
    }
}

void Viewthing::ScaleUpEvent(Event *ev)
{
    edict->s.scale += 0.05f;
    UpdateCvars();
}

void Viewthing::ScaleDownEvent(Event *ev)
{
    edict->s.scale -= 0.05f;
    UpdateCvars();
}

void Viewthing::SetScaleEvent(Event *ev)
{
    if (ev->NumArgs()) {
        float s = ev->GetFloat(1);

        edict->s.scale = s;
        UpdateCvars();
    }
}

void Viewthing::SetYawEvent(Event *ev)
{
    if (ev->NumArgs() > 0) {
        angles.setYaw(ev->GetFloat(1));
        setAngles(angles);
        UpdateCvars();
    }
}

void Viewthing::SetPitchEvent(Event *ev)
{
    if (ev->NumArgs() > 0) {
        angles.setPitch(ev->GetFloat(1));
        setAngles(angles);
        UpdateCvars();
    }
}

void Viewthing::SetRollEvent(Event *ev)
{
    if (ev->NumArgs() > 0) {
        angles.setRoll(ev->GetFloat(1));
        setAngles(angles);
        UpdateCvars();
    }
}

void Viewthing::SetAnglesEvent(Event *ev)
{
    if (ev->NumArgs() > 2) {
        angles.x = ev->GetFloat(1);
        angles.y = ev->GetFloat(2);
        angles.z = ev->GetFloat(3);
        setAngles(angles);
    }

    gi.cvar_set("g_vt_roll", va("%0.2f", angles.x));
    gi.cvar_set("g_vt_pitch", va("%0.2f", angles.y));
    gi.cvar_set("g_vt_yaw", va("%0.2f", angles.z));
    gi.Printf("angles = %0.4f, %0.4f, %0.4f\n", angles.x, angles.y, angles.z);
}

void Viewthing::AttachModel(Event *ev)
{
    Event     *event;
    Viewthing *child;

    str mdl = ev->GetString(2);

    child = new Viewthing;
    child->setModel(mdl.c_str());

    if (!gi.IsModel(child->edict->s.modelindex)) {
        ev->Error("attachmodel %s not found, attachmodel not spawned.", mdl.c_str());
        delete child;
        Viewmodel.ProcessEvent(EV_ViewThing_Prev);
        return;
    }

    event = new Event(EV_Attach);
    event->AddEntity(this);
    event->AddString(ev->GetString(1));
    child->ProcessEvent(event);
}

void Viewthing::Delete(Event *ev)
{
    Viewmodel.current_viewthing = NULL;
    PostEvent(EV_Remove, 0);
    Viewmodel.PostEvent(EV_ViewThing_Next, 0);
}

void Viewthing::DetachAll(Event *ev)
{
    int i;
    int num;

    if (!bind_info) {
        return;
    }

    num = bind_info->numchildren;
    for (i = 0; i < MAX_MODEL_CHILDREN; i++) {
        Entity *ent;
        if (!bind_info->children[i]) {
            continue;
        }
        ent = (Entity *)G_GetEntity(bind_info->children[i]);
        ent->PostEvent(EV_Remove, 0);
        num--;
        if (!num) {
            break;
        }
    }
}

void Viewthing::ChangeOrigin(Event *ev)
{
    if (ev->NumArgs()) {
        origin.x = ev->GetFloat(1);
        origin.y = ev->GetFloat(2);
        origin.z = ev->GetFloat(3);
        setOrigin(origin);
        baseorigin = origin;
        gi.cvar_set("g_vt_height", va("%f", origin.z));
        UpdateCvars();
    }
    gi.Printf("vieworigin = x%0.4f y%0.4f z%0.4f\n", origin.x, origin.y, origin.z);
}

void Viewthing::SaveSurfaces(Event *ev)
{
    memcpy(origSurfaces, edict->s.surfaces, sizeof(origSurfaces));
}



void Viewthing::ChangePitchEvent( Event *ev )
{
	if ( ev->NumArgs() > 0)
	{
		angles.setPitch(angles.pitch() + ev->GetFloat( 1 ) );
		setAngles(angles);
		UpdateCvars();
	}
}


void Viewthing::ChangeRollEvent( Event *ev )
{
	if ( ev->NumArgs() > 0)
	{
		angles.setRoll(angles.roll() + ev->GetFloat( 1 ) );
		setAngles(angles);
		UpdateCvars();
	}
}


void Viewthing::ChangeYawEvent( Event *ev )
{
	if ( ev->NumArgs() > 0)
	{
		angles.setYaw(angles.yaw() + ev->GetFloat( 1 ) );
		setAngles(angles);
		UpdateCvars();
	}
}


void Viewthing::Flash( Event *ev )
{
	SetSelected(true);
}


void Viewthing::Morph( Event *ev )
{
	const char *morph_name;
	
	morph_name = gi.Morph_NameForNum( edict->s.modelindex, current_morph );
	
	if ( morph_name )
	{
		Event *new_event;
		
		new_event = new Event ( EV_Morph );
		new_event->AddString( morph_name );
		ProcessEvent( new_event );
	}
}


void Viewthing::NextMorph( Event *ev )
{
	current_morph++;
	
	if ( current_morph >= gi.NumMorphs( edict->s.modelindex ) )
		current_morph = 0;
	
	if ( gi.Morph_NameForNum( edict->s.modelindex, current_morph ) )
		gi.Printf( "morph %s\n", gi.Morph_NameForNum( edict->s.modelindex, current_morph ) );
	
	UpdateCvars( true );
}


void Viewthing::PrevMorph( Event *ev )
{
	current_morph--;
	
	if ( current_morph < 0 )
		current_morph = gi.NumMorphs( edict->s.modelindex ) - 1;
	
	if ( gi.Morph_NameForNum( edict->s.modelindex, current_morph ) )
		gi.Printf( "morph %s\n", gi.Morph_NameForNum( edict->s.modelindex, current_morph ) );
	
	UpdateCvars( true );
}


void Viewthing::SetFrame( Event *ev )
{
	int newframe;
	
	newframe = ev->GetInteger(1);
	if ( newframe < animate->NumFrames() )
	{
		animate->SetFrame( newframe );
		animstate = 0;
		UpdateCvars();
	}
}


void Viewthing::SetSelected( qboolean state )
{
	_selected   = state ;
	_pulseCount = 0 ;
	
	if (this->hidden())
	{
		this->showModel();
	}
}


void Viewthing::SetStatic( Event *ev )
{
	if (ev->NumArgs() == 1) 
	{
		_static = (ev->GetInteger(1)) ? true : false ;
	}
	else
	{
		_static = (g_vt_makestatic->integer != 0) ? true : false ;
	}
}


void Viewthing::Unmorph( Event *ev )
{
	const char *morph_name;
	
	morph_name = gi.Morph_NameForNum( edict->s.modelindex, current_morph );
	
	if ( morph_name )
	{
		Event *new_event;
		
		new_event = new Event ( EV_Unmorph );
		new_event->AddString( morph_name );
		ProcessEvent( new_event );
	}
}




void ViewMaster::Copy( Event * )
{
	DisplayCurrentModelInSet(false);
	_updateViewthingCounts();
}


void ViewMaster::Delete( Event * )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = (Viewthing*)( (Entity*) current_viewthing);
	
	_selectPrevious();                  
	
	if (current_viewthing == viewthing) 
	{
		_selectNext();                  
	}
	
	if (current_viewthing == viewthing) 
	{
		current_viewthing = 0 ;         
	}
	
	viewthing->PostEvent( EV_Remove, 0.0f );
	_updateViewthingCounts(-1);         
}


void ViewMaster::DeleteCurrentViewthing()
{
	if (current_viewthing)
	{
		Viewthing *viewthing = (Viewthing*)( (Entity*) current_viewthing);
		viewthing->PostEvent( EV_Remove, 0.0f );		
	}
}


void ViewMaster::DisplayCurrentModelInSet( bool spawnAtLastOrigin )
{
	if ( (g_vt_modelIndex->integer <= 0) || (g_vt_modelIndex->integer > _numberOfModelsInSet) )
	{
		gi.cvar_set( "viewlistmodelname", " " );
		gi.Printf( " No models in set or position out of range\n");
		return ; 
	}
	
	
	str modelName = _modelNamesArray.ObjectAt(g_vt_modelIndex->integer);
	
	Event* spawnEvent = new Event( EV_ViewThing_Spawn);
	spawnEvent->AddString(modelName);
	_spawnAtLastItemsOrigin = spawnAtLastOrigin ;
	Spawn(spawnEvent);
	_spawnAtLastItemsOrigin = false ;
	delete spawnEvent ;
	spawnEvent = 0 ;
	
	gi.cvar_set( "viewlistnumitems",  va( "%d", _numberOfModelsInSet ) );
	gi.cvar_set( "viewlistmodelname", modelName.c_str() );
	gi.cvar_set( "g_vt_scale",  "1.0");
	gi.cvar_set( "g_vt_height", "0"  );
}


void ViewMaster::JumpToModel( Event *ev )
{
	int nextIndex = ev->GetInteger(1);
	if ((nextIndex < 1) || (nextIndex > _numberOfModelsInSet))
	{
		gi.Printf(" Illegal index for model in viewset list\n");
		return ;
	}
	
	gi.cvar_set( "g_vt_modelIndex",  va( "%d", nextIndex ) );
	DeleteCurrentViewthing();
	DisplayCurrentModelInSet();
}


void ViewMaster::LoadModelsInSet( const str &setFileName )
{	
	char  *buffer;
	char  *buf;
	char   com_token[MAX_STRING_CHARS];
	
	if (_numberOfModelsInSet)
	{
		gi.cvar_set( "viewlistmodelname", va( " ") );
		gi.cvar_set( "g_vt_modelIndex",   "0");
		_modelNamesArray.FreeObjectList();
		_numberOfModelsInSet = 0 ;
	}
	
	if ( gi.FS_ReadFile( setFileName, ( void ** )&buf, true ) != -1 )
	{
		buffer = buf;
		while ( 1 )
		{
			strcpy( com_token, COM_ParseExt( &buffer, true ) );
			
			if (!com_token[0])
				break;
			
			if ((com_token[0] != '#')) {  
				const char *relative_pathname = NULL;
				
				str modelName  = "models/" ;
				
				relative_pathname = strstr( com_token, "models/" );
				
				if ( !relative_pathname )
					relative_pathname = strstr( com_token, "models\\" );
				
				if ( relative_pathname )
				{
					relative_pathname += strlen( "models/" );
					modelName += relative_pathname;
				}
				else
				{
					modelName += com_token;
				}
				
				_modelNamesArray.AddObject(modelName);
				_numberOfModelsInSet++ ;
			}
			
			while( 1 )
			{
				strcpy( com_token, COM_ParseExt( &buffer, false ) );
				if (!com_token[0])
					break;
			}
		}
		gi.FS_FreeFile( buf );
		
		if (_numberOfModelsInSet)
		{
			gi.cvar_set( "g_vt_modelIndex",  "1");
			DisplayCurrentModelInSet(false) ;
		}
		
		str setName = _setNamesArray.ObjectAt(_currentSetIdx);
		gi.cvar_set( "viewsetname",    va( "%s", setName.c_str()) );
		gi.cvar_set( "g_vt_numModels", va( "%d", _numberOfModelsInSet) );
	}
}


void ViewMaster::LoadSet( Event *ev )
{
	if (_numberOfModelsInSet || _numberOfSets)
	{
		gi.cvar_set( "viewsetname",       va( " ") );
		gi.cvar_set( "viewlistmodelname", va( " ") );
		gi.cvar_set( "g_vt_modelIndex",   "0");
		gi.cvar_set( "g_vt_setFilename",  "");
		
		_currentSetIdx       = 0 ;
		_numberOfSets        = 0 ;
		_numberOfModelsInSet = 0 ;
		_setNamesArray.FreeObjectList();
		_modelNamesArray.FreeObjectList();
	}
	
	str filename = ev->GetString(1);
	_setNamesArray.AddObject(filename);
	_numberOfSets++ ;
	
	if (_numberOfSets)
	{
		_currentSetIdx = 1 ;
		LoadModelsInSet(_setNamesArray.ObjectAt(_currentSetIdx));
	}
}


void ViewMaster::NextModelInSet( Event * )
{
	if (!_numberOfModelsInSet)
	{
		gi.Printf(" No current set\n");
		return ;
	}
	
	int nextIndex = ( g_vt_modelIndex->integer >= _numberOfModelsInSet) ? 1 : (g_vt_modelIndex->integer + 1 );
	gi.cvar_set( "g_vt_modelIndex",  va( "%d", nextIndex ) );
	DeleteCurrentViewthing();
	DisplayCurrentModelInSet();
}


void ViewMaster::Offset( Event *ev )
{
	Viewthing *viewthing;
	Event *new_event;
	
	if ( !current_viewthing ) 
		return;
	
	viewthing = ( Viewthing * )( Entity * )current_viewthing;
	
	new_event = new Event( EV_ViewThing_SetOrigin );
	
	new_event->AddFloat( viewthing->origin.x + ev->GetFloat( 1 ) );
	new_event->AddFloat( viewthing->origin.y + ev->GetFloat( 2 ) );
	new_event->AddFloat( viewthing->origin.z + ev->GetFloat( 3 ) );
	
	viewthing->ProcessEvent( new_event );
	
	_resetOrigin();
}


void ViewMaster::PrevModelInSet( Event *ev )
{
	if (!_numberOfModelsInSet)
	{
		gi.Printf(" No models in current set\n");
		return ;
	}
	
	int nextIndex = ( g_vt_modelIndex->integer <= 1) ? _numberOfModelsInSet : ( g_vt_modelIndex->integer - 1 );
	gi.cvar_set( "g_vt_modelIndex",  va( "%d", nextIndex ) );
	DeleteCurrentViewthing();
	DisplayCurrentModelInSet() ;
}


void ViewMaster::PullToCamera( Event* )
{
	if (!current_viewthing) return ;
	
	Entity* clientEntity = g_entities[ 0 ].entity;
	if (!clientEntity) return ;
	
	_lastBaseOrigin = clientEntity->origin ;
	_setToPosition(clientEntity->origin);
}


void ViewMaster::ResetOrigin( Event* )
{
	_resetOrigin();
}


void ViewMaster::Save( Event *ev )
{
	str         buf;
	str         filename;
	int         count = 1 ;
	
	filename = "maps/";
	filename += level.mapname;
	for(int i = filename.length() - 1; i >= 0; i-- )
	{
		if ( filename[ i ] == '.' )
		{
			filename[ i ] = 0;
			break;
		}
	}
	
	filename += "_ents.map";
	gi.cvar_set("g_vt_savefile", filename.c_str());
	gi.Printf( "Saving spawned things to '%s'...\n", filename.c_str() );
	
	buf = "";
	buf += va( "//\n" );
	buf += va( "// Viewspawned entities file \"%s\".\n", filename.c_str());
	buf += va( "//\n" );
	
	Viewthing *viewthingPtr = 0 ;
	Entity    *entityPtr    = 0 ;
	entityPtr = G_FindClass( entityPtr, "viewthing" );
	
	while (entityPtr != 0)
	{
		viewthingPtr = (Viewthing*)entityPtr;
		
		str   modelName     = viewthingPtr->model ;
		char* strPtr        = (char*)modelName.c_str();
		char* endPtr        = strPtr + (strlen(strPtr) - 1) ;
		bool  found         = false ;
		
		for (int idx=strlen(strPtr); idx > 0; idx++)
		{
			if (*endPtr == '/')
			{
				endPtr++ ;
				found = true ;
				break ;
			}
			endPtr-- ;
		}			
		
		if (found)
		{
			strPtr = endPtr ;
		}
		
		buf += va( "// Entity %d\n", count);
		buf += va( "{\n");
		buf += va( "\"make_static\" \"%d\"\n",				viewthingPtr->_static);
		buf += va( "\"classname\"   \"FROM_TIKI\"\n" );
		buf += va( "\"model\"       \"%s\"\n",				strPtr);			
		buf += va( "\"scale\"       \"%f\"\n",				viewthingPtr->edict->s.scale);
		buf += va( "\"origin\"      \"%.2f %.2f %.2f\"\n",	viewthingPtr->origin.x, viewthingPtr->origin.y, viewthingPtr->origin.z );
		buf += va( "\"angles\"      \"%.2f %.2f %.2f\"\n",	viewthingPtr->angles.x, viewthingPtr->angles.y, viewthingPtr->angles.z );
		buf += va( "\"spawnflags\"  \"1\"\n");
		buf += va( "}\n" );
		
		entityPtr = G_FindClass( entityPtr, "viewthing");
		count++ ;
	}
	gi.FS_WriteFile( filename.c_str(), buf.c_str(), buf.length() + 1 );
	gi.Printf( "Done.\n" );		
}


void ViewMaster::SetXTranslation( Event *ev )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
	Event *originEvent   = new Event(EV_ViewThing_SetOrigin);
	
	float y = viewthing->origin.y ;
	float z = viewthing->origin.z ;
	originEvent->AddFloat(_lastBaseOrigin.x + ev->GetFloat(1));
	originEvent->AddFloat(y);
	originEvent->AddFloat(z);
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	gi.cvar_set( "g_vt_xtrans", va("%0.2f", ev->GetFloat(1)) );
}


void ViewMaster::SetYTranslation( Event *ev )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
	Event *originEvent = new Event(EV_ViewThing_SetOrigin);
	
	float x = viewthing->origin.x ;
	float z = viewthing->origin.z ;
	originEvent->AddFloat(x);
	originEvent->AddFloat(_lastBaseOrigin.y + ev->GetFloat(1));
	originEvent->AddFloat(z);
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	gi.cvar_set( "g_vt_ytrans", va("%0.2f", ev->GetFloat(1)) );
}


void ViewMaster::SetZTranslation( Event *ev )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
	Event *originEvent = new Event(EV_ViewThing_SetOrigin);
	
	float x = viewthing->origin.x ;
	float y = viewthing->origin.y ;
	originEvent->AddFloat(x);
	originEvent->AddFloat(y);
	originEvent->AddFloat(_lastBaseOrigin.z + ev->GetFloat(1));
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	gi.cvar_set( "g_vt_ztrans", va("%0.2f", ev->GetFloat(1)) );
}


void ViewMaster::Shoot( Event *ev )
{
	int idx = g_vt_modelIndex->integer ;
	if ( (idx <= 0) || (idx > _modelNamesArray.NumObjects()) ) return ;
	
	Vector  forward ;
	Vector  right ;
	Vector  up ;
	Vector  view ;
	Vector  start;
	Vector	end;
	Vector  mins = vec_zero ;
	Vector  maxs = vec_zero ;
	trace_t	trace;
	
	Player* player = (Player*)g_entities[ 0 ].entity;
	if ( !player ) return;
	
	Viewthing* viewthing = 0 ;
	if (current_viewthing && g_vt_usebox->integer)
	{
		viewthing = (Viewthing*) ( (Entity*)current_viewthing);
		mins = viewthing->mins ;
		maxs = viewthing->maxs ;
	}
	
	view    = player->GetVAngles();
	AngleVectors( view, forward, right, up );
	start    = player->origin ;
	start.z += player->viewheight ;
	end      = start + ( forward * 10000.0f ) ;
	
	trace = G_FullTrace( start, mins, maxs, end, player, MASK_SHOT, false, "ViewMaster::shoot" );
	if ( ( trace.fraction == 1.0f ) || trace.allsolid || !trace.ent )
	{
		return ;
	}
	
	if ( g_showbullettrace->integer )
	{
		G_DebugLine( start, end, 1.0f, 1.0f, 1.0f, 1.0f );
	}
	
	Vector pos = Vector( trace.endpos ) + ( Vector( trace.plane.normal ) * 0.2f );
	
	str    modelName(_modelNamesArray.ObjectAt(g_vt_modelIndex->integer));
	spawnAtPosition(modelName, pos);
	
	if (current_viewthing)
	{
		Event* yawEvent = new Event(EV_ViewThing_SetYaw);
		viewthing = (Viewthing*) ( (Entity*)current_viewthing);
		Vector n(trace.plane.normal);
		float yaw = n.toYaw();
		yawEvent->AddFloat(yaw);
		viewthing->SetYawEvent(yawEvent);
		delete yawEvent ;
		yawEvent = 0 ;
		
		if (g_vt_viewrandomize->integer)
		{
			_randomizeViewthing(viewthing);
		}
	}
}


void ViewMaster::SpawnFromTS( Event *ev )
{
	char *data = (char*)gi.ToolServerGetData();
	
	if (!data) 
		return;
	
	DeleteAll(NULL); 
	
	char spawnmodel[256], reloadmodel[256];
	
	strcpy(reloadmodel, "models/");
	strcat(reloadmodel, ev->GetString(1));
	
	if ( ev->NumArgs() > 1 )
	{
		strcpy(spawnmodel, ev->GetString(2));
	}
	else
	{
		strcpy(spawnmodel,"models/");
		strcat(spawnmodel,  ev->GetString(1));
	}
	
	gi.DPrintf("Reloading Model: %s...\n",reloadmodel);
	gi.TikiLoadFromTS(reloadmodel, data);
	gi.setConfigstring( CS_DEBUGINFO, va( "reload %s %d", spawnmodel, gi.modelindex(reloadmodel) ) );
	
	Event* spawnEvent = new Event( EV_ViewThing_Spawn);
	spawnEvent->AddString(spawnmodel);
	Spawn(spawnEvent);
	delete spawnEvent;
	spawnEvent = 0 ;
	
	gi.setConfigstring( CS_DEBUGINFO, "" );
}


void ViewMaster::ToggleDrop( Event * )
{
	if (g_vt_droptoggle->integer)
	{
		gi.cvar_set("g_vt_droptoggle", "0");
	}
	else
	{
		gi.cvar_set("g_vt_droptoggle", "1");
	}
}


void ViewMaster::XTranslate( Event *ev )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
	Event *originEvent   = new Event(EV_ViewThing_SetOrigin);
	
	originEvent->AddFloat(viewthing->origin.x + ev->GetFloat(1));
	originEvent->AddFloat(viewthing->origin.y);
	originEvent->AddFloat(viewthing->origin.z);
	
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	_resetOrigin();
}


void ViewMaster::YTranslate( Event *ev )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
	Event *originEvent   = new Event(EV_ViewThing_SetOrigin);
	
	originEvent->AddFloat(viewthing->origin.x);
	originEvent->AddFloat(viewthing->origin.y + ev->GetFloat(1));
	originEvent->AddFloat(viewthing->origin.z);
	
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	_resetOrigin();
}


void ViewMaster::ZTranslate( Event *ev )
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
	Event *originEvent   = new Event(EV_ViewThing_SetOrigin);
	
	originEvent->AddFloat(viewthing->origin.x);
	originEvent->AddFloat(viewthing->origin.y);
	originEvent->AddFloat(viewthing->origin.z + ev->GetFloat(1));
	
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	_resetOrigin();
}


void ViewMaster::_randomizeViewthing( Viewthing* viewthingPtr )
{
	if (!viewthingPtr)
	{
		gi.Printf( "No viewthing to randomize\n");
		return ;
	}
	
	if (g_vt_viewsrandomize->integer)
	{
		float die1        = G_Random( 100.0f );
		float die2        = G_Random( 100.0f );
		float scaleValue  = (die1 + die2) / 100.0f ;
		
		float midpoint    = ( (float)g_vt_viewmaxrandomize->integer - (float)g_vt_viewminrandomize->integer) / 2.0f ;
		float adjustment  = (float)g_vt_viewminrandomize->integer + (midpoint * scaleValue);
		float scaleFactor = (adjustment / 100.0f);
		
		Event* scaleEvent = new Event(EV_ViewThing_SetScale);
		scaleEvent->AddFloat(scaleFactor);
		viewthingPtr->SetScaleEvent(scaleEvent);
		delete scaleEvent ;
		scaleEvent = 0 ;
	}
	
	if (g_vt_viewzrandomize->integer)
	{
		Event* rotateEvent = new Event(EV_ViewThing_SetYaw);
		rotateEvent->AddFloat(G_Random(360.0f));
		viewthingPtr->SetYawEvent(rotateEvent);
		delete rotateEvent ;
		rotateEvent = 0 ;
	}
	
	if (g_vt_viewyrandomize->integer)
	{
		Event* rotateEvent = new Event(EV_ViewThing_SetPitch);
		rotateEvent->AddFloat(G_Random(360.0f));
		viewthingPtr->SetPitchEvent(rotateEvent);
		delete rotateEvent ;
		rotateEvent = 0 ;
	}
	
	if (g_vt_viewxrandomize->integer)
	{
		Event* rotateEvent = new Event(EV_ViewThing_SetRoll);
		rotateEvent->AddFloat(G_Random(360.0f));
		viewthingPtr->SetRollEvent(rotateEvent);
		delete rotateEvent ;
		rotateEvent = 0 ;
	}
}


void ViewMaster::_resetOrigin()
{
	if (!current_viewthing) return ;
	
	Viewthing *viewthing = ( Viewthing*) ( (Entity*) current_viewthing);
	_lastBaseOrigin = viewthing->origin ;
	_setToPosition(_lastBaseOrigin); 
}


void ViewMaster::_selectNext()
{
	Viewthing *viewthing;
	Entity * ent;
	
	ent = G_FindClass( current_viewthing, "viewthing" );
	if ( ent )
	{
		if (current_viewthing) 
		{
			viewthing = ( Viewthing * )( ( Entity * )current_viewthing );				
			viewthing->SetSelected( false );
		}
		
		current_viewthing = ent;
		viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
		_lastBaseOrigin = viewthing->origin ;
		viewthing->UpdateCvars();
		viewthing->SetSelected( true );
		gi.Printf( "current viewthing model %s.\n", viewthing->model.c_str() );
		_updateViewthingCounts();
	}
}


void ViewMaster::_selectPrevious()
{
	Viewthing   *viewthing;
	Entity      *prev;
	Entity      *next;
	
	next = NULL;
	do
	{
		prev = next;
		next = G_FindClass( prev, "viewthing" );
	}
	while( next != current_viewthing );
	
	if ( prev )
	{
		if (current_viewthing) 
		{
			viewthing = ( Viewthing * )( ( Entity * )current_viewthing );				
			viewthing->SetSelected( false );
		}
		
		current_viewthing = prev;
		viewthing = ( Viewthing * )( ( Entity * )current_viewthing );
		_lastBaseOrigin = viewthing->origin ;
		viewthing->UpdateCvars();
		viewthing->SetSelected(true);
		gi.Printf( "current viewthing model %s.\n", viewthing->model.c_str() );
		_updateViewthingCounts();
	}
}


void ViewMaster::_setToPosition( const Vector& pos )
{
	Event* originEvent = new Event(EV_ViewThing_SetOrigin);
	
	originEvent->AddFloat(pos.x);
	originEvent->AddFloat(pos.y);
	originEvent->AddFloat(pos.z);
	
	Viewthing *viewthing = (Viewthing*)( (Entity*)current_viewthing );
	viewthing->ChangeOrigin(originEvent);
	delete originEvent ;
	originEvent = 0 ;
	
	gi.cvar_set( "g_vt_xtrans", "0" );
	gi.cvar_set( "g_vt_ytrans", "0" );
	gi.cvar_set( "g_vt_ztrans", "0" );
}


void ViewMaster::_updateViewthingCounts(int countAdjustment)
{
	Entity* next = G_FindClass( NULL, "viewthing");
	int     numberOfViewthings    = 0 ;
	int     currentViewthingIndex = 0 ;
	
	while (next)
	{
		numberOfViewthings++ ;
		if (next == current_viewthing)
		{
			currentViewthingIndex = numberOfViewthings ;
		}
		next = G_FindClass( next, "viewthing");
	}
	
	gi.cvar_set( "g_vt_viewnumber", va("%d", currentViewthingIndex) );
	gi.cvar_set( "g_vt_viewcount",  va("%d", numberOfViewthings + countAdjustment) );
}


void ViewMaster::spawnAtPosition( const str& modelName, const Vector& pos )
{
	Viewthing *viewthing  = new Viewthing;
	current_viewthing     = viewthing;
	viewthing->baseorigin = pos ;
	viewthing->setOrigin( viewthing->baseorigin );
	_lastBaseOrigin = viewthing->baseorigin ;
	
	if (g_vt_droptoggle->integer)
	{
		viewthing->droptofloor( 10000.0f );
	}
	
	viewthing->setAngles( vec_zero );
	
	Event* event = new Event( EV_ViewThing_SetModel );
	event->AddString( modelName );
	viewthing->ProcessEvent( event );
	
	if ( !gi.IsModel( viewthing->edict->s.modelindex ) )
	{
		delete viewthing;
		current_viewthing = 0 ;
		return;
	}
	_updateViewthingCounts();
}






ViewMaster::~ViewMaster()
{
	_setNamesArray.FreeObjectList();
	_modelNamesArray.FreeObjectList();
}




void Viewthing::SetAnim( int num )
{
	int numanims;
	
	numanims = animate->NumAnims();
	if ( numanims )
	{
		memcpy( edict->s.surfaces, origSurfaces, sizeof( origSurfaces ) );
		
		animate->NewAnim( num );
		animate->SetFrame( frame );
		UpdateCvars();
	}
}


