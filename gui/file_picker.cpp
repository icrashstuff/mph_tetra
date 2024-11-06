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
#include "gui/console.h"
#include "imgui.h"
#include "util/nfd.h"

#include "file_picker.h"

/**
 * TODO-OPT: Spin this out into a dynamic library? (eg. To allow both GTK3 and xdg-portal)
 */
static int file_dialog(std::string& str, std::string& err, std::string filter_desc, std::string filter_ext, SDL_Window* window)
{
    nfdu8char_t* outPath;
    nfdu8filteritem_t filters[1] = { { filter_desc.c_str(), filter_ext.c_str() } };
    nfdopendialogu8args_t args = { 0, 0, 0, { 0, 0 } };
    args.filterList = filters;
    args.filterCount = IM_ARRAYSIZE(filters);
    NFD_GetNativeWindowFromSDLWindow(window, &args.parentWindow);
    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
    switch (result)
    {
    case NFD_OKAY:
        str.clear();
        str.append(outPath);
        NFD_FreePathU8(outPath);
        return 0;
        break;
    case NFD_CANCEL:
        return 1;
        break;
    default:
        err.clear();
        err.append(NFD_GetError());
        dc_log("NFD Error: %s", err.c_str());
        return 2;
        break;
    }
}

file_picker_widget_t::file_picker_widget_t(SDL_Window* win, std::string filter_desc, std::string filter_ext)
{
    _window = win;
    _filter_desc = filter_desc;
    _filter_ext = filter_ext;
    _changed = false;
}

bool file_picker_widget_t::draw(const char* id, const char* hint)
{
    bool ret = false;
    ImGui::PushID(id);
    if (ImGui::InputTextWithHint("##textinput", hint, &_working_buf, ImGuiInputTextFlags_EnterReturnsTrue))
        ret = true;
    ImGui::SameLine();
    if (ImGui::Button(id))
    {
        int dialog_code = file_dialog(_working_buf, _err, _filter_desc, _filter_ext, _window);
        ret = dialog_code == 0;
        if (dialog_code == 2)
            ImGui::OpenPopup("File Picker Error");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("File Picker Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("An error occurred while trying to open a file dialog");
        ImGui::TextUnformatted("Not to worry, you can still manually enter the path :)");
        ImGui::Separator();

        if (_err.size() > 0)
        {
            ImGui::TextUnformatted("Error message:");
            if (ImGui::BeginChild("err_message_text", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3), ImGuiChildFlags_FrameStyle))
            {
                ImGui::TextUnformatted(_err.c_str());
                ImGui::EndChild();
            }
            ImGui::Separator();
        }

        if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        {
            _err.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();

        ImGui::EndPopup();
    }

    if (ret)
        _str = _working_buf;

    ImGui::PopID();

    if (_changed)
    {
        ret = true;
        _changed = false;
    }
    return ret;
}
