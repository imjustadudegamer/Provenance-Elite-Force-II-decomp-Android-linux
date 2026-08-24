/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cl_ui.h"
#include "uilib/uimenu.h"

#include "../server/server.h"

#define PROBE_GET(name, def, flags) Cvar_Get(name, def, flags)
#define PROBE_PRINT                 Com_Printf
#include "../qcommon/qa_probe.h"

#include "../qcommon/qa_ui.h"

CLASS_DECLARATION(UIWidget, View3D, NULL) {
    {&W_Activated,     &View3D::OnActivate  },
    {&W_Deactivated,   &View3D::OnDeactivate},
    {&W_LeftMouseDown, &View3D::Pressed     },
    {NULL,             NULL                 }
};

View3D::View3D()
{
    setBackgroundColor(UClear, WIDGET_STATE_NORMAL, true);
    setBorderStyle(border_none);
    AllowActivate(true);

    m_printfadetime = 0.0;
    m_print_mat     = NULL;
    m_locationprint = qfalse;
}

void View3D::UpdateCenterPrint(const char *s, float inAlpha)
{
    char converted[256];

    uii.SR_ConvertStringResourceIds(s, converted, sizeof(converted));

    m_printstring = converted;

    if (s[0] == '@') {
        m_print_mat = uWinMan.RegisterShader(s + 1);
    } else {
        m_print_mat = NULL;
    }

    m_printalpha    = inAlpha;
    m_printfadetime = 4000.0;
    m_locationprint = qfalse;
}

void View3D::UpdateLocationPrint(int x, int y, const char *s, float inAlpha)
{
    char converted[256];

    uii.SR_ConvertStringResourceIds(s, converted, sizeof(converted));

    m_printstring   = converted;
    m_printalpha    = inAlpha;
    m_printfadetime = 4000.0;
    m_x_coord       = x;
    m_y_coord       = y;
    m_locationprint = qtrue;
}

void View3D::FrameInitialized(void)
{
    Connect(this, W_Activated, W_Activated);
    Connect(this, W_Deactivated, W_Deactivated);
}

void View3D::Pressed(Event *ev)
{
    IN_MouseOff();
    OnActivate(ev);
}

void View3D::OnActivate(Event *ev)
{
    Menu *menu = menuManager.CurrentMenu();

    if (menu && !menu->allowGameFocus()) {
        static int view3d_skip;
        if (PROBE_Throttle(&view3d_skip, 2, 200)) {
            PROBE(
                "activate:skip",
                "menu='%s' allowGameFocus=%d firstWidget='%s' catcher=0x%x",
                menu->m_name.c_str(),
                menu->allowGameFocus() ? 1 : 0,
                menu->GetContainerWidget() ? menu->GetContainerWidget()->getName() : "(none)",
                Key_GetCatcher());
        }
        uWinMan.ActivateControl(NULL);
        return;
    }

    Key_SetCatcher(Key_GetCatcher() & ~KEYCATCH_UI);
    uWinMan.sortWidgetContainers();
}

void View3D::OnDeactivate(Event *ev)
{
    Key_SetCatcher(Key_GetCatcher() | KEYCATCH_UI);
}

void View3D::DrawFPS(void)
{
    char  string[128];
    float lineHeight;

    setFont("Tahoma-10");

    Com_sprintf(string, sizeof(string), "FPS %4.1f", currentfps);
    if (currentfps > 23.94) {
        m_font->setColor(UWhite);
    } else if (currentfps > 18.0) {
        m_font->setColor(UYellow);
    } else {
        m_font->setColor(URed);
    }

    // port-only -- retail's DrawFPS calls exactly sprintf, setColor, getHeight and
    lineHeight = (float)m_font->getHeight(false);

    m_font->Print(lineHeight, 15.0f, string, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

    m_font->setColor(UWhite);

    if (fps->integer >= 2) {
        Com_sprintf(string, sizeof(string), "wt%5d wv%5d cl%d", cls.world_tris, cls.world_verts, cls.character_lights);

        m_font->Print(lineHeight, 15.0f + lineHeight, string, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

        Com_sprintf(
            string,
            sizeof(string),
            "t%5d v%5d Mtex%5.2f",
            cls.total_tris,
            cls.total_verts,
            (float)cls.total_texels * 0.00000095367432
        );

        m_font->Print(lineHeight, 15.0f + lineHeight * 2.0f, string, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
    }

    m_font->setColor(UBlack);
}

// View3D::Draw2D @0x001C0360 never calls it, and the body here was
// retail does not have.  Draw() therefore now ends at Draw2D(), which is exactly

void View3D::PrintSound(int channel, const char *name, float vol, int rvol, float pitch, float base, int& line)
{
    char  buf[255];
    float x;
    float xStep;
    float height;

    height = m_font->getHeight(true);
    xStep  = height;

    x = 0;
    Com_sprintf(buf, sizeof(buf), "%d", channel);
    m_font->Print(x, height * line + m_frame.pos.y, buf, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    x += xStep + xStep;
    Com_sprintf(buf, sizeof(buf), "%s", name);
    m_font->Print(x, height * line + m_frame.pos.y, buf, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    x += xStep * 30.0;
    Com_sprintf(buf, sizeof(buf), "vol:%.2f", vol);
    m_font->Print(x, height * line + m_frame.pos.y, buf, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    x += xStep * 8;
    Com_sprintf(buf, sizeof(buf), "rvol:%.2f", (float)(rvol / 128.f));
    m_font->Print(x, height * line + m_frame.pos.y, buf, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    x += xStep * 5;
    Com_sprintf(buf, sizeof(buf), "pit:%.2f", pitch);
    m_font->Print(x, height * line + m_frame.pos.y, buf, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    x += xStep * 5;
    Com_sprintf(buf, sizeof(buf), "base:%d", (int)base);
    m_font->Print(x, height * line + m_frame.pos.y, buf, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), true);

    line++;
}

void View3D::DrawSoundOverlay(void)
{
    setFont("Tahoma-10");
    m_font->setColor(UWhite);
}

void View3D::Draw2D(void)
{
    if (!cls.no_menus) {
        DrawFades();
    }

    DrawLetterbox();

    if (fps->integer && !cls.no_menus) {
        DrawFPS();
    }

    if ((cl_debuggraph->integer || cl_timegraph->integer) && !cls.no_menus) {
        SCR_DrawDebugGraph();
    }

    if (!cls.no_menus) {
        if (cge) {
            UI_ZONE(UI_FAM_V3D, UI_VZ_cgdraw, cge->CG_Draw2D());
        }

        if (m_locationprint) {
            LocationPrint();
        } else {
            CenterPrint();
        }

        PrintStringList();
    }
}

void View3D::AddPrintString(const char *text, float *pos, int width, float *color, const char *fontname)
{
    string_info_t *si = new string_info_t;

    si->x        = pos[0];
    si->y        = pos[1];
    si->width    = width;
    si->color[0] = color[0];
    si->color[1] = color[1];
    si->color[2] = color[2];
    si->color[3] = color[3];
    strncpy(si->fontname, fontname, sizeof(si->fontname));
    strncpy(si->text, text, sizeof(si->text));

    m_printstringlist.AddObject(si);
}

void View3D::PrintStringList(void)
{
    int            i, j;
    string_info_t *si;
    UIRect2D       frame;
    UIRect2D       rect;
    float          xi, yi;
    UIFont         font;

    for (i = 1; i <= m_printstringlist.NumObjects(); i++) {
        si = m_printstringlist.ObjectAt(i);
        if (!si) {
            continue;
        }

        font.setFont(si->fontname);

        xi = (si->x == -1.0f) ? 0.0f : (float)(int)(si->x * uid.scaleRes[0]);
        yi = (si->y == -1.0f) ? 0.0f : (float)(int)(si->y * uid.scaleRes[1]);

        font.setColor(
            UColor(si->color[0], si->color[1], si->color[2], si->color[3])
        );

        frame = getClientFrame();

        rect.pos.x     = xi;
        rect.pos.y     = frame.pos.y;
        rect.size.width  = (float)si->width;
        rect.size.height = (float)(int)(yi - frame.pos.y);

        font.PrintJustified(
            rect,
            FONT_JUSTHORZ_LEFT,
            (si->y == -1.0f) ? FONT_JUSTVERT_CENTER : FONT_JUSTVERT_BOTTOM,
            si->text,
            UBlack,
            false
        );
    }

    for (j = 1; j < m_printstringlist.NumObjects(); j++) {
        si = m_printstringlist.ObjectAt(j);
        if (si) {
            delete si;
        }
    }

    m_printstringlist.FreeObjectList();
}

void View3D::CenterPrint(void)
{
    float       fAlpha;
    const char *p;
    qhandle_t   mat;
    float       x, y;
    float       w, h;

    if (!m_printfadetime) {
        return;
    }

    p = m_printstring;
    if (m_printfadetime > 3250) {
        fAlpha = 1.f - (m_printfadetime - 3250.f) / 750.f * m_printalpha;
    } else if (m_printfadetime >= 750) {
        fAlpha = 1.f;
    } else {
        fAlpha = m_printfadetime / 750.f * m_printalpha;
    }

    fAlpha = Q_clamp_float(fAlpha, 0, 1);

    if (!m_print_mat) {
        UIRect2D frame;
        m_font->setColor(UColor(0, fAlpha, fAlpha, fAlpha));

        frame = getClientFrame();

        m_font->PrintJustified(
            UIRect2D(frame.pos.x + 1, frame.pos.y + 1, frame.size.width, frame.size.height),
            m_iFontAlignmentHorizontal,
            m_iFontAlignmentVertical,
            p, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

        m_font->setColor(UColor(1, fAlpha, fAlpha, fAlpha));

        frame = getClientFrame();

        m_font->PrintJustified(frame, m_iFontAlignmentHorizontal, m_iFontAlignmentVertical, p, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

        m_font->setColor(UBlack);
    } else if ((mat = m_print_mat->GetMaterial())) {
        vec4_t col {fAlpha, fAlpha, fAlpha, fAlpha};

        re.SetColor(col);

        w = re.GetShaderWidth(mat);
        h = re.GetShaderHeight(mat);
        x = (m_frame.pos.x + m_frame.size.width - w) * 0.5f;
        y = (m_frame.pos.y + m_frame.size.height - h) * 0.5f;

        re.DrawStretchPic(x, y, w, h, 0, 0, 1, 1, mat);
    }

    m_printfadetime -= cls.frametime;

    if (m_printfadetime < 0) {
        m_printfadetime = 0;
    }
}

void View3D::LocationPrint(void)
{
    fonthorzjustify_t horiz;
    fontvertjustify_t vert;
    int               x, y;
    const char       *p;
    float             fAlpha;
    UIRect2D          frame;

    if (!m_printfadetime) {
        m_locationprint = false;
        return;
    }

    horiz = FONT_JUSTHORZ_LEFT;
    vert  = FONT_JUSTVERT_TOP;

    p = m_printstring;
    if (m_printfadetime > 3250) {
        fAlpha = 1.f - (m_printfadetime - 3250.f) / 750.f * m_printalpha;
    } else if (m_printfadetime >= 750) {
        fAlpha = 1.f;
    } else {
        fAlpha = m_printfadetime / 750.f * m_printalpha;
    }

    fAlpha = Q_clamp_float(fAlpha, 0, 1);

    x = m_x_coord / 640.f * m_screenframe.size.width;
    y = (480 - m_font->getHeight(false) - m_y_coord) / 480.f * m_screenframe.size.height;

    if (m_x_coord == -1) {
        horiz = FONT_JUSTHORZ_CENTER;
        x     = 0;
    }
    if (m_y_coord == -1) {
        vert = FONT_JUSTVERT_CENTER;
        y    = 0;
    }

    m_font->setColor(UColor(0, 0, 0, fAlpha));
    frame = getClientFrame();

    m_font->PrintJustified(
        UIRect2D(frame.pos.x + x + 1, frame.pos.y + y + 1, frame.size.width, frame.size.height),
        horiz,
        vert,
        p, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

    m_font->setColor(UColor(1, 1, 1, fAlpha));
    frame = getClientFrame();

    m_font->PrintJustified(
        UIRect2D(frame.pos.x + x, frame.pos.y + y, frame.size.width, frame.size.height),
        horiz,
        vert,
        p, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

    m_font->setColor(UBlack);
    m_printfadetime -= cls.frametime;

    if (m_printfadetime < 0) {
        m_printfadetime = 0;
    }
}

void View3D::DrawLetterbox(void)
{
    float  frac;
    vec4_t col;

    col[0] = col[1] = col[2] = 0;
    col[3]                   = 1;

    frac = (float)cl.snap.ps.stats[STAT_LETTERBOX] / MAX_LETTERBOX_SIZE;
    if (frac <= 0) {
        m_letterbox_active = false;
        return;
    }

    m_letterbox_active = true;
    re.SetColor(col);

    re.DrawBox(0.0, 0.0, m_screenframe.size.width, m_screenframe.size.height * frac);
    re.DrawBox(
        0.0,
        m_screenframe.size.height - m_screenframe.size.height * frac,
        m_screenframe.size.width,
        m_screenframe.size.height
    );
}

void View3D::DrawFades(void)
{
    if (cl.snap.ps.blend[3] > 0) {
        re.SetColor(cl.snap.ps.blend);
        if (cl.snap.ps.stats[STAT_ADDFADE]) {
            re.AddBox(0.0, 0.0, m_screenframe.size.width, m_screenframe.size.height);
        } else {
            re.DrawBox(0.0, 0.0, m_screenframe.size.width, m_screenframe.size.height);
        }
    }
}

void View3D::update(void)
{
    if (cge) {
        cge->CG_UpdateClient(cl.serverTime);
    }
}

void View3D::Draw(void)
{
    UI_ROOT_BEGIN(UI_FAM_V3D);

    UI_ZONE_BEGIN(UI_VZ_scrdraw);
    if (cls.state != CA_DISCONNECTED) {
        SCR_DrawScreenField();
    }
    UI_ZONE_END(UI_FAM_V3D, UI_VZ_scrdraw);

    UI_ZONE_BEGIN(UI_VZ_set2d);
    set2D();
    UI_ZONE_END(UI_FAM_V3D, UI_VZ_set2d);

    re.SavePerformanceCounters();

    UI_ZONE_BEGIN(UI_VZ_draw2d);
    Draw2D();
    UI_ZONE_END(UI_FAM_V3D, UI_VZ_draw2d);

    UI_ROOT_END(UI_FAM_V3D);
}


void View3D::ClearCenterPrint(void)
{
    m_printfadetime = 0.0;
}

qboolean View3D::LetterboxActive(void)
{
    return m_letterbox_active;
}

CLASS_DECLARATION(UIWidget, ConsoleView, NULL) {
    {NULL, NULL}
};

void ConsoleView::Draw(void) {}
