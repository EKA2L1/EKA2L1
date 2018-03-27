#include <loader/sis_fields.h>
#include <miniz.h>

namespace eka2l1 {
    namespace loader {
        void peek(void* buf, size_t element_count, size_t element_size, FILE* file) {
             size_t crr = ftell(file);

             fread(buf, element_count, element_size, file);
             fseek(file, crr, SEEK_SET);
        }

        void sis_parser::parse_field_child(sis_field* field) {
            fread(&field->type, 1, 4, file);

            fread(&field->len_low, 1, 4, file);

            if ((field->len_low & 0xFFFFFFFF) >> 32 == 0) {
                fread(&field->len_high, 1, 4, file);
            } else {
                field->len_high = 0;
            }
        }

        sis_parser::sis_parser(const std::string name) {
            file = fopen(name.c_str(), "rb");

            if (!file) {
                // LOGGING
            }
        }

        sis_info sis_parser::parse_info() {
            sis_info info;

            parse_field_child(&info);

            info.uid = parse_uid();
            info.vendor_name = parse_string();

            return info;
        }

        sis_header sis_parser::parse_header() {
            sis_header header;
            fread(&header, 1, sizeof(sis_header), file);

            return header;
        }

        sis_controller_checksum sis_parser::parse_controller_checksum() {
            sis_controller_checksum sum;

            parse_field_child(&sum);

            fread(&sum.sum, 1, 2, file);
            valid_offset();
            return sum;
        }

        sis_data_checksum sis_parser::parse_data_checksum() {
            sis_data_checksum sum;

            parse_field_child(&sum);

            fread(&sum.sum, 1, 2, file);
            valid_offset();
            return sum;
        }

        sis_compressed sis_parser::parse_compressed() {
            sis_compressed compressed;

            parse_field_child(&compressed);

            fread(&compressed.algorithm, 1, 4, file);
            fread(&compressed.uncompressed_size, 1, 8, file);

            if (compressed.algorithm != sis_compressed_algorithm::deflated) {
                compressed.uncompressed_data.resize(compressed.uncompressed_size);
                fread(compressed.uncompressed_data.data(), 1, compressed.uncompressed_size, file);
            } else {
                uint32_t us = (compressed.len_low | (compressed.len_high << 32))
                        - 12 - (compressed.len_high ? 4 : 0) - 4 - 4;

                compressed.compressed_data.resize(us);
                fread(compressed.compressed_data.data(), 1, us, file);

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
            }

            valid_offset();

            return compressed;
        }

        // Main class
        sis_contents sis_parser::parse_contents() {
            sis_contents contents;

            parse_field_child(&contents);

            int controller_checksum_avail;
            peek(&controller_checksum_avail, 1, 4, file);

            if (controller_checksum_avail == (int)sis_field_type::SISControllerChecksum) {
                contents.controller_checksum = parse_controller_checksum();
            }

            int data_checksum_avail;
            peek(&data_checksum_avail, 1, 4, file);

            if (data_checksum_avail == (int)sis_field_type::SISDataChecksum) {
                contents.data_checksum = parse_data_checksum();
            }

            sis_compressed compress_data = parse_compressed();

            valid_offset();

            return contents;
        }

        sis_string sis_parser::parse_string() {
            sis_string str;
            parse_field_child(&str);

            str.unicode_string.resize((str.len_low) | str.len_high << 32);

            fread(&str.unicode_string, 1,
                  (str.len_low << 32) | str.len_high, file);

            valid_offset();

            return str;
        }

        sis_uid sis_parser::parse_uid() {
            sis_uid uid;
            parse_field_child(&uid);
            fread(&uid.uid, 1, 4, file);

            valid_offset();

            return uid;
        }

        void sis_parser::jump_t(uint32_t off) {
            if (file) {
                fseek(file, off, SEEK_SET);
            }
        }

        void sis_parser::valid_offset() {
            size_t crr_pos = ftell(file);

            if (crr_pos % 4 != 0) {
                fseek(file, 4- crr_pos % 4, SEEK_CUR);
            }
        }
    }
}
