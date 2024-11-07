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
 *
 * This file uses the archive header specs outlined in Archive.cs from NoneGiven's MphRead:
 * https://github.com/NoneGiven/MphRead/blob/master/src/MphRead/Utility/Archive.cs
 * However the implementation of archive_extract_entries() is original
 */
#include "archive.h"
#include "misc.h"

#include "gui/console.h"

#include <SDL_bits.h>
#include <SDL_endian.h>
#include <assert.h>
#include <string.h>

#define MAGIC "SNDFILE\0"

/* This structure was derived from MphRead:/src/Utility/Archive.cs */
struct header_archive_t
{
    /**
     * NULL-Terminated "SNDFILE\0"
     */
    char magic[8];
    /**
     * Big endian
     */
    Uint32 file_count;
    /**
     * Big endian
     */
    Uint32 archive_size;

    Uint32 reserved[4];

    /**
     * Returns an endian corrected version of this struct
     */
    header_archive_t endian_correct()
    {
        header_archive_t t;
        memcpy(&t, this, sizeof(header_archive_t));
        ASSERT_SwapBE32(t.file_count);
        ASSERT_SwapBE32(t.archive_size);
        return t;
    }
};
static_assert(sizeof(header_archive_t) == 32, "header_archive_t size incorrect!");

/* This structure was derived from MphRead:/src/Utility/Archive.cs */
struct archive_file_entry_t
{
    /**
     * MphRead doesn't explicitly say this is a null terminated string
     */
    char fname[32];
    /**
     * Big endian
     */
    Uint32 offset;
    /**
     * Big endian
     *
     * Appears to be `size_target` but padded so that `size_padded % 32 == 0`
     */
    Uint32 size_padded;
    /**
     * Big endian
     */
    Uint32 size_target;

    Uint32 reserved[5];

    /**
     * Returns an endian corrected version of this struct
     */
    archive_file_entry_t endian_correct()
    {
        archive_file_entry_t t;
        memcpy(&t, this, sizeof(archive_file_entry_t));
        ASSERT_SwapBE32(t.offset);
        ASSERT_SwapBE32(t.size_padded);
        ASSERT_SwapBE32(t.size_target);
        return t;
    }
};
static_assert(sizeof(archive_file_entry_t) == 64, "archive_file_entry_t size incorrect");

#if 0
#define bail_assert(cond) \
    do                    \
    {                     \
        assert(cond);     \
        if (!(cond))      \
            return false; \
    } while (0)
#else
#define bail_assert(cond) \
    do                    \
    {                     \
        if (!(cond))      \
            return false; \
    } while (0)
#endif

/* TODO: Is there a difference  */
bool util::archive_extract_entries(const std::vector<Uint8>& in, std::vector<util::archive_entry_t>& out)
{
    bail_assert(in.size() >= sizeof(header_archive_t));

    header_archive_t header = ((header_archive_t*)in.data())->endian_correct();

    bail_assert(strncmp(header.magic, MAGIC, sizeof(header.magic)) == 0);

    bail_assert(in.size() > (sizeof(header_archive_t) + sizeof(archive_file_entry_t) * header.file_count));

    bail_assert(header.archive_size == in.size());

    out.clear();
    out.reserve(header.file_count);

    archive_file_entry_t* file_entries = ((archive_file_entry_t*)(in.data() + sizeof(header_archive_t)));
    for (Uint32 i = 0; i < header.file_count; i++)
    {
        archive_file_entry_t current = file_entries[i].endian_correct();
        bail_assert(current.offset <= in.size());
        bail_assert(current.offset + current.size_target <= in.size());
        util::archive_entry_t entry;
        entry.fname.clear();
        entry.fname.append(current.fname, sizeof(current.fname));
        entry.data.clear();
        entry.data.resize(current.size_target);
        memcpy(entry.data.data(), in.data() + current.offset, entry.data.size());
        out.push_back(entry);
    }

    return true;
}
