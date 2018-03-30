#include <loader/sis_fields.h>
#include <miniz.h>

#include <common/log.h>

#include <iostream>
#include <fstream>

namespace eka2l1 {
    namespace loader {
        void peek(void* buf, size_t element_count, size_t element_size, std::istream* file) {
             size_t crr = file->tellg();

             file->read(static_cast<char*>(buf), element_count * element_size);
             file->seekg(crr);
        }

        void sis_parser::parse_field_child(sis_field* field, bool left_type_for_arr) {
            if (!left_type_for_arr)
                stream->read(reinterpret_cast<char*>(&field->type), 4);

            stream->read(reinterpret_cast<char*>(&field->len_low), 4);

            if ((field->len_low & 0xFFFFFFFF) >> 31 == 1) {
                stream->read(reinterpret_cast<char*>(&field->len_high), 4);
            } else {
                field->len_high = 0;
            }
        }

        sis_parser::sis_parser(const std::string name) {
            stream = std::make_shared<std::ifstream>(name, std::ifstream::binary);
        }

        sis_array sis_parser::parse_array() {
            sis_array arr;

            parse_field_child(&arr);

            stream->read(reinterpret_cast<char*>(&arr.element_type), 4);

            uint32_t crr_pos = stream->tellg();

            switch (arr.element_type) {
                case sis_field_type::SISString:
                    while ((uint32_t)stream->tellg() - crr_pos < (arr.len_low | (arr.len_high << 32)) - 4) {
                        auto str = std::make_shared<sis_string>(parse_string(true));
                        str->type = arr.element_type;
                        arr.fields.push_back(str);
                        valid_offset();
                    }

                    break;


                case sis_field_type::SISSupportedOption:
                    while ((uint32_t)stream->tellg() - crr_pos < (arr.len_low | (arr.len_high << 32)) - 4) {
                        auto str = std::make_shared<sis_supported_option>(parse_supported_option(true));
                        str->type = arr.element_type;
                        arr.fields.push_back(str);
                        valid_offset();
                    }

                    break;


                case sis_field_type::SISLanguage:
                    while ((uint32_t)stream->tellg() - crr_pos < (arr.len_low | (arr.len_high << 32)) - 4) {
                        auto str = std::make_shared<sis_supported_lang>(parse_supported_lang(true));
                        str->type = arr.element_type;
                        arr.fields.push_back(str);
                        valid_offset();
                    }

                    break;

                case sis_field_type::SISDependency:
                    while ((uint32_t)stream->tellg() - crr_pos < (arr.len_low | (arr.len_high << 32)) - 4) {
                        auto str = std::make_shared<sis_dependency>(parse_dependency(true));
                        str->type = arr.element_type;
                        arr.fields.push_back(str);
                        valid_offset();
                    }

                    break;

                case sis_field_type::SISProperty:
                    while ((uint32_t)stream->tellg() - crr_pos < (arr.len_low | (arr.len_high << 32)) - 4) {
                        auto str = std::make_shared<sis_property>(parse_property(true));
                        str->type = arr.element_type;
                        arr.fields.push_back(str);
                        valid_offset();
                    }

                    break;


                default: break;
            }

            valid_offset();

            return arr;
        }

        sis_version sis_parser::parse_version() {
            sis_version ver;

            parse_field_child(&ver);

            stream->read(reinterpret_cast<char*>(&ver.major), 4);
            stream->read(reinterpret_cast<char*>(&ver.minor), 4);
            stream->read(reinterpret_cast<char*>(&ver.build), 4);

            valid_offset();

            return ver;
        }

        sis_date sis_parser::parse_date() {
            sis_date date;

            parse_field_child(&date);

            stream->read(reinterpret_cast<char*>(&date.year), 2);
            stream->read(reinterpret_cast<char*>(&date.month), 1);
            stream->read(reinterpret_cast<char*>(&date.day), 1);

            valid_offset();

            return date;
        }

        sis_time sis_parser::parse_time() {
            sis_time time;

            parse_field_child(&time);

            stream->read(reinterpret_cast<char*>(&time.hours), 1);
            stream->read(reinterpret_cast<char*>(&time.minutes), 1);
            stream->read(reinterpret_cast<char*>(&time.secs), 1);

            valid_offset();

            return time;
        }

        sis_date_time sis_parser::parse_date_time() {
            sis_date_time dt;

            parse_field_child(&dt);

            dt.date= parse_date();
            dt.time = parse_time();

            return dt;
        }

        sis_info sis_parser::parse_info() {
            sis_info info;

            parse_field_child(&info);

            info.uid = parse_uid();
            info.vendor_name = parse_string();
            info.names = parse_array();
            info.vendor_names = parse_array();
            info.version = parse_version();
            info.creation_date = parse_date_time();

            stream->read(reinterpret_cast<char*>(&info.install_type), 1);
            stream->read(reinterpret_cast<char*>(&info.install_flags), 1);

            LOG_INFO("UID: 0x{:x}", info.uid.uid);
            LOG_INFO("Creation date: {}/{}/{}", info.creation_date.date.year, info.creation_date.date.month,
                     info.creation_date.date.day);
            LOG_INFO("Creation time: {}:{}:{}", info.creation_date.time.hours, info.creation_date.time.minutes,
                     info.creation_date.time.secs);

            valid_offset();

            return info;
        }

        sis_header sis_parser::parse_header() {
            sis_header header;
            stream->read(reinterpret_cast<char*>(&header), sizeof(sis_header));

            return header;
        }

        sis_controller_checksum sis_parser::parse_controller_checksum() {
            sis_controller_checksum sum;

            parse_field_child(&sum);

            stream->read(reinterpret_cast<char*>(&sum.sum), 2);
            valid_offset();
            return sum;
        }

        sis_data_checksum sis_parser::parse_data_checksum() {
            sis_data_checksum sum;

            parse_field_child(&sum);

            stream->read(reinterpret_cast<char*>(&sum.sum), 2);
            valid_offset();
            return sum;
        }

        sis_supported_option sis_parser::parse_supported_option(bool no_own) {
            sis_supported_option op;

            parse_field_child(&op, no_own);
            op.name = parse_string();

            valid_offset();

            return op;
        }

        sis_supported_lang sis_parser::parse_supported_lang(bool no_own) {
            sis_supported_lang lang;

            parse_field_child(&lang, no_own);
            stream->read(reinterpret_cast<char*>(&lang.lang), 2);

            valid_offset();

            return lang;
        }

        sis_supported_options sis_parser::parse_supported_options() {
             sis_supported_options ops;

             parse_field_child(&ops);

             ops.options = parse_array();

             valid_offset();

             return ops;
        }

        sis_supported_langs sis_parser::parse_supported_langs() {
            sis_supported_langs langs;

            parse_field_child(&langs);

            langs.langs = parse_array();

            valid_offset();
            return langs;
        }

        sis_compressed sis_parser::parse_compressed() {
            sis_compressed compressed;

            parse_field_child(&compressed);

            stream->read(reinterpret_cast<char*>(&compressed.algorithm), 4);
            stream->read(reinterpret_cast<char*>(&compressed.uncompressed_size), 8);

            if (compressed.algorithm != sis_compressed_algorithm::deflated) {
                compressed.uncompressed_data.resize(compressed.uncompressed_size);
                stream->read(reinterpret_cast<char*>(compressed.uncompressed_data.data()),
                             compressed.uncompressed_size);
            } else {
                uint32_t us = (compressed.len_low | (compressed.len_high << 32));

                compressed.compressed_data.resize(us);
                stream->read(reinterpret_cast<char*>(compressed.compressed_data.data()),us);

                mz_stream stream;

                stream.avail_in = 0;
                stream.next_in = 0;
                stream.zalloc = nullptr;
                stream.zfree = nullptr;

                inflateInit(&stream);

                compressed.uncompressed_data.resize(compressed.uncompressed_size);

                stream.avail_in = us;
                stream.next_in = compressed.compressed_data.data();

                stream.avail_out = compressed.uncompressed_size;
                stream.next_out = compressed.uncompressed_data.data();

                inflate(&stream,Z_NO_FLUSH);
                inflateEnd(&stream);

                LOG_INFO("Writing inflated controller metadata: inflatedController.mt");

                FILE* ftemp = fopen("inflatedController.mt", "wb");
                fwrite(compressed.uncompressed_data.data(), 1, compressed.uncompressed_size, ftemp);

                fclose(ftemp);
            }

            valid_offset();

            return compressed;
        }

        void sis_parser::switch_istrstream(char* buf, size_t size) {
            set_alternative_stream(std::make_shared<std::istringstream>(std::ios::binary));

            reinterpret_cast<std::istringstream*>(alternative_stream.get())->rdbuf()->pubsetbuf(
                     buf, size);

            switch_stream();
        }

        // Main class
        sis_contents sis_parser::parse_contents() {
            sis_contents contents;

            parse_field_child(&contents);

            int controller_checksum_avail;
            peek(&controller_checksum_avail, 1, 4, stream.get());

            if (controller_checksum_avail == (int)sis_field_type::SISControllerChecksum) {
                contents.controller_checksum = parse_controller_checksum();
            }

            int data_checksum_avail;
            peek(&data_checksum_avail, 1, 4, stream.get());

            if (data_checksum_avail == (int)sis_field_type::SISDataChecksum) {
                contents.data_checksum = parse_data_checksum();
            }

            sis_compressed compress_data = parse_compressed();

            switch_istrstream(reinterpret_cast<char*>(compress_data.uncompressed_data.data()),
                              compress_data.uncompressed_size);

            contents.controller = parse_controller();

            valid_offset();

            return contents;
        }

        sis_controller sis_parser::parse_controller() {
            sis_controller controller;

            parse_field_child(&controller);

            controller.info = parse_info();
            controller.options = parse_supported_options();
            controller.langs = parse_supported_langs();
            controller.prequisites = parse_prerequisites();
            controller.properties = parse_properties();

            return controller;
        }

        sis_string sis_parser::parse_string(bool no_arr) {
            sis_string str;
            parse_field_child(&str, no_arr);

            str.unicode_string.resize(((str.len_low) | (str.len_high << 32))/2);

            stream->read(reinterpret_cast<char*>(&str.unicode_string[0]),
                str.unicode_string.size() * 2);

            valid_offset();

            return str;
        }

        sis_version_range sis_parser::parse_version_range() {
            sis_version_range range;

            parse_field_child(&range);
            range.from_ver = parse_version();

            // If the app is installed only one and not a patch update,
            // the from version will be zero. So ignore the to
            if (!(range.from_ver.major == 0 &&
                    range.from_ver.minor == 0
                    && range.from_ver.build == 0)) {
                range.to_ver = parse_version();
            }

            valid_offset();

            return range;
        }

        sis_prerequisites sis_parser::parse_prerequisites() {
            sis_prerequisites pre;

            parse_field_child(&pre);
            pre.target_devices = parse_array();
            pre.dependencies = parse_array();

            valid_offset();

            return pre;
        }

        sis_property sis_parser::parse_property(bool no_type) {
            sis_property pr;

            parse_field_child(&pr, no_type);

            stream->read(reinterpret_cast<char*>(&pr.key), 4);
            stream->read(reinterpret_cast<char*>(&pr.val), 4);

            valid_offset();

            return pr;
        }

        sis_dependency sis_parser::parse_dependency(bool no_type) {
            sis_dependency dep;
            parse_field_child(&dep, no_type);

            dep.uid = parse_uid();
            dep.ver_range = parse_version_range();
            dep.dependency_names = parse_array();

            valid_offset();

            return dep;
        }

        sis_properties sis_parser::parse_properties() {
            sis_properties properties;

            parse_field_child(&properties);
            properties.properties = parse_array();

            valid_offset();

            return properties;
        }

        sis_uid sis_parser::parse_uid() {
            sis_uid uid;
            parse_field_child(&uid);
            stream->read(reinterpret_cast<char*>(&uid.uid), 4);

            valid_offset();

            return uid;
        }

        void sis_parser::jump_t(uint32_t off) {
            stream->seekg(off, std::ios::cur);
        }

        void sis_parser::valid_offset() {
            size_t crr_pos = stream->tellg();

            if (crr_pos % 4 != 0) {
                jump_t(4- crr_pos % 4);
            }
        }

        void sis_parser::switch_stream() {
            stream.swap(alternative_stream);
        }

        void sis_parser::set_alternative_stream(std::shared_ptr<std::istream> astream) {
            if (alternative_stream) {
                alternative_stream.reset();
            }

            alternative_stream = astream;
        }

    }
}
