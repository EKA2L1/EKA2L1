/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/unicode.h>
#include <common/log.h>
#include <vector>

namespace eka2l1::common {
    static std::uint32_t dyn_window_default[8] = {
        0x0080, // Latin-1 supplement
        0x00C0, // parts of Latin-1 supplement and Latin Extended-A
        0x0400, // Cyrillic
        0x0600, // Arabic
        0x0900, // Devanagari
        0x3040, // Hiragana
        0x30A0, // Katakana
        0xFF00 // Fullwidth ASCII
    };

    void unicode_comp_state::reset() {
        active_window_base = 0x0080;
        unicode_mode = false;

        std::copy(dyn_window_default, dyn_window_default + 8, dynamic_windows);

        static_windows[0] = 0x0000; // tags
        static_windows[1] = 0x0080; // Latin-1 supplement
        static_windows[2] = 0x0100; // Latin Extended-A
        static_windows[3] = 0x0300; // Combining Diacritics
        static_windows[4] = 0x2000; // General Punctuation
        static_windows[5] = 0x2080; // Currency Symbols
        static_windows[6] = 0x2100; // Letterlike Symbols and Number Forms
        static_windows[7] = 0x3000; // CJK Symbols and Punctuation

        special_bases[0] = 0x00C0;
        special_bases[1] = 0x0250;
        special_bases[2] = 0x0370;
        special_bases[3] = 0x0530;
        special_bases[4] = 0x3040;
        special_bases[5] = 0x30A0;
        special_bases[6] = 0xFF60;
    }

    std::uint32_t unicode_comp_state::dynamic_window_base(int offset_index) {
        if (offset_index >= 0xF9 && offset_index <= 0xFF) {
            return special_bases[offset_index - 0xF9];
        }

        if (offset_index >= 0x01 && offset_index <= 0x67) {
            return offset_index * 0x80;
        }

        if (offset_index >= 0x68 && offset_index <= 0xA7) {
            return offset_index * 0x80 + 0xAC00;
        }

        return 0;
    }

    std::int32_t unicode_comp_state::dynamic_window_offset_index(std::uint16_t code) {
        if (code < 0x0080) {
            return -1;
        }

        if ((code >= 0x3400) && (code <= 0xDFFF)) {
            return -1;
        }

        // Original comments: Prefer sections that cross half-block boundaries. These are better adapted to actual text.
        // They are represented by offset indices 0xf9..0xff.
        for (std::int32_t i = 0; i < special_base_size; i++) {
            if ((code >= special_bases[i]) && (code < special_bases[i] + 128)) {
                return 0xF9 + i;
            }
        }

        if (code >= 0xE000) {
            code -= 0xAC00;
        }

        return code / 0x80;
    }

    std::int32_t unicode_comp_state::static_window_index(std::uint16_t code) {
        for (std::int32_t i = 0; i < static_windows_size; i++) {
            if ((code >= static_windows[i]) && (code < static_windows[i] + 128))
                return i;
        }

        return -1;
    }

    bool unicode_stream::read_byte(std::uint8_t *dat) {
        if (source_size <= 0) {
            return false;
        }

        *dat = *(source_buf + (source_pointer++));
        source_size--;

        return true;
    }

    bool unicode_stream::read_byte16(std::uint16_t *dat) {
        std::uint8_t b1, b2 = 0;

        if (!read_byte(&b1)) {
            return false;
        }

        if (!read_byte(&b2)) {
            return false;
        }

        *dat = (b2 << 8) | b1;
        return true;
    }

    bool unicode_stream::write_byte8(std::uint8_t b) {
        if (dest_size <= 0) {
            return false;
        }

        if (dest_buf) {
            *(dest_buf + (dest_pointer++)) = b;
        }

        dest_size--;

        return true;
    }

    bool unicode_stream::write_byte(std::uint16_t b) {
        if (!write_byte8(static_cast<std::uint8_t>(b))) {
            return false;
        }

        bool result = write_byte8(b >> 8);
        return result;
    }

    bool unicode_stream::write_byte_be(std::uint16_t b) {
        if (!write_byte8(static_cast<std::uint8_t>(b >> 8))) {
            return false;
        }

        return write_byte8(b & 0xFF);
    }

    bool unicode_stream::write_byte32(std::uint32_t b) {
        if (b <= 0xFFFF) {
            return write_byte(static_cast<std::uint16_t>(b));
        } else if (b <= 0x10FFFF) {
            b -= 0x10000;

            if (!write_byte(static_cast<std::uint16_t>(0xD800 + (b >> 10))) || !write_byte(static_cast<std::uint16_t>(0xDC00 + (b & 0x03FF)))) {
                return false;
            }

            return true;
        }

        return false;
    }

    bool unicode_expander::define_window(const int index) {
        std::uint8_t win;

        if (!read_byte(&win)) {
            return false;
        }

        unicode_mode = false;
        active_window_base = dynamic_window_base(win);
        dynamic_windows[index] = active_window_base;

        return true;
    }

    bool unicode_expander::define_expansion_window() {
        std::uint8_t hi = 0;
        std::uint8_t lo = 0;

        if (read_byte(&hi) && read_byte(&lo)) {
            unicode_mode = false;
            active_window_base = 0x1000 + (0x80 * ((hi & 0x1F) * 0x100 + lo));
            dynamic_windows[hi >> 5] = active_window_base;

            return true;
        }

        return false;
    }

    enum {
        UDX = 0xF1,
        UQU = 0xF0,
        UD0 = 0xE8,
        UC0 = 0xE0,
        SD0 = 0x18,
        SC0 = 0x10,
        SCU = 0x0F,
        SQU = 0x0E,
        SDX = 0x0B,
        SQ0 = 0x01
    };

    bool unicode_expander::quote_unicode() {
        // Read two bytes
        std::uint8_t high;
        std::uint8_t low;

        if (!read_byte(&high)) {
            return false;
        }

        if (!read_byte(&low)) {
            return false;
        }

        std::uint16_t c = static_cast<std::uint16_t>((high << 8) | low);
        return write_byte(c);
    }

    bool unicode_expander::handle_ubyte(const std::uint8_t ubyte) {
        if (ubyte <= 0xDF || ubyte >= 0xF3) {
            std::uint8_t low;

            if (read_byte(&low)) {
                std::uint16_t c = static_cast<std::uint16_t>((ubyte << 8) | low);
                return write_byte(c);
            } else {
                return false;
            }
        }

        // Quote a Unicode character that would otherwise conflict with a tag.
        if (ubyte == UQU) {
            return quote_unicode();
        }

        if (ubyte >= UC0 && ubyte <= UC0 + 7) {
            active_window_base = dynamic_windows[ubyte - UC0];
            unicode_mode = false;

            return true;
        }

        if (ubyte >= UD0 && ubyte <= UD0 + 7) {
            return define_window(ubyte - UD0);
        }

        if (ubyte == UDX) {
            return define_expansion_window();
        }

        return false;
    }

    static bool can_i_passthrough(const std::uint16_t sbyte) {
        return (sbyte == 0x000 || sbyte == 0x0009 || sbyte == 0x000A || sbyte == 0x000D
            || (sbyte >= 0x0020 && sbyte <= 0x007F));
    }

    bool unicode_expander::handle_sbyte(const std::uint8_t sbyte) {
        if (can_i_passthrough(sbyte)) {
            return write_byte(sbyte);
        }

        if (sbyte >= 0x80) {
            return write_byte32(active_window_base + sbyte - 0x80);
        }

        if (sbyte == SQU) {
            return quote_unicode();
        }

        if (sbyte == SCU) {
            unicode_mode = true;
            return true;
        }

        // Quote from a window resides from SQ0 to SQ7
        if (sbyte >= SQ0 && sbyte <= SQ0 + 7) {
            int window = sbyte - SQ0;
            std::uint8_t byte;

            if (!read_byte(&byte)) {
                return false;
            }

            std::uint32_t c = byte;

            if (c <= 0x7F) {
                c += static_windows[window];
            } else {
                c += dynamic_windows[window] - 0x80;
            }

            if (!write_byte32(c)) {
                return false;
            } else {
                return true;
            }
        }

        if (sbyte >= SC0 && sbyte <= SC0 + 7) {
            active_window_base = dynamic_windows[sbyte - SC0];
            return true;
        }

        if (sbyte >= SD0 && sbyte <= SD0 + 7) {
            return define_window(sbyte - SD0);
        }

        if (sbyte == SDX) {
            return define_expansion_window();
        }

        return false;
    }

    int unicode_expander::expand(std::uint8_t *source, int &source_size, std::uint8_t *dest, int dest_size) {
        source_buf = source;
        dest_buf = dest;

        source_pointer = 0;
        dest_pointer = 0;

        this->source_size = source_size;
        this->dest_size = dest_size;

        unicode_mode = false;

        for (; this->dest_size > 0;) {
            std::uint8_t b;

            if (read_byte(&b)) {
                bool result = (unicode_mode ? handle_ubyte(b) : handle_sbyte(b));

                if (!result) {
                    break;
                }
            } else {
                break;
            }
        }

        source_size -= this->source_size;
        return dest_size - this->dest_size;
    }

    unicode_compressor::action::action(unicode_comp_state &state, const std::uint16_t code)
        : code_(code) {
        if (state.encode_as_is(code)) {
            treatment_ = unicode_char_treatment_plain_ascii;
        } else {
            treatment_ = state.dynamic_window_offset_index(code);
            
            if (treatment_ == -1) {
                treatment_ = state.static_window_index(code);

                if (treatment_ == -1) {
                    treatment_ = unicode_char_treatment_plain_unicode;
                } else {
                    treatment_ += unicode_char_treatment_first_static;
                }
            }
        }
    }

    bool unicode_compressor::write_uchar(std::uint16_t c) {
        if ((c >= 0xE000) && (c <= 0xF2FF)) {
            if (!write_byte8(UQU)) {
                return false;
            }
        }

        return write_byte_be(c);
    }

    bool unicode_compressor::write_schar(const action &c) {
        if (c.treatment_ == unicode_char_treatment_plain_ascii) {
            return write_byte8(static_cast<std::uint8_t>(c.code_ & 0xFF));
        }

        if ((c.treatment_ >= unicode_char_treatment_first_static) && (c.treatment_ <= unicode_char_treatment_last_static)) {
            const std::int32_t window = c.treatment_ - unicode_char_treatment_first_static;
            if (!write_byte8(static_cast<std::uint8_t>(SQ0 + window))) {
                return false;
            }

            if (!write_byte8(static_cast<std::uint8_t>(c.code_))) {
                return false;
            }

            return true;
        }

        // It's in the current active window. Fit it in as it is.
        if ((c.code_ >= active_window_base) && (c.code_ < active_window_base + 128)) {
            if (!write_byte8(static_cast<std::uint8_t>(c.code_ - active_window_base + 0x80))) {
                return false;
            }

            return true;
        }

        // Character in dynamic window can be written using sq0 too! Just need to adjust to fit in the window.
        for (std::int32_t i = 0; i < dynamic_windows_size; i++) {
            if ((c.code_ >= dynamic_windows[i]) && (c.code_ < dynamic_windows[i] + 128)) {
                if (!write_byte8(static_cast<std::uint8_t>(SQ0 + i))) {
                    return false;
                }
        
                if (!write_byte8(c.code_ - dynamic_windows[i] + 0x80)) {
                    return false;
                }

                return true;
            }
        }

        // Can't fit anywhere, quote it
        if (!write_byte8(SQU)) {
            return false;
        }

        return write_byte_be(c.code_);
    }

    bool unicode_compressor::write_char(const action &c) {
        if (unicode_mode) {
            return write_uchar(c.code_);
        }

        return write_schar(c);
    }

    bool unicode_compressor::write_treatment_selection(const std::int32_t treatment) {
        if (treatment == unicode_char_treatment_plain_unicode) {
            if (!write_byte8(SCU)) {
                return false;
            }

            unicode_mode = true;
            return true;
        }

        if (treatment == unicode_char_treatment_plain_ascii) {
            if (unicode_mode) {
                // Switch to single byte mode, using current dynamic window
                if (!write_byte8(UC0 + dynamic_window_index_)) {
                    return false;
                }

                unicode_mode = false;
            }

            return true;
        }

        if ((treatment >= unicode_char_treatment_first_dynamic) && (treatment <= unicode_char_treatment_last_dynamic)) {
            const std::uint32_t base = dynamic_window_base(treatment);

            // Switch to appropriate dynamic window if available, if not, redefine and
            // select dynamic window 4. From original comment.
            for (std::int32_t i = 0; i < dynamic_windows_size; i++) {
                if (base == dynamic_windows[i]) {
                    if (unicode_mode) {
                        if (!write_byte8(static_cast<std::uint8_t>(UC0 + i))) {
                            return false;
                        }
                    } else if (i != dynamic_window_index_) {
                        if (!write_byte8(static_cast<std::uint8_t>(SC0 + i))) {
                            return false;
                        }
                    }

                    unicode_mode = false;
                    dynamic_window_index_ = i;
                    active_window_base = base;

                    return true;
                }
            }

            if (unicode_mode) {
                if (!write_byte8(UD0 + 4)) {
                    return false;
                }
            } else {
                if (!write_byte8(SD0 + 4)) {
                    return false;
                }
            }

            if (!write_byte8(static_cast<std::uint8_t>(treatment))) {
                return false;
            }
            
            unicode_mode = false;
            dynamic_window_index_ = 4;
            dynamic_windows[4] = base;
            active_window_base = base;

            return true;
        }

        // Nothing to do
        return true;
    }

    void unicode_compressor::write_run() {
        if (!unicode_mode) {
            while (!actions_.empty()) {
                action &act = actions_.front();
                if ((act.treatment_ == unicode_char_treatment_plain_ascii) || ((act.code_ >= active_window_base)
                    && (act.code_ < active_window_base + 128))) {
                    if (!write_char(act))
                        return;

                    actions_.erase(actions_.begin());
                } else {
                    break;
                }
            }
        }

        // Try to write run of characters with same treatment
        while (!actions_.empty()) {
            const std::int32_t first_treatment = actions_.front().treatment_;
            std::size_t sequence_size = 0;

            for (std::size_t i = 0; i < actions_.size(); i++) {
                if (actions_[i].treatment_ == first_treatment)
                    sequence_size++;
                else                
                    break;
            }

            if (sequence_size > 1) {
                if (!write_treatment_selection(first_treatment)) {
                    return;
                }
            }

            for (std::size_t i = 0; i < sequence_size; i++) {
                if (!write_char(actions_[i])) {
                    return;
                }
            }

            actions_.erase(actions_.begin(), actions_.begin() + sequence_size);
        }
    }

    int unicode_compressor::compress(std::uint8_t *source, int &source_size, std::uint8_t *dest, int dest_size) {
        source_buf = source;
        dest_buf = dest;

        source_pointer = 0;
        dest_pointer = 0;

        this->source_size = source_size;
        this->dest_size = dest_size;

        unicode_mode = false;

        static constexpr std::size_t ACTION_FLUSH_SIZE = 4;

        for (; this->dest_size > 0;) {
            std::uint16_t uc = 0;

            if (!read_byte16(&uc)) {
                break;
            }

            actions_.emplace_back(static_cast<unicode_comp_state>(*this), uc);
            if (actions_.size() == ACTION_FLUSH_SIZE) {
                write_run();
            }
        }

        while ((actions_.size() > 0) && (this->dest_size > 0)) {
            write_run();
        }

        return dest_size - this->dest_size;
    }
}