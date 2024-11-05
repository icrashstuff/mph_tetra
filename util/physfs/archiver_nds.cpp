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
 * Read-only support for NDS cartridges (and only what MPH Tetra requires / what MphRead exports)
 *
 * This makes no attempt to decrypt or really check anything.
 * The extent of the checks is calling nds_cartridge_header_t::seems_valid_enough() without crc16 checking
 *
 * Exposes structure like so
 *
 * mountpoint
 * + header nds_cartridge_header_t
 * + bin
 *   + arm9
 *   + arm7
 * + nitrofs
 *   + nitrofs directory structure
 *
 * The specs of the format(s) are from the GBATEK GBA/NDS Technical Info document version 3.05
 * found here: https://problemkaputt.de/gbatek.htm
 */

#include "util/misc.h"
#include "util/nds.h"

/* vector **must** be included before physfs_internal.h otherwise things break */
#include <vector>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

struct fat_entry_t
{
    Uint32 start;
    Uint32 end;
};

struct fnt_entry_main_t
{
    /**
     * Offset is relative to the nds_cartridge_header_t.file_name_table_offset
     */
    Uint32 sub_entry_offset;

    /**
     *
     */
    Uint16 first_fat_entry_id;

    union
    {
        /**
         * Root entry only
         */
        Uint16 number_of_dirs;
        /**
         * All entries but root
         *
         * Value is from 0xF001 to 0xFFFF
         */
        Uint16 parent_id;
    };
};

/**
 * This doesn't map well to a struct because name is variable length and...
 *
 * if(type & IS_DIR_MASK) then there is a `Uint16 sub_dir_id;` after name
 */
struct fnt_entry_sub_t
{
    enum fnt_entry_file_type_flags_t_
    {
        IS_DIR_MASK = 0x80,
        LEN_MASK = 0x7F,
    };
    typedef Uint8 fnt_entry_file_type_flags;
    /**
     * Holds is_dir flag and length, value of 0x00 means end of table
     */
    Uint8 type;
    /**
     * Not null terminated
     *
     * Size is type & LEN_MASK
     */
    char name[];

    /* if(type & IS_DIR_MASK) Uint16 sub_dir_id; */
};

/**
 * Parse a NitroROM file name table entry
 *
 * @param current Pointer to pointer of current fnt entry, functions sets it to NULL if there are no more entries
 * @param is_dir Set to true if the entry defines a sub directory
 * @param name A null terminated string of no greater than 128 bytes including terminator will be written to this field
 * @param name_len Length of data written to name
 * @param is_dir If is_dir == true, then sub_dir_id is the id of the fnt_entry_main_t that it points to
 *
 * @returns true on successful parse, and false if there are no more entries or on error
 */
static bool fnt_entry_subtable_parse(fnt_entry_sub_t** current, bool& is_dir, char* name, int& name_len, int& sub_dir_id)
{
    if (!current || !(*current))
        return false;
    fnt_entry_sub_t* temp = *current;

    name_len = temp->type & fnt_entry_sub_t::LEN_MASK;

    if ((name_len & fnt_entry_sub_t::LEN_MASK) == 0)
    {
        *current = NULL;
        return false;
    }

    is_dir = temp->type & fnt_entry_sub_t::IS_DIR_MASK;
    memcpy(name, temp->name, name_len);
    name[name_len] = '\0';

    if (is_dir)
        sub_dir_id = PHYSFS_swapULE16((*(Uint16*)(temp + name_len + 1))) - 0xF000;
    else
        sub_dir_id = 0;

    temp = temp + name_len + 1 + is_dir * sizeof(Uint16);
    *current = temp;
    return true;
}

/**
 * Parse and when necessary recurse through a NitroROM and add files
 *
 * @returns non-zero on success, zero on error
 */
static int recurse_dir_table(
    PHYSFS_Io* io, void* arc, const char* parent, fnt_entry_main_t* current_entry, char* start_of_fnt, fat_entry_t* start_of_fat, int max_fat_entries)
{
    BAIL_IF_ERRPASS(max_fat_entries < 1, 0);

    fnt_entry_sub_t* next = (fnt_entry_sub_t*)start_of_fnt + current_entry->sub_entry_offset;

    size_t parent_len = strlen(parent);
    std::vector<char> name_buf;
    name_buf.resize(parent_len + 1 + 128);
    char* name = name_buf.data();
    if (!name)
        return 1;
    memcpy(name, parent, parent_len);
    name[parent_len++] = '/';
    name[parent_len] = '\0';
    int file_id = PHYSFS_swapULE16(current_entry->first_fat_entry_id);
    bool is_dir;
    int name_len;
    int sub_dir_id;
    while (fnt_entry_subtable_parse(&next, is_dir, &name[parent_len], name_len, sub_dir_id))
    {
        if (is_dir)
        {
            BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 1, -1, -1, 0, 0), 0);
            fnt_entry_main_t* next_dir_table = &((fnt_entry_main_t*)start_of_fnt)[sub_dir_id];
            BAIL_IF_ERRPASS(!recurse_dir_table(io, arc, name, next_dir_table, start_of_fnt, start_of_fat, max_fat_entries), 0);
        }
        else
        {
            BAIL_IF_ERRPASS(max_fat_entries <= file_id, 0);
            fat_entry_t fat_entry = start_of_fat[file_id];
            PHYSFS_swapULE32(fat_entry.start);
            PHYSFS_swapULE32(fat_entry.end);
            BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, -1, -1, fat_entry.start, fat_entry.end - fat_entry.start), 0);
            file_id++;
        }
    }
    return 1;
}

/**
 * Loads the FAT and FNT NitroRom tables and recurses through the file system structure adding files and directories
 */
static int dump_dir_structure(PHYSFS_Io* io, void* arc, nds_cartridge_header_t header)
{
    std::vector<char> fat_buffer;
    std::vector<char> fnt_buffer;

    fat_buffer.resize(header.file_allocation_table_size + sizeof(fat_entry_t), 0);
    fnt_buffer.resize(header.file_name_table_size + sizeof(fnt_entry_main_t), 0);

    fat_entry_t* start_of_fat = (fat_entry_t*)fat_buffer.data();
    fnt_entry_main_t* root_fnt_entry = (fnt_entry_main_t*)fnt_buffer.data();

    BAIL_IF_ERRPASS(!io->seek(io, header.file_allocation_table_offset), 0);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, start_of_fat, header.file_allocation_table_size), 0);

    BAIL_IF_ERRPASS(!io->seek(io, header.file_name_table_offset), 0);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, root_fnt_entry, header.file_name_table_size), 0);

    int max_fat_entries = header.file_allocation_table_size / sizeof(fat_entry_t);

    return recurse_dir_table(io, arc, "nitrofs", root_fnt_entry, (char*)root_fnt_entry, start_of_fat, max_fat_entries);
}

static void* NDS_add_entry_manual(void* opaque, const char* name, const int isdir, const PHYSFS_uint64 pos, const PHYSFS_uint64 len)
{
    std::vector<char> buf;
    buf.resize(strlen(name) + 1);
    strncpy(buf.data(), name, buf.size());
    return UNPK_addEntry(opaque, buf.data(), isdir, -1, -1, pos, len);
}

static int NDS_load_entries(PHYSFS_Io* io, const nds_cartridge_header_t header, void* arc)
{
#define ADD_FILE(name, offset, size) BAIL_IF_ERRPASS(!NDS_add_entry_manual(arc, name, 0, offset, size), 0)
#define ADD_DIR(name, offset, size) BAIL_IF_ERRPASS(!NDS_add_entry_manual(arc, name, 1, 0, 0), 0)
    ADD_FILE("header", 0, header.rom_size_header);

    ADD_FILE("bin/arm7.bin", header.arm7_rom_offset, header.arm7_size);
    ADD_FILE("bin/arm9.bin", header.arm9_rom_offset, header.arm9_size);

    BAIL_IF_ERRPASS(!dump_dir_structure(io, arc, header), 0);

    return 1;
#undef ADD_DIR
#undef ADD_FILE
}

static void* NDS_open_archive(PHYSFS_Io* io, const char* name, int forWriting, int* claimed)
{
    PHYSFS_uint8 buf[NDS_CARTRIDGE_HEADER_SIZE];
    PHYSFS_uint32 count;
    PHYSFS_uint32 directoryOffset;
    void* unpkarc;

    assert(io != NULL);

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, sizeof(buf)), NULL);

    /* nds_cartridge_header_t corrects for endian values */
    nds_cartridge_header_t header((char*)buf);

    /* TODO-OPT: Maybe do better checking for if this is actually an NDS archive? */
    /* TODO-OPT: Should we check the CRC? */
    if (!header.seems_valid_enough(0))
        BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

    *claimed = 1;

    unpkarc = UNPK_openArchive(io, 0, 1);
    BAIL_IF_ERRPASS(!unpkarc, NULL);

    if (!NDS_load_entries(io, header, unpkarc))
    {
        UNPK_abandonArchive(unpkarc);
        return NULL;
    }

    return unpkarc;
}

PHYSFS_Archiver MPH_TETRA_PHYSFS_Archiver_NDS = {
    .version = CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    .info = {
        "NDS",
        "NDS ROM files",
        "Ian Hangartner <icrashstuff at outlook dot com>",
        "https://github.com/icrashstuff/mph_tetra",
        0,
    },
    .openArchive = NDS_open_archive,
    .enumerate = UNPK_enumerate,
    .openRead = UNPK_openRead,
    .openWrite = UNPK_openWrite,
    .openAppend = UNPK_openAppend,
    .remove = UNPK_remove,
    .mkdir = UNPK_mkdir,
    .stat = UNPK_stat,
    .closeArchive = UNPK_closeArchive
};
