
#pragma once

typedef enum {
    SLIDER_FLOAT,
    SLIDER_INTEGER
} slidertype_t;

class UISlider : public UIWidget
{
    bool              m_initialized;
    float             m_minvalue;
    float             m_maxvalue;
    int               m_minx;
    int               m_maxx;
    int               m_arrow_width;
    int               m_arrow_width_raw;
    float             m_value;
    float             m_oldvalue;
    float             m_sliderwidth; 
    float             m_sliderwidth_raw;
    UIRect2D          m_sliderregion;
    UIRect2D          m_prev_arrow_region;
    UIRect2D          m_next_arrow_region;
    slidertype_t      m_slidertype;
    float             m_stepsize;
    bool              m_prev_arrow_depressed;
    bool              m_next_arrow_depressed;
    bool              m_thumb_depressed;
    UIReggedMaterial *m_bar_material;
    UIReggedMaterial *m_prev_arrow_material;
    UIReggedMaterial *m_next_arrow_material;
    UIReggedMaterial *m_thumbmaterial;
    UIReggedMaterial *m_prev_arrow_material_pressed;
    UIReggedMaterial *m_next_arrow_material_pressed;
    UIReggedMaterial *m_thumbmaterial_pressed;

public:
    CLASS_PROTOTYPE(UISlider);

private:
    void setThumb(void);
    void Pressed(Event *ev) override;
    void Released(Event *ev);
    void Increment(void);
    void Decrement(void);
    void AutoIncrement(Event *ev);
    void AutoDecrement(Event *ev);
    void MouseExited(Event *ev) override;
    void MouseEntered(Event *ev) override;
    void MouseDragged(Event *ev);
    void LayoutSetRange(Event *ev);
    void LayoutSetType(Event *ev);
    void LayoutSetStep(Event *ev);
    void LayoutSliderBGShader(Event *ev);
    void LayoutSliderThumbShader(Event *ev);
    void LayoutSliderLeftShader(Event *ev);
    void LayoutSliderRightShader(Event *ev);

public:
    UISlider();

    void     Draw(void) override;
    void     FrameInitialized(void) override;
    void     ChangeResolution(void) override;
    qboolean KeyEvent(int key, unsigned int time) override;
    void     UpdateData(void) override;
    void     UpdateUIElement(void) override;
    void     setType(slidertype_t type);
    void     setRange(float min, float max);
    void     setStep(float value);
};
