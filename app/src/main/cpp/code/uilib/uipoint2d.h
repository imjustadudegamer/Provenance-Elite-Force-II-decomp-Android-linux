
#pragma once

class UIPoint2D
{
public:
    float x;
    float y;

    UIPoint2D();
    UIPoint2D(float x, float y);

    bool operator==(const UIPoint2D& pos) const;
    bool operator!=(const UIPoint2D& pos) const;
};

inline UIPoint2D::UIPoint2D()
{
    x = y = 0;
}

inline UIPoint2D::UIPoint2D(float fX, float fY)
{
    this->x = fX;
    this->y = fY;
}

inline bool UIPoint2D::operator==(const UIPoint2D& pos) const
{
    return (x == pos.x && y == pos.y);
}

inline bool UIPoint2D::operator!=(const UIPoint2D& pos) const
{
    return (x != pos.x || y != pos.y);
}
