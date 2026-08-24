
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

public:
    UIFont();
    UIFont(const char *fn);

    void Print(float x, float y, const char *text, int maxlen, UColor shadowColor, int virtualScreen);
    void PrintJustified(
        const UIRect2D&   rect,
        fonthorzjustify_t horz,
        fontvertjustify_t vert,
        const char       *text,
        UColor            shadowColor,
        int               virtualScreen
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

