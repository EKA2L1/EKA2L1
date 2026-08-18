/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/font.h>

#include <utils/consts.h>

#include <unordered_map>
#include <vector>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::epoc {
    struct open_font_info {
        std::u16string family;
        std::size_t idx;

        epoc::adapter::font_file_adapter_base *adapter;
        epoc::open_font_face_attrib face_attrib;
        epoc::open_font_metrics metrics;

        std::uint32_t metric_identifier;

        // Imported by the user into <storage>/fonts rather than shipped by the
        // device; see font_store::attach_user_font_fallbacks.
        bool user_font = false;
    };

    /**
     * @brief Score a candidate face against a request, higher being better.
     *
     * Port of CFontStore's MatchFontSpecsInPixels. Exposed for tests.
     */
    int match_font_spec(const open_font_face_attrib &candidate, const std::u16string &candidate_name,
        const epoc::font_spec_base &spec, const std::int32_t candidate_height);

    // A set of fonts
    class font_store {
        std::vector<open_font_info> open_font_store;
        std::vector<epoc::adapter::font_file_adapter_instance> font_adapters;
        std::vector<epoc::typeface_support> typefaces;

        eka2l1::io_system *io;

        // Linked typefaces occupy the front of open_font_store; see
        // add_linked_font.
        std::size_t linked_font_count_ = 0;

    protected:
        void folder_change_callback(const std::u16string &path, int action);

    public:
        explicit font_store(eka2l1::io_system *io)
            : io(io) {
        }

        /**
         * @brief Add every face of a font file to the store.
         * @returns True if the file was one this adapter kind can read.
         */
        bool add_fonts(std::vector<std::uint8_t> &buf, const epoc::adapter::font_file_adapter_kind adapter_kind,
            const bool user_font = false);

        /**
         * @brief Present faces already in the store as one linked typeface.
         *
         * Mirrors the S60 linked font that a CJK variant declares in link.ini:
         * `component_names` are face names in lookup order and `canonical` is
         * the index of the one that lends the typeface its attributes.
         *
         * @returns True if every component was found and the typeface was added.
         */
        bool add_linked_font(const std::u16string &name, const std::vector<std::u16string> &component_names,
            const std::size_t canonical);

        /**
         * @brief Let the fonts the user imported stand in for glyphs the
         *        device's own fonts do not have.
         *
         * A CJK variant ships a link.ini pairing its Latin typeface with a CJK
         * one; a Latin-only ROM ships neither the font nor the link, so an
         * imported font would sit in the store unused -- nothing asks for it by
         * name, and nothing about a request says which script it is about to
         * draw. Every device face is therefore given the imported fonts as
         * trailing components of a linked typeface, which is the same
         * arrangement link.ini describes, with the device's own face canonical
         * so nothing about its appearance or metrics changes.
         */
        void attach_user_font_fallbacks();

        open_font_info *seek_the_open_font(epoc::font_spec_base &spec);
        open_font_info *seek_the_font_by_uid(const epoc::uid the_uid, epoc::open_font_metrics &target_metric, std::uint32_t *metric_identifier = nullptr);
        epoc::typeface_support *get_typeface_support(const std::uint32_t index);

        const std::size_t number_of_fonts() const {
            return open_font_store.size();
        }

        const std::size_t number_of_typefaces() const {
            return typefaces.size();
        }
    };
}