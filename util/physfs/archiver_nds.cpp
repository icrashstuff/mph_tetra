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
 *   + fnt.bin
 *   + fat.bin
 *   + banner.bin  (If it exists)
 *   + arm9_ovt.bin (If it exists)
 *   + arm9_overlays  (If it exists)
 *     + overlay_0
 *     + overlay_1, and so on
 *   + arm7_ovt.bin  (If it exists)
 *   + arm7_overlays  (If it exists)
 *     + overlay_0
 *     + overlay_1, and so on
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

    /**
     * Returns an endian corrected version of the struct
     */
    fat_entry_t endian_correct()
    {
        fat_entry_t temp;
        temp.start = PHYSFS_swapULE32(start);
        temp.end = PHYSFS_swapULE32(end);
        return temp;
    }
};

struct fnt_entry_main_t
{
    /**
     * Offset is relative to the nds_cartridge_header_t.file_name_table_offset
     */
    Uint32 sub_entry_offset;

    /**
     * Fat entry id of the first fnt_entry_sub_t
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

    /**
     * Returns an endian corrected version of the struct
     */
    fnt_entry_main_t endian_correct()
    {
        fnt_entry_main_t temp;
        temp.sub_entry_offset = PHYSFS_swapULE32(sub_entry_offset);
        temp.first_fat_entry_id = PHYSFS_swapULE16(first_fat_entry_id);
        temp.parent_id = PHYSFS_swapULE16(parent_id);
        return temp;
    }
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

struct overlay_table_entry_t
{
    Uint32 overlay_id;
    Uint32 ram_address;
    Uint32 ram_size;
    Uint32 bss_size;
    Uint32 static_initializer_address_start;
    Uint32 static_initializer_address_end;
    Uint32 fat_file_id;
    Uint32 reserved;

    /**
     * Returns an endian corrected version of the struct
     */
    overlay_table_entry_t endian_correct()
    {
        overlay_table_entry_t temp;
        temp.overlay_id = PHYSFS_swapULE32(overlay_id);
        temp.ram_address = PHYSFS_swapULE32(ram_address);
        temp.ram_size = PHYSFS_swapULE32(ram_size);
        temp.bss_size = PHYSFS_swapULE32(bss_size);
        temp.static_initializer_address_start = PHYSFS_swapULE32(static_initializer_address_start);
        temp.static_initializer_address_end = PHYSFS_swapULE32(static_initializer_address_end);
        temp.fat_file_id = PHYSFS_swapULE32(fat_file_id);
        temp.reserved = PHYSFS_swapULE32(reserved);
        return temp;
    }
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
    PHYSFS_Io* io, void* arc, const char* parent, fnt_entry_main_t* _current_entry, char* start_of_fnt, fat_entry_t* start_of_fat, int max_fat_entries)
{
    BAIL_IF_ERRPASS(max_fat_entries < 1, 0);

    fnt_entry_main_t current_entry = _current_entry->endian_correct();

    fnt_entry_sub_t* next = (fnt_entry_sub_t*)start_of_fnt + current_entry.sub_entry_offset;

    size_t parent_len = strlen(parent);
    std::vector<char> name_buf;
    name_buf.resize(parent_len + 1 + 128);
    char* name = name_buf.data();
    if (!name)
        return 1;
    memcpy(name, parent, parent_len);
    name[parent_len++] = '/';
    name[parent_len] = '\0';
    int file_id = current_entry.first_fat_entry_id;
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
            fat_entry.endian_correct();
            BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, -1, -1, fat_entry.start, fat_entry.end - fat_entry.start), 0);
            file_id++;
        }
    }
    return 1;
}

/**
 * Read from data PHYSFS_Io to a std::vector<char> buffer
 *
 * @returns non-zero on success, zero on error
 */
int read_to_buffer(PHYSFS_Io* io, std::vector<char>& buf, Uint32 offset, Uint32 len)
{
    buf.resize(len + 8, 0);

    BAIL_IF_ERRPASS(!io->seek(io, offset), 0);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf.data(), len), 0);
    BAIL_IF_ERRPASS(!buf.data(), 0);

    return 1;
}

/**
 * Manually add an entry
 *
 * Wrapper around UNPK_addEntry() to allow for name to be const
 *
 * @returns whatever UNPK_addEntry would return
 */
static void* NDS_add_entry_manual(void* opaque, const char* name, const int isdir, const PHYSFS_uint64 pos, const PHYSFS_uint64 len)
{
    std::vector<char> buf;
    buf.resize(strlen(name) + 1);
    strncpy(buf.data(), name, buf.size());
    return UNPK_addEntry(opaque, buf.data(), isdir, -1, -1, pos, len);
}

#define ADD_FILE(name, offset, size) BAIL_IF_ERRPASS(!NDS_add_entry_manual(arc, name, 0, offset, size), 0)
#define ADD_DIR(name) BAIL_IF_ERRPASS(!NDS_add_entry_manual(arc, name, 1, 0, 0), 0)

/**
 * Parses the NDS OVT (Overlay Table) (If it exists) and adds the following to the directory structure
 * OVT Table: "bin/%prefix%_ovt.bin"
 * Overlays: "bin/%prefix%_overlays/overlay_%overlay_id%"
 */
static int NDS_load_overlay_table(PHYSFS_Io* io, void* arc, Uint32 offset, Uint32 size, const char* prefix, fat_entry_t* start_of_fat, Uint32 max_fat_entries)
{
    if (offset && size)
    {
        std::string ovt_prefix = std::string("bin/") + prefix;
        std::string ovt_name_file = ovt_prefix + "_ovt.bin";
        std::string ovt_entry_pre = ovt_prefix + "_overlays/overlay_";

        ADD_FILE(ovt_name_file.c_str(), offset, size);
        if (size % 32 == 0)
        {
            std::vector<char> overlay_data;
            BAIL_IF_ERRPASS(!read_to_buffer(io, overlay_data, offset, size), 0);

            int num_overlays = size / 32;

            overlay_table_entry_t* cur = (overlay_table_entry_t*)overlay_data.data();
            for (int i = 0; i < num_overlays; i++)
            {
                overlay_table_entry_t ovte = cur->endian_correct();
                BAIL_IF_ERRPASS(max_fat_entries <= ovte.fat_file_id, 0);
                fat_entry_t fat_entry = start_of_fat[ovte.fat_file_id].endian_correct();
                ADD_FILE((ovt_entry_pre + std::to_string(ovte.overlay_id)).c_str(), fat_entry.start, fat_entry.end - fat_entry.start);
                cur = &cur[1];
            }
        }
    }

    return 1;
}

static int NDS_load_entries(PHYSFS_Io* io, const nds_cartridge_header_t header, void* arc)
{
    std::vector<char> fat_buffer;
    std::vector<char> fnt_buffer;

    BAIL_IF_ERRPASS(!read_to_buffer(io, fat_buffer, header.file_allocation_table_offset, header.file_allocation_table_size), 0);
    BAIL_IF_ERRPASS(!read_to_buffer(io, fnt_buffer, header.file_name_table_offset, header.file_name_table_size), 0);

    fat_entry_t* start_of_fat = (fat_entry_t*)fat_buffer.data();
    fnt_entry_main_t* root_fnt_entry = (fnt_entry_main_t*)fnt_buffer.data();

    int max_fat_entries = header.file_allocation_table_size / sizeof(fat_entry_t);

    ADD_FILE("header", 0, header.rom_size_header);

    ADD_FILE("bin/arm7.bin", header.arm7_rom_offset, header.arm7_size);
    ADD_FILE("bin/arm9.bin", header.arm9_rom_offset, header.arm9_size);

    ADD_FILE("bin/fat.bin", header.file_allocation_table_offset, header.file_allocation_table_size);
    ADD_FILE("bin/fnt.bin", header.file_name_table_offset, header.file_name_table_size);

    BAIL_IF_ERRPASS(!NDS_load_overlay_table(io, arc, header.arm7_overlay_offset, header.arm7_overlay_size, "arm7", start_of_fat, max_fat_entries), 0);
    BAIL_IF_ERRPASS(!NDS_load_overlay_table(io, arc, header.arm9_overlay_offset, header.arm9_overlay_size, "arm9", start_of_fat, max_fat_entries), 0);

    if (header.icon_title_offset)
        ADD_FILE("bin/banner.bin", header.icon_title_offset, 0x840);

    BAIL_IF_ERRPASS(!recurse_dir_table(io, arc, "nitrofs", root_fnt_entry, (char*)root_fnt_entry, start_of_fat, max_fat_entries), 0);

    return 1;
}

#undef ADD_DIR
#undef ADD_FILE

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
