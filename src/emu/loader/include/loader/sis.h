#pragma once 

#include <string>
#include <cstdint>

namespace eka2l1 {
	namespace loader {
        enum class sis_lang: uint16_t {
            #define LANG_DECL(x,y) x,
            #include "loader/lang.def"
            #undef LANG_DECL
            total_lang
        };

        enum class sis_app_type: uint16_t {
            app,
            system,
            option,
            config,
            patch
        };

        enum class sis_drive: uint16_t {
             drive_c,
             drive_e,
             drive_z  // This is system rom >-<
        };

        struct sis_header {
            uint32_t uid1;
            uint32_t uid2;
            uint32_t uid3;
            uint32_t uid4;

            uint16_t checksum;
            uint16_t lang_count;
            uint16_t file_count;
            uint16_t req_count;

            sis_lang install_lang;
            uint16_t install_file;
            sis_drive install_drive;

            uint16_t cap_count;
            uint32_t install_ver;

            uint16_t option;
            sis_app_type type;

            uint16_t major;
            uint16_t minor;

            uint32_t variant;
            uint32_t langs_offset;
            uint32_t files_offset;
            uint32_t reqs_offset;

            uint32_t certs_offset;
            uint32_t names_offset;
        };

        struct sis_requisite_metadata {
            uint32_t uid;
            uint16_t major;
            uint16_t minor;
            uint32_t variant;

            uint32_t name_len;
            char*    name;
        };

        struct sis_component_namecount {
            uint32_t name_len;
            char*    name;
        };

        struct sis_capabilities_metadata {
            uint32_t key;
            uint32_t data;
        };

        struct sis_cert_metadata {
            uint16_t year;
            uint16_t month;
            uint16_t day;
            uint16_t hour;
            uint16_t minutes;

            uint32_t cert_count;
        };

        bool install_sis(std::string path);
	}
}
