#pragma once

#include <common/types.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace eka2l1 {
    class io_system;

    namespace loader {
        enum class epoc_sis_type {
            epocu6 = 0x1000006D,
            epoc6 = 0x10003A12
        };

        // Only the app type will be remembered by
        // the app manager
        enum class sis_old_type {
            app,
            sys,
            optional,
            config,
            patch,
            upgrade
        };

        struct sis_old_header {
            uint32_t uid1;
            uint32_t uid2;
            uint32_t uid3;
            uint32_t uid4;
            uint16_t checksum;
            uint16_t num_langs;
            uint16_t num_files;
            uint16_t num_reqs;
            uint16_t lang;
            uint16_t install_file;
            uint16_t install_drive;
            uint16_t num_caps;
            uint32_t install_ver;
            uint16_t op;
            uint16_t type;
            uint16_t major;
            uint16_t minor;
            uint32_t variant;
            uint32_t lang_ptr;
            uint32_t file_ptr;
            uint32_t req_ptr;
            uint32_t cert_ptr;
            uint32_t comp_name_ptr;
        };

        enum class sis_old_file_type {
            standard,
            text,
            comp,
            run,
            null,
            mime
        };

        enum class sis_old_file_detail {
            cont,
            skip,
            abort,
            undo
        };

        enum class sis_old_file_detail_exec {
            install,
            remove,
            both,
            sended
        };

        struct sis_old_file_record {
            uint32_t file_record_type;
            uint32_t file_type;
            uint32_t file_details;
            uint32_t source_name_len;
            uint32_t source_name_ptr;
            uint32_t des_name_len;
            uint32_t des_name_ptr;
            uint32_t len;
            uint32_t ptr;
            uint32_t org_file_len;
            uint32_t mime_type_len;
            uint32_t mine_type_ptr;
        };

        struct sis_old_file {
            sis_old_file_record record;
            std::u16string name;
            std::u16string dest;
        };

        struct sis_old {
            epocver epoc_ver;

            sis_old_header header;
            std::vector<sis_old_file> files;

            // The app path or exe path
            std::optional<std::u16string> app_path;
            std::optional<std::u16string> exe_path;
        };

        std::optional<sis_old> parse_sis_old(const std::string path);
    }
}