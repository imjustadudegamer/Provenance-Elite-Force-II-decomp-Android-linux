
#pragma once

class UISize2D
{
public:
    float width;
    float height;

public:
    UISize2D();
    UISize2D(float width, float height);

    bool operator==(const UISize2D& pos) const;
    bool operator!=(const UISize2D& pos) const;
};

inline UISize2D::UISize2D()
{
    width = height = 0.0f;
}

inline UISize2D::UISize2D(float fWidth, float fHeight)
{
    this->width  = fWidth;
    this->height = fHeight;
}

inline bool UISize2D::operator==(const UISize2D& pos) const
{
    return (width == pos.width && height == pos.height);
}

inline bool UISize2D::operator!=(const UISize2D& pos) const
{
    return (width != pos.width || height != pos.height);
}
