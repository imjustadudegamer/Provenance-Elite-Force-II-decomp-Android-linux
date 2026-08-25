
#include "cl_ui.h"
#include "../corepp/tiki.h"
#include "../uilib/uiwinman.h"     
#include "../uilib/uipopupmenu.h"  

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

//     is one plain word the ctor zeroes and no retail code reads.  (Retail never

Event EV_GameInventory_Timeout(
    "_timeout",
    EV_DEFAULT,
    NULL,
    NULL,
    "Timeout of the menu from inactivity"
);

class GameItemList : public UIWidget
{
public:
    CLASS_PROTOTYPE(GameItemList);

    GameItemList();
    ~GameItemList();

    void Draw(void) override;

    void OnLeftMouseDown(Event *ev);
    void OnRightMouseDown(Event *ev);
    void OnLeftMouseUp(Event *ev);
    void OnRightMouseUp(Event *ev);
    void OnMouseMove(Event *ev);
    void OnMouseEnter(Event *ev);
    void OnMouseLeave(Event *ev);
    void OnMenuKilled(Event *ev);

private:
    int              m_unk1728;     
    uipopup_describe m_items[10];   
    int              m_unk2416;     
    int              m_unk2420;     
    int              m_unk2424;     
};

GameItemList::GameItemList()
{
    m_unk1728  = 0;
    m_unk2416  = 0;
    m_unk2420  = 0;
    m_unk2424  = 0;

    setBackgroundColor(UColor(0.0f, 0.0f, 0.0f, 0.0f), WIDGET_STATE_NORMAL, true);
}

GameItemList::~GameItemList()
{
}

void GameItemList::Draw(void)
{
}

void GameItemList::OnLeftMouseDown(Event *ev)
{
}

void GameItemList::OnRightMouseDown(Event *ev)
{
}

void GameItemList::OnLeftMouseUp(Event *ev)
{
}

void GameItemList::OnRightMouseUp(Event *ev)
{
}

void GameItemList::OnMouseMove(Event *ev)
{
}

void GameItemList::OnMouseEnter(Event *ev)
{
}

void GameItemList::OnMouseLeave(Event *ev)
{
}

void GameItemList::OnMenuKilled(Event *ev)
{
}

CLASS_DECLARATION(UIWidget, GameItemList, NULL) {
    {&W_LeftMouseDown, &GameItemList::OnLeftMouseDown},
    {&W_RightMouseDown, &GameItemList::OnRightMouseDown},
    {&W_LeftMouseUp, &GameItemList::OnLeftMouseUp},
    {&W_RightMouseUp, &GameItemList::OnRightMouseUp},
    {&W_MouseMoved, &GameItemList::OnMouseMove},
    {&W_MouseEntered, &GameItemList::OnMouseEnter},
    {&W_MouseExited, &GameItemList::OnMouseLeave},
    {&W_Destroyed, &GameItemList::OnMenuKilled},
    {NULL, NULL}
};

class GameInventory : public UIWidget
{
public:
    CLASS_PROTOTYPE(GameInventory);

    GameInventory();
    ~GameInventory();

    void Timeout(Event *ev);
    void OnMouseDown(Event *ev);
    void OnMouseMove(Event *ev);
    void OnMouseLeave(Event *ev);
    void OnMouseEnter(Event *ev);
    void VerifyTypeUp(inventory_type_t *type, int index);

    void Draw(void) override;
    qboolean KeyEvent(int key, unsigned int time) override;

private:
    SafePtr<GameItemList> m_itemList;   
    [[maybe_unused]] int  m_unk1748;    
};

// and registered with `__register_global_object` at 0x0017DDF4).  Retail never
SafePtr<GameInventory> gameInventory;

GameInventory::GameInventory()
    : m_unk1748(0)
{
    AllowActivate(true);

    setBackgroundColor(UColor(0.0f, 0.0f, 0.0f, 0.0f), WIDGET_STATE_NORMAL, true);
}

GameInventory::~GameInventory()
{
    if (!uWinMan.IsDead()) {
        VerifyTypeUp(NULL, 0);
    }

    if (m_itemList.Pointer()) {
        delete (GameItemList *)m_itemList.Pointer();
    }
}

void GameInventory::Timeout(Event *ev)
{
    if (gameInventory && !gameInventory->IsDying()) {
        delete (GameInventory *)gameInventory.Pointer();
        UIVK_PROBE("focus", "caller=GameInventory::Timeout State=%d", cls.state);
        UI_FocusMenuIfExists();
    }
}

void GameInventory::OnMouseDown(Event *ev)
{
}

void GameInventory::OnMouseMove(Event *ev)
{
}

void GameInventory::OnMouseLeave(Event *ev)
{
}

void GameInventory::OnMouseEnter(Event *ev)
{
}

void GameInventory::VerifyTypeUp(inventory_type_t *type, int index)
{
}

void GameInventory::Draw(void)
{
}

qboolean GameInventory::KeyEvent(int key, unsigned int time)
{
    return qtrue;
}

CLASS_DECLARATION(UIWidget, GameInventory, NULL) {
    {&W_MouseMoved, &GameInventory::OnMouseMove},
    {&W_MouseEntered, &GameInventory::OnMouseEnter},
    {&W_MouseExited, &GameInventory::OnMouseLeave},
    {&W_LeftMouseDown, &GameInventory::OnMouseDown},
    {&W_RightMouseDown, &GameInventory::OnMouseDown},
    {&EV_GameInventory_Timeout, &GameInventory::Timeout},
    {NULL, NULL}
};


// The comm-portrait prefill is NOT here: retail does it in its own caller
void CL_Draw3DModel(
    float             x,
    float             y,
    float             w,
    float             h,
    qhandle_t         model,
    vec3_t            origin,
    vec3_t            rotateoffset,
    vec3_t            offset,
    vec3_t            angle,
    const str        &anim,
    int               a11,
    refEntity_s      *a12,
    float             fov_x,
    float             fov_y
)
{
    refdef_t     inv_refdef {};
    refEntity_t  entLocal {};
    refEntity_t *ent;
    vec3_t       unprojoffset;
    dtiki_t     *tiki;
    int          i;

    if (a12) {
        ent = a12;
    } else {
        ent = &entLocal;
    }

    AnglesToAxis(angle, ent->axis);

    MatrixTransformVector(rotateoffset, ent->axis, unprojoffset);

    for (i = 0; i < 3; i++) {
        origin[i] += offset[i];
        ent->origin[i] = origin[i] + unprojoffset[i];
    }

    ent->hModel = model;

    ent->legsLive        = qtrue;
    ent->scale           = 1.0f;
    ent->fade            = 1.0f;
    ent->crossblend_lerp = -1.0f;

    tiki = re.R_Model_GetHandle(model);

    if (anim.length()) {
        int   animnum   = TIKI_Anim_NumForName(tiki, anim);
        int   numframes = TIKI_Anim_NumFrames(tiki, animnum);
        float animtime  = TIKI_Anim_Time(tiki, animnum);

        ent->anim    = animnum;
        ent->oldanim = animnum;

        if (animtime != 0.0f) {
            ent->frame = (short)((int)((float)numframes
                                       * ((float)Sys_Milliseconds() / (1000.0f * animtime)))
                                 % numframes);
        }

        {
            static int menu3dmvk_cl3d;
            if (PROBE_Throttle(&menu3dmvk_cl3d, 4, 120)) {
                UIVK_PROBE(
                    "menu3d",
                    "cl3d model=%d anim='%s' animnum=%d frames=%d animtime=%.3f frame=%d xf=%.0f yf=%.0f wf=%.0f hf=%.0f fov=%.0fx%.0f idx=%d",
                    model, anim.c_str(), animnum, numframes, (double)animtime, (int)ent->frame,
                    (double)x, (double)y, (double)w, (double)h, (double)fov_x, (double)fov_y,
                    menu3dmvk_cl3d
                );
            }
        }
    }

    ent->renderfx |= RF_FULLBRIGHT;

    if (a11) {
        ent->morph_controllers[0].index   = 0;
        ent->morph_controllers[0].percent = dm_morph_c1->value;
    }

    // RDF_NOWORLDMODEL and NOTHING ELSE (retail does not set RDF_HUD) -- and
    inv_refdef.x       = uid.originX + (int)x;
    inv_refdef.y       = uid.originY + (int)y;
    inv_refdef.width   = (int)w;
    inv_refdef.height  = (int)h;
    inv_refdef.fov_x   = fov_x;
    inv_refdef.fov_y   = fov_y;
    inv_refdef.rdflags = RDF_NOWORLDMODEL;
    inv_refdef.time    = cls.realtime;
    AxisClear(inv_refdef.viewaxis);

    // ent->tiki (+428): retail's CL_Draw3DModel never writes it, and neither does
    ent->tiki = tiki;

    ent->entityNumber = ENTITYNUM_NONE;

    // Port-only tail field (no retail offset -- see tr_types.h): skeletor's
    ent->actionWeight = 1.0f;

    {
        static int uiparityuivp3d;
        if ((uid.originX || uid.originY) && PROBE_Throttle(&uiparityuivp3d, 2, 120)) {
            Com_Printf(
                "[UIPARITY:uivp3d] cl3d model=%d refdef %d,%d %dx%d | origin %d,%d rect %dx%d surface %dx%d\n",
                model, inv_refdef.x, inv_refdef.y, inv_refdef.width, inv_refdef.height,
                uid.originX, uid.originY, uid.vidWidth, uid.vidHeight, uid.surfaceWidth, uid.surfaceHeight
            );
        }
    }

    re.ClearScene();
    re.AddRefEntityToScene(ent);
    re.RenderScene(&inv_refdef);
}

bool CL_DoIHaveThisItem(const char *name)
{
    return CL_HasInventoryItem(name) ? true : false;
}


void UI_NextInventory_f(void)
{
    PROBE("inv:cmd", "nextinv -> InventoryBar inventorynextitem");
    Cbuf_AddText("globalwidgetcommand InventoryBar inventorynextitem\n");
}

void UI_PrevInventory_f(void)
{
    PROBE("inv:cmd", "previnv -> InventoryBar inventorypreviousitem");
    Cbuf_AddText("globalwidgetcommand InventoryBar inventorypreviousitem\n");
}

void UI_WarpInventory_f(void)
{
    str sCommand;

    if (Cmd_Argc() != 2) {
        Com_WPrintf("Usage: warpinv <slot_name>");
    }

    PROBE("inv:cmd", "warpinv slot='%s'", Cmd_Argv(1));

    sCommand = "globalwidgetcommand InventoryBar inventorywarpitem ";
    sCommand += Cmd_Argv(1);
    sCommand += "\n";

    Cbuf_AddText(sCommand.c_str());
}

qboolean UI_CloseInventory(void)
{
    return qfalse;
}
