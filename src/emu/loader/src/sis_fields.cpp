#include <loader/sis_fields.h>
#include <miniz.h>

#include <fstream>

namespace eka2l1 {
    namespace loader {
        void peek(void* buf, size_t element_count, size_t element_size, std::istream* file) {
             size_t crr = file->tellg();

             file->read(static_cast<char*>(buf), element_count * element_size);
             file->seekg(crr);
        }

        void sis_parser::parse_field_child(sis_field* field) {
            stream->read(reinterpret_cast<char*>(&field->type), 4);
            stream->read(reinterpret_cast<char*>(&field->len_low), 4);

            if ((field->len_low & 0xFFFFFFFF) >> 32 == 0) {
                stream->read(reinterpret_cast<char*>(&field->len_high), 4);
            } else {
                field->len_high = 0;
            }
        }

        sis_parser::sis_parser(const std::string name) {
            stream = std::make_shared<std::ifstream>(name);
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
                uint32_t us = (compressed.len_low | (compressed.len_high << 32))
                        - 12 - (compressed.len_high ? 4 : 0) - 4 - 4;

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
            }

            valid_offset();

            return compressed;
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

            valid_offset();

            return contents;
        }

        sis_string sis_parser::parse_string() {
            sis_string str;
            parse_field_child(&str);

            str.unicode_string.resize((str.len_low) | str.len_high << 32);

            /*
            stream->read(reinterpret_cast<char*>(&str.unicode_string.data()),
                  (str.len_low << 32) | str.len_high);
            */
            valid_offset();

            return str;
        }

        sis_uid sis_parser::parse_uid() {
            sis_uid uid;
            parse_field_child(&uid);
            stream->read(reinterpret_cast<char*>(&uid.uid), 4);

            valid_offset();

            return uid;
        }

        void sis_parser::jump_t(uint32_t off) {
            stream->seekg(off);
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
