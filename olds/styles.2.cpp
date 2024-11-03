/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2022 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "styles.h"
#include "console.h"
#include "convar.h"

#define style_change_func(name) ImVec4 name(ImVec4 in)

static const char* style_change_cmd = "Hello";

static bool style_change(style_change_func((*change_func)), void (*style_func)(ImGuiStyle*) = ImGui::StyleColorsDark)
{
    IM_ASSERT(change_func);
    IM_ASSERT(style_func);
    if (change_func == NULL || style_func == NULL)
        return false;

    ImGuiStyle  style_temp;
    ImGuiStyle* style_out = &ImGui::GetStyle();

    style_func(&style_temp);

    for (int i = 0; i < ImGuiCol_COUNT; i++)
        style_out->Colors[i] = change_func(style_temp.Colors[i]);

    return true;
}

// RGB -> RGB
static style_change_func(style_change_func_none) { return ImVec4(in.x, in.y, in.z, in.w); }
// RGB -> RBG
static style_change_func(style_change_func_green1) { return ImVec4(in.x, in.z, in.y, in.w); }

// RGB -> GRB
static style_change_func(style_change_func_purple) { return ImVec4(in.y, in.x, in.z, in.w); }
// RGB -> GBR
static style_change_func(style_change_func_green2) { return ImVec4(in.y, in.z, in.x, in.w); }

// RGB -> BRG
static style_change_func(style_change_func_mute_red) { return ImVec4(in.z, in.x, in.y, in.w); }
// RGB -> BGR
static style_change_func(style_change_func_orange) { return ImVec4(in.z, in.y, in.x, in.w); }

// Make convar
static int   _style_base  = 0;
static int   _hue        = 0;
static float _saturation = 1.0;
static float _value      = 1.0;

static style_change_func(style_change_func_hsv)
{
    float  h;
    float  s;
    float  v;
    ImVec4 out;
    ImGui::ColorConvertRGBtoHSV(in.x, in.y, in.z, h, s, v);
    h += _hue / 360.0f;
    s *= _saturation;
    v *= _value;
    ImGui::ColorConvertHSVtoRGB(h, s, v, out.x, out.y, out.z);
    out.w = in.w;
    return out;
}

void style_colors_editor()
{
    if (ImGui::Begin("MPH Tetra Style Editor"))
    {
        // Make convars and maybe add Convar::DragInt, Convar::DragFloat, Convar::Combo

        int change = 0;

        change += ImGui::DragInt("Hue Offset", &_hue, 1.0f, 0, 360, "%d", ImGuiSliderFlags_WrapAround | ImGuiSliderFlags_AlwaysClamp);
        change += ImGui::DragFloat("Saturation Multiplier", &_saturation, 1.0f / 360.0f, 0, 2, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        change += ImGui::DragFloat("Value Multiplier", &_value, 1.0f / 360.0f, 0.2f, 2, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        change += ImGui::Combo("Style Base", &_style_base, "Dark\0Light\0Classic\0");

        if (change)
        {
            switch (_style_base)
            {
            case 0:
                style_change(style_change_func_hsv, ImGui::StyleColorsDark);
                break;
            case 1:
                style_change(style_change_func_hsv, ImGui::StyleColorsLight);
                break;
            case 2:
                style_change(style_change_func_hsv, ImGui::StyleColorsClassic);
                break;
            }
        }
        
        ImGui::ShowFontSelector("Fonts");

        ImGui::End();
    }
}

void style_colors_rotate_hue(int style_base, int hue, float saturation, float value)
{
    _style_base = style_base;
    _saturation = saturation;
    _value      = value;
    _hue        = hue;
    switch (_style_base)
    {
    case 0:
        style_change(style_change_func_hsv, ImGui::StyleColorsDark);
        break;
    case 1:
        style_change(style_change_func_hsv, ImGui::StyleColorsLight);
        break;
    case 2:
        style_change(style_change_func_hsv, ImGui::StyleColorsClassic);
        break;
    }
}
