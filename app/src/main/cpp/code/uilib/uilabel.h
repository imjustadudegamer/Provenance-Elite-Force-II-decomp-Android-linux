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

class linkstring
{
public:
    str value;
    str string;
    str raw;

    linkstring(const str& value, const str& string);
};

class UILabel : public UIWidget
{
    str                     label;
    str                     m_rawlabel;
    Container<linkstring *> m_linkstrings;
    str                     m_sCurrentShaderName;

public:
    CLASS_PROTOTYPE(UILabel);

private:
    void LinkString(Event *ev);
    int  FindLinkString(str val);
    void LabelLayoutShader(Event *ev);
    void LabelLayoutTileShader(Event *ev);

public:
    UILabel();

    void SetLabel(str lab);
    void Draw(void) override;
    void reloadStrings(void) override;
    void        setData(const str& data) override;
    const char *getData(void) override;
    const str  &getRawData(void) const { return m_rawlabel; }
};
