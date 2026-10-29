/*
===========================================================================
Copyright (C) 2015-2024 the OpenMoHAA team

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

#include "ui_local.h"

#define PROBE_GET(probe_n, probe_d, probe_f) ((cvar_t *)0)
#define PROBE_PRINT                       Com_Printf
#define PROBE_ON_OVERRIDE                       (atoi(CvarGetForUI("qa_probe", "0")) != 0)  
#include "../qcommon/qa_probe.h"
#define SMP_PROBE(tag, ...) PROBE_EMIT("[SMP:", tag, __VA_ARGS__)

#include "../qcommon/qa_ui.h"

qboolean UI_FontDBCSIsLeadByte(fontheader_t *font, unsigned short uch);
int      UI_FontCodeSearch(const fontheader_t *font, unsigned short uch);

UIFont::UIFont()
{
    color = UBlack;

    m_textScale    = 1.0f;
    m_textScaleSet = false;

    loadDefaultFont();

    setColor(color);
}

UIFont::UIFont(const char *fn)
{
    color = UBlack;

    m_textScale    = 1.0f;
    m_textScaleSet = false;

    m_font = uii.Rend_LoadFont(fn);
    if (m_font) {
        name = fn;
    } else {
        loadDefaultFont();
    }
}

void UIFont::setTextScale(float scale)
{
    m_textScale    = scale;
    m_textScaleSet = true;
}

float UIFont::getTextScale(void) const
{
    if (m_textScaleSet) {
        return m_textScale;
    }

    return UIWidget::HudTextScale();
}

void UIFont::setColor(UColor col)
{
    color = col;
}

void UIFont::setAlpha(float alpha)
{
    color.a = alpha;
}

//   0xA9B84  stw r0, 0(r28)                   -> +0  (never written by either ctor)
void UIFont::copyFrom(const UIFont *src)
{
    color  = src->color;
    m_font = src->m_font;
    name   = src->name;

    m_textScale    = src->m_textScale;
    m_textScaleSet = src->m_textScaleSet;
}

void UIFont::loadDefaultFont(void)
{
    m_font = uii.Rend_LoadFont("tahoma-10");
    if (m_font) {
        name = "tahoma-10";
    } else {
        uii.Sys_Error(ERR_DROP, "Couldn't load default font %s\n", "tahoma-10");
    }
}

void UIFont::setFont(const char *fontname)
{
    m_font = uii.Rend_LoadFont(fontname);
    if (!m_font) {
        uii.Sys_Error(ERR_DROP, "Couldn't load font %s\n", fontname);
    }
}

// In retail the flag scales the GLYPHS, not the string's origin.  Retail
void UIFont::Print(float x, float y, const char *text, int maxlen, UColor shadowColor, int virtualScreen)
{
    const float *scale = NULL;
    float        vx    = x;
    float        vy    = y;
    float        textScaleBuf[2];

    /* The factor reaches `Rend_DrawString' as the virtual screen's division does: it scales the
       glyphs and the origin is divided by it, because the renderer multiplies the quad's
       coordinates by the same pair. Retail's flag does the same -- R_DrawString @0x0000D250 scales
       the glyphs and UIFont::Print @0x0005FB70 passes x and y through. Without the division a
       scaled widget's text lands at twice its own line and leaves the box. */
    if (virtualScreen) {
        scale = uid.scaleRes;
    } else {
        float textScale = getTextScale();

        if (textScale != 1.0f) {
            textScaleBuf[0] = textScale;
            textScaleBuf[1] = textScale;
            scale           = textScaleBuf;
        }
    }

    if (scale && scale[0] != 0.0f && scale[1] != 0.0f) {
        vx = x / scale[0];
        vy = y / scale[1];
    }

    if (shadowColor.a != 0.0f) {
        uii.Rend_SetColor(shadowColor);
        uii.Rend_DrawString(m_font, text, (int)vx - 1, (int)vy + 1, maxlen, scale);
    }

    uii.Rend_SetColor(color);
    uii.Rend_DrawString(m_font, text, (int)vx, (int)vy, maxlen, scale);
}

static void UIFont_WrapLines(UIFont *font, const char *text, float maxWidth, bool virtualScale, Container<str>& lines)
{
    str         line;
    float       penX        = 0.0f;
    const char *p           = text;
    bool        atLineStart = true; 

    /* A text with no newline, no leading blank and a width inside the region is
       one line equal to itself; the token walk would only rebuild it. */
    if (text[0] && text[0] != ' ' && text[0] != '\t' && !strchr(text, '\n')
        && font->getFloatWidth(text, -1, virtualScale) <= maxWidth) {
        lines.AddObject(str(text));
        return;
    }

    while (*p) {
        if (*p == '\n') {
            lines.AddObject(line);
            line        = "";
            penX        = 0.0f;
            atLineStart = true;
            p++;
            continue;
        }

        const char *start   = p;
        bool        isSpace = (*p == ' ' || *p == '\t');
        while (*p && *p != '\n') {
            bool cs = (*p == ' ' || *p == '\t');
            if (cs != isSpace) {
                break;
            }
            p++;
        }

        if (isSpace && atLineStart) {
            continue;
        }

        char tok[1024];
        int  n = (int)(p - start);
        if (n >= (int)sizeof(tok)) {
            n = (int)sizeof(tok) - 1;
        }
        memcpy(tok, start, n);
        tok[n] = '\0';

        float tw = font->getFloatWidth(tok, -1, virtualScale);

        if (!isSpace && line.length() && (penX + tw) > maxWidth) {
            lines.AddObject(line);
            line        = "";
            penX        = 0.0f;
            atLineStart = true;
            if (isSpace) {
                continue; 
            }
        }

        line += tok;
        penX += tw;
        atLineStart = false;
    }

    if (line.length()) {
        lines.AddObject(line);
    }
}

void UIFont::PrintJustified(
    const UIRect2D& rect, fonthorzjustify_t horz, fontvertjustify_t vert, const char *text, UColor shadowColor, int
    virtualScreen
)
{
    float    newx, newy;
    float    textheight;
    float    wrapwidth;
    UIRect2D sizedRect;

    UI_COUNT(UI_FAM_UI, UI_UC_text, 1);

    // flag scales the metrics, not the rect.  Retail @0x0005FE20 reads the rect
    // off_228198 load at 0x5FE5C feeds one fmuls at 0x5FFE0, never a division).
    sizedRect = rect;

    /* the left/right inset the justification applies to the drawn edge is part of the
       band, so the text is wrapped to the band it really draws in, not to the frame */
    const float hbias    = 2.0f * (virtualScreen ? uid.scaleRes[0] : 1.0f);
    const bool  insetted = (horz != FONT_JUSTHORZ_CENTER);

    wrapwidth = sizedRect.size.width - (insetted ? hbias : 0.0f);
    if (wrapwidth < 0.0f) {
        wrapwidth = 0.0f;
    }

    Container<str> lines;
    UIFont_WrapLines(this, text, wrapwidth, virtualScreen != 0, lines);

    /* the band is justified against the lines it will actually hold: centring on a
       one-line height put the first line where it belonged and sent every wrapped
       line out under the box, which is where the clipped row fragments came from */
    textheight = (float)lines.NumObjects() * (float)getHeight(virtualScreen != 0);

    switch (vert) {
    case FONT_JUSTVERT_TOP:
        newy = sizedRect.pos.y;
        break;
    case FONT_JUSTVERT_CENTER:
        newy = sizedRect.pos.y + (sizedRect.size.height - textheight) * 0.5;
        break;
    case FONT_JUSTVERT_BOTTOM:
        newy = sizedRect.pos.y + sizedRect.size.height - textheight;
        break;
    default:
        newy = 0;
        break;
    }

    for (int li = 1; li <= lines.NumObjects(); li++) {
        const char *string  = lines.ObjectAt(li).c_str();
        float       lineLen = (float)(int)getFloatWidth(string, -1, virtualScreen != 0);

        switch (horz) {
        case FONT_JUSTHORZ_CENTER:
            newx = sizedRect.pos.x + (sizedRect.size.width - lineLen) * 0.5f;
            break;
        case FONT_JUSTHORZ_LEFT:
            newx = (float)(int)sizedRect.pos.x + hbias;
            break;
        case FONT_JUSTHORZ_RIGHT:
            newx = (float)(int)(sizedRect.pos.x + sizedRect.size.width - lineLen) - hbias;
            break;
        default:
            newx = 0.0;
            break;
        }

        Print(newx, newy, string, -1, shadowColor, virtualScreen);

        if (li == 1) {
            static int smp_textpos;

            if (PROBE_On() && PROBE_Throttle(&smp_textpos, 3, 200)) {
                SMP_PROBE(
                    "uitextpos",
                    "horz=%d vert=%d rect=(%.1f,%.1f %.1fx%.1f) lines=%d line=1 newx=%.1f newy=%.1f "
                    "bias=%.2f virtual=%d text='%.32s'\n",
                    (int)horz,
                    (int)vert,
                    (double)sizedRect.pos.x,
                    (double)sizedRect.pos.y,
                    (double)sizedRect.size.width,
                    (double)sizedRect.size.height,
                    lines.NumObjects(),
                    (double)newx,
                    (double)newy,
                    (double)hbias,
                    virtualScreen ? 1 : 0,
                    string
                );
            }
        }

        newy += (float)getHeight(string, -1, virtualScreen != 0);
    }
}

/* Retail's layout at a larger glyph size, for the notification family: the metrics scale with the
   glyphs so the justification stays on the same centre.  The widget path does not need this -- its
   frames are already scaled, so its left-justified rows land right with the raw metrics and a
   divided origin, which is what Print does.  Same wrap width, so the same words land on the same
   lines as retail's. */
void UIFont::PrintJustifiedScaled(
    const UIRect2D&   rect,
    fonthorzjustify_t horz,
    fontvertjustify_t vert,
    const char       *text,
    UColor            shadowColor,
    float             scale
)
{
    float    newx, newy;
    float    textheight;
    UIRect2D sizedRect;
    float    vs[2];

    if (scale <= 0.0f) {
        return;
    }

    sizedRect   = rect;
    textheight  = (float)getHeight(text, -1, false) * scale;

    switch (vert) {
    case FONT_JUSTVERT_TOP:
        newy = sizedRect.pos.y;
        break;
    case FONT_JUSTVERT_CENTER:
        newy = sizedRect.pos.y + (sizedRect.size.height - textheight) * 0.5;
        break;
    case FONT_JUSTVERT_BOTTOM:
        newy = sizedRect.pos.y + sizedRect.size.height - textheight;
        break;
    default:
        newy = 0;
        break;
    }

    Container<str> lines;
    UIFont_WrapLines(this, text, sizedRect.size.width, false, lines);

    vs[0] = scale;
    vs[1] = scale;

    for (int li = 1; li <= lines.NumObjects(); li++) {
        const char *string  = lines.ObjectAt(li).c_str();
        float       lineLen = (float)(int)getFloatWidth(string, -1, false) * scale;

        switch (horz) {
        case FONT_JUSTHORZ_CENTER:
            newx = sizedRect.pos.x + (sizedRect.size.width - lineLen) * 0.5f;
            break;
        case FONT_JUSTHORZ_LEFT:
            newx = (float)(int)sizedRect.pos.x + 2.0f;
            break;
        case FONT_JUSTHORZ_RIGHT:
            newx = (float)(int)(sizedRect.pos.x + sizedRect.size.width - lineLen) - 2.0f;
            break;
        default:
            newx = 0.0f;
            break;
        }

        if (shadowColor.a != 0.0f) {
            uii.Rend_SetColor(shadowColor);
            uii.Rend_DrawString(m_font, string, (int)(newx / scale) - 1, (int)(newy / scale) + 1, -1, vs);
        }

        uii.Rend_SetColor(color);
        uii.Rend_DrawString(m_font, string, (int)(newx / scale), (int)(newy / scale), -1, vs);

        newy += (float)getHeight(string, -1, false) * scale;
    }
}

float UIFont::getFloatWidth(const char *text, int maxlen, bool virtualScale)
{
    float width    = 0.0f;
    float maxwidth = 0.0f;
    float scale;

    if (!m_font) {
        return 0.0f;
    }

    if (virtualScale) {
        scale = uid.scaleRes[0];
    } else {
        /* The owning widget's factor, resolved the same way wherever the metric
           is asked from, so a wrap and the frame it wraps into share a unit. */
        scale = getTextScale();
    }

    for (int i = 0;; i++) {
        char c = text[i];

        if (!c || (maxlen != -1 && i >= maxlen)) {
            break;
        }

        if (c == '^' && (signed char)text[i + 1] >= 48 && (signed char)text[i + 1] <= 57) {
            i++;
            continue;
        } else if (c == '\t') {
            int indirected = m_font->sgl[0]->indirection[( unsigned char )'\t'];

            if (indirected < 0x100) {
                width += 3.0f * m_font->sgl[0]->locations[indirected].size[0] * scale;
                if (width > maxwidth) {
                    maxwidth = width;
                }
            }
        } else if (c == '\n') {
            width = 0.0f;
        } else {
            int indirected = m_font->sgl[0]->indirection[(unsigned char)c];

            if (indirected < 0x100) {
                width += m_font->sgl[0]->locations[indirected].size[0] * scale;
                if (width > maxwidth) {
                    maxwidth = width;
                }
            }
        }
    }

    return maxwidth * 256.0f;
}

int UIFont::getWidth(const char *text, int maxlen, bool virtualScale)
{
    return (int)getFloatWidth(text, maxlen, virtualScale);
}

int UIFont::getCharWidth(char ch, bool virtualScale)
{
    int   indirected;
    float width;

    if (!m_font) {
        return 4;
    }

    if (ch == '\t') {
        indirected = m_font->sgl[0]->indirection[( unsigned char )'\t'];
        if (indirected < 0x100) {
            return (int)(3.0f * 256.0f * m_font->sgl[0]->locations[indirected].size[0]);
        }
        return 0;
    }

    indirected = m_font->sgl[0]->indirection[(unsigned char)ch];
    if (indirected < 0x100) {
        width = 256.0f * m_font->sgl[0]->locations[indirected].size[0];
        if (virtualScale) {
            width *= uid.scaleRes[0];
        } else {
            width *= getTextScale();
        }
        return (int)width;
    }

    return 0;
}

int UIFont::getHeight(const char *text, int maxlen, bool virtualScale)
{
    float height;
    int   i;

    if (!m_font) {
        return 0;
    }

    height = (float)getHeight(virtualScale);

    for (i = 0; text[i] && (maxlen == -1 || i < maxlen); i++) {
        if (text[i] == '\n') {
            height += (float)getHeight(virtualScale);
        }
    }

    return (int)height;
}

int UIFont::getHeight(bool virtualScale)
{
    if (!m_font) {
        return 16;
    }

    if (virtualScale) {
        return (int)(m_font->sgl[0]->height * uid.scaleRes[1]);
    }

    return (int)(m_font->sgl[0]->height * getTextScale());
}

qboolean UI_FontDBCSIsLeadByte(fontheader_t *font, unsigned short uch)
{
    switch (font->codePage) {
    case 932:
        return ((uch >= 0x81) && (uch <= 0x9F)) || ((uch >= 0xE0) && (uch <= 0xFC));
    case 936:
        return (uch >= 0x81) && (uch <= 0xFE);
    case 949:
        return (uch >= 0x81) && (uch <= 0xFE);
    case 950:
        return (uch >= 0x81) && (uch <= 0xFE);
    case 1361:
        return ((uch >= 0x84) && (uch <= 0xD3)) || ((uch >= 0xD8) && (uch <= 0xDE)) || ((uch >= 0xE0) && (uch <= 0xF9));
    }
    return false;
}

int UI_FontCodeSearch(const fontheader_t *font, unsigned short uch)
{
    int mid;
    int l, r;

    r = font->charTableLength;
    l = 0;
    while (l < r) {
        mid = (l + r) / 2;

        if (font->charTable[mid].cp > uch) {
            r = (l + r) / 2;
            continue;
        }

        if (uch == font->charTable[mid].cp) {
            return (l + r) / 2;
        }

        l = mid + 1;
    }

    if (uch != font->charTable[l].cp) {
        return -1;
    }

    return l;
}

float UI_FontgetCharWidthf(fontheader_t *font, unsigned short uch)
{
    int   index;
    int   indirected;
    float widthMul = 1.0f; 

    if (!font) {
        return 0.f;
    }

    if (uch == '\t') {
        uch      = ' ';
        widthMul = 3.0f;
    }

    if (font->numPages) {
        int code = UI_FontCodeSearch(font, uch);
        if (code < 0) {
            return 0.f;
        }

        index      = font->charTable[code].index;
        indirected = font->charTable[code].loc;
    } else {
        indirected = font->sgl[0]->indirection[uch];
        if (indirected < 0 || indirected > 255) {
            return 0.f;
        }

        index = 0;
    }

    return font->sgl[index]->locations[indirected].size[0] * widthMul;
}

