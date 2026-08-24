/*
===========================================================================
Reconstructed from the retail Star Trek: Voyager -- Elite Force II binaries.
The publisher released no source for this file.  This port asserts no copyright
in it and grants no rights to it: it remains the property of its respective
rights holders, and it is covered neither by the GNU GPL nor by the Ritual
Elite Force II SDK agreement.  See LICENSING.md.
===========================================================================
*/
// SPDX-License-Identifier: LicenseRef-EF2-Retail-Reconstruction


#pragma once

class UIRenderModel : public UIWidget
{
protected:
    str m_modelName;
    str m_animName;

    vec3_t m_offset;       
    vec3_t m_rotateOffset; 
    vec3_t m_angles;       
    vec3_t m_spin;

    float m_fovX;
    float m_fovY;
    float m_fitScale;

    bool m_precache;

    str m_hoverAnim;
    str m_armedAnim;
    str m_selectedAnim;
    str m_disabledAnim;

    vec3_t m_normalSpin;
    float  m_normalSpinPeriod;
    vec3_t m_hoverSpin;
    float  m_hoverSpinPeriod;
    vec3_t m_normalPivot;
    float  m_normalPivotPeriod;
    vec3_t m_hoverPivot;
    float  m_hoverPivotPeriod;
    vec3_t m_normalBob;
    float  m_normalBobPeriod;
    vec3_t m_hoverBob;
    float  m_hoverBobPeriod;

    str     m_modelPropertyGroup;
    str     m_modelPropertyName;
    qboolean m_modelPropertyInUse;


    qhandle_t m_hModel;

    void PrecacheModel(void);

public:
    CLASS_PROTOTYPE(UIRenderModel);

    UIRenderModel();

    void Draw(void) override;

    void Pressed(Event *ev) override;
    void RightMouseButtonPressed(Event *ev) override;

    void setModel(Event *ev);
    void setModelPrecache(Event *ev);
    void setModelOffset(Event *ev);
    void setModelRotateOffset(Event *ev);
    void setModelAngles(Event *ev);
    void setModelPitch(Event *ev);
    void setModelYaw(Event *ev);
    void setModelRoll(Event *ev);
    void setModelAnimation(Event *ev);
    void setModelHoverAnimation(Event *ev);
    void setModelArmedAnimation(Event *ev);
    void setModelSelectedAnimation(Event *ev);
    void setModelDisabledAnimation(Event *ev);
    void setModelFov(Event *ev);
    void LayoutRenderModelSpin(Event *ev);
    void setModelGameplayProperty(Event *ev);
    void setNormalSpin(Event *ev);
    void setHoverSpin(Event *ev);
    void setNormalPivot(Event *ev);
    void setHoverPivot(Event *ev);
    void setNormalBob(Event *ev);
    void setHoverBob(Event *ev);
    void setClickSoundProperty(Event *ev);
    void setAltClickSoundProperty(Event *ev);
};
