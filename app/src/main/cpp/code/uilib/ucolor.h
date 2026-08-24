
#pragma once

class UColor
{
public:
    float r;
    float g;
    float b;
    float a;

    UColor();
    UColor(float r, float g, float b, float a = 1.0);
    UColor(class UColorHSV hsv);
    UColor(const UColor& color);

    operator float *();
    operator float *() const;

    void ScaleColor(float scale);
    void ScaleAlpha(float scale);
    void set(float r, float g, float b, float a);
};

class UColorHSV
{
public:
    float h;
    float s;
    float v;
    float a;

    UColorHSV();
    UColorHSV(UColor rgb);
    UColorHSV(float h, float s, float v, float a);

    void set(float h, float s, float v, float a);
};

typedef enum {
    DARK,
    REALLYDARK,
    LIGHT,
    NORMAL
} colorType_t;

class UBorderColor
{
public:
    UColor dark;
    UColor reallydark;
    UColor light;
    UColor original;

    UBorderColor();
    UBorderColor(const UColor& dark, const UColor& reallydark, const UColor& light);
    UBorderColor(const UColor& color);

    void CreateSolidBorder(const UColor& color, colorType_t type);
};

extern const UColor UClear;
extern const UColor UWhite;
extern const UColor UBlack;
extern const UColor ULightGrey;
extern const UColor UGrey;
extern const UColor UDarkGrey;
extern const UColor URed;
extern const UColor UGreen;
extern const UColor UBlue;
extern const UColor UYellow;

extern const UColor UHudColor;
