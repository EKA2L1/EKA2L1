/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#pragma once

#include <locale>

namespace eka2l1::epoc {
    using uchar = char32_t;            ///< Unicode character type.

    /**
     * @brief Unicode character category.
     */
    enum uchar_category {
        UCHAR_CATEGORY_ALPHA_GROUP = 0x00,
        UCHAR_CATEGORY_LETTER_OTHER_GROUP = 0x10,
        UCHAR_CATEGORY_LETTER_MODIFIER_GROUP = 0x20,
        UCHAR_CATEGORY_MARK_GROUP = 0x30,
        UCHAR_CATEGORY_NUMBER_GROUP = 0x40,
        UCHAR_CATEGORY_PUNCTUATION_GROUP = 0x50,
        UCHAR_CATEGORY_SYMBOL_GROUP = 0x60,
        UCHAR_CATEGORY_SEPARATOR_GROUP = 0x70,
        UCHAR_CATEGORY_CONTROL_GROUP = 0x80,
        UCHAR_CATEGORY_ASSIGNED_GROUP = 0xE0,
        UCHAR_CATEGORY_UNASSIGNED_GROUP = 0xF0,
        UCHAR_CATEGORY_LETTER_UPPERCASE = UCHAR_CATEGORY_ALPHA_GROUP | 0x00,
        UCHAR_CATEGORY_LETTER_LOWERCASE = UCHAR_CATEGORY_ALPHA_GROUP | 0x01,
        UCHAR_CATEGORY_LETTER_TITLECASE = UCHAR_CATEGORY_ALPHA_GROUP | 0x02,
        UCHAR_CATEGORY_MARK_NO_SPACING = UCHAR_CATEGORY_MARK_GROUP | 0,
        UCHAR_CATEGORY_MARK_COMBINING = UCHAR_CATEGORY_MARK_GROUP | 1,
        UCHAR_CATEGORY_MARK_ENCLOSING = UCHAR_CATEGORY_MARK_GROUP | 2,
        UCHAR_CATEGORY_NUMBER_DECIMAL_DIGIT = UCHAR_CATEGORY_NUMBER_GROUP | 0,
        UCHAR_CATEGORY_NUMBER_LETTER = UCHAR_CATEGORY_NUMBER_GROUP | 1,
        UCHAR_CATEGORY_NUMBER_OTHER = UCHAR_CATEGORY_NUMBER_GROUP | 2,
        UCHAR_CATEGORY_PUNC_CONNECTOR = UCHAR_CATEGORY_PUNCTUATION_GROUP | 0,
        UCHAR_CATEGORY_PUNC_DASH = UCHAR_CATEGORY_PUNCTUATION_GROUP | 1,
        UCHAR_CATEGORY_PUNC_OPEN = UCHAR_CATEGORY_PUNCTUATION_GROUP | 2,
        UCHAR_CATEGORY_PUNC_CLOSE = UCHAR_CATEGORY_PUNCTUATION_GROUP | 3,
        UCHAR_CATEGORY_PUNC_INITIAL_QUOTE = UCHAR_CATEGORY_PUNCTUATION_GROUP | 4,
        UCHAR_CATEGORY_PUNC_FINAL_QUOTE = UCHAR_CATEGORY_PUNCTUATION_GROUP | 5,
        UCHAR_CATEGORY_PUNC_OTHER = UCHAR_CATEGORY_PUNCTUATION_GROUP | 6,
        UCHAR_CATEGORY_SYMBOL_MATH = UCHAR_CATEGORY_SYMBOL_GROUP | 0,
        UCHAR_CATEGORY_SYMBOL_CURRENCY = UCHAR_CATEGORY_SYMBOL_GROUP | 1,
        UCHAR_CATEGORY_SYMBOL_MODIFIER = UCHAR_CATEGORY_SYMBOL_GROUP | 2,
        UCHAR_CATEGORY_SYMBOL_OTHER = UCHAR_CATEGORY_SYMBOL_GROUP | 3,
        UCHAR_CATEGORY_SEPARATOR_SPACE = UCHAR_CATEGORY_SEPARATOR_GROUP | 0,
        UCHAR_CATEGORY_SEPARATOR_LINE = UCHAR_CATEGORY_SEPARATOR_GROUP | 1,
        UCHAR_CATEGORY_SEPARATOR_PARAGRAPH = UCHAR_CATEGORY_SEPARATOR_GROUP | 2,
        UCHAR_CATEGORY_OTHER_CONTROL = UCHAR_CATEGORY_CONTROL_GROUP | 0,
        UCHAR_CATEGORY_OTHER_FORMAT = UCHAR_CATEGORY_CONTROL_GROUP | 1,
        UCHAR_CATEGORY_OTHER_SURROGATE = UCHAR_CATEGORY_UNASSIGNED_GROUP | 0,
        UCHAR_CATEGORY_OTHER_PRIVATE_USE = UCHAR_CATEGORY_UNASSIGNED_GROUP | 1,
        UCHAR_CATEGORY_OTHER_NOT_ASSIGNED = UCHAR_CATEGORY_UNASSIGNED_GROUP | 2
    };

    /**
     * @brief   Get categories of an Unicode character.
     * 
     * These categories are built up from the uchar_category enum.
     * 
     * @param   c       The Unicode character.
     * @param   ln      The locale to check for categories from.
     * 
     * @returns Categories of the character.
     */
    std::uint32_t get_uchar_category(const uchar c, std::locale &ln);

    /**
     * @brief   Uppercase an Unicode character.
     * 
     * If the character is already uppercased, the passed character is returned.
     * 
     * @param   c      Character to uppercase.
     * @param   ln     The locale to base this uppercase on.
     * 
     * @returns Uppercased character
     * @see     lowercase_uchar
     */
    const uchar uppercase_uchar(const uchar c, std::locale &ln);

    /**
     * @brief   Lowercase an Unicode character.
     * 
     * If the character is already lowercased, the passed character is returned.
     * 
     * @param   c      Character to lowercase.
     * @param   ln     The locale to base this lowercase on.
     * 
     * @returns Lowercased character
     * @see     uppercase_uchar
     */
    const uchar lowercase_uchar(const uchar c, std::locale &ln);

    /**
     * @brief   Fold an Unicode character.
     * 
     * An Unicode character when folded can be used in comparision that do not care about things such as case and accents.
     * 
     * @param   c      Character to fold.
     * @param   ln     The locale to base this fold on.
     * 
     * @returns Folded character
     */
    const uchar fold_uchar(const uchar c, std::locale &ln);
}