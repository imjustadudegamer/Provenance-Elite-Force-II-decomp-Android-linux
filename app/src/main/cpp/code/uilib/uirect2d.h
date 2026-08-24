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

#include "uipoint2d.h"
#include "uisize2d.h"

class UIRect2D
{
public:
    UIPoint2D pos;
    UISize2D  size;

public:
    UIRect2D();
    UIRect2D(float x, float y, float width, float height);
    UIRect2D(const UIPoint2D& pos, const UISize2D& size);

    float     getMinX() const;
    float     getMinY() const;
    float     getMaxX() const;
    float     getMaxY() const;
    UIPoint2D getUpperLeft() const;
    UIPoint2D getUpperRight() const;
    UIPoint2D getLowerLeft() const;
    UIPoint2D getLowerRight() const;

    bool contains(const UIPoint2D& pos) const;
    bool contains(float x, float y) const;
    bool contains(int x, int y) const;
    bool contains(const UIRect2D& pos) const;
    bool intersects(const UIRect2D& pos) const;
};

inline UIRect2D::UIRect2D() {}

inline UIRect2D::UIRect2D(float x, float y, float width, float height)
{
    pos  = UIPoint2D(x, y);
    size = UISize2D(width, height);
}

inline UIRect2D::UIRect2D(const UIPoint2D& newPos, const UISize2D& newSize)
{
    this->pos  = newPos;
    this->size = newSize;
}

inline float UIRect2D::getMinX(void) const
{
    return pos.x;
}

inline float UIRect2D::getMinY(void) const
{
    return pos.y;
}

inline float UIRect2D::getMaxX(void) const
{
    return pos.x + size.width;
}

inline float UIRect2D::getMaxY(void) const
{
    return pos.y + size.height;
}

inline UIPoint2D UIRect2D::getUpperLeft(void) const
{
    return UIPoint2D(getMinX(), getMinY());
}

inline UIPoint2D UIRect2D::getUpperRight(void) const
{
    return UIPoint2D(getMaxX(), getMinY());
}

inline UIPoint2D UIRect2D::getLowerLeft(void) const
{
    return UIPoint2D(getMinX(), getMaxY());
}

inline UIPoint2D UIRect2D::getLowerRight(void) const
{
    return UIPoint2D(getMaxX(), getMaxY());
}

inline bool UIRect2D::contains(const UIPoint2D& point) const
{
    return point.x >= this->pos.x && point.x < this->pos.x + size.width && point.y >= this->pos.y
        && point.y < this->pos.y + size.height;
}

inline bool UIRect2D::contains(float x, float y) const
{
    return x >= this->pos.x && x < this->pos.x + size.width && y >= this->pos.y && y < this->pos.y + size.height;
}

inline bool UIRect2D::contains(int x, int y) const
{
    return x >= this->pos.x && x < this->pos.x + size.width && y >= this->pos.y && y < this->pos.y + size.height;
}

inline bool UIRect2D::contains(const UIRect2D& rect) const
{
    return rect.pos.x >= this->pos.x && rect.pos.x <= this->pos.x + size.width && rect.pos.y >= this->pos.y
        && rect.pos.y <= this->pos.y + size.height;
}

inline bool UIRect2D::intersects(const UIRect2D& rect) const
{
    return false;
}
