/*
===========================================================================
Copyright (C) 2015 the OpenMoHAA team

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

#pragma once

class UIRect2D;

typedef enum {
    FONT_JUSTHORZ_CENTER,
    FONT_JUSTHORZ_LEFT,
    FONT_JUSTHORZ_RIGHT
} fonthorzjustify_t;

typedef enum {
    FONT_JUSTVERT_TOP,
    FONT_JUSTVERT_CENTER,
    FONT_JUSTVERT_BOTTOM
} fontvertjustify_t;

class UIFont
{
protected:
    UColor        color;
    fontheader_t *m_font;
    str           name;
    /* The factor the widget that owns this font had its frame scaled by.  A
       font no widget has laid out follows the HUD draw scope instead. */
    float         m_textScale;
    bool          m_textScaleSet;

public:
    UIFont();
    UIFont(const char *fn);

    void  setTextScale(float scale);
    float getTextScale(void) const;

    void Print(float x, float y, const char *text, int maxlen, UColor shadowColor, int virtualScreen);
    void PrintJustified(
        const UIRect2D&   rect,
        fonthorzjustify_t horz,
        fontvertjustify_t vert,
        const char       *text,
        UColor            shadowColor,
        int               virtualScreen
    );
    void PrintJustifiedScaled(
        const UIRect2D&   rect,
        fonthorzjustify_t horz,
        fontvertjustify_t vert,
        const char       *text,
        UColor            shadowColor,
        float             scale
    );
    void setColor(UColor col);
    void setAlpha(float alpha);
    void setFont(const char *fontname);
    void copyFrom(const UIFont *src);
    const str& getName(void) const { return name; }
    float getFloatWidth(const char *text, int maxlen, bool virtualScale);
    int   getWidth(const char *text, int maxlen, bool virtualScale);
    int   getCharWidth(char ch, bool virtualScale);
    int   getHeight(const char *text, int maxlen, bool virtualScale);
    int   getHeight(bool virtualScale);

protected:
    void loadDefaultFont(void);
};

