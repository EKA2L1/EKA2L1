/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <kernel/codeseg.h>
#include <kernel/kernel.h>
#include <kernel/libmanager.h>
#include <services/fbs/fbs.h>
#include <services/fbs/palette.h>
#include <system/epoc.h>

#include <cstring>

namespace eka2l1::epoc {
    static bool read_rom_data(const std::uint8_t *code, const std::size_t size, const std::uint32_t code_address,
        const std::uint64_t address, void *out, const std::size_t count) {
        if (!code || address < code_address || address - code_address > size
            || count > size - (address - code_address)) {
            return false;
        }
        std::memcpy(out, code + (address - code_address), count);
        return true;
    }

    std::optional<palette_256> read_rom_palette_256(const std::uint8_t *code, const std::size_t size,
        const std::uint32_t code_address, const std::uint32_t getter, const std::uint32_t setter) {
        const auto read = [&](const std::uint64_t address, void *out, const std::size_t count) {
            return read_rom_data(code, size, code_address, address, out, count);
        };

        // Only immutable palettes are safe to cache: the setter must immediately return.
        if (setter & 1) {
            std::uint16_t op;
            if (!read(setter & ~1U, &op, sizeof(op)) || op != 0x4770) {
                return std::nullopt;
            }
        } else {
            std::uint32_t op;
            if (!read(setter, &op, sizeof(op)) || (op != 0xe12fff1e && op != 0xe1a0f00e)) {
                return std::nullopt;
            }
        }

        std::uint64_t literal;
        if (getter & 1) {
            std::uint16_t ops[2];
            if (!read(getter & ~1U, ops, sizeof(ops)) || (ops[0] & 0xff00) != 0x4800 || ops[1] != 0x4770) {
                return std::nullopt;
            }
            // Thumb LDR r0,[pc,#imm]; BX lr, with the architectural aligned PC.
            literal = ((static_cast<std::uint64_t>(getter & ~1U) + 4) & ~3ULL) + (ops[0] & 0xff) * 4;
        } else {
            std::uint32_t ops[2];
            if (!read(getter, ops, sizeof(ops)) || (ops[0] & 0xff7ff000) != 0xe51f0000
                || (ops[1] != 0xe12fff1e && ops[1] != 0xe1a0f00e)) {
                return std::nullopt;
            }
            literal = static_cast<std::uint64_t>(getter) + 8;
            const auto displacement = ops[0] & 0xfff;
            if (!(ops[0] & 0x800000) && literal < displacement) {
                return std::nullopt;
            }
            literal = (ops[0] & 0x800000) ? literal + displacement : literal - displacement;
        }

        std::uint32_t table;
        palette_256 result;
        // TColor256Util begins with iColorTable[256], followed by the inverse lookup table.
        if (!read(literal, &table, sizeof(table)) || (table & 3) || !read(table, result.data(), sizeof(result))) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<palette_256> read_rom_gdi_palette_256(const std::uint8_t *code, const std::size_t size,
        const std::uint32_t code_address, const std::uint32_t color256) {
        std::uint32_t ops[5];
        // ARM TRgb::Color256: load a table literal, mask the scaled index, load the entry, return.
        if ((color256 & 3) || !read_rom_data(code, size, code_address, color256, ops, sizeof(ops))
            || (ops[0] & 0xfffff000) != 0xe59f3000 || ops[1] != 0xe1a00100
            || ops[2] != 0xe2000fff || ops[3] != 0xe7930000 || ops[4] != 0xe12fff1e) {
            return std::nullopt;
        }
        const auto literal = static_cast<std::uint64_t>(color256) + 8 + (ops[0] & 0xfff);
        std::uint32_t table;
        palette_256 result;
        if (!read_rom_data(code, size, code_address, literal, &table, sizeof(table)) || (table & 3)
            || !read_rom_data(code, size, code_address, table, result.data(), sizeof(result))) {
            return std::nullopt;
        }
        return result;
    }
}

namespace eka2l1 {
    void fbs_server::initialize_palette() {
        palette_256_ = epoc::get_suitable_palette_256(sys->get_symbian_version_use());
        std::optional<epoc::palette_256> rom_palette;
        if (const auto palette = sys->get_lib_manager()->load(u"palette.dll"); palette && palette->is_rom()) {
            std::uint8_t *code = nullptr;
            const auto code_address = palette->get_code_run_addr(nullptr, &code);
            // DynamicPalette ordinals 3/4 are DefaultColor256Util/SetColor256Util.
            rom_palette = epoc::read_rom_palette_256(code, palette->get_code_size(),
                code_address, palette->lookup_no_relocate(3), palette->lookup_no_relocate(4));
        }
        if (!rom_palette && kern->is_eka1()) {
            if (const auto gdi = sys->get_lib_manager()->load(u"gdi.dll"); gdi && gdi->is_rom()) {
                std::uint8_t *code = nullptr;
                const auto code_address = gdi->get_code_run_addr(nullptr, &code);
                // EKA1 GDI ordinal 164 is TRgb::Color256(TInt).
                rom_palette = epoc::read_rom_gdi_palette_256(code, gdi->get_code_size(),
                    code_address, gdi->lookup_no_relocate(164));
            }
        }
        if (rom_palette) {
            palette_256_ = *rom_palette;
        }
    }
}
