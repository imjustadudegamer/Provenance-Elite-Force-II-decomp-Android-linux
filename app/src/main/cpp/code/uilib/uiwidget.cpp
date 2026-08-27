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

#include "ui_local.h"
#include "../qcommon/stringresource.h"
#include "../qcommon/gameplaymanager.h"

#define PROBE_GET(name, def, flags) ((cvar_t *)0)
#define PROBE_PRINT                 Com_Printf
#define PROBE_ON_OVERRIDE                 (atoi(CvarGetForUI("qa_probe", "0")) != 0)  
#include "../qcommon/qa_probe.h"

#include "../qcommon/qa_ui.h"

#include "limit_uiprobe.h"

/* The HUD scope's depth and factor.  Declared here because the layout pass
   (`ChangeResolution') reads them and is defined above their setter. */
static int   s_hudScaleDepth;
static float s_hudScaleValue = 1.0f;

static UBorderColor DeriveBorderColor(const UColor& color)
{
    constexpr float borderDarkScale = 0x1.555556p-1f;   
    constexpr float borderDimScale  = 0x1.555556p-2f;   
    constexpr float borderSatScale  = 0x1.8p-1f;        
    constexpr float borderValScale  = 0x1.555556p+1f;   
    constexpr float borderValClamp  = 0x1p+0f;          

    UBorderColor border;
    UColorHSV    hsv(color);

    border.dark.r = color.r * borderDarkScale;
    border.dark.g = color.g * borderDarkScale;
    border.dark.b = color.b * borderDarkScale;
    border.dark.a = color.a;

    border.reallydark.r = color.r * borderDimScale;
    border.reallydark.g = color.g * borderDimScale;
    border.reallydark.b = color.b * borderDimScale;
    border.reallydark.a = color.a;

    hsv.s *= borderSatScale;
    hsv.v *= borderValScale;
    if (hsv.v > borderValClamp) {
        hsv.v = borderValClamp;
    }
    border.light = UColor(hsv);

    border.original = color;
    return border;
}

Event W_Destroyed
(
    "widget_destroyed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Specifies that the widget has been destroyed." 
);

Event W_SizeChanged
(
    "size_changed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Specifies that the size of the widget has changed." 
);

Event W_FrameChanged
(
    "frame_changed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Specifies that the frame has changed." 
);

Event W_OriginChanged
(
    "origin_changed",
    EV_DEFAULT,
    NULL,
    NULL,
    "Specifies that the origin f the widget has changed." 
);

Event W_Activated
(
    "activated",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signals that the widget has been activated." 
);

Event EV_Widget_Activate
(
    "activate",
    EV_DEFAULT,
    "B",
    "force",
    "Called when the widget activates"
);

// does.  It is POSTED, not ignored: UIWindowManager::ActivateControl @0x000B3F70 and
Event EV_Widget_Deactivate
(
    "deactivate",
    EV_DEFAULT,
    NULL,
    NULL,
    "Called when the widget deactivates."
);

Event W_Deactivated
(
    "deactivated",
    EV_DEFAULT,
    NULL,
    NULL,
    "Signals that the widget has been deactivated." 
);

Event EV_Widget_Hide
(
    "hide",
    EV_DEFAULT,
    NULL,
    NULL,
    "Hides the widget." 
);

Event EV_Widget_Enable
(
    "enable",
    EV_DEFAULT,
    NULL,
    NULL,
    "enable the widget." 
);

Event EV_Widget_Disable
(
    "disable",
    EV_DEFAULT,
    NULL,
    NULL,
    "enable the widget." 
);

Event W_RealignWidget
(
    "realign_widget",
    EV_DEFAULT,
    NULL,
    NULL,
    "Specifies that the widget has been realigned." 
);

Event W_Draw
(
    "draw_widget",
    EV_DEFAULT,
    NULL,
    NULL,
    "Draw the widget." 
);

Event EV_Layout_Size
(
    "size",
    EV_DEFAULT,
    "ffff",
    "x y width height",
    "Set the size of the widget" 
);

Event EV_Layout_Rect
(
    "rect",
    EV_DEFAULT,
    "ffffB",
    "x y width height resize",
    "Set the rect of the widget" 
);

Event EV_Layout_Name
(
    "name",
    EV_DEFAULT,
    "s",
    "name",
    "Set the name of the widget"
);

Event EV_Layout_Title
(
    "title",
    EV_DEFAULT,
    "s",
    "title",
    "Set the title of the widget to be displayed in the center of it"
);

Event EV_Layout_LabelText
(
    "labeltext",
    EV_DEFAULT,
    "s",
    "text",
    "Used by labels to simply dump text into the label, justified top-left."
);

Event EV_Layout_Font
(
    "font",
    EV_DEFAULT,
    "s",
    "title",
    "Set the font of the widget"
);

Event EV_Layout_BGColor
(
    "bgcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Set the background color of the widget"
);

Event EV_Layout_FGColor
(
    "fgcolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Set the foreground color of the widget"
);

Event EV_Layout_Borderstyle
(
    "borderstyle",
    EV_DEFAULT,
    "s",
    "style",
    "Set the borderstyle of the widget(NONE, 3D_BORDER, INDENT_BORDER) "
);

Event EV_Layout_BorderColor
(
    "bordercolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Set the border color of the widget"
);

Event EV_Layout_Shader
(
    "shader",
    EV_DEFAULT,
    "s",
    "shaderName",
    "Set the shader to draw on the background of the widget"
);

Event EV_Layout_TileShader
(
    "tileshader",
    EV_DEFAULT,
    "s",
    "shaderName",
    "Set the shader to draw tiled on the background of the widget"
);

Event EV_Layout_HoverShader
(
    "hovershader",
    EV_DEFAULT,
    "s",
    "shaderName",
    "Set the shader to on the background of the widget when the mouse is over the widget"
);

Event EV_Layout_PressedShader
(
    "pressedshader",
    EV_DEFAULT,
    "s",
    "shaderName",
    "Set the shader to on the background of the widget when the widget is pressed down"   
);

Event EV_Layout_FadeIn
(
    "fadein",
    EV_DEFAULT,
    "f",
    "fadetime",
    "Set the amount of time it takes for the widget to fade in"
);

Event EV_Layout_FadeDelay
(
    "fadedelay",
    EV_DEFAULT,
    "f",
    "fadedelay",
    "Sets the delay before the fade out occurs"
);

Event EV_Layout_StopSound
(
    "stopsound",
    EV_DEFAULT,
    "s",
    "soundName",
    "Set the sound to be played when the widget stops moving"
);

Event EV_Layout_ClickSound
(
    "clicksound",
    EV_DEFAULT,
    "s",
    "soundName",
    "Set the sound to be played when the widget is clicked"
);

Event EV_Layout_AltClickSound
(
    "altclicksound",
    EV_DEFAULT,
    "s",
    "soundName",
    "Set the sound to be played when the widget is right-clicked."
);

Event EV_Layout_SelectedShader
(
    "selectedshader",
    EV_DEFAULT,
    "s",
    "shaderName",
    "Set the select shader on the background of the widget when the widget is selected"
);

Event EV_Layout_FadeOut
(
    "fadeout",
    EV_DEFAULT,
    "f",
    "fadetime",
    "Sets the amount it takes for the widget to fade out."
);

Event EV_Layout_Stretch
(
    "stretch",
    EV_DEFAULT,
    "s",
    "stretchdir",
    "Set the widget to be stretched horizontally or vertically across the screen"
);


Event EV_Layout_InitData
(
    "initdata",
    EV_DEFAULT,
    "s",
    "datastring",
    "Set the linkcvar to be initialized to this value"
);

Event EV_Layout_Direction
(
    "direction",
    EV_DEFAULT,
    "sF",
    "direction time",
    "Set the direction the widget will appear from on the screen\n(from_top, from_bottom, from_left, or from_right)\nTime is specified to determine how fast the widget will scroll in\n"
);

Event EV_Layout_Align
(
    "align",
    EV_DEFAULT,
    "s",
    "alignment",
    "Set the side of the screen to align the widget(left right top bottom)"   
);

Event EV_Layout_StuffCommand
(
    "stuffcommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set the command to be stuffed when the widget is clicked"
);

Event EV_Layout_LinkCvar
(
    "linkcvar",
    EV_DEFAULT,
    "s",
    "cvarname",
    "Set the cvar that will be updated when the widget is changed"
);

Event EV_Layout_NoParentClipping
(
    "noparentclip",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the widget to not be clipped by the parent widget"
);

Event EV_Layout_NoParentAdjustment
(
    "noparentadjust",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the widget to not be adjusted by the parent widget"
);

Event EV_Widget_HideCommand
(
    "hidecommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set a command to be executed when the widget is hidden"
);

Event EV_Widget_ShowCommand
(
    "showcommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set a command to be executed when the widget is shown"
);

Event EV_Widget_PushCommand
(
    "pushcommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set a command to be executed when the widget is pushed on the stack"
);

Event EV_Widget_PopCommand
(
    "popcommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set a command to be executed when the widget is popped off the stack"
);

Event EV_Widget_KeyCommand
(
    "keycommand",
    EV_DEFAULT,
    "ss",
    "key command",
    "Sets the command to execute when a key is pressed"
);

Event EV_Widget_OrderNumber
(
    "ordernumber",
    EV_DEFAULT,
    "d",
    "num",
    "Set the order the widget should be activated in"
);

Event EV_Layout_AliasCache
(
    "aliascache",
    EV_DEFAULT,
    "ssSSSSSS",
    "alias realPath arg1 arg2 arg3 arg4 arg5 arg6",
    "Create an alias to the specified path and cache the resource"
);

Event EV_Layout_BGFill
(
    "bgfill",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Set a color to fill the entire background"
);

Event EV_Layout_Fullscreen
(
    "fullscreen",
    EV_DEFAULT,
    "b",
    "fullscreen",
    "Set container to be fullscreen.  This will disable the game rendering if it's active"
);

Event EV_Layout_NoEscape
(
    "noEscape",
    EV_DEFAULT,
    "b",
    "noEscape",
    "Sets the menu if escape key sequence is allowed."
);

Event EV_Layout_VidMode
(
    "vidmode",
    EV_DEFAULT,
    "i",
    "vidmode",
    "Set container's vidmode to change to when it's active"
);

Event EV_Layout_HorizontalJustify
(
    "horizontaljustify",
    EV_DEFAULT,
    "s",
    "horizontal",
    "Specifies the horizontal justification of text - left, right or center"
);
Event EV_Layout_VerticalJustify
(
    "verticaljustify",
    EV_DEFAULT,
    "s",
    "vertical",
    "Specifies the vertical justification of text - top bottom, center"
);

Event EV_Layout_TextureCoordinates
(
    "texturecoordinates",
    EV_DEFAULT,
    "ffff",
    "s1 t1 s2 t2",
    "The texture coordinates for the shader"
);
Event EV_Layout_ShaderColor
(
    "shadercolor",
    EV_DEFAULT,
    "ffff",
    "red green blue alpha",
    "Sets the material color"
);
Event EV_Layout_HoverCommand
(
    "hovercommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set the command to be stuffed when the widget is hovered over"
);
Event EV_Layout_HoverExitCommand
(
    "hoverexitcommand",
    EV_DEFAULT,
    "s",
    "command",
    "Set the command to be stuffed when the widget is no longer being hovered over"
);
Event EV_Layout_HoverSound
(
    "hoversound",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the sound to play when mouse enters widget"
);
Event EV_Layout_ShowSound
(
    "showsound",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the sound to play when the widget is shown"
);
Event EV_Layout_HideSound
(
    "hidesound",
    EV_DEFAULT,
    NULL,
    NULL,
    "Set the sound to play when the widget is hidden"
);
Event EV_Layout_CvarVisibility
(
    "cvarvisibility",
    EV_DEFAULT,
    "ssS",
    "cvar_name cvar_value checkType",
    "Shows a widget when the specified cvar equals the specified value. Otherwise it hides.\nTo apply to see if this needs to be visible ( ==, !=, <, <=, >, or >=, defaults to == )"
);
Event EV_Layout_HoverFGColor
(
    "hoverfgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "The hover foreground color"
);
Event EV_Layout_HoverBGColor
(
    "hoverbgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "The hover background color"
);
Event EV_Layout_SelectedFGColor
(
    "selectedfgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the selected foreground color"
);
Event EV_Layout_SelectedBGColor
(
    "selectedbgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the selected background color"
);
Event EV_Layout_DisabledFGColor
(
    "disabledfgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the disabled foreground color"
);
Event EV_Layout_TextShadowColor
(
    "textshadowcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Set the text shadow color of the widget"
);
Event EV_Layout_HoverTextShadowColor
(
    "hovertextshadowcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Set the hover text shadow color of the widget"
);
Event EV_Layout_HoverShaderColor
(
    "hovershadercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the material color"
);
Event EV_Layout_ArmedShaderColor
(
    "armedshadercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the material color"
);
Event EV_Layout_SelectedShaderColor
(
    "selectedshadercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the material color"
);
Event EV_Layout_DisabledShaderColor
(
    "disabledshadercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the disabled shader color"
);
Event EV_Layout_BorderWidth
(
    "borderwidth", EV_DEFAULT, "f", "width", "Pixel width of the border"
);
Event EV_Layout_ArmedShader
(
    "armedshader", EV_DEFAULT, "s", "shaderName", "Set the armed shader on the background of the widget when the widget is armed"
);
Event EV_Layout_CvarCheckDisable
(
    "cvarcheckdisable",
    EV_DEFAULT,
    "ssS",
    "cvar_name cvar_value checkType",
    "Adds a cvar check for disabling to the widget, where they checkType is what determines what check to\nTo apply to see if this needs to be disabled ( ==, !=, <, <=, >, or >=, defaults to == )"
);
Event EV_Layout_ArmedFGColor
(
    "armedfgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the armed foreground color"
);
Event EV_Layout_ArmedBGColor
(
    "armedbgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the armed background color"
);
Event EV_Layout_ArmedTextShadowColor
(
    "armedtextshadowcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Set the armed text shadow color of the widget"
);
Event EV_Layout_SelectedTextShadowColor
(
    "selectedtextshadowcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Set the selected text shadow color of the widget"
);
Event EV_Layout_HoverBorderColor
(
    "hoverbordercolor", EV_DEFAULT, "ffff", "red green blue alpha", "The hover border color"
);
Event EV_Layout_SelectedBorderColor
(
    "selectedbordercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the selected border color"
);
Event EV_Layout_ArmedBorderColor
(
    "armedbordercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the armed border color"
);
Event EV_Layout_DisabledBGColor
(
    "disabledbgcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the disabled background color"
);
Event EV_Layout_DisabledTextShadowColor
(
    "disabledtextshadowcolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the text shadow color"
);
Event EV_Layout_DisabledBorderColor
(
    "disabledbordercolor", EV_DEFAULT, "ffff", "red green blue alpha", "Sets the disabled border color"
);
//   0x2483AB  "Shows a widget when the specified stat is equal/not equal to the specified value. Otherwise it hides it."
Event EV_Layout_GameplayStringProperty
(
    "stringProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Hooks this widget up to the specified gameply object string property"
);
Event EV_Layout_GameplayFloatProperty
(
    "floatProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Hooks this widget up to the specified gameply object float property"
);
Event EV_Layout_GameplayVisibilityProperty
(
    "visibilityProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Hooks this widget up to the specified gameply object visibility property"
);
Event EV_Layout_GameplayShaderProperty
(
    "shaderProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Hooks this widget up to display the specified gameplay object shader"
);
Event EV_Layout_GameplayEnableProperty
(
    "enableProperty",
    EV_DEFAULT,
    "ss",
    "object property",
    "Sets the widget to enable/disable based on the gameplay property"
);
Event EV_Widget_StatVisibility
(
    "statVisibility",
    EV_DEFAULT,
    "ssb",
    "stat_name stat_value equal",
    "Shows a widget when the specified stat is equal/not equal to the specified value. Otherwise it hides it."
);
Event EV_Layout_WidgetState
(
    "widgetstate", EV_DEFAULT, "s", "widgetState", "Sets the widget state"
);

Event EV_Layout_Depth
(
    "depth", EV_DEFAULT, "f", "depth", "Sets the widget containers zdepth"
);
Event EV_Layout_NoResize
(
    "noresize", EV_DEFAULT, NULL, NULL, "Do not resize the widget when the resolution changes"   
);
Event EV_Layout_HideDuringLoad
(
    "hideDuringLoad", EV_DEFAULT, NULL, NULL, "Hides this widget or menu from when loading."
);
Event EV_Layout_NoMouse
(
    "nomouse", EV_DEFAULT, NULL, NULL, "Tells container not to pop up mouse."
);
Event EV_Layout_GameFocus
(
    "gamefocus", EV_DEFAULT, NULL, NULL, "Allows focus to the game window"
);

Event EV_Widget_RefreshShader
(
    "refreshShader",
    EV_DEFAULT,
    NULL,
    NULL,
    "Refreshes the shader for this widget"
);

Event EV_Widget_SetAltCommand
(
    "setaltcommand",
    EV_DEFAULT,
    "s",
    "command",
    "Sets the alternate command. By default, this command is executed when the "
    "right mouse button is pressed on a widget"
);

Event EV_Widget_MwcMove("mwc_move", EV_DEFAULT, "fff", "x y time", "Moves the widget.");

Event EV_Widget_MwcMoveTo(
    "mwc_moveto",
    EV_DEFAULT,
    NULL,
    NULL,
    "Moves the widget to the specified location over the time period specified."
);

Event EV_Widget_MwcMoveToCvar(
    "mwc_movetocvar",
    EV_DEFAULT,
    "ssFF",
    "x y scale time",
    "Moves the widget to (x+cvar1*scale,y+cvar2*scale) over the specified time."
);

Event EV_Widget_MwcMoveToStat(
    "mwc_movetostat",
    EV_DEFAULT,
    NULL,
    NULL,
    "Moves the widget to (x+stat*scale,y+stat*scale) over the specified time."
);

Event EV_Widget_MwcMoveToProp(
    "mwc_movetoprop",
    EV_DEFAULT,
    "ssFF",
    "x y scale time",
    "Moves the widget to (x+obj.prop*scale,y+obj.prop*scale) over the specified time."
);

Event EV_Widget_MwcRestart("mwc_restart", EV_DEFAULT, NULL, NULL, "Makes the widget commands start from the beginning.");

Event EV_Widget_MwcWait("mwc_wait", EV_DEFAULT, "f", "time", "Waits the specified amount of time.");

Event EV_Widget_ClearCommands("clearcommands", EV_DEFAULT, NULL, NULL, "Clears all of the commands.");

Event EV_Widget_RestartCommands("restartcommands", EV_DEFAULT, NULL, NULL, "Restarts all of the commands.");

Event EV_Widget_MwcSetPosition("mwc_setposition", EV_DEFAULT, "ff", "x y", "Immediately moves the position of the widget.");

Event EV_Widget_SetChild("setchild", EV_DEFAULT, "s", "child", "Makes target widget a child of current widget");

CLASS_DECLARATION(USignal, UIWidget, NULL) {
    {&W_LeftMouseDown,              &UIWidget::Pressed                 },
    {&W_RightMouseDown,             &UIWidget::RightMouseButtonPressed },
    {&W_MouseEntered,               &UIWidget::MouseEntered             },
    {&W_MouseExited,                &UIWidget::MouseExited              },
    {&EV_Widget_SetChild,           &UIWidget::SetChildWidget           },
    {&EV_Widget_Hide,               &UIWidget::Hide                    },
    {&EV_Widget_MwcMove,            &UIWidget::move                    },
    {&EV_Widget_MwcMoveTo,          &UIWidget::moveTo                  },
    {&EV_Widget_MwcMoveToCvar,      &UIWidget::moveToCvar              },
    {&EV_Widget_MwcMoveToStat,      &UIWidget::moveToStat              },
    {&EV_Widget_MwcMoveToProp,      &UIWidget::moveToProp              },
    {&EV_Widget_MwcRestart,         &UIWidget::restart                 },
    {&EV_Widget_MwcWait,            &UIWidget::wait                    },
    {&EV_Widget_ClearCommands,      &UIWidget::clearCommands           },
    {&EV_Widget_RestartCommands,    &UIWidget::restartCommands         },
    {&EV_Widget_MwcSetPosition,     &UIWidget::setPosition             },
    {&EV_Widget_Activate,           &UIWidget::Activate                },
    {&EV_Layout_Size,               &UIWidget::LayoutSize              },
    {&EV_Layout_Rect,               &UIWidget::LayoutRect              },
    {&EV_Layout_Name,               &UIWidget::LayoutName              },
    {&EV_Layout_Title,              &UIWidget::LayoutTitle             },
    {&EV_Layout_LabelText,          &UIWidget::LayoutLabelText         },
    {&EV_Layout_Font,               &UIWidget::LayoutFont              },
    {&EV_Layout_BGColor,            &UIWidget::LayoutBgColor           },
    {&EV_Layout_FGColor,            &UIWidget::LayoutFgColor           },
    {&EV_Layout_Borderstyle,        &UIWidget::LayoutBorderStyle       },
    {&EV_Layout_BorderColor,        &UIWidget::LayoutBorderColor       },
    {&EV_Layout_Shader,             &UIWidget::LayoutShader            },
    {&EV_Layout_TileShader,         &UIWidget::LayoutTileShader        },
    {&EV_Layout_HoverShader,        &UIWidget::LayoutHoverShader       },
    {&EV_Layout_FadeIn,             &UIWidget::LayoutFadeIn            },
    {&EV_Layout_FadeDelay,          &UIWidget::LayoutFadeDelay         },
    {&EV_Layout_FadeOut,            &UIWidget::LayoutFadeOut           },
    {&EV_Layout_StopSound,          &UIWidget::LayoutStopSound         },
    {&EV_Layout_ClickSound,         &UIWidget::LayoutClickSound        },
    {&EV_Layout_AltClickSound,      &UIWidget::LayoutAltClickSound     },
    {&EV_Layout_Stretch,            &UIWidget::LayoutStretch           },
    {&EV_Layout_InitData,           &UIWidget::LayoutInitData          },
    {&EV_Layout_Direction,          &UIWidget::LayoutDirection         },
    {&EV_Layout_Align,              &UIWidget::LayoutAlign             },
    {&EV_Layout_NoParentClipping,   &UIWidget::LayoutNoParentClipping  },
    {&EV_Layout_NoParentAdjustment, &UIWidget::LayoutNoParentAdjustment},
    {&EV_Layout_StuffCommand,       &UIWidget::LayoutStuffCommand      },
    {&EV_Layout_LinkCvar,           &UIWidget::LayoutLinkCvar          },
    {&EV_Layout_AliasCache,         &UIWidget::LayoutAliasCache        },
    {&EV_Widget_ShowCommand,        &UIWidget::ShowCommand             },
    {&EV_Widget_HideCommand,        &UIWidget::HideCommand             },
    {&EV_Widget_PushCommand,        &UIWidget::PushCommand             },
    {&EV_Widget_PopCommand,         &UIWidget::PopCommand              },
    {&EV_Widget_KeyCommand,         &UIWidget::KeyCommand              },
    {&EV_Widget_OrderNumber,        &UIWidget::LayoutOrderNumber       },
    {&EV_Widget_Enable,             &UIWidget::EnableEvent             },
    {&EV_Widget_Disable,            &UIWidget::DisableEvent            },


    {&EV_Layout_HorizontalJustify,  &UIWidget::LayoutHorizontalJustify },
    {&EV_Layout_VerticalJustify,    &UIWidget::LayoutVerticalJustify   },
    {&EV_Layout_TextureCoordinates, &UIWidget::Layout_TextureCoordinates},
    {&EV_Layout_ShaderColor,        &UIWidget::LayoutShaderColor       },
    {&EV_Layout_HoverCommand,       &UIWidget::LayoutHoverCommand      },
    {&EV_Layout_HoverExitCommand,   &UIWidget::LayoutHoverExitCommand  },
    {&EV_Widget_RefreshShader,      &UIWidget::refreshShader           },
    {&EV_Widget_SetAltCommand,      &UIWidget::SetAltCommand           },
    {&EV_Layout_HoverSound,         &UIWidget::SetHoverSound           },
    {&EV_Layout_ShowSound,          &UIWidget::SetShowSound            },
    {&EV_Layout_HideSound,          &UIWidget::SetHideSound            },
    {&EV_Layout_CvarVisibility,     &UIWidget::cvarVisibility          },
    {&EV_Layout_HoverFGColor,       &UIWidget::LayoutHoverFgColor      },
    {&EV_Layout_HoverBGColor,       &UIWidget::LayoutHoverBgColor      },
    {&EV_Layout_SelectedFGColor,    &UIWidget::LayoutSelectedFgColor   },
    {&EV_Layout_SelectedBGColor,    &UIWidget::LayoutSelectedBgColor   },
    {&EV_Layout_DisabledFGColor,    &UIWidget::LayoutDisabledFgColor   },
    {&EV_Layout_TextShadowColor,    &UIWidget::LayoutTextShadowColor   },
    {&EV_Layout_HoverTextShadowColor, &UIWidget::LayoutHoverTextShadowColor},
    {&EV_Layout_HoverShaderColor,   &UIWidget::LayoutHoverShaderColor  },
    {&EV_Layout_ArmedShaderColor,   &UIWidget::LayoutArmedShaderColor  },
    {&EV_Layout_SelectedShaderColor, &UIWidget::LayoutSelectedShaderColor},
    {&EV_Layout_DisabledShaderColor, &UIWidget::LayoutDisabledShaderColor},
    {&EV_Layout_BorderWidth,        &UIWidget::LayoutBorderWidth        },
    {&EV_Layout_ArmedShader,        &UIWidget::LayoutArmedShader        },
    {&EV_Layout_SelectedShader,     &UIWidget::LayoutSelectedShader     },
    {&EV_Layout_CvarCheckDisable,   &UIWidget::cvarCheckDisable         },
    {&EV_Layout_ArmedFGColor,       &UIWidget::LayoutArmedFgColor       },
    {&EV_Layout_ArmedBGColor,       &UIWidget::LayoutArmedBgColor       },
    {&EV_Layout_ArmedTextShadowColor, &UIWidget::LayoutArmedTextShadowColor},
    {&EV_Layout_SelectedTextShadowColor, &UIWidget::LayoutSelectedTextShadowColor},
    {&EV_Layout_HoverBorderColor,   &UIWidget::LayoutHoverBorderColor   },
    {&EV_Layout_SelectedBorderColor, &UIWidget::LayoutSelectedBorderColor},
    {&EV_Layout_ArmedBorderColor,   &UIWidget::LayoutArmedBorderColor   },
    {&EV_Layout_DisabledBorderColor, &UIWidget::LayoutDisabledBorderColor},
    {&EV_Layout_DisabledBGColor,    &UIWidget::LayoutDisabledBgColor    },
    {&EV_Layout_DisabledTextShadowColor, &UIWidget::LayoutDisabledTextShadowColor},
    {&EV_Layout_GameplayShaderProperty,     &UIWidget::LayoutGameplayShaderProperty    },
    {&EV_Layout_GameplayStringProperty,     &UIWidget::LayoutGameplayStringProperty    },
    {&EV_Layout_GameplayFloatProperty,      &UIWidget::LayoutGameplayFloatProperty     },
    {&EV_Layout_GameplayVisibilityProperty, &UIWidget::LayoutGameplayVisibilityProperty},
    {&EV_Layout_GameplayEnableProperty,     &UIWidget::LayoutGameplayEnableProperty    },
    {&EV_Widget_StatVisibility,             &UIWidget::statVisibility                  },
    {&EV_Layout_WidgetState,        &UIWidget::SetWidgetState           },
    {&EV_Layout_Depth,              &UIWidget::setDepth                 },
    {&EV_Layout_NoResize,           &UIWidget::NoResize                 },
    {&EV_Layout_HideDuringLoad,     &UIWidget::setHideDuringLoad        },

    {NULL,                          NULL                               }
};

void SetColor(const UColor& color, float alpha)
{
    const vec4_t col = {color.r, color.g, color.b, color.a * alpha};

    uii.Rend_SetColor(col);
}

void DrawBox(const UIRect2D& rect, const UColor& color, float alpha)
{
    DrawBox(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height, color, alpha);
}

void Draw3DBox(const UIRect2D& rect, bool indent, const UBorderColor& color, float alpha)
{
    Draw3DBox(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height, indent, color, alpha);
}

void DrawBox(float x, float y, float width, float height, const UColor& color, float alpha)
{
    SetColor(color, alpha);
    uii.Rend_DrawBox(x, y, width, height);
}

void Draw3DBox(float x, float y, float width, float height, bool indent, const UBorderColor& color, float alpha)
{
    if (indent) {
        DrawBox(x, y, 1.0, height - 1.0, color.dark, alpha);
        DrawBox(x + 1.0, y + 1.0, 1.0, height - 3.0, color.reallydark, alpha);
        DrawBox(x, y, width - 1.0, 1.0, color.dark, alpha);
        DrawBox(x + 1.0, y + 1.0, width - 3.0, 1.0, color.reallydark, alpha);
        DrawBox(x + width - 1.0, y, 1.0, height, color.light, alpha);
        DrawBox(x, y + height - 1.0, width, 1.0, color.light, alpha);
    } else {
        DrawBox(x + width - 2.0, y + 1.0, 1.0, height - 2.0, color.dark, alpha);
        DrawBox(x + width - 1.0, y, 1.0, height, color.reallydark, alpha);
        DrawBox(x + 1.0, y + height - 2.0, width - 2.0, 1.0, color.dark, alpha);
        DrawBox(x, y + height - 1.0, width, 1.0, color.reallydark, alpha);
        DrawBox(x, y, 1.0, height - 1.0, color.light, alpha);
        DrawBox(x, y, width - 1.0, 1.0, color.light, alpha);
    }
}

void DrawMac3DBox(const UIRect2D& rect, bool indent, const UBorderColor& color, int inset, float alpha)
{
    float x, y;
    float width, height;

    x      = rect.pos.x + inset;
    y      = rect.pos.y + inset;
    width  = rect.size.width - inset * 2;
    height = rect.size.height - inset * 2;

    if (indent) {
        DrawBox(x + 1.0, y + 1.0, width - 2.0, height - 2.0, color.dark, alpha);
        DrawBox(x, y, width, 1.0, color.reallydark, alpha);
        DrawBox(x, y + 1.0, 1.0, height - 2.0, color.reallydark, alpha);
        DrawBox(x + width - 1.0, y + 1.0, 1.0, height - 1.0, color.light, alpha);
        DrawBox(x, y + height - 1.0, width - 1.0, 1.0, color.light, alpha);
    } else {
        DrawBox(x, y, width, 1.0, color.light, alpha);
        DrawBox(x, y + 1.0, 1.0, height - 2.0, color.light, alpha);
        DrawBox(x + width - 1.0, y + 1.0, 1.0, height - 1.0, color.dark, alpha);
        DrawBox(x, y + height - 1.0, width - 1.0, 1.0, color.dark, alpha);
    }
}

void DrawBoxWithSolidBorder(
    const UIRect2D& rect, const UColor& inside, const UColor& outside, int size, int flags, float alpha
)
{
    if (flags & 1) {
        DrawBox(
            rect.pos.x + size, rect.pos.y + size, rect.size.width - size * 2, rect.size.height - size * 2, inside, alpha
        );
    }

    if (size && flags & 2) {
        DrawBox(rect.pos.x, rect.pos.y, size, rect.size.height - size, outside, alpha);
        DrawBox(rect.pos.x + size, rect.pos.y, rect.size.width - size, size, outside, alpha);
        DrawBox(rect.pos.x + rect.size.width - size, rect.pos.y + size, size, rect.size.height - size, outside, alpha);
        DrawBox(rect.pos.x, rect.pos.y + rect.size.height - size, rect.size.width - size, size, outside, alpha);
    }
}

UIReggedMaterial::UIReggedMaterial()
{
    hMat  = 0;
    isSet = true;
    isGot = false;
}

uihandle_t UIReggedMaterial::GetMaterial()
{
    /* `RegisterShaderNoMip' answers 0 when a shader resolved to the renderer's
       defaultShader, and a 0 latched here is permanent: the material is asked
       for every frame and would keep drawing the default.  The cursor arrow is
       the one material with no other route to the screen, so a failed resolve
       is retried instead of being remembered. */
    if (!isGot || hMat == 0) {
        isGot      = true;
        this->hMat = uii.Rend_RegisterMaterial(name.c_str());
    }
    return hMat;
}

void UIReggedMaterial::ReregisterMaterial(void)
{
    if (isGot) {
        uihandle_t refreshed = uii.Rend_RegisterMaterial(name.c_str());

        /* A refresh must not downgrade a live handle to the default-shader 0;
           GetMaterial retries on the next ask. */
        if (refreshed) {
            this->hMat = refreshed;
        }
    }
}

void UIReggedMaterial::CleanupMaterial(void)
{
    isGot = false;
}

void UIReggedMaterial::SetMaterial(const str& newName)
{
    this->name = newName;
}

void UIReggedMaterial::RefreshMaterial(void)
{
    if (isGot) {
        this->hMat = uii.Rend_RefreshMaterial(name.c_str());
    }
}

str UIReggedMaterial::GetName(void)
{
    return name;
}


UIReggedModel::UIReggedModel()
{
    handle = 0;
}

void UIReggedModel::SetModel(const str& newName)
{
    this->name = newName;
    ReregisterModel();
}

void UIReggedModel::ReregisterModel(void)
{
    if (name.length()) {
        this->handle = uii.Rend_RegisterModel(name.c_str());
    } else {
        this->handle = 0;
    }
}

qhandle_t UIReggedModel::GetHandle(void) const
{
    return handle;
}

str UIReggedModel::GetName(void) const
{
    return name;
}

UIWidget::UIWidget()
    : m_foreground_color(m_foregroundColors[WIDGET_STATE_NORMAL].color)
    , m_background_color(m_backgroundColors[WIDGET_STATE_NORMAL].color)
    , m_border_color(m_borderColors[WIDGET_STATE_NORMAL].color)
{
    // retail for every widget that does not get one (e.g. the inventory bar's
    m_frame      = UIRect2D(0.0f, 0.0f, 0.0f, 0.0f);
    m_drawRegion = UIRect2D(0.0f, 0.0f, 0.0f, 0.0f);
    m_parent          = NULL;
    m_hovercommandCol = 0;
    // restores retail for every widget that does not get one.
    m_frame           = UIRect2D(0.0f, 0.0f, 0.0f, 0.0f);
    m_drawRegion      = UIRect2D(0.0f, 0.0f, 0.0f, 0.0f);
    // they are not ported yet, so m_mover stays NULL exactly as retail's does for any
    m_mover           = NULL;
    for (int i = 0; i < 5; i++) {
        m_foregroundColors[i].color.set(0, 0, 0, 0);
        m_backgroundColors[i].color.set(0, 0, 0, 0);
        m_textShadowColors[i].color.set(0, 0, 0, 0);
        m_materialColors[i].color.set(0, 0, 0, 0);
        m_foregroundColors[i].set = qfalse;
        m_backgroundColors[i].set = qfalse;
        m_textShadowColors[i].set = qfalse;
        m_materialColors[i].set   = qfalse;

        m_borderColors[i].color.dark       = UColor(0, 0, 0, 0);
        m_borderColors[i].color.reallydark = UColor(0, 0, 0, 0);
        m_borderColors[i].color.light      = UColor(0, 0, 0, 1);
        m_borderColors[i].color.original   = UColor(0, 0, 0, 1);
        m_borderColors[i].set              = qfalse;
    }

    // Draw3DBox and never reads the width) now stays at 0 as retail has it.
    m_borderWidth = 0;
    m_gameplayPropertyGroup  = "";
    m_gameplayPropertyName   = "";
    m_gameplayPropertyType   = 0;
    m_gameplayPropertyInUse  = false;
    setForegroundColor(UColor(UGrey), WIDGET_STATE_NORMAL);
    setBackgroundColor(UColor(UWhite), WIDGET_STATE_NORMAL, true);
    setTextShadowColor(UColor(UClear), WIDGET_STATE_NORMAL);
    setForegroundColor(UColor(UGrey), WIDGET_STATE_DISABLED);
    m_indent       = 10.0f;
    m_bottomindent             = 5.0f;
    m_borderStyle              = border_indent;
    m_visible                  = true;
    m_enabled                  = true;
    m_canactivate              = false;
    m_commandhandler           = NULL;
    m_font                     = NULL;
    m_material                 = NULL;
    m_hovermaterial            = NULL;
    m_pressedmaterial          = NULL;
    m_selectedmaterial         = NULL;
    m_disabledmaterial         = NULL;
    m_direction                = D_NONE;
    m_fadetime                 = 0.0f;
    m_alpha                    = 1.0f;
    m_motiontime               = 0.0f;
    m_starttime                = 0.0f;
    // (0x9EFD4) are all zeroed there, and +1416 m_fadeStartTime is deliberately
    m_fadeDelay                = 0.0f;
    m_fadeout                  = 0.0f;
    m_fadeGate                 = 0.0f;
    m_direction_orig           = D_NONE;
    m_motiontype               = MOTION_IN;
    m_align                    = 0;
    m_flags                    = 0;
    m_widgetState              = WIDGET_STATE_NORMAL;
    m_dying                    = false;
    m_ordernum                 = 0;
    m_configstring_index       = -1;
    m_iFontAlignmentHorizontal = FONT_JUSTHORZ_CENTER;
    m_iFontAlignmentVertical   = FONT_JUSTVERT_CENTER;
    m_bVirtual                 = qtrue;
    m_depth                    = 1.0f;
    m_noresize                 = false;
    m_hideDuringLoad           = false;
    m_size.width               = 0.0f;
    m_size.height              = 0.0f;
    m_bMoveActive              = false;

    UIRect2D frame(6.0f, 6.0f, 100.0f, 13.0f);
    setFrame(frame);
}

UIWidget::~UIWidget()
{

    m_dying = true;

    if (IsActive()) {
        uWinMan.DeactivateCurrentSmart();
    }


    if (m_parent) {
        m_parent->removeChild(this);
    }

    if (m_font) {
        delete m_font;
        m_font = NULL;
    }

    for (int i = 1; i <= m_statVisibilityData.NumObjects(); i++) {
        delete m_statVisibilityData.ObjectAt(i);
    }
    m_statVisibilityData.ClearObjectList();

    for (int i = 1; i <= m_cvarVisibilityData.NumObjects(); i++) {
        delete m_cvarVisibilityData.ObjectAt(i);
    }
    m_cvarVisibilityData.ClearObjectList();

    for (int i = 1; i <= m_showcommands.NumObjects(); i++) {
        delete m_showcommands.ObjectAt(i);
    }
    m_showcommands.FreeObjectList();

    for (int i = 1; i <= m_hidecommands.NumObjects(); i++) {
        delete m_hidecommands.ObjectAt(i);
    }
    m_hidecommands.FreeObjectList();

    for (int i = 1; i <= m_pushcommands.NumObjects(); i++) {
        delete m_pushcommands.ObjectAt(i);
    }
    m_pushcommands.ClearObjectList();

    for (int i = 1; i <= m_popcommands.NumObjects(); i++) {
        delete m_popcommands.ObjectAt(i);
    }
    m_popcommands.ClearObjectList();

    for (int i = 1; i <= m_keyCommands.NumObjects(); i++) {
        delete m_keyCommands.ObjectAt(i);
    }
    m_keyCommands.FreeObjectList();

    if (m_mover) {
        delete m_mover;
        m_mover = NULL;
    }

    SendSignal(W_Destroyed);
}

bool UIWidget::addChild(UIWidget *widget)
{
    if (!widget) {
        return false;
    }

    if (m_children.ObjectInList(widget)) {
        return false;
    }

    m_children.AddObject(widget);
    return true;
}

bool UIWidget::removeChild(UIWidget *widget)
{
    if (m_children.ObjectInList(widget)) {
        m_children.RemoveObject(widget);
        return true;
    }

    return false;
}

void UIWidget::PropogateCoordinateSystem(void)
{
    int       i;
    int       n;
    UIWidget *subview;

    if (!m_parent || (m_flags & WF_NOPARENTADJUST)) {
        m_screenorigin  = m_origin;
        m_clippedorigin = m_origin;
        m_screenframe   = m_frame;
        m_clippedframe  = m_frame;
    } else {
        m_screenframe.pos.x = m_frame.pos.x + m_parent->m_screenorigin.x;
        m_screenframe.pos.y = m_frame.pos.y + m_parent->m_screenorigin.y;
        m_screenframe.size  = m_frame.size;
        m_screenorigin.x    = m_screenframe.pos.x - m_origin.x;
        m_screenorigin.y    = m_screenframe.pos.y - m_origin.y;
        m_clippedframe      = m_screenframe;
        m_clippedorigin.x   = -m_origin.x;
        m_clippedorigin.y   = -m_origin.y;
    }

    if (m_parent && !(m_flags & WF_DIRECTED)) {
        if (m_parent->m_clippedframe.pos.x > m_clippedframe.pos.x) {
            m_clippedframe.size.width -= (m_parent->m_clippedframe.pos.x - m_clippedframe.pos.x);

            if (m_clippedframe.size.width < 0.0) {
                m_clippedframe.size.width = 0.0;
            }

            m_clippedframe.pos.x = m_parent->m_clippedframe.pos.x;
        }

        if (m_parent->m_clippedframe.pos.y > m_clippedframe.pos.y) {
            m_clippedframe.size.height -= (m_parent->m_clippedframe.pos.y - m_clippedframe.pos.y);

            if (m_clippedframe.size.height < 0.0) {
                m_clippedframe.size.height = 0.0;
            }

            m_clippedframe.pos.y = m_parent->m_clippedframe.pos.y;
        }

        if (m_parent->m_clippedframe.getMaxX() < m_clippedframe.getMaxX()) {
            m_clippedframe.size.width = m_parent->m_clippedframe.getMaxX() - m_clippedframe.pos.x;

            if (m_clippedframe.size.width < 0.0) {
                m_clippedframe.size.width = 0.0;
            }
        }

        if (m_parent->m_clippedframe.getMaxY() < m_clippedframe.getMaxY()) {
            m_clippedframe.size.height = m_parent->m_clippedframe.getMaxY() - m_clippedframe.pos.y;

            if (m_clippedframe.size.height < 0.0) {
                m_clippedframe.size.height = 0.0;
            }
        }
    }

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        subview = m_children.ObjectAt(i);
        subview->PropogateCoordinateSystem();
    }
}

void UIWidget::set2D(void)
{
    int limit_subT0 = LIMIT_SubT0();

    uii.Rend_Set2D(
        uid.originX + (int)m_screenframe.pos.x,
        uid.surfaceHeight - uid.originY - (int)(m_screenframe.size.height + m_screenframe.pos.y),
        m_screenframe.size.width,
        m_screenframe.size.height,
        m_clippedorigin.x,
        m_clippedorigin.x + m_screenframe.size.width,
        m_clippedorigin.y + m_screenframe.size.height,
        m_clippedorigin.y,
        -1.0,
        1.0
    );

    LIMIT_SubPushEnd(limit_subT0);

    uii.Rend_Scissor(
        uid.originX + (int)m_clippedframe.pos.x,
        uid.surfaceHeight - uid.originY - (int)(m_clippedframe.size.height + m_clippedframe.pos.y),
        m_clippedframe.size.width,
        m_clippedframe.size.height
    );
}

//        * `r31` is NOT uid.  r31 = the TOC slot at -0x3F68(r2) = 0x22B258, whose
//      EV_Widget_Hide, not draw_widget (that one is slot 0x2287F8).  The delivered
void UIWidget::Draw(void)
{
    UI_COUNT(UI_FAM_UI, UI_UC_draw, 1);

    if (m_mover) {
        m_mover->Motion();
    }

    if (m_fadeDelay != 0.0f && m_fadeGate == 0.0f && m_fadeDelay < (float)uid.time - m_fadeStartTime) {
        ProcessEvent(new Event(EV_Widget_Hide));
    }
}

static int Fctiwz(float f)
{
    if (!(f > -2147483648.0f)) { 
        return -2147483647 - 1;
    }

    if (f >= 2147483648.0f) {
        return 2147483647;
    }

    return (int)f;
}

// m_frame.size.height, NOT m_startingpos.  Motion @0x000A0590 proves it: every
void UIWidget::calculateDrawRegion(void)
{
    const int   n = Fctiwz((float)m_borderWidth * uid.scaleRes[0] + 0.5f);
    const float f = (float)n;

    m_drawRegion.pos.x       = f;
    m_drawRegion.pos.y       = f;
    m_drawRegion.size.width  = m_frame.size.width - 2.0f * f;
    m_drawRegion.size.height = m_frame.size.height - 2.0f * f;
}

void UIWidget::FrameInitialized(void)
{
    calculateDrawRegion();
}

UIRect2D UIWidget::getDrawRegion(void)
{
    return m_drawRegion;
}

void UIWidget::DrawTitle(float x, float y)
{
    // 0x000A610C has `li r9, 0` (raw), not the widget's m_bVirtual.  Only the
    m_font->setColor(m_foreground_color);
    m_font->Print(x, y, m_title, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
}

//      this port (it never existed in retail: retail's m_motiontime at +1444 is
//   one-argument import never read r4; retail S_StartLocalSound @0x001D06F0 does
void UIWidget::Motion(void)
{
    float t;
    float frac;
    float nx, ny;
    float base;
    int   i;

    t = (float)uid.time / 1000.0f;

    if (m_fadetime != 0.0f) {
        m_alpha = (t - m_starttime) / m_fadetime;

        if (m_alpha > 1.0f) {
            m_alpha = 1.0f;
        }

        if (m_motiontype == MOTION_OUT) {
            m_alpha = 1.0f - m_alpha;
        }
    }

    if (m_direction) {
        frac = (t - m_starttime) / m_motiontime;

        if (m_motiontype == MOTION_OUT) {
            frac = 1.0f - frac;

            if (frac <= 0.0f) {
                switch (m_direction) {
                case D_FROM_BOTTOM:
                    setFrame(UIRect2D(m_frame.pos.x, (float)uid.vidHeight, m_frame.size.width, m_frame.size.height));
                    break;
                case D_FROM_TOP:
                    setFrame(UIRect2D(
                        m_frame.pos.x,
                        -m_frame.size.height - m_parent->m_screenorigin.y,
                        m_frame.size.width,
                        m_frame.size.height
                    ));
                    break;
                case D_FROM_LEFT:
                    setFrame(UIRect2D(
                        -m_frame.size.width - m_parent->m_screenorigin.x,
                        m_frame.pos.y,
                        m_frame.size.width,
                        m_frame.size.height
                    ));
                    break;
                case D_FROM_RIGHT:
                    setFrame(UIRect2D((float)uid.vidWidth, m_frame.pos.y, m_frame.size.width, m_frame.size.height));
                    break;
                default:
                    setFrame(m_frame);
                    break;
                }

                m_direction = D_NONE;
            }
        } else {
            if (frac >= 1.0f) {
                if (m_size.width != 0.0f && m_size.height != 0.0f) {
                    setSize(m_size);
                }

                setFrame(UIRect2D(m_startingpos, m_frame.size));
                FrameInitialized();

                for (i = 1; i <= m_children.NumObjects(); i++) {
                    m_children.ObjectAt(i)->FrameInitialized();
                }

                m_textureCoordinates[0] = 0.0f;
                m_textureCoordinates[1] = 0.0f;
                m_textureCoordinates[2] = 1.0f;
                m_textureCoordinates[3] = 1.0f;

                m_direction = D_NONE;
            }
        }

        switch (m_direction) {
        case D_FROM_BOTTOM:
            nx = m_frame.pos.x;
            ny = (float)uid.vidHeight - ((float)uid.vidHeight - m_startingpos.y) * frac;
            setFrame(UIRect2D(nx, ny, m_frame.size.width, m_frame.size.height));
            break;
        case D_FROM_TOP:
            nx = m_frame.pos.x;
            base = 0.0f;
            if (m_parent) {
                base = m_frame.size.height + m_parent->m_screenorigin.y;
            }
            ny = frac * (m_startingpos.y + base) - base;
            setFrame(UIRect2D(nx, ny, m_frame.size.width, m_frame.size.height));
            break;
        case D_FROM_LEFT:
            base = 0.0f;
            if (m_parent) {
                base = m_frame.size.width + m_parent->m_screenorigin.x;
            }
            nx = frac * (m_startingpos.x + base) - base;
            ny = m_frame.pos.y;
            setFrame(UIRect2D(nx, ny, m_frame.size.width, m_frame.size.height));
            break;
        case D_FROM_RIGHT:
            nx = (float)uid.vidWidth - ((float)uid.vidWidth - m_startingpos.x) * frac;
            ny = m_frame.pos.y;
            setFrame(UIRect2D(nx, ny, m_frame.size.width, m_frame.size.height));
            break;
        case D_STRETCH_LEFT:
            setSize(UISize2D(m_size.width * frac, m_size.height));
            setFrame(UIRect2D(
                m_size.width * (1.0f - frac) + m_startingpos.x,
                m_startingpos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_STRETCH_RIGHT:
            setSize(UISize2D(m_size.width * frac, m_size.height));
            break;
        case D_STRETCH_DOWN:
            setSize(UISize2D(m_size.width, m_size.height * frac));
            setFrame(UIRect2D(
                m_startingpos.x,
                m_size.height * (1.0f - frac) + m_startingpos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_STRETCH_UP:
            setSize(UISize2D(m_size.width, m_size.height * frac));
            break;
        case D_ROLL_LEFT:
            m_textureCoordinates[0] = 1.0f - frac;
            setSize(UISize2D(m_size.width * frac, m_size.height));
            setFrame(UIRect2D(
                m_size.width * (1.0f - frac) + m_startingpos.x,
                m_startingpos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_ROLL_RIGHT:
            m_textureCoordinates[2] = frac;
            setSize(UISize2D(m_size.width * frac, m_size.height));
            break;
        case D_ROLL_UP:
            m_textureCoordinates[1] = 1.0f - frac;
            setSize(UISize2D(m_size.width, m_size.height * frac));
            setFrame(UIRect2D(
                m_startingpos.x,
                m_size.height * (1.0f - frac) + m_startingpos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_ROLL_DOWN:
            m_textureCoordinates[3] = frac;
            setSize(UISize2D(m_size.width, m_size.height * frac));
            break;
        default:
            break;
        }
    }

    if (m_direction == D_NONE) {
        if (m_stopsound.length()) {
            uii.Snd_PlaySound(m_stopsound, CHAN_MENU);  
        }
    }
}

void UIWidget::AlignPosition(void)
{
    if (m_flags & WF_STRETCH_VERTICAL) {
        m_frame.pos.y       = 0;
        m_frame.size.height = uid.vidHeight;
    }
    if (m_flags & WF_STRETCH_HORIZONTAL) {
        m_frame.pos.x      = 0;
        m_frame.size.width = uid.vidWidth;
    }

    if (m_align & WA_CENTERX) {
        m_frame.pos.x = uid.vidWidth * 0.5 - m_frame.size.width * 0.5;
    }

    if (m_align & WA_RIGHT) {
        m_frame.pos.x = uid.vidWidth - m_frame.size.width;
    } else if (m_align & WA_LEFT) {
        m_frame.pos.x = 0;
    }

    if (m_align & WA_CENTERY) {
        m_frame.pos.y = uid.vidHeight * 0.5 - m_frame.size.height * 0.5;
    }

    if (m_align & WA_BOTTOM) {
        m_frame.pos.y = uid.vidHeight - m_frame.size.height;
    } else if (m_align & WA_TOP) {
        m_frame.pos.y = 0;
    }

    setFrame(m_frame);
}

void UIWidget::Hide(Event *ev)
{
    setShow(false);
}

void UIWidget::Activate(Event *ev)
{
    uWinMan.ActivateControl(this);
}

void UIWidget::Shutdown(void)
{
    int i;

    if (IsActive()) {
        uWinMan.DeactivateCurrentSmart();
    }

    for (i = m_children.NumObjects(); i > 0; i--) {
        UIWidget *wid = m_children.ObjectAt(i);

        wid->Shutdown();
        delete wid;
    }

    if (m_font) {
        delete m_font;
        m_font = NULL;
    }
}

void UIWidget::InitFrame(
    UIWidget *parentview, float x, float y, float width, float height, int border, const char *fontname
)
{
    if (border != -1) {
        setBorderStyle((borderstyle_t)border);
    }

    setFrame(UIRect2D(x, y, width, height));

    if (parentview) {
        setParent(parentview);
        m_depth = parentview->m_depth;
    } else {
        if (this != &uWinMan) {
            setParent(&uWinMan);
        }
        m_depth = 1.0f;
    }

    m_startingpos = m_frame.pos;

    m_bVirtual = m_parent ? m_parent->isResizeable() : qtrue;

    if (!m_font) {
        setFont(fontname);
    }

    FrameInitialized();
}

void UIWidget::InitFrame(UIWidget *parentview, const UIRect2D& r, int border, const char *fontname)
{
    InitFrame(parentview, r.pos.x, r.pos.y, r.size.width, r.size.height, border, fontname);
}

void UIWidget::LayoutSize(Event *ev)
{
    LayoutRect(ev);
}

void UIWidget::LayoutRect(Event *ev)
{
    UIPoint2D pos;
    UISize2D  size;

    if (ev->NumArgs() >= 4) {
        pos.x       = ev->GetFloat(1);
        pos.y       = ev->GetFloat(2);
        size.width  = ev->GetFloat(3);
        size.height = ev->GetFloat(4);

        m_size = size;

        setFrame(UIRect2D(pos, size));

        m_startingpos = pos;

        // @0x000A4F60, i.e. re-latch the authored frame from the NEW rect and
        if (ev->NumArgs() == 5) {
            m_bFrameLatched = false;

            ChangeResolution();
        }
    }
}

void UIWidget::LayoutName(Event *ev)
{
    setName(ev->GetString(1));
}

void UIWidget::LayoutTitle(Event *ev)
{
    setTitle(ev->GetString(1));
}

void UIWidget::LayoutLabelText(Event *ev)
{
    setLabelText(ev->GetString(1));
}

void UIWidget::LayoutFont(Event *ev)
{
    setFont(ev->GetString(1));
}

void UIWidget::LayoutBgColor(Event *ev)
{
    setBackgroundColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_NORMAL, true);

    {
        static cvar_t *shadow_inv = NULL;
        static int     shadow_invRows;

        if (!shadow_inv) {
            shadow_inv = Cvar_Get("shadow_invStats", "1", 0);
        }
        if (shadow_inv && shadow_inv->integer && shadow_invRows < 8
            && getName() && !Q_stricmp(getName(), "InventoryBar")) {
            UColor c = getBackgroundColor();
            Com_Printf("[SHADOW:inv] bgcolor= event on '%s' -> bg=(%.3f,%.3f,%.3f,%.3f)\n",
                       getName(), c.r, c.g, c.b, c.a);
            shadow_invRows++;
        }
    }
}

void UIWidget::LayoutFgColor(Event *ev)
{
    setForegroundColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)));
}


void UIWidget::LayoutHoverFgColor(Event *ev)
{
    setForegroundColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_HOVER);
}

void UIWidget::LayoutHoverBgColor(Event *ev)
{
    setBackgroundColor(
        UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)),
        WIDGET_STATE_HOVER,
        true
    );
}

void UIWidget::LayoutSelectedFgColor(Event *ev)
{
    setForegroundColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_SELECTED);
}

void UIWidget::LayoutSelectedBgColor(Event *ev)
{
    setBackgroundColor(
        UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)),
        WIDGET_STATE_SELECTED,
        true
    );
}

void UIWidget::LayoutDisabledFgColor(Event *ev)
{
    setForegroundColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_DISABLED);
}

void UIWidget::LayoutTextShadowColor(Event *ev)
{
    setTextShadowColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_NORMAL);
}

void UIWidget::LayoutHoverTextShadowColor(Event *ev)
{
    setTextShadowColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_HOVER);
}

void UIWidget::LayoutHoverShaderColor(Event *ev)
{
    setMaterialColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_HOVER);
}

void UIWidget::LayoutArmedShaderColor(Event *ev)
{
    setMaterialColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_ARMED);
}

void UIWidget::LayoutSelectedShaderColor(Event *ev)
{
    setMaterialColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_SELECTED);
}

void UIWidget::LayoutDisabledShaderColor(Event *ev)
{
    setMaterialColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_DISABLED);
}

void UIWidget::LayoutArmedFgColor(Event *ev)
{
    setForegroundColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_ARMED);
}

void UIWidget::LayoutArmedBgColor(Event *ev)
{
    setBackgroundColor(
        UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)),
        WIDGET_STATE_ARMED,
        true
    );
}

void UIWidget::LayoutArmedTextShadowColor(Event *ev)
{
    setTextShadowColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_ARMED);
}

void UIWidget::LayoutSelectedTextShadowColor(Event *ev)
{
    setTextShadowColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_SELECTED);
}

void UIWidget::LayoutHoverBorderColor(Event *ev)
{
    setBorderColor(
        DeriveBorderColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4))),
        WIDGET_STATE_HOVER
    );
}

void UIWidget::LayoutSelectedBorderColor(Event *ev)
{
    setBorderColor(
        DeriveBorderColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4))),
        WIDGET_STATE_SELECTED
    );
}

void UIWidget::LayoutArmedBorderColor(Event *ev)
{
    setBorderColor(
        DeriveBorderColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4))),
        WIDGET_STATE_ARMED
    );
}

void UIWidget::LayoutDisabledBgColor(Event *ev)
{
    setBackgroundColor(
        UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)),
        WIDGET_STATE_DISABLED,
        true
    );
}

void UIWidget::LayoutDisabledTextShadowColor(Event *ev)
{
    setTextShadowColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_DISABLED);
}

void UIWidget::LayoutDisabledBorderColor(Event *ev)
{
    setBorderColor(
        DeriveBorderColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4))),
        WIDGET_STATE_DISABLED
    );
}

static const char *UIWidget_GameplayArg(Event *ev, int index)
{
    if (ev->NumArgs() < index) {
        ev->Error("argument %d missing", index);
        return "";
    }

    return ev->GetString(index);
}

static void UIWidget_LayoutGameplayProperty(Event *ev, UIWidget *widget, int kind)
{
    str group = UIWidget_GameplayArg(ev, 1);
    str name  = UIWidget_GameplayArg(ev, 2);

    widget->setGameplayProperty(group.c_str(), name.c_str(), kind);

    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm || !gpm->hasProperty(group, name)) {
        Com_Printf("WARNING: Couldn't find gameplay property %s.%s\n", group.c_str(), name.c_str());
    }
}

void UIWidget::LayoutGameplayStringProperty(Event *ev)
{
    UIWidget_LayoutGameplayProperty(ev, this, 0);
}

void UIWidget::LayoutGameplayFloatProperty(Event *ev)
{
    UIWidget_LayoutGameplayProperty(ev, this, 1);
}

void UIWidget::LayoutGameplayVisibilityProperty(Event *ev)
{
    UIWidget_LayoutGameplayProperty(ev, this, 2);
}

void UIWidget::LayoutGameplayShaderProperty(Event *ev)
{
    UIWidget_LayoutGameplayProperty(ev, this, 4);
}

void UIWidget::LayoutGameplayEnableProperty(Event *ev)
{
    // retail @0x0009E280 -- kind 5.  NOT kind 2: the ledger's "both visibilityProperty
    UIWidget_LayoutGameplayProperty(ev, this, 5);
}

void UIWidget::setGameplayProperty(const char *group, const char *name, int kind)
{
    m_gameplayPropertyGroup = group;
    m_gameplayPropertyName  = name;
    m_gameplayPropertyType  = kind;
    m_gameplayPropertyInUse = true;
}

void UIWidget::statVisibility(Event *ev)
{
    WidgetStatData *data = new WidgetStatData;

    data->statnum = CL_GetStatByName(ev->NumArgs() >= 1 ? ev->GetString(1) : "");
    data->value   = ev->NumArgs() >= 2 ? ev->GetInteger(2) : 0;
    data->equal   = (ev->NumArgs() >= 3 ? ev->GetInteger(3) : 0) != 0;

    m_statVisibilityData.AddObject(data);
}

void UIWidget::SetWidgetState(Event *ev)
{
    str state = ev->GetString(1);

    if (!stricmp(state, "disable")) {
        setWidgetState(WIDGET_STATE_DISABLED);
    } else if (!stricmp(state, "normal")) {
        setWidgetState(WIDGET_STATE_NORMAL);
    } else if (!stricmp(state, "hover")) {
        setWidgetState(WIDGET_STATE_HOVER);
    } else if (!stricmp(state, "selected")) {
        setWidgetState(WIDGET_STATE_SELECTED);
    } else if (!stricmp(state, "armed")) {
        setWidgetState(WIDGET_STATE_ARMED);
    }
}

void UIWidget::LayoutBorderWidth(Event *ev)
{
    m_borderWidth = ev->GetInteger(1);
    calculateDrawRegion();
}

void UIWidget::cvarCheckDisable(Event *ev)
{
    m_disableCvars.AddObject(ev->GetString(1));
    m_disableValues.AddObject(ev->GetFloat(2));

    m_disableOps.AddObject(ev->NumArgs() >= 3 ? ev->GetString(3) : "==");
}

void UIWidget::setWidgetState(WidgetState state)
{
    m_widgetState = state;
}

void UIWidget::ExecuteHoverCommand(void)
{
    if (m_hovercommand.length()) {
        uii.Cmd_Stuff((m_hovercommand + "\n").c_str());
    }
}

void UIWidget::ExecuteHoverExitCommand(void)
{
    if (m_hoverexitcommand.length()) {
        uii.Cmd_Stuff((m_hoverexitcommand + "\n").c_str());
    }
} 
void UIWidget::setNormalState(void)
{
    if (m_widgetState < WIDGET_STATE_DISABLED && m_widgetState >= WIDGET_STATE_HOVER) {
        ExecuteHoverExitCommand();
        setWidgetState(WIDGET_STATE_NORMAL);
    }
}

void UIWidget::setHoverState(void)
{
    if (m_widgetState == WIDGET_STATE_HOVER) {
        return;
    }

    if (m_widgetState >= WIDGET_STATE_HOVER) {
        if (m_widgetState >= WIDGET_STATE_DISABLED) {
            return;
        }
    } else if (m_widgetState < WIDGET_STATE_NORMAL) {
        return;
    }

    ExecuteHoverCommand();
    setWidgetState(WIDGET_STATE_HOVER);
}

void UIWidget::setSelectedState(void)
{
    if (m_widgetState == WIDGET_STATE_HOVER) {
        ExecuteHoverExitCommand();
        setWidgetState(WIDGET_STATE_SELECTED);
        return;
    }

    if (m_widgetState >= WIDGET_STATE_HOVER) {
        if (m_widgetState >= WIDGET_STATE_DISABLED) {
            return;
        }
    } else if (m_widgetState < WIDGET_STATE_NORMAL) {
        return;
    }

    setWidgetState(WIDGET_STATE_SELECTED);
}

void UIWidget::setArmedState(void)
{
    if (m_widgetState != WIDGET_STATE_SELECTED) {
        if (m_widgetState != WIDGET_STATE_HOVER) {
            return;
        }
        ExecuteHoverExitCommand();
    }

    setWidgetState(WIDGET_STATE_ARMED);
}

bool UIWidget::IsDisabled(void)
{
    for (int i = 1; i <= m_disableCvars.NumObjects(); i++) {
        cvar_t* cvar = UI_FindCvar(m_disableCvars.ObjectAt(i).c_str());

        if (!cvar) {
            continue;
        }

        const float cur = UI_GetCvarFloat(m_disableCvars.ObjectAt(i).c_str(), 0);
        const float ref = m_disableValues.ObjectAt(i);
        const str&  op  = m_disableOps.ObjectAt(i);

        bool disabled;

        if (op == "<") {
            disabled = cur < ref;
        } else if (op == ">") {
            disabled = cur > ref;
        } else if (op == "<=") {
            disabled = cur <= ref;
        } else if (op == ">=") {
            disabled = cur >= ref;
        } else if (op == "!=") {
            disabled = cur != ref;
        } else {
            disabled = cur == ref;
        }

        if (disabled) {
            return true;
        }
    }
    return false;
}

void UIWidget::UITEXTDumpTextColor(const char *accessor)
{
    static int uitext_text;
    char       table[5 * 40];
    int        used;
    int        i;

    if (!PROBE_On() || atoi(CvarGetForUI("r_2dTrace", "0")) == 0) {
        return;
    }

    if (!PROBE_Throttle(&uitext_text, 40, 400)) {
        return;
    }

    used     = m_foregroundColors[m_widgetState].set ? (int)m_widgetState : 0;
    table[0] = '\0';

    for (i = 0; i < WIDGET_STATE_COUNT; i++) {
        char row[40];

        if (m_foregroundColors[i].set) {
            Com_sprintf(
                row,
                sizeof(row),
                " e%d=%d,%d,%d,%d#1",
                i,
                (int)(m_foregroundColors[i].color.r * 255.0f + 0.5f),
                (int)(m_foregroundColors[i].color.g * 255.0f + 0.5f),
                (int)(m_foregroundColors[i].color.b * 255.0f + 0.5f),
                (int)(m_foregroundColors[i].color.a * 255.0f + 0.5f)
            );
        } else {
            Com_sprintf(row, sizeof(row), " e%d=unset", i);
        }

        Q_strcat(table, sizeof(table), row);
    }

    Com_Printf(
        "[UITEXT:uitext] accessor=%s class='%s' name='%s' state=%d fg=%d,%d,%d,%d "
        "src=e%d(%s) table=[%s ]\n",
        accessor,
        getClassname(),
        getName(),
        (int)m_widgetState,
        (int)(m_foregroundColors[used].color.r * 255.0f + 0.5f),
        (int)(m_foregroundColors[used].color.g * 255.0f + 0.5f),
        (int)(m_foregroundColors[used].color.b * 255.0f + 0.5f),
        (int)(m_foregroundColors[used].color.a * 255.0f + 0.5f),
        used,
        m_foregroundColors[m_widgetState].set ? "state-set" : "normal-fallback",
        table
    );
}

static WidgetState UIWidgetStateIndex(WidgetState state, bool set)
{
    if (!set) {
        return WIDGET_STATE_NORMAL;
    }
    return state;
}

UColor UIWidget::getStateForegroundColor(void)
{
    WidgetState state = m_widgetState;

    UITEXTDumpTextColor("getStateForegroundColor");

    return m_foregroundColors[UIWidgetStateIndex(state, m_foregroundColors[state].set)].color;
}

UColor UIWidget::getStateBackgroundColor(void)
{
    WidgetState state = m_widgetState;

    return m_backgroundColors[UIWidgetStateIndex(state, m_backgroundColors[state].set)].color;
}

UColor UIWidget::getStateShaderColor(void)
{
    WidgetState state = m_widgetState;

    if (!m_materialColors[state].set) {
        return UColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return m_materialColors[state].color;
}

UColor UIWidget::getStateTextShadowColor(void)
{
    WidgetState state = m_widgetState;

    return m_textShadowColors[UIWidgetStateIndex(state, m_textShadowColors[state].set)].color;
}

UColor UIWidget::getStateBorderColor(void)
{
    WidgetState state = m_widgetState;

    return m_borderColors[UIWidgetStateIndex(state, m_borderColors[state].set)].color.original;
}

void UIWidget::LayoutBorderStyle(Event *ev)
{
    str style = ev->GetString(1);

    if (!stricmp(style, "NONE")) {
        setBorderStyle(border_none);
        m_borderWidth = 0;
    } else if (!stricmp(style, "3D_BORDER")) {
        setBorderStyle(border_3D);
    } else if (!stricmp(style, "INDENT_BORDER")) {
        setBorderStyle(border_indent);
    } else {
        setBorderStyle(border_outline);
    }

    if (m_borderStyle && !m_borderWidth) {
        m_borderWidth = 2;
        calculateDrawRegion();
    }
}

void UIWidget::LayoutBorderColor(Event *ev)
{
    setBorderColor(DeriveBorderColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4))));
}

void UIWidget::LayoutShader(Event *ev)
{
    setMaterial(uWinMan.RegisterShader(ev->GetString(1)), WIDGET_STATE_NORMAL);
    m_flags &= ~WF_TILESHADER;
}

void UIWidget::LayoutTileShader(Event *ev)
{
    setMaterial(uWinMan.RegisterShader(ev->GetString(1)), WIDGET_STATE_NORMAL);
    m_flags |= WF_TILESHADER;
}

void UIWidget::LayoutHoverShader(Event *ev)
{
    setHoverMaterial(uWinMan.RegisterShader(ev->GetString(1)));
}

void UIWidget::LayoutArmedShader(Event *ev)
{
    setPressedMaterial(uWinMan.RegisterShader(ev->GetString(1)));
}

void UIWidget::LayoutSelectedShader(Event *ev)
{
    setMaterial(uWinMan.RegisterShader(ev->GetString(1)), WIDGET_STATE_SELECTED);
}

void UIWidget::LayoutFadeOut(Event *ev)
{
    m_fadeout = ev->GetFloat(1);
}

void UIWidget::LayoutAltClickSound(Event *ev)
{
    m_altclicksound = ev->GetString(1);
}

void UIWidget::Layout_TextureCoordinates(Event *ev)
{
    m_textureCoordinates[0] = ev->GetFloat(1);
    m_textureCoordinates[1] = ev->GetFloat(2);
    m_textureCoordinates[2] = ev->GetFloat(3);
    m_textureCoordinates[3] = ev->GetFloat(4);
}

void UIWidget::LayoutFadeIn(Event *ev)
{
    m_fadetime = ev->GetFloat(1);
    if (m_motiontime != 0.0) {
        setMotionTime(m_fadetime);
    }
}

void UIWidget::LayoutFadeDelay(Event *ev)
{
    m_fadeDelay = 1000.0f * ev->GetFloat(1);
}

void UIWidget::LayoutStopSound(Event *ev)
{
    m_stopsound = ev->GetString(1);
}

void UIWidget::LayoutClickSound(Event *ev)
{
    m_clicksound = ev->GetString(1);
}

void UIWidget::LayoutStretch(Event *ev)
{
    str stretch = ev->GetString(1);

    if (!stricmp(stretch, "vertical")) {
        m_flags |= WF_STRETCH_VERTICAL;
    } else if (!stricmp(stretch, "horizontal")) {
        m_flags |= WF_STRETCH_HORIZONTAL;
    } else if (!stricmp(stretch, "both")) {
        m_flags |= WF_STRETCH_VERTICAL | WF_STRETCH_HORIZONTAL;
    }

    m_flags |= WF_NOPARENTADJUST | WF_DIRECTED;
}


void UIWidget::LayoutInitData(Event *ev)
{
    if (m_cvarname.length()) {
        uii.Cvar_Set(m_cvarname, ev->GetString(1));
    }
}

void UIWidget::turnOn(void)
{
    if (m_cvarvalue.length()) {
        uii.Cvar_Set(m_cvarname, m_cvarvalue);
    }
}

void UIWidget::turnOff(void) {}

void UIWidget::LayoutDirection(Event *ev)
{
    str direction = ev->GetString(1);

    if (!stricmp(direction, "from_bottom")) {
        setDirection(D_FROM_BOTTOM);
    } else if (!stricmp(direction, "from_top")) {
        setDirection(D_FROM_TOP);
    } else if (!stricmp(direction, "from_left")) {
        setDirection(D_FROM_LEFT);
    } else if (!stricmp(direction, "from_right")) {
        setDirection(D_FROM_RIGHT);
    } else if (!stricmp(direction, "stretch_left")) {
        setDirection(D_STRETCH_LEFT);
    } else if (!stricmp(direction, "stretch_right")) {
        setDirection(D_STRETCH_RIGHT);
    } else if (!stricmp(direction, "stretch_down")) {
        setDirection(D_STRETCH_DOWN);
    } else if (!stricmp(direction, "stretch_up")) {
        setDirection(D_STRETCH_UP);
    } else if (!stricmp(direction, "roll_left")) {
        setDirection(D_ROLL_LEFT);
    } else if (!stricmp(direction, "roll_right")) {
        setDirection(D_ROLL_RIGHT);
    } else if (!stricmp(direction, "roll_down")) {
        setDirection(D_ROLL_DOWN);
    } else if (!stricmp(direction, "roll_up")) {
        setDirection(D_ROLL_UP);
    } else {
        setDirection(D_NONE);
    }

    if (ev->NumArgs() > 1) {
        setMotionTime(ev->GetFloat(2));
    }

    m_flags |= WF_DIRECTED;
}

void UIWidget::LayoutAlign(Event *ev)
{
    for (int i = 1; i <= ev->NumArgs(); i++) {
        str align = ev->GetString(i);

        if (!stricmp(align, "left")) {
            m_align |= WA_LEFT;
        } else if (!stricmp(align, "right")) {
            m_align |= WA_RIGHT;
        } else if (!stricmp(align, "top")) {
            m_align |= WA_TOP;
        } else if (!stricmp(align, "bottom")) {
            m_align |= WA_BOTTOM;
        } else if (!stricmp(align, "centerx")) {
            m_align |= WA_CENTERX;
        } else if (!stricmp(align, "centery")) {
            m_align |= WA_CENTERY;
        }
    }

    if (!isSubclassOf(UIWidgetContainer)) {
        m_flags |= WF_NOPARENTADJUST | WF_DIRECTED;
    }
}

void UIWidget::LayoutStuffCommand(Event *ev)
{
    LinkCommand(ev->GetString(1));
}

void UIWidget::LayoutLinkCvar(Event *ev)
{
    LinkCvar(ev->GetString(1));
}

void UIWidget::LayoutNoParentClipping(Event *ev)
{
    m_flags |= WF_DIRECTED;
}

void UIWidget::LayoutNoParentAdjustment(Event *ev)
{
    m_flags |= WF_NOPARENTADJUST;
}

void UIWidget::LayoutOrderNumber(Event *ev)
{
    m_ordernum = ev->GetInteger(1);
}


void UIWidget::LayoutHorizontalJustify(Event *ev)
{
    str just = ev->GetString(1);

    if (!stricmp(just, "left")) {
        setFontHorizontalAlignment(FONT_JUSTHORZ_LEFT);
    } else if (!stricmp(just, "right")) {
        setFontHorizontalAlignment(FONT_JUSTHORZ_RIGHT);
    } else {
        setFontHorizontalAlignment(FONT_JUSTHORZ_CENTER);
    }
}

void UIWidget::LayoutVerticalJustify(Event *ev)
{
    str just = ev->GetString(1);

    if (!stricmp(just, "top")) {
        setFontVerticalAlignment(FONT_JUSTVERT_TOP);
    } else if (!stricmp(just, "bottom")) {
        setFontVerticalAlignment(FONT_JUSTVERT_BOTTOM);
    } else {
        setFontVerticalAlignment(FONT_JUSTVERT_CENTER);
    }
}

void UIWidget::LayoutShaderColor(Event *ev)
{
    setMaterialColor(UColor(ev->GetFloat(1), ev->GetFloat(2), ev->GetFloat(3), ev->GetFloat(4)), WIDGET_STATE_NORMAL);
}

void UIWidget::LayoutHoverCommand(Event *ev)
{
    m_hovercommand = ev->GetString(1);
    m_hovercommandCol = (ev->NumArgs() > 1) ? ev->GetInteger(2) : 0;
}

void UIWidget::LayoutHoverExitCommand(Event *ev)
{
    m_hoverexitcommand = ev->GetString(1);
}

void UIWidget::SetHoverSound(Event *ev)
{
    m_hoversound = ev->GetString(1);
}

void UIWidget::SetShowSound(Event *ev)
{
    m_showsound = ev->GetString(1);
}

void UIWidget::SetHideSound(Event *ev)
{
    m_hidesound = ev->GetString(1);
}

void UIWidget::cvarVisibility(Event *ev)
{
    WidgetCvarPair *pair = new WidgetCvarPair;

    pair->cvarname = ev->GetString(1);
    pair->value    = ev->GetString(2);
    pair->type     = 0;

    if (ev->NumArgs() > 2) {
        pair->type = getCheckType(ev->GetString(3));
    }

    m_cvarVisibilityData.AddObject(pair);
}

int UIWidget::getCheckType(const str& op)
{
    if (op == "==") {
        return 0;
    }
    if (op == "=") {
        return 0;
    }
    if (op == "!=") {
        return 1;
    }
    if (op == "<") {
        return 2;
    }
    if (op == "<=") {
        return 3;
    }
    if (op == ">") {
        return 4;
    }
    if (op == ">=") {
        return 5;
    }

    return 0;
}

bool UIWidget::testCvarTypes(const Container<WidgetCvarPair *>& data)
{
    for (int i = 1; i <= data.NumObjects(); i++) {
        WidgetCvarPair *pair = data.ObjectAt(i);

        if (!pair) {
            continue;
        }

        cvar_t *cvar = uii.Cvar_Find(pair->cvarname.c_str());

        if (!cvar) {
            continue;
        }

        float cur = cvar->value;

        if (cvar->latchedString) {
            cur = atof(cvar->latchedString);
        }

        const float ref = atof(pair->value.c_str());

        switch (pair->type) {
        case 3: 
            if (cur <= ref) {
                return true;
            }
            break;
        case 5: 
            if (cur >= ref) {
                return true;
            }
            break;
        case 4: 
            if (cur > ref) {
                return true;
            }
            break;
        case 1: 
            if (cur != ref) {
                return true;
            }
            break;
        case 2: 
            if (cur < ref) {
                return true;
            }
            break;
        case 0: 
            if (cur == ref) {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

void UIWidget::LayoutAliasCache(Event *ev)
{
    int i;
    str parms;
    str realSound;

    if (ev->NumArgs() <= 1) {
        return;
    }

    for (i = 3; i <= ev->NumArgs(); i++) {
        parms += ev->GetToken(i);
        parms += " ";
    }

    realSound = ev->GetString(2);

    uii.Alias_Add(parms, realSound, parms);

    if (strstr(realSound, ".wav") || strstr(realSound, ".mp3")) {
        uii.Snd_RegisterSound(realSound, false);
    }
}

void UIWidget::setParent(UIWidget *parent)
{
    if (m_parent) {
        m_parent->removeChild(this);
    }

    m_parent = parent;

    if (m_parent) {
        m_parent->addChild(this);
    }

    PropogateCoordinateSystem();
}

UIWidget *UIWidget::getParent(void)
{
    return m_parent;
}

UIWidget *UIWidget::getFirstChild(void)
{
    if (m_children.NumObjects() > 0) {
        return m_children.ObjectAt(1);
    }

    return NULL;
}

UIWidget *UIWidget::getNextSibling(void)
{
    if (m_parent) {
        return m_parent->getNextChild(this);
    }

    return NULL;
}

UIWidget *UIWidget::getNextChild(UIWidget *child)
{
    int index = m_children.IndexOfObject(child);

    if (index && m_children.NumObjects() != index) {
        return m_children.ObjectAt(index + 1);
    }

    return NULL;
}

UIWidget *UIWidget::getPrevChild(UIWidget *child)
{
    int index;

    if (child) {
        index = m_children.IndexOfObject(child);
    } else {
        index = m_children.NumObjects();
    }

    if (index > 1) {
        return m_children.ObjectAt(index - 1);
    }

    return NULL;
}

UIWidget *UIWidget::getPrevSibling(void)
{
    if (m_parent) {
        return m_parent->getPrevChild(this);
    }

    return NULL;
}

UIWidget *UIWidget::getPrevSibling(UIWidget *curr)
{
    if (m_parent) {
        return m_parent->getPrevChild(curr);
    }

    return NULL;
}

UIWidget *UIWidget::getLastSibling(void)
{
    if (m_parent && m_children.NumObjects()) {
        UIWidget *wid = m_parent->m_children.ObjectAt(m_parent->m_children.NumObjects());
        if (wid == this) {
            return getPrevSibling();
        } else {
            return wid;
        }
    } else {
        return NULL;
    }
}

UIWidget *UIWidget::findSibling(str name)
{
    UIWidget *wid;

    for (wid = getLastSibling(); wid != NULL; wid = getPrevSibling(wid)) {
        if (wid->getName() == name) {
            return wid;
        }
    }

    return NULL;
}

void UIWidget::Enable(void)
{
    int n;
    int i;

    setShow(true);
    m_enabled = true;

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        m_children.ObjectAt(i)->Enable();
    }
}

void UIWidget::Disable(void)
{
    int n;
    int i;

    ProcessEvent(EV_Widget_Hide);
    m_enabled = false;

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        m_children.ObjectAt(i)->Disable();
    }

}

bool UIWidget::isEnabled(void)
{
    return m_enabled;
}

bool UIWidget::IsDying(void)
{
    return m_dying;
}

UIWidget *UIWidget::FindResponder(const UIPoint2D& pos)
{
    UIWidget *subview;
    UIWidget *responder;
    int       i;

    if (!m_visible || !m_clippedframe.contains(pos)) {
        return NULL;
    }

    for (i = m_children.NumObjects(); i >= 1; i--) {
        subview   = m_children.ObjectAt(i);
        responder = subview->FindResponder(pos);
        if (responder) {
            return responder;
        }
    }

    return this;
}

void UIWidget::setFont(const char *name)
{
    if (m_font) {
        delete m_font;
    }

    m_font = new UIFont(name);
}

void UIWidget::setFontHorizontalAlignment(fonthorzjustify_t alignment)
{
    m_iFontAlignmentHorizontal = alignment;
}

void UIWidget::setFontVerticalAlignment(fontvertjustify_t alignment)
{
    m_iFontAlignmentVertical = alignment;
}

bool UIWidget::isResizeable(void)
{
    const UIWidget *w = this;

    while (true) {
        if (w->m_noresize) {
            return w->m_bVirtual;
        }

        const UIWidget *parent = w->m_parent;
        if (!parent) {
            return w->m_bVirtual;
        }

        if (parent->m_noresize) {
            return parent->m_bVirtual;
        }

        w = parent;
    }
}

void UIWidget::ChangeResolution(void)
{
    int   i;
    float scalex;
    float scaley;

    if (m_mover) {
        m_mover->ChangeResolution();
    }

    if (!m_bFrameLatched) {
        m_authoredFrame = m_frame;
        m_bFrameLatched = true;
    }

    if (isResizeable()) {
        /* A widget authored in the 640x480 virtual space follows the display
           division, and its text takes it through the virtual flag. */
        scalex = uid.scaleRes[0];
        scaley = uid.scaleRes[1];

        if (m_font) {
            m_font->setTextScale(1.0f);
        }
    } else if (s_hudScaleDepth > 0) {
        /* A noresize widget is authored in screen pixels and retail never
           rescales it, so the factor applies to the authored frame alone.  The
           font is told the same factor here, where the frame is chosen, not at
           draw time: the dialogue console wraps its text from a print event, and
           a metric that changed unit between that event and the draw is what
           left a line unwrapped and a line advance doubled. */
        scalex = s_hudScaleValue;
        scaley = s_hudScaleValue;

        if (m_font) {
            m_font->setTextScale(s_hudScaleValue);
        }
    } else {
        /* No scope: the frame below stays authored, so the metrics do too. */
        if (m_font) {
            m_font->setTextScale(1.0f);
        }
        return;
    }

    float newx = (float)(int)(0.5f + scalex * m_authoredFrame.pos.x);
    float newy = (float)(int)(0.5f + scaley * m_authoredFrame.pos.y);
    float neww = (float)(int)(0.5f + (scalex * (m_authoredFrame.size.width + m_authoredFrame.pos.x) - newx));
    float newh = (float)(int)(0.5f + (scaley * (m_authoredFrame.size.height + m_authoredFrame.pos.y) - newy));

    setFrame(UIRect2D(newx, newy, neww, newh));

    m_size         = m_frame.size;
    m_startingpos  = m_frame.pos;

    for (i = 1; i <= m_children.NumObjects(); i++) {
        m_children.ObjectAt(i)->ChangeResolution();
    }
}

// PAINT TEMP PROBE -- PORT-ONLY, TEMPORARY, NEVER MERGE.  One line per widget: name,
void UIWidget::PAINTDumpState(int index, const char *menuName)
{
    if (!PROBE_On()) {
        return;
    }

    Com_Printf(
        "[PAINT:menu:widgets]   [%d] menu='%s' name='%s' class='%s' enabled=%d visible=%d "
        "depth=%.3f frame=(%.1f,%.1f %.1fx%.1f) children=%d\n",
        index,
        menuName,
        getName(),
        getClassname(),
        isEnabled() ? 1 : 0,
        getShow() ? 1 : 0,
        (double)m_depth,
        (double)m_frame.pos.x,
        (double)m_frame.pos.y,
        (double)m_frame.size.width,
        (double)m_frame.size.height,
        (int)m_children.NumObjects());
}

void UIWidget::setShow(bool visible)
{
    if (visible) {
        if (!m_visible) {
            turnOn();
            m_fadeStartTime = (float)uid.time;
            ExecuteShowCommands();
        }
    } else {
        if (m_visible) {
            turnOff();
            m_fadeGate = 0.0f;
        }
    }

    m_visible = visible;

    if (!visible) {
        if (IsThisOrChildActive()) {
            uWinMan.DeactivateCurrentSmart();
        }

    }
}

bool UIWidget::getShow(void)
{
    // nothing else.  cvarvisibility is NOT folded in here: retail evaluates it
    return m_visible;
}

void UIWidget::updateCvarVisibility(void)
{
    if (m_cvarVisibilityData.NumObjects()) {
        if (testCvarTypes(m_cvarVisibilityData)) {
            Enable();
        } else {
            Disable();
        }
    }
}

void UIWidget::update(void)
{
    int i;

    int limit_t0 = LIMIT_VW_UpdEnter();
    int limit_self;

    UI_COUNT(UI_FAM_UI, UI_UC_upd, 1);

    updateVisibility();
    updateCvarVisibility();
    updateStatVisibility();

    limit_self = LIMIT_VW_UpdSelf(limit_t0, getClassname(), getName());

    if (!m_visible) {
        LIMIT_VW_UpdExit(limit_self);
        return;
    }

    updateGameplayProperty();

    // to NORMAL (retail never restores the state it had before, it always lands on
    if (m_disableCvars.NumObjects()) {
        WidgetState state = WIDGET_STATE_NORMAL;

        if (IsDisabled()) {
            state = WIDGET_STATE_DISABLED;
        }

        if (state == WIDGET_STATE_DISABLED || m_widgetState == WIDGET_STATE_DISABLED) {
            setWidgetState(state);
        }
    }

    int n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        m_children.ObjectAt(i)->update();
    }

    LIMIT_VW_UpdExit(limit_self);
}

void UIWidget::updateVisibility(void)
{
    GameplayManager *gpm;
    float            value;

    if (!m_gameplayPropertyInUse) {
        return;
    }

    if (m_gameplayPropertyType != 2) {
        return;
    }

    gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) {
        return;
    }

    value = gpm->getFloatValue(m_gameplayPropertyGroup, m_gameplayPropertyName);

    if (value == 1.0f) {
        Enable();
    } else {
        Disable();
    }
}

void UIWidget::updateGameplayProperty(void)
{
    GameplayManager *gpm;
    str              value;

    if (!m_gameplayPropertyInUse) {
        return;
    }

    gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) {
        return;
    }

    switch (m_gameplayPropertyType) {
    case 0:
        value = gpm->getStringValue(m_gameplayPropertyGroup, m_gameplayPropertyName);

        if (value == "EMPTY") {
            value = "";
        }

        setTitle(value);
        break;

    case 1:
        {
            float f;

            f = gpm->getFloatValue(m_gameplayPropertyGroup, m_gameplayPropertyName);

            if (m_foregroundColors[m_widgetState].set) {
                m_font->setColor(m_foregroundColors[m_widgetState].color);
            } else {
                m_font->setColor(m_foregroundColors[WIDGET_STATE_NORMAL].color);
            }

            m_font->PrintJustified(
                getClientFrame(),
                m_iFontAlignmentHorizontal,
                m_iFontAlignmentVertical,
                va("%g", f), UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
        }
        break;

    case 4:
        value = gpm->getStringValue(m_gameplayPropertyGroup, m_gameplayPropertyName);

        if (value == "EMPTY") {
            m_material = NULL;
            break;
        }

        if (m_material && !stricmp(m_material->GetName().c_str(), value.c_str())) {
            break;
        }

        setMaterial(uWinMan.RefreshShader(value), WIDGET_STATE_NORMAL);
        m_flags &= ~WF_TILESHADER;
        break;

    case 5:
        {
            float f;

            f = gpm->getFloatValue(m_gameplayPropertyGroup, m_gameplayPropertyName);

            if (f == 0.0f) {
                setWidgetState(WIDGET_STATE_DISABLED);
            } else if (m_widgetState == WIDGET_STATE_DISABLED) {
                setWidgetState(WIDGET_STATE_NORMAL);
            }
        }
        break;

    default:
        break;
    }
}

void UIWidget::updateStatVisibility(void)
{
    GameplayManager *gpm;
    bool             show;
    int              n;
    int              i;

    n = m_statVisibilityData.NumObjects();
    if (!n) {
        return;
    }

    gpm = GameplayManager::getTheGameplayManager();
    if (!gpm) {
        return;
    }

    show = false;

    for (i = 1; i <= n; i++) {
        WidgetStatData *data = m_statVisibilityData.ObjectAt(i);

        if (!data || data->statnum < 0) {
            continue;
        }

        if (data->equal) {
            show = (CL_SafePlayerStat(data->statnum) == data->value);
        } else {
            show = (CL_SafePlayerStat(data->statnum) != data->value);
        }
    }

    if (show) {
        Enable();
    } else {
        Disable();
    }
}


UColor UIWidget::getBackgroundColor(void)
{
    WidgetState state = m_widgetState;

    if (!m_backgroundColors[state].set) {
        state = WIDGET_STATE_NORMAL;
    }

    return m_backgroundColors[state].color;
}

void UIWidget::setBackgroundColor(const UColor& color, WidgetState state, bool setbordercolor)
{
    m_backgroundColors[state].color = color;
    m_backgroundColors[state].set   = qtrue;

    if (setbordercolor) {
        setBorderColor(DeriveBorderColor(color), state);
    }
}

UBorderColor UIWidget::getBorderColor(void)
{
    WidgetState state = m_widgetState;

    if (!m_borderColors[state].set) {
        state = WIDGET_STATE_NORMAL;
    }

    return m_borderColors[state].color;
}

void UIWidget::setBorderColor(const UBorderColor& color)
{
    setBorderColor(color, WIDGET_STATE_NORMAL);
}

void UIWidget::setBorderColor(const UBorderColor& color, WidgetState state)
{
    m_borderColors[state].color = color;
    m_borderColors[state].set   = qtrue;
}

UColor UIWidget::getForegroundColor(void)
{
    WidgetState state = m_widgetState;

    UITEXTDumpTextColor("getForegroundColor");

    if (!m_foregroundColors[state].set) {
        state = WIDGET_STATE_NORMAL;
    }

    return m_foregroundColors[state].color;
}

void UIWidget::setForegroundColor(const UColor& color, WidgetState state)
{
    m_foregroundColors[state].color = color;
    m_foregroundColors[state].set   = qtrue;
}

UColor UIWidget::getTextShadowColor(void)
{
    WidgetState state = m_widgetState;

    if (!m_textShadowColors[state].set) {
        state = WIDGET_STATE_NORMAL;
    }

    return m_textShadowColors[state].color;
}

void UIWidget::setTextShadowColor(const UColor& color, WidgetState state)
{
    m_textShadowColors[state].color = color;
    m_textShadowColors[state].set   = qtrue;
}

UColor UIWidget::getMaterialColor(void)
{
    WidgetState state = m_widgetState;

    if (!m_materialColors[state].set) {
        return UWhite;
    }

    return m_materialColors[state].color;
}

void UIWidget::setMaterialColor(const UColor& color, WidgetState state)
{
    m_materialColors[state].color = color;
    m_materialColors[state].set   = qtrue;
}

borderstyle_t UIWidget::getBorderStyle(void)
{
    return m_borderStyle;
}

void UIWidget::setBorderStyle(borderstyle_t style)
{
    m_borderStyle = style;
}

UISize2D UIWidget::getSize(void)
{
    return m_size;
}

void UIWidget::setSize(const UISize2D& newsize)
{
    if (newsize.width != m_frame.size.width || newsize.height != m_frame.size.height) {
        m_frame.size = newsize;
        PropogateCoordinateSystem();
        SendSignal(W_SizeChanged);
    }
}

UIRect2D UIWidget::getFrame(void)
{
    return m_frame;
}

void UIWidget::setFrame(const UIRect2D& newframe)
{
    bool bNewSize;

    bNewSize = newframe.size != m_frame.size;

    m_frame = newframe;

    PropogateCoordinateSystem();
    calculateDrawRegion();
    SendSignal(W_FrameChanged);

    if (bNewSize) {
        SendSignal(W_SizeChanged);
    }
}

UIPoint2D UIWidget::getOrigin(void)
{
    return m_origin;
}

void UIWidget::setOrigin(const UIPoint2D& neworigin)
{
    if (neworigin != m_origin) {
        m_origin = neworigin;
        PropogateCoordinateSystem();
    }
}

void UIWidget::setName(str name)
{
    m_name = name;
}

const char *UIWidget::getName(void)
{
    return m_name;
}

void UIWidget::setTitle(str title)
{
    if (isStringResource(title.c_str())) {
        m_rawtitle = title;
        m_title = getStringResource(title.c_str());
    } else {
        m_title = title;
    }
}

void UIWidget::setLabelText(str text)
{
    int i;

    if (isStringResource(text.c_str())) {
        m_rawtitle = text;
        m_title = getStringResource(text.c_str());
        return;
    }

    m_labelText = text;

    for (i = 0; i < (int)m_labelText.length(); i++) {
        if (m_labelText[i] == '~') {
            m_labelText[i] = '\n';
        }
        if (m_labelText[i] == '^') {
            m_labelText[i] = ' ';
        }
    }
}

void UIWidget::reloadStrings(void)
{
    if (isStringResource(m_rawtitle.c_str())) {
        setTitle(getStringResource(m_rawtitle.c_str()));
    }
}

const char *UIWidget::getTitle(void)
{
    return m_title;
}

const char *UIWidget::getCvarName(void)
{
    return m_cvarname;
}

void UIWidget::setDirection(direction_t dir)
{
    m_direction      = dir;
    m_direction_orig = dir;
}

direction_t UIWidget::getDirection(void)
{
    return m_direction;
}

void UIWidget::setMotionType(motion_t type)
{
    m_motiontype = type;
}

motion_t UIWidget::getMotionType(void)
{
    return m_motiontype;
}

void UIWidget::setMotionTime(float time)
{
    m_motiontime = time;
}

float UIWidget::getMotionTime(void)
{
    return m_motiontime;
}

void UIWidget::setAlign(int align)
{
    m_align = align;
}

int UIWidget::getAlign(void)
{
    return m_align;
}

void UIWidget::setMaterial(UIReggedMaterial *mat, WidgetState state)
{
    if (!mat) {
        return;
    }

    switch (state) {
    case WIDGET_STATE_HOVER:
        m_hovermaterial = mat;
        break;
    case WIDGET_STATE_SELECTED:
        m_selectedmaterial = mat;
        break;
    case WIDGET_STATE_ARMED:
        m_pressedmaterial = mat;
        break;
    case WIDGET_STATE_DISABLED:
        m_disabledmaterial = mat;
        break;
    default:
        m_material = mat;
        break;
    }
}

void UIWidget::setHoverMaterial(UIReggedMaterial *mat)
{
    m_hovermaterial = mat;
}

void UIWidget::SetChildWidget(Event *ev)
{
    if (ev->NumArgs() != 1) {
        return;
    }

    str name = ev->GetString(1);

    UIWidget *child = NULL;

    if (m_parent && m_parent->m_children.NumObjects()) {
        child = m_parent->m_children.ObjectAt(m_parent->m_children.NumObjects());

        if (child == this) {
            int index = m_parent->m_children.IndexOfObject(this);

            child = (index > 1) ? m_parent->m_children.ObjectAt(index - 1) : NULL;
        }
    }

    while (child) {
        if (!strcmp(child->m_name.c_str(), name.c_str())) {
            break;
        }

        if (m_parent) {
            int index = m_parent->m_children.IndexOfObject(child);

            child = (index > 1) ? m_parent->m_children.ObjectAt(index - 1) : NULL;
        } else {
            child = NULL;
        }
    }

    if (!child) {
        return;
    }

    if (child->m_parent) {
        UIWidget *oldparent = child->m_parent;

        if (oldparent->m_children.ObjectInList(child)) {
            int index = oldparent->m_children.IndexOfObject(child);

            oldparent->m_children.ObjectAt(index) = NULL;
            oldparent->m_children.RemoveObjectAt(index);
        }
    }

    child->m_parent = this;

    if (!m_children.ObjectInList(child)) {
        m_children.AddObject(child);
    }

    child->PropogateCoordinateSystem();
}

void UIWidget::MouseEntered(Event *ev)
{
    WidgetState state = m_widgetState;

    if (state != WIDGET_STATE_SELECTED && state != WIDGET_STATE_HOVER) {
        if (state < WIDGET_STATE_DISABLED && state >= WIDGET_STATE_NORMAL) {
            ExecuteHoverCommand();
            setWidgetState(WIDGET_STATE_HOVER);
        }
    }

    if (m_parent && ev->GetSourceObject() != m_parent && m_parent != &uWinMan) {
        m_parent->MouseEntered(ev);
    }

    if (m_hoversound.length()) {
        uii.Snd_PlaySound(m_hoversound.c_str(), CHAN_MENU);  
    }
}

void UIWidget::MouseExited(Event *ev)
{
    WidgetState state = m_widgetState;

    if (state < WIDGET_STATE_DISABLED && state >= WIDGET_STATE_HOVER) {
        ExecuteHoverExitCommand();
        setWidgetState(WIDGET_STATE_NORMAL);
    }

    if (m_parent && m_parent != &uWinMan) {
        m_parent->MouseExited(ev);
    }
}

void UIWidget::Pressed(Event *ev)
{
    if (m_widgetState == WIDGET_STATE_DISABLED) {
        return;
    }

    uWinMan.ActivateControl(this);

    if (m_widgetState == WIDGET_STATE_HOVER) {
        ExecuteHoverExitCommand();
    }

    if (m_widgetState >= WIDGET_STATE_HOVER && m_widgetState < WIDGET_STATE_DISABLED) {
        setWidgetState(WIDGET_STATE_SELECTED);
    }

    ev->SetSourceObject(this);

    if (m_parent && m_parent != &uWinMan) {
        m_parent->Pressed(ev);
    }

    if (m_clicksound.length()) {
        uii.Snd_PlaySound(m_clicksound.c_str(), CHAN_MENU2);
    }
}

void UIWidget::RightMouseButtonPressed(Event *ev)
{
    ExecuteAltCommand();

    if (m_altclicksound.length()) {
        uii.Snd_PlaySound(m_altclicksound.c_str(), CHAN_MENU2);
    }
}

int UIWidget::getNumItems(void)
{
    return 0;
}

void UIWidget::PasteFromWindowsClipboard(void) {}

void UIWidget::setData(const str& data)
{
    //   0xA53B8  clrlwi. r0, r0, 0x18 / beq 0xA53CC    ; NOT equal -> register
    // sentinel: Scoreboard::addIcons @0x001CE490 appends the literal `"EMPTY" `
    // NOTE the sentinel arm writes the slot DIRECTLY and does not call
    if (!strcmp(data.c_str(), "EMPTY")) {
        m_material = NULL;
    } else {
        setMaterial(uWinMan.RegisterShader(data), WIDGET_STATE_NORMAL);
    }

    m_flags &= ~8u;
}

const char *UIWidget::getData(void)
{
    return "";
}

// DELETED here (they were never retail's).  Retail also does not store +1048
void UIWidget::copyProperties(UIWidget& other)
{
    int i;

    if (m_font) {
        UIFont *cloned = new UIFont();
        cloned->copyFrom(m_font);
        if (other.m_font) {
            delete other.m_font;
        }
        other.m_font = cloned;
    }

    other.setForegroundColor(m_foregroundColors[WIDGET_STATE_NORMAL].color, WIDGET_STATE_NORMAL);
    for (i = 1; i < 4; i++) {
        if (m_foregroundColors[i].set) {
            other.setForegroundColor(m_foregroundColors[i].color, (WidgetState)i);
        }
    }

    for (i = 0; i < 4; i++) {
        if (m_textShadowColors[i].set) {
            other.setTextShadowColor(m_textShadowColors[i].color, (WidgetState)i);
        }
    }

    other.setBackgroundColor(m_backgroundColors[WIDGET_STATE_NORMAL].color, WIDGET_STATE_NORMAL, true);
    for (i = 1; i < 4; i++) {
        if (m_backgroundColors[i].set) {
            other.setBackgroundColor(m_backgroundColors[i].color, (WidgetState)i, true);
        }
    }

    other.setBorderColor(m_borderColors[WIDGET_STATE_NORMAL].color, WIDGET_STATE_NORMAL);
    for (i = 1; i < 4; i++) {
        if (m_borderColors[i].set) {
            other.setBorderColor(m_borderColors[i].color, (WidgetState)i);
        }
    }

    other.m_borderStyle = m_borderStyle;
    other.m_borderWidth = m_borderWidth;
    other.calculateDrawRegion();

    // slot leaves the destination's slot alone -- retail does NOT copy the
    other.setBackgroundAlpha(m_alpha);

    UIReggedMaterial *sourceMaterials[4] = {m_material, m_hovermaterial, m_selectedmaterial, m_pressedmaterial};
    for (i = 0; i < 4; i++) {
        other.setMaterial(sourceMaterials[i], (WidgetState)i);
    }

    other.m_align = m_align;

    other.m_direction      = m_direction;
    other.m_direction_orig = m_direction;

    other.setFrame(m_frame);

    other.setLabelText(m_labelText);

    other.m_motiontype = m_motiontype;
    other.m_motiontime = m_motiontime;

    if (other.m_origin.x != m_origin.x || other.m_origin.y != m_origin.y) {
        other.m_origin = m_origin;
        other.PropogateCoordinateSystem();
    }

    other.setTitle(m_title);

    other.m_iFontAlignmentHorizontal = m_iFontAlignmentHorizontal;
    other.m_iFontAlignmentVertical   = m_iFontAlignmentVertical;

    other.m_cvarname    = m_cvarname;
    other.m_command     = m_command;
    other.m_hoversound  = m_hoversound;
    other.m_clicksound  = m_clicksound;
}

void UIWidget::refreshShader(Event *ev)
{
    UIReggedMaterial *mat;

    switch (m_widgetState) {
    case WIDGET_STATE_HOVER:
        mat = m_hovermaterial;
        break;
    case WIDGET_STATE_SELECTED:
        mat = m_selectedmaterial;
        break;
    case WIDGET_STATE_ARMED:
        mat = m_pressedmaterial;
        break;
    case WIDGET_STATE_DISABLED:
        mat = m_disabledmaterial;
        break;
    default:
        mat = m_material;
        break;
    }

    if (!mat) {
        mat = m_material;
    }

    if (mat) {
        setMaterial(uWinMan.RefreshShader(mat->GetName()), m_widgetState);
    }
}

void UIWidget::SetAltCommand(Event *ev)
{
    m_altcommand = ev->GetString(1);
}

void UIWidget::ExecuteAltCommand(void)
{
    if (m_altcommand.length()) {
        uii.Cmd_Stuff((m_altcommand + "\n").c_str());
    }
}

void UIWidget::setPressedMaterial(UIReggedMaterial *mat)
{
    m_pressedmaterial = mat;
}

UIPoint2D UIWidget::getLocalPoint(const UIPoint2D& pos)
{
    if (m_parent) {
        return UIPoint2D(pos.x - m_screenframe.pos.x, pos.y - m_screenframe.pos.y);
    } else {
        return pos;
    }
}

UIPoint2D UIWidget::getGlobalPoint(const UIPoint2D& pos)
{
    if (m_parent) {
        return UIPoint2D(pos.x + m_screenframe.pos.x, pos.y + m_screenframe.pos.y);
    } else {
        return pos;
    }
}

void UIWidget::setBackgroundAlpha(float f)
{
    m_alpha = f;
}

float UIWidget::getBackgroundAlpha(void)
{
    return m_alpha;
}

void UIWidget::setHudScale(float scale)
{
    m_hudScale    = scale;
    m_hudScaleSet = true;
}

float UIWidget::getHudScale(void) const
{
    return m_hudScale;
}

bool UIWidget::isHudScaleSet(void) const
{
    return m_hudScaleSet;
}

/* A HUD menu authored across the whole 640x480 virtual screen is an overlay:
   its edges and its widget geometry stay where retail puts them so nothing is
   pushed past the screen, and the factor reaches its text through the draw
   scope.  A smaller HUD panel has room to grow, so its geometry scales too and
   keeps the corner its `align' declares. */
bool UIWidget::isHudFullScreen(void) const
{
    return m_authoredFrame.size.width >= 640.0f && m_authoredFrame.size.height >= 480.0f;
}

/*
 * The HUD scope carries the factor and deliberately does NOT multiply
 * `uid.scaleRes'.  A HUD element is authored `noresize', so `uid.scaleRes' is
 * exactly the division noresize exists to ignore: its authored frame is already
 * in screen pixels.  Multiplying the shared virtual space instead left the
 * geometry alone -- `ChangeResolution' returns early for a noresize widget --
 * while `uid.scaleRes' still reached the resolution-scaled widgets that read it,
 * which rescaled the inventory bar a second time and pushed it off the screen.
 * The factor is therefore applied once, to the authored frame of a noresize
 * widget, and to the text drawn inside the scope.
 */
void UIWidget::HudScalePush(float scale)
{
    if (s_hudScaleDepth == 0) {
        s_hudScaleValue = scale;
    }

    s_hudScaleDepth++;
}

void UIWidget::HudScalePop(void)
{
    if (s_hudScaleDepth <= 0) {
        return;
    }

    s_hudScaleDepth--;

    if (s_hudScaleDepth == 0) {
        s_hudScaleValue = 1.0f;
    }
}

float UIWidget::HudScaleCurrent(void)
{
    return s_hudScaleDepth > 0 ? s_hudScaleValue : 1.0f;
}

float UIWidget::HudTextScale(void)
{
    return HudScaleCurrent();
}

void UIWidget::Display(const UIRect2D& drawframe, float parent_alpha)
{
    int    i;
    vec4_t col = {1, 1, 1, 1};

    int    limit_t0 = LIMIT_VW_Enter();
    int    limit_self;

    UI_COUNT(UI_FAM_UI, UI_UC_disp, 1);
    UI_COUNT(UI_FAM_DISP, UI_DC_disps, 1);

    // shown but disabled draws in retail and did not draw here.  Nothing depends on it
    if (!getShow()) {
        LIMIT_VW_Exit(LIMIT_VW_Self(limit_t0, getClassname(), getName()));
        return;
    }

    if (m_hideDuringLoad && menuManager.getLoading()) {
        LIMIT_VW_Exit(LIMIT_VW_Self(limit_t0, getClassname(), getName()));
        return;
    }

    /* A HUD menu's subtree is the only place the HUD factor reaches text and
       the other draw-time metrics.  The scope covers this widget and every
       child it recurses into, and is popped before anything else draws. */
    bool bHudScale = m_hudScaleSet;

    if (bHudScale) {
        HudScalePush(m_hudScale);
    }

    if (m_direction || m_fadetime != 0.0f || m_bMoveActive) {
        Motion();
    }

    m_local_alpha = m_alpha * parent_alpha;
    set2D();

    col[3] = m_local_alpha;
    uii.Rend_SetColor(col);

    UColor bgColor = getStateBackgroundColor();
    if (bgColor.a != 0.0) {
        DrawBox(0, 0, m_frame.size.width, m_frame.size.height, bgColor, bgColor.a * m_local_alpha);
    }

    if (m_borderStyle == border_outline) {
        DrawBoxWithSolidBorder(
            UIRect2D(0, 0, m_frame.size.width, m_frame.size.height),
            UWhite,
            getStateBorderColor(),
            m_borderWidth,
            2,
            m_local_alpha
        );
    } else if (m_borderStyle) {
        WidgetState borderState = m_widgetState;

        const UBorderColor& stateBorder =
            m_borderColors[UIWidgetStateIndex(borderState, m_borderColors[borderState].set)].color;

        Draw3DBox(
            0, 0, m_frame.size.width, m_frame.size.height, m_borderStyle == border_indent, stateBorder, m_local_alpha
        );
    }

    UIReggedMaterial *statematerial;

    switch (m_widgetState) {
    case WIDGET_STATE_DISABLED:
        statematerial = m_disabledmaterial;
        break;
    case WIDGET_STATE_ARMED:
        statematerial = m_pressedmaterial;
        break;
    case WIDGET_STATE_SELECTED:
        statematerial = m_selectedmaterial;
        break;
    case WIDGET_STATE_HOVER:
        statematerial = m_hovermaterial;
        break;
    default:
        statematerial = m_material;
        break;
    }

    if (!statematerial) {
        statematerial = m_material;
    }

    if (statematerial) {

        UColor shaderColor = getStateShaderColor();
        col[0] = shaderColor.r;
        col[1] = shaderColor.g;
        col[2] = shaderColor.b;
        col[3] = shaderColor.a * m_local_alpha;
        uii.Rend_SetColor(col);

        if (m_flags & WF_TILESHADER) {
            if (m_bVirtual) {
                float fvWidth =
                    m_frame.size.width / uid.scaleRes[0] / uii.Rend_GetShaderWidth(statematerial->GetMaterial());
                float fvHeight =
                    m_frame.size.height / uid.scaleRes[1] / uii.Rend_GetShaderHeight(statematerial->GetMaterial());

                uii.Rend_DrawPicStretched(
                    0, 0, m_frame.size.width, m_frame.size.height, 0, 0, fvWidth, fvHeight, statematerial->GetMaterial()
                );
            } else {
                uii.Rend_DrawPicTiled(0, 0, m_frame.size.width, m_frame.size.height, statematerial->GetMaterial());
            }
        } else {
            uii.Rend_DrawPicStretched(
                0, 0, m_frame.size.width, m_frame.size.height, 0, 0, 1, 1, statematerial->GetMaterial()
            );
        }
    }


    Draw();

    limit_self = LIMIT_VW_Self(limit_t0, getClassname(), getName());

    for (i = 1; i <= m_children.NumObjects(); i++) {
        UIWidget *child = m_children.ObjectAt(i);

        if (child->m_visible) {
            child->Display(m_frame, m_local_alpha);
        }
    }

    if (bHudScale) {
        HudScalePop();
    }

    LIMIT_VW_Exit(limit_self);
}

qboolean UIWidget::KeyEvent(int key, unsigned int time)
{
    int i;

    for (i = 1; i <= m_keyCommands.NumObjects(); i++) {
        WidgetKeyCommand *keycommand = m_keyCommands.ObjectAt(i);

        if (keycommand->key == key) {
            uii.Cmd_Stuff(keycommand->command.c_str());
            return true;
        }
    }

    if (m_parent && m_parent != &uWinMan) {
        return m_parent->KeyEvent(key, time);
    }

    return false;
}

void UIWidget::KeyCommand(Event *ev)
{
    WidgetKeyCommand *keycommand = new WidgetKeyCommand;

    keycommand->key = atoi(ev->GetString(1));

    keycommand->command = ev->GetString(2);

    m_keyCommands.AddObject(keycommand);

    m_canactivate = true;
}

void UIWidget::CharEvent(int ch) {}

void UIWidget::UpdateData(void) {}

void UIWidget::UpdateUIElement(void) {}

float UIWidget::getTitleWidth(void)
{
    return m_font->getWidth(getTitle(), -1, m_bVirtual);
}

float UIWidget::getTitleHeight(void)
{
    return m_font->getHeight(m_bVirtual);
}

bool UIWidget::CanActivate(void)
{
    if (!m_canactivate || m_dying) {
        return false;
    }

    for (UIWidget *wid = this; wid != NULL; wid = wid->getParent()) {
        if (!wid->getShow()) {
            return false;
        }
    }

    return true;
}

void UIWidget::AllowActivate(bool canactivate)
{
    m_canactivate = canactivate;
}

bool UIWidget::IsActive(void)
{
    return uWinMan.ActiveControl() == this;
}

bool UIWidget::IsVisible(void)
{
    return m_visible;
}

void UIWidget::LinkCvar(str cvarname)
{
    m_cvarname = cvarname;
}

void UIWidget::LinkCommand(str cmd)
{
    m_command = cmd;
}

void UIWidget::ExecuteShowCommands(void)
{
    if (m_showsound.length()) {
        uii.Snd_PlaySound(m_showsound.c_str(), CHAN_MENU);
    }

    for (int i = 1; i <= m_showcommands.NumObjects(); i++) {
        uii.Cmd_Stuff(m_showcommands.ObjectAt(i)->c_str());
    }
}

void UIWidget::ExecuteHideCommands(void)
{
    if (m_hidesound.length()) {
        uii.Snd_PlaySound(m_hidesound.c_str(), CHAN_MENU);
    }

    for (int i = 1; i <= m_hidecommands.NumObjects(); i++) {
        uii.Cmd_Stuff(m_hidecommands.ObjectAt(i)->c_str());
    }
}

void UIWidget::ExecutePushCommands(void)
{
    int i, j, k, m, n, ii, jj, kk, mm, nn;

    for (i = 1; i <= m_pushcommands.NumObjects(); i++) {
        uii.Cmd_Stuff(m_pushcommands.ObjectAt(i)->c_str());
    }

    for (j = 1; j <= m_children.NumObjects(); j++) {
        UIWidget *c1 = m_children.ObjectAt(j);

        for (k = 1; k <= c1->m_pushcommands.NumObjects(); k++) {
            uii.Cmd_Stuff(c1->m_pushcommands.ObjectAt(k)->c_str());
        }

        for (m = 1; m <= c1->m_children.NumObjects(); m++) {
            UIWidget *c2 = c1->m_children.ObjectAt(m);

            for (n = 1; n <= c2->m_pushcommands.NumObjects(); n++) {
                uii.Cmd_Stuff(c2->m_pushcommands.ObjectAt(n)->c_str());
            }

            for (ii = 1; ii <= c2->m_children.NumObjects(); ii++) {
                UIWidget *c3 = c2->m_children.ObjectAt(ii);

                for (jj = 1; jj <= c3->m_pushcommands.NumObjects(); jj++) {
                    uii.Cmd_Stuff(c3->m_pushcommands.ObjectAt(jj)->c_str());
                }

                for (kk = 1; kk <= c3->m_children.NumObjects(); kk++) {
                    UIWidget *c4 = c3->m_children.ObjectAt(kk);

                    for (mm = 1; mm <= c4->m_pushcommands.NumObjects(); mm++) {
                        uii.Cmd_Stuff(c4->m_pushcommands.ObjectAt(mm)->c_str());
                    }

                    for (nn = 1; nn <= c4->m_children.NumObjects(); nn++) {
                        c4->m_children.ObjectAt(nn)->ExecutePushCommands();
                    }
                }
            }
        }
    }
}

void UIWidget::ExecutePopCommands(void)
{
    int i, j, k, m, n, ii, jj, kk, mm, nn;

    for (i = 1; i <= m_popcommands.NumObjects(); i++) {
        uii.Cmd_Stuff(m_popcommands.ObjectAt(i)->c_str());
    }

    for (j = 1; j <= m_children.NumObjects(); j++) {
        UIWidget *c1 = m_children.ObjectAt(j);

        for (k = 1; k <= c1->m_popcommands.NumObjects(); k++) {
            uii.Cmd_Stuff(c1->m_popcommands.ObjectAt(k)->c_str());
        }

        for (m = 1; m <= c1->m_children.NumObjects(); m++) {
            UIWidget *c2 = c1->m_children.ObjectAt(m);

            for (n = 1; n <= c2->m_popcommands.NumObjects(); n++) {
                uii.Cmd_Stuff(c2->m_popcommands.ObjectAt(n)->c_str());
            }

            for (ii = 1; ii <= c2->m_children.NumObjects(); ii++) {
                UIWidget *c3 = c2->m_children.ObjectAt(ii);

                for (jj = 1; jj <= c3->m_popcommands.NumObjects(); jj++) {
                    uii.Cmd_Stuff(c3->m_popcommands.ObjectAt(jj)->c_str());
                }

                for (kk = 1; kk <= c3->m_children.NumObjects(); kk++) {
                    UIWidget *c4 = c3->m_children.ObjectAt(kk);

                    for (mm = 1; mm <= c4->m_popcommands.NumObjects(); mm++) {
                        uii.Cmd_Stuff(c4->m_popcommands.ObjectAt(mm)->c_str());
                    }

                    for (nn = 1; nn <= c4->m_children.NumObjects(); nn++) {
                        c4->m_children.ObjectAt(nn)->ExecutePopCommands();
                    }
                }
            }
        }
    }
}

void UIWidget::InitializeCommandHandler(void (*fcn)(const char *, void *))
{
    m_commandhandler = fcn;
}

UIWidget *UIWidget::FindWidget(str name)
{
    int i;
    int n;

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        UIWidget *wid = m_children.ObjectAt(i);
        if (wid->getName() == name) {
            return wid;
        }

        UIWidget *w = wid->FindWidget(name);
        if (w) {
            return w;
        }
    }

    return NULL;
}

void UIWidget::ResetMotion(motion_t type)
{
    AlignPosition();

    m_direction  = m_direction_orig;
    m_motiontype = type;

    if (type == MOTION_IN) {
        switch (m_direction) {
        case D_FROM_BOTTOM:
            setFrame(UIRect2D(m_frame.pos.x, (float)uid.vidHeight, m_frame.size.width, m_frame.size.height));
            break;
        case D_FROM_TOP:
            setFrame(UIRect2D(
                m_frame.pos.x,
                -m_frame.size.height - m_parent->m_screenorigin.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_FROM_LEFT:
            setFrame(UIRect2D(
                -m_frame.size.width - m_parent->m_screenorigin.x,
                m_frame.pos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_FROM_RIGHT:
            setFrame(UIRect2D((float)uid.vidWidth, m_frame.pos.y, m_frame.size.width, m_frame.size.height));
            break;
        case D_STRETCH_LEFT:
            setSize(UISize2D(0.0f, m_size.height));
            setFrame(UIRect2D(
                m_startingpos.x + m_size.width,
                m_startingpos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_STRETCH_RIGHT:
            setSize(UISize2D(0.0f, m_size.height));
            setFrame(m_frame);
            break;
        case D_STRETCH_DOWN:
            setSize(UISize2D(m_size.width, 0.0f));
            setFrame(UIRect2D(
                m_startingpos.x + m_size.width,
                m_startingpos.y + m_size.height,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_STRETCH_UP:
            setSize(UISize2D(m_size.width, 0.0f));
            setFrame(m_frame);
            break;
        case D_ROLL_LEFT:
            m_textureCoordinates[0] = 1.0f;
            setSize(UISize2D(0.0f, m_size.height));
            setFrame(UIRect2D(
                m_startingpos.x + m_size.width,
                m_startingpos.y,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_ROLL_RIGHT:
            m_textureCoordinates[2] = 0.0f;
            setSize(UISize2D(0.0f, m_size.height));
            setFrame(m_frame);
            break;
        case D_ROLL_UP:
            m_textureCoordinates[1] = 1.0f;
            setSize(UISize2D(m_size.width, 0.0f));
            setFrame(UIRect2D(
                m_startingpos.x + m_size.width,
                m_startingpos.y + m_size.height,
                m_frame.size.width,
                m_frame.size.height
            ));
            break;
        case D_ROLL_DOWN:
            m_textureCoordinates[3] = 0.0f;
            setSize(UISize2D(m_size.width, 0.0f));
            setFrame(m_frame);
            break;
        default:
            setFrame(m_frame);
            FrameInitialized();
            break;
        }
    }

    m_starttime = (float)uid.time / 1000.0f;
}

void UIWidget::Realign(void)
{
    if (m_flags & WF_STRETCH_VERTICAL) {
        m_frame.pos.y       = 0;
        m_frame.size.height = uid.vidHeight;
    }
    if (m_flags & WF_STRETCH_HORIZONTAL) {
        m_frame.pos.x      = 0;
        m_frame.size.width = uid.vidWidth;
    }

    if (m_align & WA_CENTERX) {
        m_frame.pos.x = uid.vidWidth * 0.5 - m_frame.size.width * 0.5;
    }

    if (m_align & WA_RIGHT) {
        m_frame.pos.x = uid.vidWidth - m_frame.size.width;
    } else if (m_align & WA_LEFT) {
        m_frame.pos.x = 0;
    }

    if (m_align & WA_CENTERY) {
        m_frame.pos.y = uid.vidHeight * 0.5 - m_frame.size.height * 0.5;
    }

    if (m_align & WA_BOTTOM) {
        m_frame.pos.y = uid.vidHeight - m_frame.size.height;
    } else if (m_align & WA_TOP) {
        m_frame.pos.y = 0;
    }

    if ((m_align & WA_FULL) || (m_flags & (WF_STRETCH_HORIZONTAL | WF_STRETCH_VERTICAL))) {
        setFrame(m_frame);
        m_startingpos = m_frame.pos;
    }
}

void UIWidget::BringToFrontPropogated(void)
{
    for (UIWidget *at = this; at->getParent() != NULL; at = at->getParent()) {
        Container<UIWidget *>& siblings = at->getParent()->m_children;

        if (siblings.NumObjects() > 1 && siblings.ObjectAt(siblings.NumObjects()) != at) {
            siblings.RemoveObject(at);
            siblings.AddObject(at);
        }
    }

    uWinMan.sortWidgetContainers();

    UIWidget *painttop = this;

    while (painttop->getParent() != NULL && painttop->getParent() != &uWinMan) {
        painttop = painttop->getParent();
    }

    int paintindex = 0;
    int painttotal = uWinMan.GetChildCount();

    for (int painti = 1; painti <= painttotal; painti++) {
        if (uWinMan.GetChild(painti) == painttop) {
            paintindex = painti;
            break;
        }
    }

    PAINT_PROBE("raise:order",
               "name='%s' class='%s' depth=%.3f top='%s' managerIndex=%d of=%d\n",
               getName() ? getName() : "?",
               getClassname() ? getClassname() : "?",
               (double)m_depth,
               painttop->getName() ? painttop->getName() : "?",
               paintindex,
               painttotal);
}

UIWidget *UIWidget::IsThisOrChildActive(void)
{
    UIWidget *child = this;

    if (child->IsActive()) {
        return child;
    }

    for (int i = 1; i <= m_children.NumObjects(); i++) {
        child = m_children.ObjectAt(i);
        if (child->IsThisOrChildActive()) {
            return child;
        }
    }

    return NULL;
}

UIPoint2D UIWidget::MouseEventToClientPoint(Event *ev)
{
    UIPoint2D p;

    p.x = ev->GetFloat(1) - m_screenframe.pos.x;
    p.y = ev->GetFloat(2) - m_screenframe.pos.y;

    return p;
}

UIRect2D UIWidget::getClientFrame(void)
{
    return UIRect2D(UIPoint2D(0, 0), m_frame.size);
}

bool UIWidget::SendSignal(Event& event)
{
    if (uWinMan.IsDead()) {
        return false;
    } else {
        return USignal::SendSignal(event);
    }
}

void UIWidget::ShowCommand(Event *ev)
{
    int  i;
    str *command = new str;

    for (i = 1; i <= ev->NumArgs(); i++) {
        *command += ev->GetToken(i);
    }

    m_showcommands.AddObject(command);
}

void UIWidget::HideCommand(Event *ev)
{
    int  i;
    str *command = new str;

    for (i = 1; i <= ev->NumArgs(); i++) {
        *command += ev->GetToken(i);
    }

    m_hidecommands.AddObject(command);
}

void UIWidget::PushCommand(Event *ev)
{
    str *command = new str;

    *command = ev->GetString(1);

    m_pushcommands.AddObject(command);
}

void UIWidget::PopCommand(Event *ev)
{
    str *command = new str;

    *command = ev->GetString(1);

    m_popcommands.AddObject(command);
}

void UIWidget::AddFlag(int flag)
{
    m_flags |= flag;
}

int UIWidget::getOrderNum(void)
{
    return m_ordernum;
}

str UIWidget::getCommand(void)
{
    return m_command;
}

void UIWidget::ActivateOrder(void)
{
    UIWidget *selwidget;

    for (selwidget = this; selwidget != NULL; selwidget = selwidget->getParent()) {
        if (selwidget->getParent() == &uWinMan) {
            if (selwidget->isSubclassOf(UIWidgetContainer)) {
                UIWidgetContainer *widcon = (UIWidgetContainer *)selwidget;
                widcon->SetActiveWidgetOrderNum(this);
            }

            return;
        }
    }
}

void UIWidget::EnableEvent(Event *ev)
{
    Enable();
}

void UIWidget::DisableEvent(Event *ev)
{
    Disable();
}

void UIWidget::setConfigstringIndex(int cs)
{
    m_configstring_index = cs;
}

int UIWidget::getConfigstringIndex(void)
{
    return m_configstring_index;
}

bool UIWidget::PassEventToWidget(str name, Event *ev, qboolean deleteOnFail)
{
    int i;
    int n;

    // NOTE: do NOT early-out on !isEnabled() here. This function delivers a NAMED
    if (!str::cmp(name, m_name)) {
        PROBE("inv:deliver", "widget='%s' event='%s' HIT", name.c_str(), ev->getName());
        BOT_PROBE("sb:owner",
                   "widget='%s' event='%s' parent='%s' iscontainer=%d enabled=%d visible=%d numchildren=%d",
                   name.c_str(), ev->getName(),
                   m_parent ? m_parent->getName() : "(root)", m_parent ? 0 : 1,
                   m_enabled ? 1 : 0, m_visible ? 1 : 0, m_children.NumObjects());
        ProcessEvent(ev);
        return true;
    }

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        if (m_children.ObjectAt(i)->PassEventToWidget(name, ev, qfalse)) {
            return true;
        }
    }

    if (deleteOnFail && ev) {
        delete ev;
    }

    return false;
}

UIFont *UIWidget::getFont() const
{
    return m_font;
}


// propagation is deliberately one level deep in retail, not recursive.
void UIWidget::setDepth(Event *ev)
{
    int i;
    int n;

    m_depth = ev->GetFloat(1);

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        m_children.ObjectAt(i)->m_depth = m_depth;
    }
}

void UIWidget::NoResize(Event *ev)
{
    int i;
    int n;

    m_bVirtual = false;
    m_noresize = true;

    n = m_children.NumObjects();
    for (i = 1; i <= n; i++) {
        m_children.ObjectAt(i)->NoResize(ev);
    }
}

void UIWidget::setHideDuringLoad(Event *ev)
{
    m_hideDuringLoad = true;
}

// whose algorithm is not retail's, and the menu container and view3d -- both at depth 1.0
int UIWidget::DepthCompare(const void *elem1, const void *elem2)
{
    const float a = (*(UIWidget *const *)elem1)->m_depth;
    const float b = (*(UIWidget *const *)elem2)->m_depth;

    if (a < b) {
        return 1;
    }
    return -1;
}


void UIWidgetContainer::NoMouse(Event *ev)
{
    m_nomouse = true;
}

void UIWidgetContainer::AllowGameFocus(Event *ev)
{
    m_allowGameFocus = true;
}

// latch; none of the three has a Mac engine symbol, and retail's every scale read

CLASS_DECLARATION(UIWidget, UIWidgetContainer, NULL) {
    {&EV_Layout_BGFill,     &UIWidgetContainer::SetBGFill    },
    {&EV_Layout_Fullscreen,  &UIWidgetContainer::SetFullscreen },
    {&EV_Layout_VidMode,     &UIWidgetContainer::SetVidMode    },
    {&EV_Layout_NoEscape,    &UIWidgetContainer::SetNoEscape   },
    {&EV_Layout_NoMouse,     &UIWidgetContainer::NoMouse       },
    {&EV_Layout_GameFocus,   &UIWidgetContainer::AllowGameFocus},
    {NULL,                  NULL                             }
};

UIWidgetContainer::UIWidgetContainer()
{
    m_depth         = 0.0f;
    m_bgfill        = UColor(0, 0, 0, 0);
    m_fullscreen    = false;
    m_vidmode       = -1;
    m_currentwidnum = 0;
    m_maxordernum   = 0;
    AllowActivate(false);
}

void UIWidgetContainer::AlignPosition(void)
{
    if (m_align & WA_LEFT) {
        m_frame.pos.x = 0;
    } else if (m_align & WA_RIGHT) {
        m_frame.pos.x = uid.vidWidth - m_frame.size.width;
    } else {
        m_frame.pos.x = (uid.vidWidth - m_frame.size.width) * 0.5;
    }

    if (m_align & WA_BOTTOM) {
        m_frame.pos.y = uid.vidHeight - m_frame.size.height;
    } else if (m_align & WA_TOP) {
        m_frame.pos.y = 0;
    } else {
        m_frame.pos.y = (uid.vidHeight - m_frame.size.height) * 0.5;
    }

    if (m_frame.pos.x < 0.0f) {
        m_frame.pos.x = 0.0f;
    }
    if (m_frame.pos.y < 0.0f) {
        m_frame.pos.y = 0.0f;
    }

    setFrame(m_frame);
    m_startingpos = m_frame.pos;
}

void UIWidgetContainer::Draw(void)
{
    if (m_mover) {
        m_mover->Motion();
    }

    if (m_fadeDelay != 0.0f && m_fadeGate == 0.0f && m_fadeDelay < (float)uid.time - m_fadeStartTime) {
        ProcessEvent(new Event(EV_Widget_Hide));
    }

    uii.Rend_Set2D(
        uid.originX,
        uid.surfaceHeight - uid.originY - uid.vidHeight,
        uid.vidWidth,
        uid.vidHeight,
        0,
        uid.vidWidth,
        uid.vidHeight,
        0,
        -1.0,
        1.0
    );
    uii.Rend_Scissor(uid.originX, uid.surfaceHeight - uid.originY - uid.vidHeight, uid.vidWidth, uid.vidHeight);

    if (m_bgfill.a != 0.0) {
        DrawBox(0, 0, uid.vidWidth, uid.vidHeight, m_bgfill, m_local_alpha);
    }

    set2D();
}

void UIWidgetContainer::Realign(void)
{
    AlignPosition();
}

void UIWidgetContainer::SetBGFill(Event *ev)
{
    m_bgfill.r = ev->GetFloat(1);
    m_bgfill.g = ev->GetFloat(2);
    m_bgfill.b = ev->GetFloat(3);
    m_bgfill.a = ev->GetFloat(4);
}

void UIWidgetContainer::SetFullscreen(Event *ev)
{
    m_fullscreen = ev->GetBoolean(1);
}

void UIWidgetContainer::SetVidMode(Event *ev)
{
    m_vidmode = ev->GetInteger(1);
}

int UIWidgetContainer::getVidMode(void)
{
    return m_vidmode;
}

qboolean UIWidgetContainer::isFullscreen(void)
{
    return m_fullscreen;
}

void UIWidgetContainer::SetNoEscape(Event *ev)
{
    m_bNoEscape = ev->GetInteger(1) ? qtrue : qfalse;
}

// port-only readers are gone, so `ordernumber` navigation is retail's own:
void UIWidgetContainer::SetActiveWidgetOrderNum(UIWidget *wid)
{
    int i;

    for (i = 1; i <= m_children.NumObjects(); i++) {
        if (wid == m_children.ObjectAt(i)) {
            m_currentwidnum = wid->getOrderNum();
        }
    }
}

void UIWidgetContainer::SetActiveWidgetOrderNum(int num)
{
    int i;

    // its keyboard-focus highlight clear.  That arm (a) does not exist in retail and
    for (i = 1; i <= m_children.NumObjects(); i++) {
        UIWidget *wid = m_children.ObjectAt(i);

        if (wid->getOrderNum() == num) {
            uWinMan.ActivateControl(wid);
        }
    }
}

void UIWidgetContainer::SetLastActiveWidgetOrderNum(void)
{
    if (!m_currentwidnum) {
        m_currentwidnum = 1;
    }

    SetActiveWidgetOrderNum(m_currentwidnum);
}

/*
 * The pad focus model.  The highlight is the widget's own mouse-over state, so
 * the focused child is drawn by the same material, colour, hover command and
 * hover sound a mouse-over uses -- there is no second highlight to keep in
 * step.  Movement walks the container's visible, enabled children in child
 * order and clamps at both ends, which is what keeps a held D-pad or stick from
 * running past the last entry of a list.
 */
UIWidget *UIWidgetContainer::getFocusWidget(void) const
{
    return m_focusWidget;
}

void UIWidgetContainer::setFocusWidget(UIWidget *wid)
{
    if (m_focusWidget == wid) {
        return;
    }

    if (m_focusWidget) {
        m_focusWidget->setNormalState();
    }

    m_focusWidget = wid;

    if (m_focusWidget) {
        m_focusWidget->setHoverState();
    }
}

void UIWidgetContainer::clearFocusWidget(void)
{
    setFocusWidget(NULL);
}

qboolean UIWidgetContainer::moveFocus(int dir)
{
    Container<UIWidget *> focusable;
    int                   i;
    int                   cur;
    int                   next;

    if (!dir) {
        return qfalse;
    }

    for (i = 1; i <= m_children.NumObjects(); i++) {
        UIWidget *child = m_children.ObjectAt(i);

        if (child && child->isEnabled() && child->getShow()) {
            focusable.AddObject(child);
        }
    }

    if (!focusable.NumObjects()) {
        return qfalse;
    }

    cur = m_focusWidget ? focusable.IndexOfObject(m_focusWidget) : 0;

    if (cur < 1) {
        next = (dir > 0) ? 1 : focusable.NumObjects();
    } else {
        next = cur + ((dir > 0) ? 1 : -1);
    }

    if (next < 1) {
        next = 1;
    }
    if (next > focusable.NumObjects()) {
        next = focusable.NumObjects();
    }

    setFocusWidget(focusable.ObjectAt(next));
    return qtrue;
}


#define WIDGETMOVER_MSEC_PER_SEC 1000.0f

WidgetMover::WidgetMover(UIWidget *widget)
{
    m_currentcommand = 0;       
    m_widget         = widget;  
    m_time           = 0.0f;
    m_totaltime      = 0.0f;
}

WidgetMover::~WidgetMover(void)
{
    deleteCommands();           
    m_widget = NULL;            
}

WidgetMoverCommand *WidgetMover::getCommand(void)
{
    int prev = m_currentcommand;

    if (!m_currentcommand) {
        m_currentcommand = 1;
        m_time           = m_totaltime;
    } else if (m_time >= m_endtime) {
        m_currentcommand++;
    }

    if (m_currentcommand > m_commands.NumObjects()) {
        m_currentcommand = m_commands.NumObjects();
        return NULL;
    }

    WidgetMoverCommand *cmd = m_commands.ObjectAt(m_currentcommand);

    if (m_currentcommand != prev) {
        m_starttime = m_time;
        m_endtime   = m_time + cmd->time;
        m_started   = false;
    }

    return cmd;
}

void WidgetMover::deleteCommands(void)
{
    int i;

    for (i = m_commands.NumObjects(); i > 0; i--) {
        WidgetMoverCommand *cmd = m_commands.ObjectAt(i);

        if (cmd) {
            delete cmd;
        }
    }

    m_commands.ClearObjectList();
}

void WidgetMover::Motion(void)
{
    m_totaltime = (float)uid.time / WIDGETMOVER_MSEC_PER_SEC;

    if (m_time > m_totaltime) {
        m_time = m_totaltime;
    }

    for (;;) {
        if (m_time >= m_totaltime) {
            return;
        }

        WidgetMoverCommand *cmd = getCommand();
        if (!cmd) {
            return;
        }

        switch (cmd->type) {
        case 1:
            move(cmd);
            break;
        case 2:
            moveTo(cmd);
            break;
        case 3:
            moveToCvar(cmd);
            break;
        case 4:
            moveToProp(cmd);
            break;
        case 5:
            moveToStat(cmd);
            break;
        case 7:
            m_currentcommand = 0;
            m_starttime      = m_totaltime;
            m_endtime        = 0.0f;
            m_time           = 0.0f;
            break;
        default:
            break;
        }

        if (m_totaltime <= m_endtime) {
            m_time = m_totaltime;
        } else {
            m_time = m_endtime;
        }
    }
}

void WidgetMover::setPosition(float x, float y)
{
    UIRect2D frame;

    frame.pos.x = x * uid.scaleRes[0];
    frame.pos.y = y * uid.scaleRes[1];
    frame.size  = m_widget->getFrame().size;

    m_widget->setFrame(frame);

    m_currentcommand = 0;
    m_starttime      = m_totaltime;
    m_endtime        = 0.0f;
    m_time           = 0.0f;
}

float WidgetMover::Fraction(const WidgetMoverCommand *cmd)
{
    float frac = (float)uid.time / WIDGETMOVER_MSEC_PER_SEC;

    if (cmd->time > 0.0f) {
        frac = (m_totaltime - m_starttime) / cmd->time;
    }

    if (frac > 1.0f) {
        frac = 1.0f;
    } else if (frac < 0.0f) {
        frac = 0.0f;
    }

    return frac;
}

void WidgetMover::SetFrameFraction(const WidgetMoverCommand *cmd, float frac)
{
    UIRect2D frame;

    frame.pos.x = (m_endx - m_startx) * frac + m_startx;
    frame.pos.y = (m_endy - m_starty) * frac + m_starty;
    frame.size  = m_widget->getFrame().size;

    m_widget->setFrame(frame);
}

void WidgetMover::move(WidgetMoverCommand *cmd)
{
    if (!m_started) {
        m_startx   = m_widget->getFrame().pos.x;
        m_starty   = m_widget->getFrame().pos.y;
        m_endx     = m_startx + cmd->x;
        m_endy     = m_starty + cmd->y;
        m_started  = true;
    }

    SetFrameFraction(cmd, Fraction(cmd));
}

void WidgetMover::moveTo(WidgetMoverCommand *cmd)
{
    if (!m_started) {
        m_startx  = m_widget->getFrame().pos.x;
        m_starty  = m_widget->getFrame().pos.y;
        m_endx    = cmd->x;
        m_endy    = cmd->y;
        m_started = true;
    }

    SetFrameFraction(cmd, Fraction(cmd));
}

void WidgetMover::moveToCvar(WidgetMoverCommand *cmd)
{
    if (!m_started) {
        m_startx = m_widget->getFrame().pos.x;
        m_starty = m_widget->getFrame().pos.y;
        m_endx   = cmd->xscale * Cvar_VariableValue(cmd->arg1.c_str()) + cmd->x;
        m_endy   = cmd->yscale * Cvar_VariableValue(cmd->arg2.c_str()) + cmd->y;
        m_started = true;
    }

    SetFrameFraction(cmd, Fraction(cmd));
}

void WidgetMover::moveToStat(WidgetMoverCommand *cmd)
{
    if (!m_started) {
        m_startx = m_widget->getFrame().pos.x;
        m_starty = m_widget->getFrame().pos.y;
        m_endx   = cmd->xscale * 0.0f + cmd->x;
        m_endy   = cmd->yscale * 0.0f + cmd->y;
        m_started = true;
    }

    SetFrameFraction(cmd, Fraction(cmd));
}

void WidgetMover::moveToProp(WidgetMoverCommand *cmd)
{
    if (!m_started) {
        m_startx = m_widget->getFrame().pos.x;
        m_starty = m_widget->getFrame().pos.y;
        m_endx   = cmd->xscale * 0.0f + cmd->x;
        m_endy   = cmd->yscale * 0.0f + cmd->y;
        m_started = true;
    }

    SetFrameFraction(cmd, Fraction(cmd));
}

WidgetMoverCommand::WidgetMoverCommand(void)
{
    type       = 0;
    x          = 0.0f;
    y          = 0.0f;
    xpos       = 0.0f;
    ypos       = 0.0f;
    time       = 0.0f;
    xscale     = 176.0f;
    yscale     = 176.0f;
    xscalepos  = 176.0f;
    yscalepos  = 176.0f;
}

void WidgetMover::ParseEquationString(const str& eq, float& value, str& name, float& scale, float def)
{
    const char *text = eq.c_str();
    int         len  = eq.length();
    float       integerPart;
    float       fracPart;
    int         i;

    integerPart = 0.0f;
    fracPart    = 0.0f;

    for (i = 0; i < len; i++) {
        char c = text[i];

        if (c < '0' || c > '9') {
            break;
        }

        /* retail: fmadd f30, f3, f30, f0 then frsp -- one rounding, not two. */
        integerPart = (float)fma(10.0, (double)integerPart, (double)(c - '0'));
    }

    if (i < len && text[i] == '.') {
        i++;
    }

    for (; i < len; i++) {
        char c = text[i];

        if (c < '0' || c > '9') {
            break;
        }

        fracPart = fracPart * 1.0f + (float)(c - '0');
    }

    while (fracPart > 0.0f) {
        fracPart = (float)((double)fracPart / 10.0);
    }

    scale = 0.0f;

    if (i < len && text[i] == '+') {
        scale = def;
    } else if (i < len && text[i] == '-') {
        scale = -def;
    }

    name = "";
    for (i = i + 1; i < len; i++) {
        char c[2];

        c[0] = text[i];
        c[1] = 0;
        name += c;
    }

    value = integerPart + fracPart;
}

void WidgetMover::move(Event *ev)
{
    WidgetMoverCommand *cmd = new WidgetMoverCommand();

    cmd->type = 1;
    cmd->x    = ev->GetFloat(1);
    cmd->y    = ev->GetFloat(2);
    cmd->time = ev->GetFloat(3);

    cmd->xpos   = cmd->x;
    cmd->ypos   = cmd->y;

    m_commands.AddObject(cmd);
}

void WidgetMover::moveTo(Event *ev)
{
    WidgetMoverCommand *cmd = new WidgetMoverCommand();

    cmd->type = 2;
    cmd->x    = ev->GetFloat(1);
    cmd->y    = ev->GetFloat(2);
    cmd->time = ev->GetFloat(3);

    cmd->xpos   = cmd->x;
    cmd->ypos   = cmd->y;

    m_commands.AddObject(cmd);
}

// WidgetMover::moveToCvar(Event*) @0x000AC5A0 (1148 B).  Two EQUATIONS, not two
void WidgetMover::moveToCvar(Event *ev)
{
    WidgetMoverCommand *cmd;
    float               def;
    str                 equation;

    if (ev->NumArgs() >= 3) {
        def = ev->GetFloat(3);
    } else {
        def = 1.0f;
    }

    cmd       = new WidgetMoverCommand();
    cmd->type = 3;

    equation = ev->GetString(1);
    ParseEquationString(equation, cmd->x, cmd->arg1, cmd->xscale, def);

    equation = ev->GetString(2);
    ParseEquationString(equation, cmd->y, cmd->arg2, cmd->yscale, def);

    if (ev->NumArgs() >= 4) {
        cmd->time = ev->GetFloat(4);
    } else {
        cmd->time = 0.5f;
    }

    cmd->xpos      = cmd->x;
    cmd->ypos      = cmd->y;
    cmd->xscalepos = cmd->xscale;
    cmd->yscalepos = cmd->yscale;

    m_commands.AddObject(cmd);
}

void WidgetMover::moveToStat(Event *ev)
{
    WidgetMoverCommand *cmd;
    float               def;
    str                 equation;

    if (ev->NumArgs() >= 3) {
        def = ev->GetFloat(3);
    } else {
        def = 1.0f;
    }

    cmd       = new WidgetMoverCommand();
    cmd->type = 5;

    equation = ev->GetString(1);
    ParseEquationString(equation, cmd->x, cmd->arg1, cmd->xscale, def);

    equation = ev->GetString(2);
    ParseEquationString(equation, cmd->y, cmd->arg2, cmd->yscale, def);

    if (ev->NumArgs() >= 4) {
        cmd->time = ev->GetFloat(4);
    } else {
        cmd->time = 0.5f;
    }

    cmd->xpos      = cmd->x;
    cmd->ypos      = cmd->y;
    cmd->xscalepos = cmd->xscale;
    cmd->yscalepos = cmd->yscale;

    m_commands.AddObject(cmd);
}

void WidgetMover::moveToProp(Event *ev)
{
    WidgetMoverCommand *cmd;
    float               def;
    str                 equation;

    if (ev->NumArgs() >= 3) {
        def = ev->GetFloat(3);
    } else {
        def = 1.0f;
    }

    cmd       = new WidgetMoverCommand();
    cmd->type = 4;

    equation = ev->GetString(1);
    ParseEquationString(equation, cmd->x, cmd->arg1, cmd->xscale, def);

    equation = ev->GetString(2);
    ParseEquationString(equation, cmd->y, cmd->arg2, cmd->yscale, def);

    if (ev->NumArgs() >= 4) {
        cmd->time = ev->GetFloat(4);
    } else {
        cmd->time = 0.5f;
    }

    cmd->xpos      = cmd->x;
    cmd->ypos      = cmd->y;
    cmd->xscalepos = cmd->xscale;
    cmd->yscalepos = cmd->yscale;

    m_commands.AddObject(cmd);
}

// duration is ZERO -- retail never reads an argument here (`v6 = island+12' is
void WidgetMover::restart(Event *ev)
{
    WidgetMoverCommand *cmd = new WidgetMoverCommand();

    cmd->type = 7;

    m_commands.AddObject(cmd);
}

void WidgetMover::wait(Event *ev)
{
    WidgetMoverCommand *cmd = new WidgetMoverCommand();

    if (ev->NumArgs() >= 1) {
        cmd->time = ev->GetFloat(1);
    }

    m_commands.AddObject(cmd);
}

void WidgetMover::clearCommands(Event *ev)
{
    deleteCommands();
    restartCommands(ev);
}

void WidgetMover::restartCommands(Event *ev)
{
    m_currentcommand = 0;
    m_starttime      = m_totaltime;
    m_endtime        = 0.0f;
    m_time           = 0.0f;
}

void WidgetMover::ChangeResolution(void)
{
    int i;

    for (i = 1; i <= m_commands.NumObjects(); i++) {
        WidgetMoverCommand *cmd = m_commands.ObjectAt(i);

        cmd->x      = cmd->xpos * uid.scaleRes[0];
        cmd->y      = cmd->ypos * uid.scaleRes[1];
        cmd->xscale = cmd->xscalepos * uid.scaleRes[0];
        cmd->yscale = cmd->yscalepos * uid.scaleRes[1];
    }
}

void UIWidget::move(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->move(ev);
}

void UIWidget::moveTo(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->moveTo(ev);
}

void UIWidget::moveToCvar(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->moveToCvar(ev);
}

void UIWidget::moveToStat(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->moveToStat(ev);
}

void UIWidget::moveToProp(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->moveToProp(ev);
}

void UIWidget::restart(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->restart(ev);
}

void UIWidget::wait(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->wait(ev);
}

void UIWidget::clearCommands(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->clearCommands(ev);
}

void UIWidget::restartCommands(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->restartCommands(ev);
}

void UIWidget::setPosition(Event *ev)
{
    if (!m_mover) {
        m_mover = new WidgetMover(this);
    }

    m_mover->setPosition(ev->GetFloat(1), ev->GetFloat(2));
}
