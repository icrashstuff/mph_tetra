/* SPDX-License-Identifier: MIT
 *
 * This file is a C++ port of lzss3.py
 * Source repo: https://github.com/magical/nlzss
 *
 * SPDX-FileCopyrightText: Copyright (c) 2010, 2012, 2014 magical
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

#include "lzss.h"

#include <SDL_bits.h>
#include <SDL_endian.h>
#include <algorithm>

/**
 * Leaving this on until I am confident I didn't break anything - Ian (2024-11-06)
 */
#if 1
#include "gui/console.h"
#define TRACE(fmt, ...) dc_log_trace(fmt, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...) (0)
#endif

#define next(it) (_in[iter++])
#define bail_if_next_next_is_unsafe() \
    do                                \
    {                                 \
        if (iter > _in.size())        \
            goto end;                 \
    } while (0)

static bool decompress_lz10(const std::vector<Uint8>& _in, Uint32 offset, Uint32 decompressed_size, std::vector<Uint8>& _out, bool is_overlay)
{
    _out.clear();
    _out.reserve(decompressed_size);

    Uint32 iter = offset;
    Uint32 disp_extra = is_overlay ? 3 : 1;

    while (_out.size() < decompressed_size)
    {
        bail_if_next_next_is_unsafe();
        Uint8 b = next(it);
        for (int i = 7; i >= 0; i--)
        {
            bool flag = (b >> i) & 1;
            if (!flag)
            {
                bail_if_next_next_is_unsafe();
                _out.push_back(next(it));
            }
            else
            {
                bail_if_next_next_is_unsafe();
                Uint16 sh = (next(it) << 8);
                bail_if_next_next_is_unsafe();
                sh |= next(it);
                sh = SDL_SwapLE16(sh);
                Uint32 count = (sh >> 0xc) + 3;
                Uint32 disp = (sh & 0xfff) + disp_extra;

                for (Uint32 i = 0; i < count; i++)
                    _out.push_back(_out[_out.size() - disp]);
            }
            if (decompressed_size <= _out.size())
                goto end;
        }
    }

end:
    if (_out.size() != decompressed_size)
        return false;
    return true;
}

static bool decompress_lz11(const std::vector<Uint8>& _in, Uint32 offset, Uint32 decompressed_size, std::vector<Uint8>& _out, bool is_overlay)
{
    if (is_overlay)
        return false;

    _out.clear();
    _out.reserve(decompressed_size);

    Uint32 iter = offset;

    while (_out.size() < decompressed_size)
    {
        bail_if_next_next_is_unsafe();
        Uint8 b = next(it);

        for (int i = 7; i >= 0; i--)
        {
            bool flag = (b >> i) & 1;
            if (!flag)
            {
                bail_if_next_next_is_unsafe();
                _out.push_back(next(it));
            }
            else
            {
                bail_if_next_next_is_unsafe();
                b = next(it);
                Uint32 indicator = b >> 4;
                Uint32 count = 0;

                if (indicator == 0)
                {
                    // # 8 bit count, 12 bit disp
                    // # indicator is 0, don't need to mask b
                    count = (b << 4);
                    bail_if_next_next_is_unsafe();
                    b = next(it);
                    count += b >> 4;
                    count += 0x11;
                }
                else if (indicator == 1)
                {
                    // # 16 bit count, 12 bit disp
                    bail_if_next_next_is_unsafe();
                    count = ((b & 0xf) << 12) + (next(it) << 4);
                    bail_if_next_next_is_unsafe();
                    b = next(it);
                    count += b >> 4;
                    count += 0x111;
                }
                else
                {
                    // # indicator is count (4 bits), 12 bit disp
                    count = indicator;
                    count += 1;
                }

                bail_if_next_next_is_unsafe();
                Uint32 disp = ((b & 0xf) << 8) + next(it);
                disp += 1;

                for (Uint32 i = 0; i < count; i++)
                {
                    size_t pos = _out.size() - disp;
                    if (pos > _out.size())
                        goto end;
                    _out.push_back(_out[pos]);
                }
            }

            if (decompressed_size <= _out.size())
                goto end;
        }
    }

end:
    if (_out.size() != decompressed_size)
        return false;

    return false;
}

static bool decompress_lz_normal(const std::vector<Uint8>& _in, std::vector<Uint8>& _out)
{
    if (_in.size() < 4)
        return 0;

    bool (*algorithm)(const std::vector<Uint8>&, Uint32, Uint32, std::vector<Uint8>&, bool);

    TRACE("Magic byte: 0x%02X", _in.data()[0]);

    switch (_in.data()[0])
    {
    case 0x10:
        algorithm = decompress_lz10;
        break;
    case 0x11:
        algorithm = decompress_lz11;
        break;
    default:
        return 0;
        break;
    }

    int decompressed_size = 0;
    {
        char header[4];
        memcpy(header, _in.data() + 1, 3);
        header[3] = 0;

        decompressed_size = SDL_SwapLE16(*(Uint32*)header);
    }

    {
        size_t in_size = _in.size();
        float increase = ((float)decompressed_size * 100.0) / ((float)in_size);
        TRACE("Size: %zu->%d bytes (%.2f%%)", _in.size(), decompressed_size, increase);
    }

    return algorithm(_in, 4, decompressed_size, _out, 0);
}

struct overlay_compression_header_t
{
    Uint32 end_delta;
    Uint32 start_delta;

    overlay_compression_header_t endian_correct()
    {
        overlay_compression_header_t temp;
        temp.end_delta = SDL_SwapLE32(end_delta);
        temp.start_delta = SDL_SwapLE32(start_delta);
        return temp;
    }
};

/**
 * It is very possible that this function is too paranoid in its checks - Ian (2024-11-06)
 */
static bool decompress_lz_overlay(const std::vector<Uint8>& _in, std::vector<Uint8>& _out)
{
    _out.clear();

    if (_in.size() > UINT32_MAX)
        return false;
    Uint32 filelen = _in.size();
    Uint32 pos = filelen;

    if (filelen < 8)
        return false;

    // # the compression header is at the end of the file
    pos -= 8;

    overlay_compression_header_t header = ((overlay_compression_header_t*)(&_in.data()[pos]))->endian_correct();

    // # decompression goes backwards.
    // # end < here < start
    // # end_delta == here - decompression end address
    // # start_delta == decompression start address - here

    Uint32 padding = header.end_delta >> 0x18;
    header.end_delta &= 0xFFFFFF;
    Uint32 decompressed_size = header.start_delta + header.end_delta;

    {
        size_t in_size = _in.size();
        float increase = ((float)decompressed_size * 100.0) / ((float)in_size);
        TRACE("Size: %zu->%d bytes (%.2f%%)", _in.size(), decompressed_size, increase);
    }

    if (header.end_delta > filelen)
        return false;
    pos = filelen - header.end_delta;

    std::vector<Uint8> flipped_data;

    if (header.end_delta < padding)
        return false;

    flipped_data.reserve(header.end_delta - padding);

    for (Uint32 i = 0; i < header.end_delta - padding; i++)
    {
        Uint32 j = pos + i;
        if (j >= filelen)
            return false;
        flipped_data.push_back(_in[j]);
    }
    std::reverse(flipped_data.begin(), flipped_data.end());

    std::vector<Uint8> uncompressed_data;
    if (!decompress_lz10(flipped_data, 0, decompressed_size, uncompressed_data, 1))
        return false;
    std::reverse(uncompressed_data.begin(), uncompressed_data.end());

    // # first we write up to the portion of the file which was "overwritten" by
    // # the decompressed data, then the decompressed data itself.
    // # i wonder if it's possible for decompression to overtake the compressed
    // # data, so that the decompression code is reading its own output...
    pos = 0;

    if (header.end_delta > filelen)
        return false;

    _out.reserve(decompressed_size);

    for (Uint32 i = 0; i < filelen - header.end_delta; i++)
    {
        Uint32 j = pos + i;
        if (j >= filelen)
            return false;
        _out.push_back(_in[j]);
    }

    for (size_t i = 0; i < uncompressed_data.size(); i++)
        _out.push_back(uncompressed_data[i]);

    return true;
}

bool util::decompress_lz(const std::vector<Uint8>& _in, std::vector<Uint8>& _out, bool is_overlay)
{
    _out.clear();
    if (is_overlay)
        return decompress_lz_overlay(_in, _out);
    else
        return decompress_lz_normal(_in, _out);
}
