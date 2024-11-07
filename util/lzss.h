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
#ifndef MPH_TETRA_UTIL_LZSS_H
#define MPH_TETRA_UTIL_LZSS_H
#include <SDL_bits.h>
#include <vector>

namespace util
{
/**
 * C++ Re-Implementation of lzss3.py
 * Source repo: https://github.com/magical/nlzss
 *
 * Decompress LZSS-compressed bytes. Returns a bytearray.
 *
 * Original python implementation is licensed under the MIT License
 *
 * This doesn't seem to quite agree with MphRead so...
 *
 * @param in LZ10 or LZ11 compressed data
 * @param out Buffer to be written to, WARNING: this is cleared at the beginning of the function
 * @param is_overlay Enables overlay specific decoding
 *
 * @returns non-zero on success, and zero on error
 */
bool decompress_lz(const std::vector<Uint8>& in, std::vector<Uint8>& out, bool is_overlay = false);
}
#endif
