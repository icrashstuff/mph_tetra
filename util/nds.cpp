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
#include "nds.h"
#include "misc.h"
#include <stdio.h>

struct rom_data
{
    const char* code;
    Uint8 ver;
};

/**
 * Pulled from MphRead
 */
static const rom_data roms_kiosk[] = { { "A76E", 0 } };

/**
 * Pulled from MphRead
 */
static const rom_data roms_release[] = {
    { "AMHE", 0 },
    { "AMHE", 1 },
    { "AMHP", 0 },
    { "AMHP", 1 },
    { "AMHJ", 0 },
    { "AMHJ", 1 },
    { "AMHK", 0 },
};

/**
 * Pulled from MphRead
 */
static const rom_data roms_first_hunt[] = { { "AMFE", 0 }, { "AMFP", 0 } };

nds_cartridge_header_t::nds_cartridge_header_t(char raw_data[NDS_CARTRIDGE_HEADER_SIZE])
{
    memcpy(this, raw_data, NDS_CARTRIDGE_HEADER_SIZE);

    ASSERT_SwapLE32(arm9_rom_offset);
    ASSERT_SwapLE32(arm9_address_entry);
    ASSERT_SwapLE32(arm9_address_ram);
    ASSERT_SwapLE32(arm9_size);

    ASSERT_SwapLE32(arm7_rom_offset);
    ASSERT_SwapLE32(arm7_address_entry);
    ASSERT_SwapLE32(arm7_address_ram);
    ASSERT_SwapLE32(arm7_size);

    ASSERT_SwapLE32(file_name_table_offset);
    ASSERT_SwapLE32(file_name_table_size);
    ASSERT_SwapLE32(file_allocation_table_offset);
    ASSERT_SwapLE32(file_allocation_table_size);

    ASSERT_SwapLE32(arm9_overlay_offset);
    ASSERT_SwapLE32(arm9_overlay_size);

    ASSERT_SwapLE32(arm7_overlay_offset);
    ASSERT_SwapLE32(arm7_overlay_size);

    ASSERT_SwapLE32(port_40001A4_setting_normal);
    ASSERT_SwapLE32(port_40001A4_setting_key1);

    ASSERT_SwapLE32(icon_title_offset);

    ASSERT_SwapLE16(secure_area_crc16);
    ASSERT_SwapLE16(secure_area_delay);

    ASSERT_SwapLE32(arm9_auto_load_list_hook_address_ram);
    ASSERT_SwapLE32(arm7_auto_load_list_hook_address_ram);

    ASSERT_SwapLE64(secure_area_disable);

    ASSERT_SwapLE32(rom_size_total_used);
    ASSERT_SwapLE32(rom_size_header);

    ASSERT_SwapLE32(unknown);

    ASSERT_SwapLE16(nand_end_of_rom_area);
    ASSERT_SwapLE16(nand_start_of_rw_area);

    ASSERT_SwapLE16(header_crc16);

    ASSERT_SwapLE32(debug_rom_offset);
    ASSERT_SwapLE32(debug_size);
    ASSERT_SwapLE32(debug_ram_address);
}

std::string nds_cartridge_header_t::get_friendly_game_name()
{
    char buffer[64];
    const char* region = "Unknown Region";
    const char* kiosk_text = "";

    if (is_mph_kiosk())
        kiosk_text = " (Kiosk)";

#define REGION(c, str) \
    case c:            \
        region = str;  \
        break
    switch (game_code[3])
    {
        REGION('E', "USA");
        REGION('P', "EUR");
        REGION('J', "JPN");
        REGION('K', "KOR");
    default:
        break;
    }
#undef R

    static_assert(12 == sizeof(game_title), "FMT String needs updating");

    snprintf(buffer, sizeof(buffer), "%.12s%s %s (rev %u)", game_title, kiosk_text, region, rom_version);
    std::string s(buffer);
    return s;
}

std::string nds_cartridge_header_t::get_friendly_game_code()
{
    char buffer[64];

    static_assert(4 == sizeof(game_code), "FMT String needs updating");

    snprintf(buffer, sizeof(buffer), "%.4s (rev %u)", game_code, rom_version);
    std::string s(buffer);
    return s;
}

std::string nds_cartridge_header_t::get_suitable_filename()
{
    char _game_title[sizeof(game_title)];
    char buffer[64];
    const char* kiosk_text = "";

    if (is_mph_kiosk())
        kiosk_text = "-Kiosk";

    for (int i = 0; i < sizeof(_game_title); i++)
    {
        char c = game_title[i];
        if (islower(c) || isupper(c) || isdigit(c) || c == '\0')
            _game_title[i] = c;
        else
            _game_title[i] = '_';
    }

    static_assert(12 == sizeof(game_title), "FMT String needs updating");
    static_assert(4 == sizeof(game_code), "FMT String needs updating");
    static_assert(2 == sizeof(maker_code), "FMT String needs updating");

    snprintf(buffer, sizeof(buffer), "%.12s%s-%.4s-%.2s-rev%u.nds", _game_title, kiosk_text, game_code, maker_code, rom_version);
    std::string s(buffer);
    return s;
}

bool nds_cartridge_header_t::is_mph_first_hunt()
{
    for (size_t i = 0; i < SDL_arraysize(roms_first_hunt); i++)
        if (strncmp(game_code, roms_first_hunt[i].code, 4) == 0 && rom_version == roms_first_hunt[i].ver)
            return true;
    return false;
}

bool nds_cartridge_header_t::is_mph_kiosk()
{
    for (size_t i = 0; i < SDL_arraysize(roms_kiosk); i++)
        if (strncmp(game_code, roms_kiosk[i].code, 4) == 0 && rom_version == roms_kiosk[i].ver)
            return true;
    return false;
}

bool nds_cartridge_header_t::is_mph_release()
{
    for (size_t i = 0; i < SDL_arraysize(roms_release); i++)
        if (strncmp(game_code, roms_release[i].code, 4) == 0 && rom_version == roms_release[i].ver)
            return true;
    return false;
}

Uint16 nds_cartridge_header_t::compute_header_crc16()
{
    /* Reverse endianness corrections for crc16 */
    nds_cartridge_header_t h2((char*)this);

    return SDL_crc16(0xFFFF, &h2, offsetof(nds_cartridge_header_t, header_crc16));
}

bool nds_cartridge_header_t::seems_valid_enough(bool check_crc)
{
    // GBATEK seems to indicate that this should be 0x4000 but portalDS.nds has a value of 0x0200...
    // so we'll just ensure it is at least offsetof(nds_cartridge_header_t, header_crc16)
    if (rom_size_header <= offsetof(nds_cartridge_header_t, header_crc16))
        return false;

    /* TODO-OPT: Check: must NDS rom have an arm9 binary? (In our case yes, but in general must it?) */
    /* GBATEK seems to indicate that arm9_rom_offset should be at least 0x4000, but portalDS.nds has a value of 0x0200*/
    if (arm9_address_entry < 0x02000000 || arm9_address_ram < 0x02000000 || !arm9_size || arm9_rom_offset < rom_size_header)
        return false;

    /* TODO-OPT: Check: must NDS rom have an arm7 binary? (In our case yes, but in general must it?) */
    if (arm7_address_entry < 0x02000000 || arm7_address_ram < 0x02000000 || !arm7_size || arm7_rom_offset < rom_size_header)
        return false;

        /** If the offset is non zero then the size should probably not be 0 */
#define NOT_ZERO_IF_OFF(name)          \
    if (name##_offset && !name##_size) \
    {                                  \
        return 4;                      \
    }

    NOT_ZERO_IF_OFF(file_allocation_table);
    NOT_ZERO_IF_OFF(file_name_table);
    NOT_ZERO_IF_OFF(arm9_overlay);
    NOT_ZERO_IF_OFF(arm7_overlay);

    if (icon_title_offset && icon_title_offset < 0x8000)
        return false;

    if (check_crc && compute_header_crc16() != header_crc16)
        return false;

    return true;
}
