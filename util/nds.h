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
#ifndef MPH_TETRA_UTIL_NDS_H
#define MPH_TETRA_UTIL_NDS_H

#include <SDL_bits.h>

#include <string>

#define NDS_CARTRIDGE_HEADER_SIZE 512

/**
 * Implemented from from GBATEK specs > DS > DS Cartridge Header
 *
 * The specs of this struct are from the GBATEK GBA/NDS Technical Info document version 3.05
 * found here: https://problemkaputt.de/gbatek.htm
 *
 * Comments are for fields where MPH Tetra has use for them
 *
 * All fields are entered for the sake of completeness rather then necessity
 */
struct nds_cartridge_header_t
{
    enum device_capacity_t : Uint8
    {
        NDS_CAPACITY_128KB = 0,
        NDS_CAPACITY_256KB = 1,
        NDS_CAPACITY_512KB = 2,
        NDS_CAPACITY_1MB = 3,
        NDS_CAPACITY_2MB = 4,
        NDS_CAPACITY_4MB = 5,
        NDS_CAPACITY_8MB = 6,
        NDS_CAPACITY_16MB = 7,
        NDS_CAPACITY_32MB = 8,
        NDS_CAPACITY_64MB = 9,
        NDS_CAPACITY_128MB = 10,
        NDS_CAPACITY_256MB = 11,
        NDS_CAPACITY_512MB = 12,
    };

    /**
     * Null terminated */
    char game_title[12];
    /**
     * In our case AMHE, AMHP, ...
     */
    char game_code[4];
    /**
     * In our case 01
     */
    char maker_code[2];
    Uint8 unit_code;
    Uint8 encryption_seed_select;
    /**
     * In our case: probably [7: 16MB (Demo), 9: 64MB (Release)]
     */
    device_capacity_t device_capacity;
    Uint8 reserved_0[7];
    Uint8 reserved_1;
    Uint8 nds_region;
    /**
     * In our case [0: v1.0, 1: v1.1]
     */
    Uint8 rom_version;

    Uint32 arm9_rom_offset;
    Uint32 arm9_address_entry;
    Uint32 arm9_address_ram;
    Uint32 arm9_size;

    Uint32 arm7_rom_offset;
    Uint32 arm7_address_entry;
    Uint32 arm7_address_ram;
    Uint32 arm7_size;

    Uint32 file_name_table_offset;
    Uint32 file_name_table_size;
    Uint32 file_allocation_table_offset;
    Uint32 file_allocation_table_size;

    Uint32 arm9_overlay_offset;
    Uint32 arm9_overlay_size;

    Uint32 arm7_overlay_offset;
    Uint32 arm7_overlay_size;

    Uint32 port_40001A4_setting_normal;
    Uint32 port_40001A4_setting_key1;

    Uint32 icon_title_offset;

    Uint16 secure_area_crc16;
    Uint16 secure_area_delay;

    Uint32 arm9_auto_load_list_hook_address_ram;
    Uint32 arm7_auto_load_list_hook_address_ram;

    Uint64 secure_area_disable;

    Uint32 rom_size_total_used;
    Uint32 rom_size_header;

    Uint32 unknown;
    char reserved_2[8];

    Uint16 nand_end_of_rom_area;
    Uint16 nand_start_of_rw_area;

    char reserved_3[0x18];
    char reserved_4[0x10];

    char logo[0x9C];
    char logo_crc16;

    Uint16 header_crc16;

    Uint32 debug_rom_offset;
    Uint32 debug_size;
    Uint32 debug_ram_address;

    /* This reserved space is only kept to pad the struct to 512 bytes */
    char reserved_5_padding[4];
    char reserved_6_padding[0x90];

    /**
     * Initialize header from raw data
     *
     * Also corrects for endianness values if correct_endian == true
     */
    nds_cartridge_header_t(char raw_data[NDS_CARTRIDGE_HEADER_SIZE]);

    /**
     * Returns a user friendly name by decoding game_code, game_title, and rom_version
     *
     * Format: "%game_title% %region% (rev %rom_version%)"
     */
    std::string get_friendly_game_name();

    /**
     * Returns a more informative game code
     *
     * Format: "%game_code% (rev %rom_version%)"
     */
    std::string get_friendly_game_code();

    /**
     * Returns an ideal filename for the rom based on the rom header with file system friendly characters
     *
     * Format: "%game_title%-%game_code%-%maker_code%-rev%rom_version%.nds"
     */
    std::string get_suitable_filename();

    /**
     * Returns true if this is a recognized first hunt rom
     */
    bool is_mph_first_hunt();

    /**
     * Returns true if this is a recognized release rom
     */
    bool is_mph_release();

    /**
     * Returns true if this is a recognized kiosk rom
     */
    bool is_mph_kiosk();

    /**
     * Returns true if this is a recognized rom
     */
    inline bool is_mph_recognized() { return is_mph_release() || is_mph_first_hunt() || is_mph_kiosk(); }

    /**
     * Returns true if the rom header seems valid enough
     *
     * Checks the size and offset of FNT, FAT, ARM9, ARM7, ARM9 overlay, ARM7 overlay
     * Checks icon_title_offset
     *
     * @param check_crc Compute a new header_crc16 field and check against the existing one
     */
    bool seems_valid_enough(bool check_crc = true);

    /**
     * Computes a new header_crc16
     */
    Uint16 compute_header_crc16();
};

static_assert(NDS_CARTRIDGE_HEADER_SIZE == sizeof(nds_cartridge_header_t), "NDS_CARTRIDGE_SIZE does not match nds_cartridge_header_t size!");
#endif
