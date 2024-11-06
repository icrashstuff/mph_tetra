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
#ifndef MPH_TETRA_GUI_FILE_PICKER_H
#define MPH_TETRA_GUI_FILE_PICKER_H

/**
 * Currently limited to only one file type, this can be fixed but I don't feel like it right now
 */
class file_picker_widget_t
{
public:
    /**
     * Creates file picker widget
     *
     * @param win Parent window handle
     * @param filter_desc Description of the file to filter for
     * @param filter_ext Extension of the file to filter for
     */
    file_picker_widget_t(SDL_Window* win, std::string filter_desc, std::string filter_ext);

    /**
     * WARNING: This may block execution, be prepared
     *
     * @returns True if the file is changed, false if not
     */
    bool draw(const char* id, const char* hint = NULL);

    inline std::string get_filename() { return _str; }

    /**
     * Sets the filename
     *
     * The next draw() call will return true if the new name is different
     */
    inline void set_filename(std::string fname)
    {
        if (fname != _str)
            _changed = true;
        _str = fname;
        _working_buf = fname;
    };

    inline void set_window(SDL_Window* win) { _window = win; }

private:
    std::string _str;
    std::string _working_buf;
    std::string _err;
    std::string _filter_desc;
    std::string _filter_ext;
    SDL_Window* _window;
    bool _changed;
};
#endif
