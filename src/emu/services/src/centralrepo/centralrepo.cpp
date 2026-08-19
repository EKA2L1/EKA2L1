/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/dynamicfile.h>
#include <common/log.h>

#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/cre.h>
#include <services/context.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <utils/err.h>
#include <vfs/vfs.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace eka2l1 {
    // A central repository INI is not a generic INI file. Symbian's own reader (CIniFileIn,
    // persistentstorage/centralrepository) walks it as a single token stream with TLex and
    // expects a fixed section order, so the grammar below follows that reader rather than a
    // key/value INI grammar: quoted and escaped strings, the '-' empty binary marker, negative
    // integers, and the optional per-setting metadata and access policies all depend on it.
    namespace {
        enum class centrep_ini_value_type {
            integer,
            real,
            string, ///< 16-bit string, stored as raw UCS-2 bytes.
            string8, ///< 8-bit string, stored as UTF-8 bytes.
            binary
        };

        class centrep_ini_reader {
            std::u16string data_;
            std::size_t pos_;

        public:
            explicit centrep_ini_reader(std::u16string data)
                : data_(std::move(data))
                , pos_(0) {
            }

            static bool is_space(const char16_t c) {
                return (c == u' ') || (c == u'\t') || (c == u'\r') || (c == u'\n') || (c == u'\v') || (c == u'\f');
            }

            static bool is_digit(const char16_t c) {
                return (c >= u'0') && (c <= u'9');
            }

            /**
             * \brief Narrow a token for keyword matching, logging and number conversion.
             *
             * Everything this format spells out is ASCII, so anything else only has to stay
             * recognisable - and narrowing it must never throw the way a UTF-8 conversion of a
             * damaged repository would.
             */
            static std::string narrow_token(const std::u16string &token) {
                std::string narrowed;
                narrowed.reserve(token.length());

                for (const char16_t c : token) {
                    narrowed.push_back((c < 0x80) ? static_cast<char>(c) : '?');
                }

                return narrowed;
            }

            static int digit_value(const char16_t c, const int radix) {
                int value = -1;

                if (is_digit(c)) {
                    value = c - u'0';
                } else if ((c >= u'a') && (c <= u'f')) {
                    value = c - u'a' + 10;
                } else if ((c >= u'A') && (c <= u'F')) {
                    value = c - u'A' + 10;
                }

                return (value < radix) ? value : -1;
            }

            bool eos() const {
                return pos_ >= data_.length();
            }

            char16_t peek() const {
                return eos() ? u'\0' : data_[pos_];
            }

            char16_t get() {
                return eos() ? u'\0' : data_[pos_++];
            }

            void inc() {
                if (!eos()) {
                    pos_++;
                }
            }

            void skip_space() {
                while (!eos() && is_space(data_[pos_])) {
                    pos_++;
                }
            }

            /**
             * \brief Skip blanks that do not end the current statement.
             *
             * A setting's optional trailing fields stop at the line break, the same way the
             * original reader stops at the carriage return.
             */
            void skip_blank() {
                while (!eos() && ((data_[pos_] == u' ') || (data_[pos_] == u'\t'))) {
                    pos_++;
                }
            }

            bool at_line_end() const {
                return eos() || (peek() == u'\r') || (peek() == u'\n');
            }

            void skip_line() {
                while (!eos() && (data_[pos_] != u'\n')) {
                    pos_++;
                }

                inc();
            }

            void skip_comments() {
                for (;;) {
                    skip_space();

                    if (peek() != u'#') {
                        break;
                    }

                    skip_line();
                }
            }

            std::u16string next_token() {
                const std::size_t begin = pos_;

                while (!eos() && !is_space(data_[pos_])) {
                    pos_++;
                }

                return data_.substr(begin, pos_ - begin);
            }

            /**
             * \brief Consume the next keyword when it is the expected one.
             *
             * Every section is optional, so the position is restored on a mismatch.
             */
            bool next_keyword_is(const char *keyword) {
                const std::size_t saved = pos_;

                skip_comments();

                if (common::lowercase_string(narrow_token(next_token())) != keyword) {
                    pos_ = saved;
                    return false;
                }

                return true;
            }

            /**
             * \brief Consume the next word when it is the expected one.
             *
             * A word also ends on '=', so that "mask = 1" and "mask=1" read the same. Which of
             * the two a repository uses is up to whoever wrote it.
             */
            bool next_word_is(const char *keyword) {
                const std::size_t saved = pos_;

                skip_comments();

                const std::size_t begin = pos_;

                while (!eos() && !is_space(data_[pos_]) && (data_[pos_] != u'=')) {
                    pos_++;
                }

                if (common::lowercase_string(narrow_token(data_.substr(begin, pos_ - begin))) != keyword) {
                    pos_ = saved;
                    return false;
                }

                return true;
            }

            /**
             * \brief Walk to the given keyword without interpreting anything in between.
             *
             * The reader stops right before the keyword, leaving it to be consumed.
             */
            bool skip_to_keyword(const char *keyword) {
                while (!eos()) {
                    const std::size_t saved = pos_;
                    skip_comments();

                    if (eos()) {
                        break;
                    }

                    if (common::lowercase_string(narrow_token(next_token())) == keyword) {
                        pos_ = saved;
                        return true;
                    }
                }

                return false;
            }

            bool read_uint32(std::uint32_t &value) {
                const std::size_t saved = pos_;
                int radix = 10;

                if (peek() == u'0') {
                    inc();

                    if ((peek() == u'x') || (peek() == u'X')) {
                        inc();
                        radix = 16;
                    } else {
                        pos_--;
                    }
                }

                std::uint64_t result = 0;
                std::size_t digit_count = 0;

                for (;;) {
                    const int digit = digit_value(peek(), radix);

                    if (digit < 0) {
                        break;
                    }

                    result = result * radix + digit;
                    digit_count++;

                    inc();
                }

                if (digit_count == 0) {
                    pos_ = saved;
                    return false;
                }

                value = static_cast<std::uint32_t>(result);
                return true;
            }

            /**
             * \brief Read a signed decimal number, the form the original reader uses for
             *        negative integer settings.
             */
            bool read_int32(std::int32_t &value) {
                const std::size_t saved = pos_;
                bool negative = false;

                if (peek() == u'-') {
                    negative = true;
                    inc();
                }

                std::uint64_t result = 0;
                std::size_t digit_count = 0;

                while (is_digit(peek())) {
                    result = result * 10 + (get() - u'0');
                    digit_count++;
                }

                if (digit_count == 0) {
                    pos_ = saved;
                    return false;
                }

                value = negative ? -static_cast<std::int32_t>(result) : static_cast<std::int32_t>(result);
                return true;
            }

            bool read_uint64(std::uint64_t &value) {
                const std::size_t saved = pos_;
                std::uint64_t result = 0;
                std::size_t digit_count = 0;

                while (is_digit(peek())) {
                    result = result * 10 + (get() - u'0');
                    digit_count++;
                }

                if ((digit_count == 0) || !is_space(peek())) {
                    pos_ = saved;
                    return false;
                }

                value = result;
                return true;
            }

            bool read_real(double &value) {
                const std::size_t saved = pos_;
                const std::string token = narrow_token(next_token());

                char *end = nullptr;
                const double result = std::strtod(token.c_str(), &end);

                if (token.empty() || (end == token.c_str())) {
                    pos_ = saved;
                    return false;
                }

                value = result;
                return true;
            }

            /**
             * \brief Read a string setting value.
             *
             * The value is either quoted with ' or ", or runs until the next blank. Backslash
             * escapes are expanded in both cases. An unquoted value leaves its terminator in
             * the stream so that the caller can still tell where the line ends.
             */
            bool read_string(std::u16string &value) {
                static const char16_t *ESCAPED = u"abfnrvt0";
                static const char16_t *ESCAPES = u"\a\b\f\n\r\v\t\0";

                value.clear();

                char16_t quote = u'\0';

                if ((peek() == u'\'') || (peek() == u'"')) {
                    quote = get();
                }

                bool complete = false;

                while (!eos()) {
                    const std::size_t before = pos_;
                    char16_t c = get();

                    if (quote ? (c == quote) : is_space(c)) {
                        if (!quote) {
                            pos_ = before;
                        }

                        complete = true;
                        break;
                    }

                    if (c == u'\\') {
                        if (eos()) {
                            break;
                        }

                        c = get();

                        for (std::size_t i = 0; ESCAPED[i] != u'\0'; i++) {
                            if (ESCAPED[i] == c) {
                                c = ESCAPES[i];
                                break;
                            }
                        }
                    }

                    value.push_back(c);
                }

                // A quote that is never closed swallows the rest of the file, so only an
                // unquoted value may end on the end of the stream.
                return complete || (!quote && eos());
            }

            bool read_binary(std::string &value) {
                const std::size_t saved = pos_;
                const std::u16string token = next_token();

                value.clear();

                // A lone '-' is how the format spells "no data".
                if (token == u"-") {
                    return true;
                }

                if (token.empty() || ((token.length() % 2) != 0)) {
                    pos_ = saved;
                    return false;
                }

                for (std::size_t i = 0; i < token.length(); i += 2) {
                    const int hi = digit_value(token[i], 16);
                    const int lo = digit_value(token[i + 1], 16);

                    if ((hi < 0) || (lo < 0)) {
                        pos_ = saved;
                        value.clear();

                        return false;
                    }

                    value.push_back(static_cast<char>((hi << 4) | lo));
                }

                return true;
            }
        };

        bool identify_centrep_ini_value_type(const std::string &token, centrep_ini_value_type &type) {
            if (token == "int") {
                type = centrep_ini_value_type::integer;
                return true;
            }

            if (token == "real") {
                type = centrep_ini_value_type::real;
                return true;
            }

            if (token == "string") {
                type = centrep_ini_value_type::string;
                return true;
            }

            if (token == "string8") {
                type = centrep_ini_value_type::string8;
                return true;
            }

            if (token == "binary") {
                type = centrep_ini_value_type::binary;
                return true;
            }

            return false;
        }

        bool read_centrep_ini_value(centrep_ini_reader &reader, const centrep_ini_value_type type,
            central_repo_entry_variant &data) {
            switch (type) {
            case centrep_ini_value_type::integer: {
                data.etype = central_repo_entry_type::integer;

                if (reader.peek() == u'-') {
                    std::int32_t signed_value = 0;

                    if (!reader.read_int32(signed_value)) {
                        return false;
                    }

                    data.intd = static_cast<std::uint64_t>(static_cast<std::int64_t>(signed_value));
                    return true;
                }

                std::uint32_t value = 0;

                if (!reader.read_uint32(value)) {
                    return false;
                }

                data.intd = value;
                return true;
            }

            case centrep_ini_value_type::real: {
                data.etype = central_repo_entry_type::real;
                return reader.read_real(data.reald);
            }

            case centrep_ini_value_type::string: {
                data.etype = central_repo_entry_type::string;

                std::u16string value;

                if (!reader.read_string(value)) {
                    return false;
                }

                // A 16-bit setting is handed to the guest as it is stored: raw UCS-2 bytes.
                data.strd.resize(value.length() * 2);
                std::memcpy(data.strd.data(), value.data(), data.strd.length());

                return true;
            }

            case centrep_ini_value_type::string8: {
                data.etype = central_repo_entry_type::string;

                std::u16string value;

                if (!reader.read_string(value)) {
                    return false;
                }

                try {
                    data.strd = common::ucs2_to_utf8(value);
                } catch (...) {
                    // A damaged 8-bit setting is worth losing on its own, not the repository.
                    return false;
                }

                return true;
            }

            case centrep_ini_value_type::binary: {
                data.etype = central_repo_entry_type::string;
                return reader.read_binary(data.strd);
            }

            default:
                break;
            }

            return false;
        }

        bool parse_centrep_ini_default_meta(centrep_ini_reader &reader, central_repo &repo, const std::string &path) {
            // The section opens with the repository-wide default, then lists ranges.
            reader.skip_comments();
            reader.read_uint32(repo.default_meta);

            reader.skip_comments();

            std::uint32_t low_key = 0;

            while (reader.read_uint32(low_key)) {
                central_repo_default_meta def_meta;
                def_meta.low_key = low_key;
                def_meta.high_key = 0;
                def_meta.key_mask = 0;
                def_meta.default_meta_data = 0;

                reader.skip_space();

                if (centrep_ini_reader::is_digit(reader.peek())) {
                    if (!reader.read_uint32(def_meta.high_key)) {
                        LOG_ERROR(SERVICE_CENREP, "Invalid end of metadata range in {}", path);
                        return false;
                    }
                } else {
                    // Not a range but a partial key and its mask: "<key> mask = <mask> <meta>".
                    if (!reader.next_word_is("mask")) {
                        LOG_ERROR(SERVICE_CENREP, "Missing 'mask' keyword in metadata section of {}", path);
                        return false;
                    }

                    reader.skip_space();

                    if (reader.get() != u'=') {
                        LOG_ERROR(SERVICE_CENREP, "Missing '=' after 'mask' in metadata section of {}", path);
                        return false;
                    }

                    reader.skip_space();

                    if (!reader.read_uint32(def_meta.key_mask)) {
                        LOG_ERROR(SERVICE_CENREP, "Invalid metadata mask in {}", path);
                        return false;
                    }
                }

                reader.skip_space();

                if (!reader.read_uint32(def_meta.default_meta_data)) {
                    LOG_ERROR(SERVICE_CENREP, "Metadata range without a default value in {}", path);
                    return false;
                }

                repo.meta_range.push_back(def_meta);
                reader.skip_comments();
            }

            return true;
        }
    }

    bool parse_new_centrep_ini(const std::string &path, central_repo &repo) {
        common::dynamic_ifile creini(path);

        if (creini.fail()) {
            return false;
        }

        // Sections are located by position, so the file is walked as one stream. Reading it
        // as UCS-2 keeps the reader independent of whether the file carries a BOM.
        std::u16string content;

        if (creini.is_ucs2()) {
            std::u16string line;

            while (creini.getline(line)) {
                content += line;
                content += u'\n';
            }
        } else {
            // The odd repository ships as 8-bit. Widening it byte by byte keeps the reader
            // simple and, unlike a UTF-8 conversion, cannot throw on a stray byte.
            std::string line;

            while (creini.getline(line)) {
                for (const char c : line) {
                    content.push_back(static_cast<char16_t>(static_cast<std::uint8_t>(c)));
                }

                content += u'\n';
            }
        }

        // A byte order mark only survives here when the file is UTF-8: the UCS-2 one is eaten
        // while the encoding is detected.
        if (!content.empty() && (content.front() == u'\xFEFF')) {
            content.erase(0, 1);
        }

        centrep_ini_reader reader(std::move(content));

        // Header: the "cenrep" signature, then the version.
        if (!reader.next_keyword_is("cenrep")) {
            return false;
        }

        if (!reader.next_keyword_is("version")) {
            LOG_ERROR(SERVICE_CENREP, "Central repo INI {} declares no version", path);
            return false;
        }

        std::uint32_t version = 0;
        reader.skip_space();

        if (!reader.read_uint32(version)) {
            LOG_ERROR(SERVICE_CENREP, "Central repo INI {} declares an invalid version", path);
            return false;
        }

        // Symbian rejects anything newer than version 1. Repositories shipped with emulated
        // ROMs are not always that tidy and the rest of the file still parses the same, so the
        // version is only recorded.
        repo.ver = static_cast<std::uint8_t>(version);

        // "protected" marks an application-centric keyspace.
        if (reader.next_keyword_is("protected")) {
            repo.keyspace_type = 1;
        }

        repo.owner_uid = repo.uid;

        if (reader.next_keyword_is("[owner]")) {
            std::uint32_t owner_uid = 0;
            reader.skip_comments();

            if (reader.read_uint32(owner_uid)) {
                repo.owner_uid = owner_uid;
            } else {
                LOG_ERROR(SERVICE_CENREP, "Central repo INI {} has an invalid owner UID", path);
            }
        }

        if (reader.next_keyword_is("[timestamp]")) {
            std::uint64_t time_stamp = 0;
            reader.skip_comments();

            // The stamp may also be spelled YYYYMMDD:HHMMSS.MMMMMM, which nothing in the
            // emulator consumes: skipping it still leaves the reader on the next section.
            if (reader.read_uint64(time_stamp)) {
                repo.time_stamp = time_stamp;
            } else {
                reader.next_token();
            }
        }

        if (reader.next_keyword_is("[defaultmeta]")) {
            if (!parse_centrep_ini_default_meta(reader, repo, path)) {
                return false;
            }
        }

        if (reader.next_keyword_is("[platsec]")) {
            // TODO (pent0): Capability supply. The policies are not read, but the section still
            // has to be walked so that the settings are looked for in the right place.
            reader.skip_to_keyword("[main]");
        }

        // The sections are supposed to come in this order, but a repository that lists them
        // differently still keeps all of its settings in the main section.
        if (!reader.next_keyword_is("[main]") && !(reader.skip_to_keyword("[main]") && reader.next_keyword_is("[main]"))) {
            LOG_ERROR(SERVICE_CENREP, "Central repo INI {} has no main section", path);
            return false;
        }

        // Settings, one per line: <key> <type> <value> [<metadata>] [<access policies>]
        for (;;) {
            reader.skip_comments();

            if (reader.eos() || (reader.peek() == u'[')) {
                break;
            }

            std::uint32_t key = 0;

            if (!reader.read_uint32(key)) {
                LOG_ERROR(SERVICE_CENREP, "Setting of repo 0x{:X} has an invalid key, skipping it", repo.uid);
                reader.skip_line();

                continue;
            }

            reader.skip_blank();

            const std::string type_name = common::lowercase_string(centrep_ini_reader::narrow_token(reader.next_token()));
            centrep_ini_value_type type = centrep_ini_value_type::integer;

            if (!identify_centrep_ini_value_type(type_name, type)) {
                LOG_ERROR(SERVICE_CENREP, "Setting 0x{:X} of repo 0x{:X} has unknown type {}, skipping it",
                    key, repo.uid, type_name);
                reader.skip_line();

                continue;
            }

            reader.skip_blank();

            central_repo_entry_variant data{};

            if (!read_centrep_ini_value(reader, type, data)) {
                LOG_ERROR(SERVICE_CENREP, "Setting 0x{:X} of repo 0x{:X} has an invalid {} value, skipping it",
                    key, repo.uid, type_name);
                reader.skip_line();

                continue;
            }

            reader.skip_blank();

            // Metadata is optional. Without one the repository defaults decide, which is not
            // the same as a metadata of 0.
            std::uint32_t metadata = 0;

            if (!reader.at_line_end() && reader.read_uint32(metadata)) {
                repo.add_new_entry(key, data, metadata);
            } else {
                repo.add_new_entry(key, data);
            }

            // What is left of the line are this setting's access policies.
            // TODO (pent0): Capability supply
            reader.skip_line();
        }

        return true;
    }

    central_repo_server::central_repo_server(eka2l1::system *sys)
        : service::server(sys->get_kernel_system(), sys, nullptr, CENTRAL_REPO_SERVER_NAME, true)
        , id_counter(0) {
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_init, "CenRep::Init");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_create_int, "CenRep::CreateInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_create_real, "CenRep::CreateReal");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_create_string, "CenRep::CreateString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_close, "CenRep::Close");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_reset, "CenRep::Reset");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_int, "CenRep::SetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_string, "CenRep::SetStr");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_real, "CenRep::SetStr");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req, "CenRep::NofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_group_nof_req, "CenRep::GroupNofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_int, "CenRep::GetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_real, "CenRep::GetReal");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_string, "CenRep::GetString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req_check, "CenRep::NofReqCheck");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_eq_int, "CenRep::FindEqInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_neq_int, "CenRep::FindNeqInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_eq_string, "CenRep::FindEqString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find, "CenRep::Find");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_find_res, "CenRep::GetFindResult");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel, "CenRep::NofCancel");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_group_nof_cancel, "CenRep::GroupNofCancel");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel_all, "CenRep::NofCancelAll");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_transaction_start, "CenRep::TransactionStart");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_transaction_cancel, "CenRep::TransactionCancel");
    }

    void central_repo_client_session::init(service::ipc_context *ctx) {
        // The UID repo to load
        const std::uint32_t repo_uid = *ctx->get_argument_value<std::uint32_t>(0);
        device_manager *mngr = ctx->sys->get_device_manager();
        eka2l1::central_repo *repo = server->load_repo_with_lookup(ctx->sys->get_io_system(), mngr, repo_uid);

        if (!repo) {
            LOG_TRACE(SERVICE_CENREP, "Repository not found with UID 0x{:X}", repo_uid);
            ctx->complete(epoc::error_not_found);
            return;
        }

        // New client session
        central_repo_client_subsession clisubsession;
        clisubsession.attach_repo = repo;
        clisubsession.server = server;

        auto res = client_subsessions.emplace(++idcounter, std::move(clisubsession));

        if (!res.second) {
            ctx->complete(epoc::error_no_memory);
            return;
        }

        repo->attached.push_back(&res.first->second);

        bool result = ctx->write_data_to_descriptor_argument<std::uint32_t>(3, idcounter);
        ctx->complete(epoc::error_none);
    }

    void central_repo_server::rescan_drives(eka2l1::io_system *io) {
        for (drive_number d = drive_a; d <= drive_z; d = static_cast<drive_number>(static_cast<int>(d) + 1)) {
            std::optional<eka2l1::drive> drv = io->get_drive_entry(d);

            if (!drv) {
                continue;
            }

            if (drv->media_type == drive_media::rom) {
                rom_drv = d;
            }

            if (static_cast<bool>(drv->attribute & io_attrib_internal) && !static_cast<bool>(drv->attribute & io_attrib_write_protected)) {
                avail_drives.push_back(d);
            }
        }
    }

    void central_repo_server::callback_on_drive_change(eka2l1::io_system *io, const drive_number drv, int act) {
        // Eject
        if (act == 0) {
            if (rom_drv == drv) {
                // Invalid
                rom_drv = drive_number::drive_count;
            } else {
                auto res = std::find(avail_drives.begin(), avail_drives.end(), drv);
                if (res != avail_drives.end()) {
                    avail_drives.erase(res);
                }
            }
        }

        // Mount
        if (act == 1) {
            eka2l1::drive drvi = std::move(*io->get_drive_entry(drv));
            if (drvi.media_type == drive_media::rom) {
                rom_drv = drv;
            }

            if (!static_cast<bool>(drvi.attribute & io_attrib_write_protected)) {
                avail_drives.push_back(drv);
            }
        }
    }

    void central_repo_server::redirect_msg_to_session(service::ipc_context &ctx) {
        const kernel::uid session_uid = ctx.msg->msg_session->unique_id();
        auto session_ite = client_sessions.find(session_uid);

        if (session_ite == client_sessions.end()) {
            LOG_ERROR(SERVICE_CENREP, "Session ID passed not found 0x{:X}", session_uid);
            ctx.complete(epoc::error_argument);

            return;
        }

        session_ite->second.handle_message(&ctx);
    }

    int central_repo_server::load_repo_adv(eka2l1::io_system *io, device_manager *mngr, central_repo *repo, const std::uint32_t key,
        bool scan_org_only) {
        bool is_first_repo = first_repo;
        first_repo ? (first_repo = false) : 0;

        if (is_first_repo) {
            rescan_drives(io);
        }

        LOG_TRACE(SERVICE_CENREP, "Try to open repo 0x{:X}", key);

        std::u16string keystr = common::utf8_to_ucs2(common::to_string(key, std::hex));

        // We prefer cre if it's available
        std::u16string repocre = keystr + u".CRE";
        std::u16string repoini = keystr + u".TXT";

        const std::u16string private_dir_persists = u":\\Private\\10202be9\\";
        const std::u16string firmcode = common::utf8_to_ucs2(common::lowercase_string(mngr->get_current()->firmware_code));
        const std::u16string private_dir_persists_glob = private_dir_persists + u"persists\\";
        const std::u16string private_dir_persists_separate_firm = private_dir_persists_glob + firmcode + u"\\";

        // Temporary push rom drive so scan works
        avail_drives.push_back(rom_drv);

        // Internal should only contains CRE
        for (auto &drv : avail_drives) {
            std::u16string repo_dir{ drive_to_char16(drv) };

            // Don't add separate firmware code on rom drive (it already did itself)
            std::vector<std::u16string> repo_folder_to_searches;

            // Search priority:
            // 1. Own device's persists folder. Many devices have their own configuration that goes to their own folder for avoiding conflicts
            // 2. Persists folder globally. Sometimes device shipped with modified persists, but it does not store to device's own folder like emulator. We grab this second
            // 3. TXT folder, this is the outer private folder of cenrep.
            if (drv != rom_drv) {
                repo_folder_to_searches.push_back(repo_dir + private_dir_persists_separate_firm);
                repo_folder_to_searches.push_back(repo_dir + private_dir_persists_glob);
            }

            repo_folder_to_searches.push_back(repo_dir + private_dir_persists);

            for (const std::u16string &repo_folder : repo_folder_to_searches) {
                if (is_first_repo && !io->exist(repo_folder)) {
                    // Create one if it doesn't exist, for the future
                    io->create_directories(repo_folder);
                } else {
                    // We can continue already
                    std::u16string repo_path = repo_folder + repocre;

                    if (io->exist(repo_path)) {
                        // Load and check for success
                        symfile repofile = io->open_file(repo_path, READ_MODE | BIN_MODE);

                        if (!repofile) {
                            LOG_ERROR(SERVICE_CENREP, "Found repo but open failed: {}", common::ucs2_to_utf8(repo_path));
                            continue;
                        }

                        std::vector<std::uint8_t> buf;
                        buf.resize(repofile->size());

                        if (buf.empty()) {
                            // A persist that was truncated (host killed mid-flush) reads back
                            // empty. Skip it and let the ROM/TXT default be picked up instead.
                            LOG_ERROR(SERVICE_CENREP, "Found empty repo persist, skipping: {}", common::ucs2_to_utf8(repo_path));
                            repofile->close();

                            continue;
                        }

                        repofile->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));
                        repofile->close();

                        common::chunkyseri seri(&buf[0], buf.size(), common::SERI_MODE_READ);

                        if (int err = do_state_for_cre(seri, *repo)) {
                            LOG_ERROR(SERVICE_CENREP, "Loading CRE file failed with code: 0x{:X}, repo 0x{:X}", err, key);
                            continue;
                        }

                        repo->reside_place = avail_drives[0];
                        repo->access_count = 1;

                        avail_drives.pop_back();
                        return 0;
                    }

                    // Try to load the INI
                    auto path = io->get_raw_path(repo_folder + repoini);

                    if (!path) {
                        continue;
                    }

                    repo->uid = key;
                    if (parse_new_centrep_ini(common::ucs2_to_utf8(*path), *repo)) {
                        repo->reside_place = avail_drives[0];
                        repo->access_count = 1;
                        avail_drives.pop_back();

                        return 0;
                    }
                }
            }
        }

        avail_drives.pop_back();
        return -1;
    }

    /* It should be like follow:
     *
     * - The ROM INI are for rollback
     * - And repo initialisation file resides outside private/1020be9/
     * 
     * That's for rollback when calling reset. Any changes in repo will be saved in persists folder
     * of preferable drive (usually internal).
    */
    eka2l1::central_repo *central_repo_server::load_repo(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key) {
        eka2l1::central_repo repo;
        if (load_repo_adv(io, mngr, &repo, key, false) != 0) {
            return nullptr;
        }

        repos.emplace(key, std::make_unique<eka2l1::central_repo>(repo));
        return repos[key].get();
    }

    eka2l1::central_repo *central_repo_server::load_repo_with_lookup(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key) {
        auto result = repos.find(key);

        if (result != repos.end()) {
            result->second->access_count++;
            return result->second.get();
        }

        return load_repo(io, mngr, key);
    }

    eka2l1::central_repo *central_repo_server::get_initial_repo(eka2l1::io_system *io,
        device_manager *mngr, const std::uint32_t key) {
        // Load from cache first
        eka2l1::central_repo *repo = backup_cacher.get_cached_repo(key);

        if (!repo) {
            // Load
            eka2l1::central_repo trepo;
            if (load_repo_adv(io, mngr, &trepo, key, true) != 0) {
                return nullptr;
            }

            return backup_cacher.add_repo(key, trepo);
        }

        return repo;
    }

    void central_repo_client_session::handle_message(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case cen_rep_init: {
            init(ctx);
            break;
        }

        case cen_rep_close: {
            close(ctx);
            break;
        }

        default: {
            // We find the repo subsession and redirect message to subsession
            const std::uint32_t subsession_uid = *ctx->get_argument_value<std::uint32_t>(3);
            auto subsession_ite = client_subsessions.find(subsession_uid);

            if (subsession_ite == client_subsessions.end()) {
                LOG_ERROR(SERVICE_CENREP, "Subsession ID passed not found 0x{:X}", subsession_uid);
                ctx->complete(epoc::error_argument);

                return;
            }

            subsession_ite->second.handle_message(ctx);
            break;
        }
        }
    }

    void central_repo_client_subsession::handle_message(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        // TODO: Faster way
        case cen_rep_create_int:
        case cen_rep_create_real:
        case cen_rep_create_string:
            create_value(ctx);
            break;

        case cen_rep_notify_req_check:
            notify_nof_check(ctx);
            break;

        case cen_rep_group_nof_cancel:
        case cen_rep_notify_cancel:
            notify_cancel(ctx);
            break;

        case cen_rep_notify_cancel_all:
            cancel_all_notify_requests();
            ctx->complete(epoc::error_none);
            break;

        case cen_rep_group_nof_req:
        case cen_rep_notify_req:
            notify(ctx);
            break;

        case cen_rep_get_int:
        case cen_rep_get_real:
        case cen_rep_get_string:
            get_value(ctx);
            break;

        case cen_rep_set_int:
        case cen_rep_set_string:
        case cen_rep_set_real:
            set_value(ctx);
            break;

        case cen_rep_reset:
            reset(ctx);
            break;

        case cen_rep_find_eq_int:
        case cen_rep_find_eq_real:
        case cen_rep_find_eq_string:
        case cen_rep_find_neq_int:
        case cen_rep_find_neq_real:
        case cen_rep_find_neq_string:
        case cen_rep_find: {
            find(ctx);
            break;
        }

        case cen_rep_transaction_start:
            start_transaction(ctx);
            break;

        case cen_rep_transaction_cancel:
            cancel_transaction(ctx);
            break;

        case cen_rep_get_find_res:
            get_find_result(ctx);
            break;

        default: {
            LOG_ERROR(SERVICE_CENREP, "Unhandled message opcode for cenrep 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    int central_repo_client_session::closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, decltype(client_subsessions)::iterator repo_subsession_ite) {
        auto &repo_subsession = repo_subsession_ite->second;

        if (repo_id != 0 && repo_subsession.attach_repo->uid != repo_id) {
            LOG_CRITICAL(SERVICE_CENREP, "Fail safe check: REPO id != provided id");
            return -2;
        }

        // Sensei, did i do it correct
        // Save it and than wipe it out
        repo_subsession.write_changes(io, mngr);
        LOG_TRACE(SERVICE_CENREP, "Repo 0x{:X}: changes saved", repo_subsession.attach_repo->uid);

        // Remove from attach
        auto &all_attached = repo_subsession.attach_repo->attached;
        auto attach_this_ite = std::find(all_attached.begin(), all_attached.end(),
            &repo_subsession);

        if (attach_this_ite != all_attached.end()) {
            all_attached.erase(attach_this_ite);
        }

        // Decrease access count
        if (repo_subsession.attach_repo->access_count > 0) {
            repo_subsession.attach_repo->access_count--;
        } else {
            LOG_ERROR(SERVICE_CENREP, "Repo 0x{:X} has access count to be negative!", repo_subsession.attach_repo->uid);
        }

        // Bie...
        return 0;
    }

    int central_repo_client_session::closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, const std::uint32_t id) {
        auto repo_subsession_ite = client_subsessions.find(id);

        if (repo_subsession_ite == client_subsessions.end()) {
            return -1;
        }

        const int result = closerep(io, mngr, repo_id, repo_subsession_ite);
        if (result == 0) {
            client_subsessions.erase(repo_subsession_ite);
        }

        return result;
    }

    void central_repo_client_session::close(service::ipc_context *ctx) {
        device_manager *mngr = ctx->sys->get_device_manager();
        const int err = closerep(ctx->sys->get_io_system(), mngr, 0, *ctx->get_argument_value<std::uint32_t>(3));

        switch (err) {
        case 0: {
            ctx->complete(epoc::error_none);
            break;
        }

        case -1: {
            ctx->complete(epoc::error_not_found);
            break;
        }

        case -2: {
            ctx->complete(epoc::error_argument);
            break;
        }

        default: {
            LOG_ERROR(SERVICE_CENREP, "Unknown return error from closerep {}", err);
            break;
        }
        }
    }

    // If a session disconnect, we should at least save all changes it did
    // At least, if the session connected still exist
    void central_repo_server::disconnect(service::ipc_context &ctx) {
        // Close all repos that are currently being opened.
        const kernel::uid ss_id = ctx.msg->msg_session->unique_id();
        auto ss_ite = client_sessions.find(ss_id);

        io_system *io = ctx.sys->get_io_system();
        device_manager *mngr = ctx.sys->get_device_manager();

        if (ss_ite != client_sessions.end()) {
            central_repo_client_session &ss = ss_ite->second;

            for (auto ite = ss.client_subsessions.begin(); ite != ss.client_subsessions.end(); ite++) {
                ss.closerep(io, mngr, 0, ite);
            }

            ss.client_subsessions.clear();
            client_sessions.erase(ss_ite);
        }

        // Ignore all errors
        ctx.complete(epoc::error_none);
    }

    void central_repo_server::connect(service::ipc_context &ctx) {
        central_repo_client_session session;
        session.server = this;

        const kernel::uid id = ctx.msg->msg_session->unique_id();

        // Put all process code here
        client_sessions.insert(std::make_pair(static_cast<const std::uint32_t>(id), std::move(session)));

        ctx.complete(epoc::error_none);
    }
}
