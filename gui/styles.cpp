/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
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
#include "gui_registrar.h"
#include "util/convar.h"

#define style_change_func(name) ImVec4 name(ImVec4 in)

static bool style_change(style_change_func((*change_func)), void (*style_func)(ImGuiStyle*) = ImGui::StyleColorsDark)
{
    IM_ASSERT(change_func);
    IM_ASSERT(style_func);
    if (change_func == NULL || style_func == NULL)
        return false;

    ImGuiStyle style_temp;
    ImGuiStyle* style_out = &ImGui::GetStyle();

    style_func(&style_temp);

    for (int i = 0; i < ImGuiCol_COUNT; i++)
        style_out->Colors[i] = change_func(style_temp.Colors[i]);

    return true;
}

void style_colors_update();

// Make convar
static convar_int_t cl_style_hue("cl_style_hue", 160, 0, 360, "Set HSV hue offset for the Dear ImGui style", 0, style_colors_update);
static convar_float_t cl_style_saturation("cl_style_saturation", 1, 0, 2, "Set HSV saturation multiplier for the Dear ImGui style", 0, style_colors_update);
static convar_float_t cl_style_value("cl_style_value", 1, 0.2, 2, "Set HSV value multiplier for the Dear ImGui style", 0, style_colors_update);
static convar_int_t cl_style_base("cl_style_base", 0, 0, 2, "Set base style for Dear ImGui [0: Dark, 1: Light, 2: Classic]", 0, style_colors_update);
static convar_int_t cl_style_editor_window("cl_style_picker_window", false, false, true, "Show window for editing the Dear ImGui style");

static style_change_func(style_change_func_hsv)
{
    float h;
    float s;
    float v;
    ImVec4 out;
    ImGui::ColorConvertRGBtoHSV(in.x, in.y, in.z, h, s, v);
    h += cl_style_hue.get() / 360.0f;
    s *= cl_style_saturation.get();
    v *= cl_style_value.get();
    ImGui::ColorConvertHSVtoRGB(h, s, v, out.x, out.y, out.z);
    out.w = in.w;
    return out;
}

void style_colors_update()
{
    switch (cl_style_base.get())
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

bool style_colors_editor()
{
    if (!cl_style_editor_window.get())
        return false;
    if (ImGui::BeginCVR("MPH Tetra Style Editor", &cl_style_editor_window))
    {
        cl_style_hue.imgui_edit();
        cl_style_saturation.imgui_edit();
        cl_style_value.imgui_edit();
        cl_style_base.imgui_edit();

        ImGui::ShowFontSelector("Fonts");

        ImGui::End();
        return true;
    }
    return false;
}

static gui_register_menu register_menu(style_colors_editor);

void style_colors_rotate_hue(int style_base, int hue, float saturation, float value)
{
    cl_style_hue.set(hue);
    cl_style_saturation.set(saturation);
    cl_style_value.set(value);
    cl_style_base.set(style_base);
    style_colors_update();
}
