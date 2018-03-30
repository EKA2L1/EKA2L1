#pragma once

#include <vector>
#include <string>

#include <memory>
#include <sstream>

#include <cstdint>

namespace eka2l1 {
    namespace loader {
       // Symbian creators make such a complex piece
       // of file format shit that is hard to parse

       enum class sis_field_type: uint32_t {
           SISString = 1,
           SISArray = 2,
           SISCompressed = 3,
           SISVersion = 4,
           SISVersionRange = 5,
           SISDate = 6,
           SISTime = 7,
           SISDateTime = 8,
           SISUid= 9,
           Unused = 10,
           SISLanguage = 11,
           SISContents = 12,
           SISController = 13,
           SISInfo = 14,
           SISSupportedLanguages = 15,
           SISSupportedOptions = 16,
           SISPrerequisites = 17,
           SISDependency = 18,
           SISProperties = 19,
           SISProperty = 20,
           SISSignatures = 21,
           SISCertChain = 22,
           SISLogo = 23,
           SISFileDes = 24,
           SISHash = 25,
           SISIf = 26,
           SISElseIf = 27,
           SISInstallBlock = 28,
           SISExpression = 29,
           SISData = 30,
           SISdataUnit= 31,
           SISFileData = 32,
           SISSupportedOption = 33,
           SISControllerChecksum = 34,
           SISDataChecksum = 35,
           SISSignature = 36,
           SISBlob = 37,
           SISSignatureAlgorithm = 38,
           SISSignatureCertChain = 39,
           SISDataIndex = 40,
           SISCapabilites = 41
       };

       struct sis_field {
           sis_field_type type;
           uint32_t len_low;
           uint32_t len_high;
       };

       using utf16_str = std::basic_string<uint16_t>;

       struct sis_string: public sis_field {
           utf16_str unicode_string;
       };

       struct sis_array: public sis_field {
           sis_field_type element_type;
           std::vector<std::shared_ptr<sis_field>> fields;
       };

       enum class sis_compressed_algorithm: uint32_t {
            none = 0,
            deflated = 1
       };

       struct sis_compressed: public sis_field {
            sis_compressed_algorithm algorithm;
            uint64_t                 uncompressed_size;

            std::vector<unsigned char>        uncompressed_data;
            std::vector<unsigned char>        compressed_data;

            uint64_t offset;
       };

       struct sis_version: public sis_field {
            int32_t major;
            int32_t minor;
            int32_t build;
       };

       struct sis_version_range: public sis_field {
            sis_version from_ver, to_ver;
       };

       struct sis_date: public sis_field {
            uint16_t year;
            uint8_t month;
            uint8_t day;
       };

       struct sis_time: public sis_field{
            uint8_t hours;
            uint8_t minutes;
            uint8_t secs;
       };

       struct sis_date_time: public sis_field {
           sis_date date;
           sis_time time;
       };

       struct sis_uid: public sis_field {
           int32_t uid;
       };

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
            drive_z
       };

       struct sis_header {
           uint32_t uid1;
           uint32_t uid2;
           uint32_t uid3;
           uint32_t checksum;
       };

       struct sis_language: public sis_field {
           sis_lang language;
       };

       struct sis_blob: public sis_field {
           std::vector<char> raw_data;
       };

       struct sis_data_index: public sis_field {
           uint32_t data_index;
       };

       struct sis_controller_checksum : public sis_field {
           uint16_t sum;
       };

       struct sis_data_checksum: public sis_field {
           uint16_t sum;
       };

       enum class sis_install_type: uint8_t {
           installation,
           augmentation,
           partial_upgrade,
           preinstall_app,
           preinstall_patch
       };

       enum sis_install_flag: uint8_t {
           shutdown_apps = 1
       };

       struct sis_info: public sis_field {
           sis_uid uid;
           sis_string vendor_name;
           sis_array names;
           sis_array vendor_names;
           sis_version version;
           sis_date_time creation_date;
           sis_install_type install_type;
           sis_install_flag install_flags;
       };

       struct sis_supported_langs: public sis_field {
           sis_array langs;
       };

       struct sis_supported_options: public sis_field {
           sis_array options; // sis_array
       };

       struct sis_supported_option: public sis_field {
           sis_string name; // sis_string
       };

       struct sis_supported_lang: public sis_field {
           sis_lang lang;
       };

       struct sis_dependency: public sis_field {
           sis_uid uid;
           sis_version_range ver_range;
           sis_array dependency_names;
       };

       struct sis_prerequisites: public sis_field {
           sis_array dependencies;
           sis_array target_devices;
       };

       struct sis_property: public sis_field {
           int32_t key;
           int32_t val;
       };

       struct sis_properties: public sis_field {
           sis_array properties;
       };

       enum class sis_hash_algorithm {
           sha1 = 1
       };

       struct sis_hash: public sis_field {
            sis_hash_algorithm hash_method;
            sis_blob hash_data;
       };

       enum class ss_op {
           EOpInstall = 1,
           EOpRun = 2,
           EOpText = 4,
           EOpNull = 8
       };

       enum class ss_io_option {
           EInstVerifyOnRestore = 1 << 15,
       };

       enum class ss_fr_option {
           EInstFileRunOptionInstall = 1 << 1,
           EInstFileRunOptionUninstall = 1 << 2,
           EInstFileRunOptionByMimeTime = 1 << 3,
           EInstFileRunOptionWaitEnd = 1 << 4,
           EInstFileRunOptionSendEnd = 1 << 5
       };

       enum class ss_ft_option {
           EInstFileTextOptionContinue = 1 << 9,
           EInstFileTextOptionSkipIfNo = 1 << 10,
           EInstFileTextOptionAbortIfNo = 1 << 11,
           EInstFileTextOptionExitIfNo = 1 << 12
       };

       struct sis_capabilities: public sis_blob {};

       struct sis_file_des: public sis_field {
           sis_string target;
           sis_string mime_type;
           sis_capabilities caps;
           sis_hash hash;

           ss_op op;
           uint32_t op_op;

           uint64_t len;
           uint64_t uncompressed_len;

           uint32_t idx;
       };

       struct sis_logo: public sis_field {
           sis_file_des logo;
       };


       struct sis_certificate_chain: public sis_field {
           sis_blob cert_data;
       };

       struct sis_sig_cert_chain: public sis_field {
           sis_array sigs;
           sis_certificate_chain cert_chain;
       };

       struct sis_sig_algorithm: public sis_field {
           sis_string algorithm;
       };

       struct sis_sig: public sis_field {
           sis_sig_algorithm algorithm;
           sis_blob signature_data;
       };

       enum class ss_expr_op {
           EBinOpEqual = 1,
           EBinOpNotEqual,
           EBinOpGreaterThan,
           EBinOpLessThan,
           EBinOpGreaterThanOrEqual,
           EBinOpLessOrEqual,

           ELogOpAnd,
           ELogOpOr,

           EUnaryOpNot,

           EFuncExists,
           EFuncAppProperties,
           EFuncDevProperties,

           EPrimTypeString,
           EPrimTypeOption,
           EPrimTypeVariable,
           EPrimTypeNumber
       };

       struct sis_install_block: public sis_field {
            sis_array files;
            sis_array controllers;
            sis_array if_blocks;
       };

       struct sis_controller: public sis_field {
           sis_info info;
           sis_supported_options options;
           sis_supported_langs langs;
           sis_prerequisites prequisites;
           sis_properties properties;
           sis_logo logo;
           sis_install_block install_block;
           std::vector<sis_sig_cert_chain> sigcert_chains;
           sis_data_index idx;
       };

       struct sis_expression;

       struct sis_expression: public sis_field {
           ss_expr_op op;
           int32_t int_val;
           sis_string val;

           std::shared_ptr<sis_expression> left_expr, right_expr;
       };

       struct sis_if: public sis_field {
            sis_expression expr;
            sis_install_block install_block;
            sis_array else_if;
       };

       struct sis_else_if: public sis_field {
            sis_expression expr;
            sis_install_block install_block;
       };

       struct sis_data: public sis_field {
           sis_array data_units;
       };

       struct sis_data_unit: public sis_field {
           sis_array data_unit;
       };

       struct sis_file_data: public sis_field {
            sis_compressed raw_data;
       };

       struct sis_contents: public sis_field {
           sis_controller_checksum controller_checksum;
           sis_data_checksum data_checksum;
           sis_controller    controller;
           sis_data          data;
       };

       class sis_parser {
           std::shared_ptr<std::istream> stream;
           std::shared_ptr<std::istream> alternative_stream;

       private:
           void valid_offset();
           void switch_istrstream(char* buf, size_t size);
       public:
           // Extensively use for hooking between compressed in-context data
           void switch_stream();
           void set_alternative_stream(std::shared_ptr<std::istream> astream);

           sis_parser(const std::string name);
           sis_header parse_header();

           // Parse a compressed block. Inflate the block if needed
           // Data inflating do not use parse compressed
           sis_compressed parse_compressed(bool no_extract = false);

           sis_info parse_info();
           sis_uid  parse_uid();
           sis_controller parse_controller(bool no_type = false);

           sis_hash parse_hash();
           sis_blob parse_blob();
           sis_logo parse_logo();
           sis_file_des parse_file_description(bool no_type = false);

           sis_array parse_array();

           sis_supported_options parse_supported_options();
           sis_supported_langs parse_supported_langs();

           sis_supported_option parse_supported_option(bool no_own = false);
           sis_supported_lang parse_supported_lang(bool no_own = false);

           void parse_field_child(sis_field* field, bool left_type_for_arr = false);

           sis_contents parse_contents();

           sis_controller_checksum parse_controller_checksum();
           sis_data_checksum parse_data_checksum();

           sis_dependency parse_dependency(bool no_type = false);
           sis_version_range parse_version_range();

           sis_prerequisites parse_prerequisites();

           sis_string parse_string(bool no_type = false);

           sis_capabilities parse_caps();

           sis_version parse_version();
           sis_date parse_date();
           sis_time parse_time();
           sis_date_time parse_date_time();

           sis_property parse_property(bool no_type = false);
           sis_properties parse_properties();

           sis_data parse_data();
           sis_data_unit parse_data_unit(bool no_type= false);
           sis_file_data parse_file_data(bool no_type  = false);

           sis_if parse_if(bool no_type = false);
           sis_else_if parse_if_else(bool no_type = false);

           sis_data_index parse_data_index();

           sis_expression parse_expression(bool no_type = false);
           sis_install_block parse_install_block(bool no_type = false);

           sis_sig_algorithm parse_signature_algorithm(bool no_type = false);
           sis_sig parse_signature(bool no_type = false);
           sis_certificate_chain parse_cert_chain(bool no_type = false);
           sis_sig_cert_chain parse_sig_cert_chain(bool no_type= false);

           void jump_t(uint32_t off);
       };
    }
}
