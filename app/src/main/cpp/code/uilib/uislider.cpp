
#include "ui_local.h"
#include <string.h>

Event EV_AutoIncrement
(
    "autoinc",
    EV_DEFAULT,
    NULL,
    NULL,
    "Used to auto increment the slider"
);

Event EV_AutoDecrement
(
    "autodec",
    EV_DEFAULT,
    NULL,
    NULL,
    "Used to auto decrement the slider"
);

Event EV_Layout_Range
(
    "setrange",
    EV_DEFAULT,
    "ff",
    "min max",
    "Used to set the range of a slider"
);

Event EV_Layout_SliderType
(
    "slidertype",
    EV_DEFAULT,
    "s",
    "sliderType(float|integer)",
    "Set the slider type to be floating point or integral"
);

Event EV_Layout_SliderStep
(
    "stepsize",
    EV_DEFAULT,
    "f",
    "stepsize",
    "Set the step size of the slider"
);

Event EV_Layout_SliderThumbShader
(
    "thumbshader",
    EV_DEFAULT,
    "sS",
    "shader depressed_shader",
    "Set the shader of the slider's thumb"
);

Event EV_Layout_SliderLeftShader
(
    "leftshader",
    EV_DEFAULT,
    "sS",
    "shader depressed_shader",
    "Set the shader of the slider's left arrow"
);

Event EV_Layout_SliderRightShader
(
    "rightshader",
    EV_DEFAULT,
    "sS",
    "shader depressed_shader",
    "Set the shader of the slider's right arrow"
);

CLASS_DECLARATION(UIWidget, UISlider, NULL) {
    {&W_LeftMouseDown,             &UISlider::Pressed                },
    {&W_LeftMouseDragged,          &UISlider::MouseDragged           },
    {&W_LeftMouseUp,               &UISlider::Released               },
    {&W_MouseExited,               &UISlider::MouseExited            },
    {&W_MouseEntered,              &UISlider::MouseEntered           },
    {&EV_AutoIncrement,            &UISlider::AutoIncrement          },
    {&EV_AutoDecrement,            &UISlider::AutoDecrement          },
    {&EV_Layout_Range,             &UISlider::LayoutSetRange         },
    {&EV_Layout_SliderType,        &UISlider::LayoutSetType          },
    {&EV_Layout_SliderStep,        &UISlider::LayoutSetStep          },
    {&EV_Layout_SliderThumbShader, &UISlider::LayoutSliderThumbShader},
    {&EV_Layout_SliderLeftShader,  &UISlider::LayoutSliderLeftShader },
    {&EV_Layout_SliderRightShader, &UISlider::LayoutSliderRightShader},
    {NULL,                         NULL                              }
};

UISlider::UISlider()
{
    m_sliderregion         = {};
    m_prev_arrow_region    = {};
    m_next_arrow_region    = {};
    m_minvalue             = 0.0f;
    m_maxvalue             = 100.0f;
    m_value                = 0.0f;
    m_oldvalue             = 0.0f;
    m_sliderwidth          = 10.0f;
    m_sliderwidth_raw      = 10.0f;
    m_stepsize             = 1.0f;
    m_slidertype           = SLIDER_INTEGER;
    m_arrow_width          = 15;
    m_arrow_width_raw      = 15;
    m_prev_arrow_depressed = qfalse;
    m_next_arrow_depressed = qfalse;
    m_thumb_depressed      = qfalse;
    m_initialized          = qfalse;

    m_prev_arrow_material         = uWinMan.RegisterShader("textures/menu/arrow_left");
    m_prev_arrow_material_pressed = uWinMan.RegisterShader("textures/menu/arrow_left_sel");
    m_next_arrow_material         = uWinMan.RegisterShader("textures/menu/arrow_right");
    m_next_arrow_material_pressed = uWinMan.RegisterShader("textures/menu/arrow_right_sel");
    m_thumbmaterial               = uWinMan.RegisterShader("textures/menu/slider_thumb");
    m_thumbmaterial_pressed       = uWinMan.RegisterShader("textures/menu/slider_thumb_sel");

    m_bar_material = NULL; 
    m_maxx         = 0;
    m_minx         = 0;
}

void UISlider::setThumb(void)
{
}

void UISlider::Pressed(Event *ev)
{
    float xpos = ev->GetFloat(1);
    float ypos = ev->GetFloat(2);

    if (m_clicksound.length()) {
        uii.Snd_PlaySound(m_clicksound, CHAN_MENU2);  
    }

    if (m_prev_arrow_region.contains(xpos, ypos)) 
    {
        Decrement();
        m_prev_arrow_depressed = qtrue;
        CancelEventsOfType(&EV_AutoDecrement);
        Event *newEv = new Event(EV_AutoDecrement);
        PostEvent(newEv, 0.2f);
        return;
    } else if (m_next_arrow_region.contains(xpos, ypos)) 
    {
        Increment();
        m_next_arrow_depressed = qtrue;
        CancelEventsOfType(&EV_AutoIncrement);
        Event *newEv = new Event(EV_AutoIncrement);
        PostEvent(newEv, 0.2f);
        return;
    }

    m_oldvalue        = m_value;
    m_thumb_depressed = qtrue;

    if (xpos <= m_minx) {
        m_value = m_minvalue;
    } else if (xpos >= m_maxx) {
        m_value = m_maxvalue;
    } else {
        m_value = (xpos - m_minx) / (m_maxx - m_minx) * (m_maxvalue - m_minvalue) + m_minvalue;
    }

    UpdateData();
}

void UISlider::Released(Event *ev)
{
    m_prev_arrow_depressed = qfalse;
    m_next_arrow_depressed = qfalse;
    m_thumb_depressed      = qfalse;
}

void UISlider::Increment(void)
{
    m_value = Q_clamp_float(m_value + m_stepsize, m_minvalue, m_maxvalue);
    UpdateData();
}

void UISlider::Decrement(void)
{
    m_value = Q_clamp_float(m_value - m_stepsize, m_minvalue, m_maxvalue);
    UpdateData();
}

void UISlider::AutoIncrement(Event *ev)
{
    if (m_next_arrow_depressed) {
        if (m_next_arrow_region.contains(uid.mouseX, uid.mouseY)) {
            Increment();
            Event *event = new Event(EV_AutoIncrement);
            PostEvent(event, 0.1f);
        } else {
            Released(NULL);
        }
    }
}

void UISlider::AutoDecrement(Event *ev)
{
    if (m_prev_arrow_depressed) {
        if (m_prev_arrow_region.contains(uid.mouseX, uid.mouseY)) {
            Decrement();
            Event *event = new Event(EV_AutoDecrement);
            PostEvent(event, 0.1f);
        } else {
            Released(NULL);
        }
    }
}

void UISlider::MouseExited(Event *ev)
{
    UIWidget::MouseExited(ev);

    if (m_thumb_depressed) {
        UpdateData();
    }

    m_thumb_depressed      = qfalse;
    m_prev_arrow_depressed = qfalse;
    m_next_arrow_depressed = qfalse;
}

void UISlider::MouseDragged(Event *ev)
{
    float xpos = ev->GetFloat(1);
    float ypos = ev->GetFloat(2);

    if (m_thumb_depressed) {
        if (xpos <= m_minx) {
            m_value = m_minvalue;
        } else if (xpos >= m_maxx) {
            m_value = m_maxvalue;
        } else {
            m_value = (xpos - m_minx) / (m_maxx - m_minx) * (m_maxvalue - m_minvalue) + m_minvalue;
        }

        UpdateData();
        return;
    }

    if ((!m_prev_arrow_depressed && m_prev_arrow_region.contains(xpos, ypos))
        || (!m_next_arrow_depressed && m_next_arrow_region.contains(xpos, ypos))) {
        Pressed(ev);
    }
}

void UISlider::MouseEntered(Event *ev)
{
    UIWidget::MouseEntered(ev);

    int buttons = ev->GetInteger(3);
    uWinMan.ActivateControl(this);
    if (buttons & 1) 
    {
        Pressed(ev);
    }
}

void UISlider::UpdateData(void)
{
    if (m_cvarname.length()) {
        if (m_slidertype == SLIDER_INTEGER) {
            UI_SetCvarInt(m_cvarname, m_value);
        } else {
            UI_SetCvarFloat(m_cvarname, m_value);
        }
    }

    if (m_command.length()) {
        char text[32];
        if (m_slidertype == SLIDER_INTEGER) {
            Q_snprintf(text, 32, "%d", (int)m_value);
        } else {
            Q_snprintf(text, 32, "%.3f", m_value);
        }

        // read off the shipped data) -- and never appends to m_command.  The
        str cmd = m_command + ' ' + text + '\n';
        Cbuf_AddText(cmd.c_str());
    }
}

void UISlider::UpdateUIElement(void)
{
    if (m_cvarname.length()) {
        m_value = UI_GetCvarFloat(m_cvarname, 0.0f);
    }
}

void UISlider::LayoutSetRange(Event *ev)
{
    float min = ev->GetFloat(1);
    float max = ev->GetFloat(2);
    setRange(min, max);
}

void UISlider::LayoutSetType(Event *ev)
{
    str type = ev->GetString(1);
    if (!Q_stricmp(type, "float")) {
        setType(SLIDER_FLOAT);
    } else if (!Q_stricmp(type, "integer")) {
        setType(SLIDER_INTEGER);
    }
}

void UISlider::LayoutSetStep(Event *ev)
{
    m_stepsize = ev->GetFloat(1);
}

void UISlider::LayoutSliderBGShader(Event *ev)
{
}

void UISlider::LayoutSliderThumbShader(Event *ev)
{
    str shader      = ev->GetString(1);
    m_thumbmaterial = uWinMan.RegisterShader(shader);

    if (ev->NumArgs() <= 1) {
        m_thumbmaterial_pressed = m_thumbmaterial;
    } else {
        str pressedShader       = ev->GetString(2);
        m_thumbmaterial_pressed = uWinMan.RegisterShader(pressedShader);
    }

    m_sliderwidth         = uii.Rend_GetShaderWidth(m_thumbmaterial->GetMaterial());
    m_sliderwidth_raw     = m_sliderwidth;
    m_initialized = qfalse;
}

void UISlider::LayoutSliderLeftShader(Event *ev)
{
    str shader            = ev->GetString(1);
    m_prev_arrow_material = uWinMan.RegisterShader(shader);

    if (ev->NumArgs() <= 1) {
        m_prev_arrow_material_pressed = m_prev_arrow_material;
    } else {
        str pressedShader             = ev->GetString(2);
        m_prev_arrow_material_pressed = uWinMan.RegisterShader(pressedShader);
    }

    m_arrow_width_raw = uii.Rend_GetShaderWidth(m_prev_arrow_material->GetMaterial());
    m_arrow_width     = m_arrow_width_raw;
    m_initialized = qfalse;
}

void UISlider::LayoutSliderRightShader(Event *ev)
{
    str shader            = ev->GetString(1);
    m_next_arrow_material = uWinMan.RegisterShader(shader);

    if (ev->NumArgs() <= 1) {
        m_next_arrow_material_pressed = m_next_arrow_material;
    } else {
        str pressedShader             = ev->GetString(2);
        m_next_arrow_material_pressed = uWinMan.RegisterShader(pressedShader);
    }

    // W31-VERIFY6 D7-1: retail's LayoutSliderRightShader @0x8C460 does NOT write the
    // arrow-width members -- the contiguous body 0x8C460..0x8C688 never touches
    m_initialized = qfalse;
}

void UISlider::FrameInitialized(void)
{
    float cframeX     = m_clippedframe.pos.x;
    float cframeY     = m_clippedframe.pos.y;
    float cframeWidth = m_clippedframe.size.width;
    float frameHeight = m_frame.size.height;
    float arrowWidth  = (float)m_arrow_width;

    m_prev_arrow_region.pos.x       = cframeX;
    m_prev_arrow_region.pos.y       = cframeY;
    m_prev_arrow_region.size.height = frameHeight;
    m_prev_arrow_region.size.width  = arrowWidth;

    m_next_arrow_region.pos.x       = cframeX + cframeWidth - arrowWidth;
    m_next_arrow_region.pos.y       = cframeY;
    m_next_arrow_region.size.height = frameHeight;
    m_next_arrow_region.size.width  = arrowWidth;

    m_sliderregion.pos.x       = cframeX + arrowWidth;
    m_sliderregion.pos.y       = cframeY;
    m_sliderregion.size.height = frameHeight;
    m_sliderregion.size.width  = m_sliderwidth - arrowWidth;

    float unreachableEdgeWidth = arrowWidth + m_sliderwidth * 0.5f;

    m_minx = (int)(cframeX + unreachableEdgeWidth);
    m_maxx = (int)(cframeX + m_frame.size.width - unreachableEdgeWidth);
}

void UISlider::ChangeResolution(void)
{
    UIWidget::ChangeResolution();
    FrameInitialized();

    m_arrow_width = (int)((float)m_arrow_width_raw * uid.scaleRes[0]);
    m_sliderwidth = m_sliderwidth_raw * uid.scaleRes[0];
}

void UISlider::Draw(void)
{
    UIWidget::Draw();

    if (!m_initialized) {
        m_initialized = qtrue;
        FrameInitialized();
    }

    UpdateUIElement();

    float sliderpos = (m_value - m_minvalue) / (m_maxvalue - m_minvalue);
    sliderpos       = Q_clamp_float(sliderpos, 0.0f, 1.0f);
    float thumbWidth = m_sliderwidth;
    float x          = (m_maxx - m_minx) * sliderpos + m_arrow_width;

    if (m_thumbmaterial) {
        UIReggedMaterial *material = m_thumb_depressed ? m_thumbmaterial_pressed : m_thumbmaterial;
        uii.Rend_DrawPicStretched(x, 0.0f, thumbWidth, m_frame.size.height, 0.0f, 0.0f, 1.0f, 1.0f, material->GetMaterial());
    } else {
        Draw3DBox(x, 0.0f, thumbWidth, m_frame.size.height, qfalse, getBorderColor(), m_local_alpha);
    }

    if (m_prev_arrow_material == m_prev_arrow_material_pressed) {
        Draw3DBox(0.0f, 0.0f, (float)m_arrow_width, m_frame.size.height, m_prev_arrow_depressed, getBorderColor(), m_local_alpha);

        uii.Rend_DrawPicStretched(
            2.0f,
            2.0f,
            (float)m_arrow_width - 4.0f,
            m_frame.size.height - 4.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            m_prev_arrow_material->GetMaterial()
        );
    } else {
        uihandle_t material;
        if (m_prev_arrow_depressed) {
            material = m_prev_arrow_material_pressed->GetMaterial();
        } else {
            material = m_prev_arrow_material->GetMaterial();
        }

        uii.Rend_DrawPicStretched(0.0f, 0.0f, (float)m_arrow_width, m_frame.size.height, 0.0f, 0.0f, 1.0f, 1.0f, material);
    }

    if (m_next_arrow_material == m_next_arrow_material_pressed) {
        Draw3DBox(
            m_frame.size.width - (float)m_arrow_width,
            0.0f,
            (float)m_arrow_width,
            m_frame.size.height,
            m_next_arrow_depressed,
            getBorderColor(),
            m_local_alpha
        );

        uii.Rend_DrawPicStretched(
            m_frame.size.width - (float)m_arrow_width + 2.0f,
            2.0f,
            (float)m_arrow_width - 4.0f,
            m_frame.size.height - 4.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            m_next_arrow_material->GetMaterial()
        );
    } else {
        uihandle_t material;
        if (m_next_arrow_depressed) {
            material = m_next_arrow_material_pressed->GetMaterial();
        } else {
            material = m_next_arrow_material->GetMaterial();
        }

        uii.Rend_DrawPicStretched(
            m_frame.size.width - (float)m_arrow_width,
            0.0f,
            (float)m_arrow_width,
            m_frame.size.height,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            material
        );
    }
}

void UISlider::setType(slidertype_t type)
{
    m_slidertype = type;
}

qboolean UISlider::KeyEvent(int key, unsigned int time)
{
    if (key == K_RIGHTARROW) {
        Increment();
    } else if (key == K_LEFTARROW) {
        Decrement();
    } else {
        return qfalse;
    }

    return qtrue;
}

void UISlider::setRange(float min, float max)
{
    m_minvalue = min;
    m_maxvalue = max;
}

void UISlider::setStep(float value)
{
    m_stepsize = value;
}
