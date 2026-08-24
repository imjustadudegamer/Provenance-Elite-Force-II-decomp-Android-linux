
#include "ucolor.h"
#include <cmath>

const UColor UClear(0, 0, 0, 0);
const UColor UWhite(1, 1, 1, 1);
const UColor UBlack(0, 0, 0, 1);
const UColor ULightGrey(0.875, 0.875, 0.875, 1.0);
const UColor UGrey(0.75, 0.75, 0.75, 1.0);
const UColor UDarkGrey(0.5, 0.5, 0.5, 1.0);
const UColor URed(1.0, 0.0, 0.0, 1.0);
const UColor UGreen(0.3f, 0.8f, 0.0f, 1.0f);
const UColor UBlue(0.0, 0.0, 1.0, 1.0);
const UColor UYellow(1.0, 1.0, 0.0, 1.0);

// Not one of retail's globals -- retail has exactly the ten above.  It is kept
// not survive transcription: __ct__8UIWidgetFv (@0x0009E9A0) fills the widget
// block 0x218a60) and never writes m_foregroundColors at +248 at all, so retail
const UColor UHudColor(0.7f, 0.6f, 0.05f, 1.0f);

UColor::UColor(void)
{
    r = g = b = 0;
    a         = 1.0f;
}

UColor::UColor(float fR, float fG, float fB, float fA)
{
    set(fR, fG, fB, fA);
}

UColor::UColor(UColorHSV hsv)
{
    float hh, p, q, t, ff;
    int   i;

    a = hsv.a;

    if (fabs(hsv.s) <= 0.035f) {
        r = hsv.v;
        g = hsv.v;
        b = hsv.v;
        return;
    }

    hh = hsv.h;
    if (hh >= 360.0) {
        hh = 0.0;
    }
    hh /= 60.0;
    i  = (long)hh;
    ff = hh - i;
    p  = hsv.v * (1.0f - hsv.s);
    q  = hsv.v * (1.0f - (hsv.s * ff));
    t  = hsv.v * (1.0f - (hsv.s * (1.0f - ff)));

    switch (i) {
    case 0:
        r = hsv.v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = hsv.v;
        b = p;
        break;
    case 2:
        r = p;
        g = hsv.v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = hsv.v;
        break;
    case 4:
        r = t;
        g = p;
        b = hsv.v;
        break;
    case 5:
    default:
        r = hsv.v;
        g = p;
        b = q;
        break;
    }
}

UColor::UColor(const UColor& color)
    : r(color.r)
    , g(color.g)
    , b(color.b)
    , a(color.a)
{}

UColor::operator float *()
{
    return (float *)this;
}

UColor::operator float *() const
{
    return (float *)this;
}

void UColor::ScaleColor(float scale)
{
    r *= scale;
    g *= scale;
    b *= scale;
}

void UColor::ScaleAlpha(float scale)
{
    a *= scale;
}

void UColor::set(float fR, float fG, float fB, float fA)
{
    this->r = fR;
    this->g = fG;
    this->b = fB;
    this->a = fA;
}

UColorHSV::UColorHSV()
{
    h = s = v = a = 0;
}

UColorHSV::UColorHSV(UColor rgb)
{
    float min;
    float max;
    float delta;

    a = rgb.a;

    min = rgb.r < rgb.g ? rgb.r : rgb.g;
    min = min < rgb.b ? min : rgb.b;

    max = rgb.r > rgb.g ? rgb.r : rgb.g;
    max = max > rgb.b ? max : rgb.b;

    v     = max;
    delta = max - min;

    if (max > 0.0) {
        s = (delta / max);
    } else {
        s = 0.0;
    }

    if (fabs(s) <= 0.035f) {
        h = -1.0;
        return;
    }

    if (rgb.r >= max) {
        h = (rgb.g - rgb.b) / delta;
    } else {
        if (rgb.g >= max) {
            h = 2.0f + (rgb.b - rgb.r) / delta;
        } else {
            h = 4.0f + (rgb.r - rgb.g) / delta;
        }
    }

    h *= 60.0;

    if (h < 0.0) {
        h += 360.0;
    }
}

UColorHSV::UColorHSV(float fH, float fS, float fV, float fA)
{
    set(fH, fS, fV, fA);
}

void UColorHSV::set(float fH, float fS, float fV, float fA)
{
    this->h = fH;
    this->s = fS;
    this->v = fV;
    this->a = fA;
}

UBorderColor::UBorderColor() {}

UBorderColor::UBorderColor(const UColor& darkColor, const UColor& reallyDarkColor, const UColor& lightColor)
{
    this->dark       = darkColor;
    this->reallydark = reallyDarkColor;
    this->light      = lightColor;
}

UBorderColor::UBorderColor(const UColor& color)
{
    UColorHSV lighttemp;

    dark.r       = color.r * 0.6f;
    dark.g       = color.g * 0.6f;
    dark.b       = color.b * 0.6f;
    dark.a       = color.a;
    reallydark.r = color.r * 0.3f;
    reallydark.g = color.g * 0.3f;
    reallydark.b = color.b * 0.3f;
    reallydark.a = color.a;
    lighttemp    = color;
    lighttemp.s *= 0.75f;
    lighttemp.v *= 1.3f;
    light    = lighttemp;
    original = color;
}

void UBorderColor::CreateSolidBorder(const UColor& color, colorType_t type)
{
    switch (type) {
    case colorType_t::DARK:
        dark.r = color.r * 0.666f;
        dark.g = color.g * 0.666f;
        dark.b = color.b * 0.666f;
        dark.a = 1.0;
        break;
    case colorType_t::REALLYDARK:
        reallydark.r = color.r * 0.333f;
        reallydark.g = color.g * 0.333f;
        reallydark.b = color.b * 0.333f;
        reallydark.a = 1.0;
        break;
    case colorType_t::LIGHT:
        light.r = color.r;
        light.g = color.g;
        light.b = color.b;
        light.a = 1.0;
        break;
    case colorType_t::NORMAL:
        original = color;
        break;
    }
}
