
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
