/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <manager/sis_fields.h>
#include <miniz.h>

#include <common/advstream.h>
#include <common/log.h>

#include <fstream>
#include <iostream>

namespace eka2l1 {
    namespace loader {
        void peek(void *buf, size_t element_count, size_t element_size, std::istream *file) {
            size_t crr = file->tellg();

            file->read(static_cast<char *>(buf), element_count * element_size);
            file->seekg(crr);
        }

        void sis_parser::parse_field_child(sis_field *field, bool left_type_for_arr) {
            if (!left_type_for_arr)
                stream->read(reinterpret_cast<char *>(&field->type), 4);

            stream->read(reinterpret_cast<char *>(&field->len_low), 4);

            if ((field->len_low & 0xFFFFFFFF) >> 31 == 1) {
                stream->read(reinterpret_cast<char *>(&field->len_high), 4);
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

            stream->read(reinterpret_cast<char *>(&arr.element_type), 4);
            std::uint64_t crr_pos = stream->tellg();

#define PARSE_ELEMENT_TYPE(element_case, handler_result_type, handler)                                               \
    case sis_field_type::element_case:                                                                               \
        while ((uint64_t)stream->tellg() - crr_pos < ((uint64_t)arr.len_low | ((uint64_t)arr.len_high << 32)) - 4) { \
            std::shared_ptr<sis_field> elem = std::make_shared<handler_result_type>(handler(true));                  \
            elem->type = arr.element_type;                                                                           \
            arr.fields.push_back(elem);                                                                              \
            valid_offset();                                                                                          \
        }                                                                                                            \
        break;

            switch (arr.element_type) {
                PARSE_ELEMENT_TYPE(SISString, sis_string, parse_string)
                PARSE_ELEMENT_TYPE(SISSupportedOption, sis_supported_option, parse_supported_option)
                PARSE_ELEMENT_TYPE(SISLanguage, sis_supported_lang, parse_supported_lang)
                PARSE_ELEMENT_TYPE(SISDependency, sis_dependency, parse_dependency)
                PARSE_ELEMENT_TYPE(SISProperty, sis_property, parse_property)
                PARSE_ELEMENT_TYPE(SISFileDes, sis_file_des, parse_file_description)
                PARSE_ELEMENT_TYPE(SISController, sis_controller, parse_controller)
                PARSE_ELEMENT_TYPE(SISFileData, sis_file_data, parse_file_data)
                PARSE_ELEMENT_TYPE(SISDataUnit, sis_data_unit, parse_data_unit)
                PARSE_ELEMENT_TYPE(SISExpression, sis_expression, parse_expression)
                PARSE_ELEMENT_TYPE(SISIf, sis_if, parse_if)
                PARSE_ELEMENT_TYPE(SISElseIf, sis_else_if, parse_if_else)
                PARSE_ELEMENT_TYPE(SISSignature, sis_sig, parse_signature)

            default:
                break;
            }

            valid_offset();

            return arr;
        }

        sis_version sis_parser::parse_version() {
            sis_version ver;

            parse_field_child(&ver);

            stream->read(reinterpret_cast<char *>(&ver.major), 4);
            stream->read(reinterpret_cast<char *>(&ver.minor), 4);
            stream->read(reinterpret_cast<char *>(&ver.build), 4);

            valid_offset();

            return ver;
        }

        sis_date sis_parser::parse_date() {
            sis_date date;

            parse_field_child(&date);

            stream->read(reinterpret_cast<char *>(&date.year), 2);
            stream->read(reinterpret_cast<char *>(&date.month), 1);
            stream->read(reinterpret_cast<char *>(&date.day), 1);

            valid_offset();

            return date;
        }

        sis_time sis_parser::parse_time() {
            sis_time time;

            parse_field_child(&time);

            stream->read(reinterpret_cast<char *>(&time.hours), 1);
            stream->read(reinterpret_cast<char *>(&time.minutes), 1);
            stream->read(reinterpret_cast<char *>(&time.secs), 1);

            valid_offset();

            return time;
        }

        sis_date_time sis_parser::parse_date_time() {
            sis_date_time dt;

            parse_field_child(&dt);

            dt.date = parse_date();
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

            stream->read(reinterpret_cast<char *>(&info.install_type), 1);
            stream->read(reinterpret_cast<char *>(&info.install_flags), 1);

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
            stream->read(reinterpret_cast<char *>(&header), sizeof(sis_header));

            LOG_INFO("Header UID: 0x{:x}", header.uid3);

            return header;
        }

        sis_controller_checksum sis_parser::parse_controller_checksum() {
            sis_controller_checksum sum;

            parse_field_child(&sum);

            stream->read(reinterpret_cast<char *>(&sum.sum), 2);
            valid_offset();
            return sum;
        }

        sis_data_checksum sis_parser::parse_data_checksum() {
            sis_data_checksum sum;

            parse_field_child(&sum);

            stream->read(reinterpret_cast<char *>(&sum.sum), 2);
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
            stream->read(reinterpret_cast<char *>(&lang.lang), 2);

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

        sis_compressed sis_parser::parse_compressed(bool no_extract) {
            sis_compressed compressed;

            parse_field_child(&compressed);

            stream->read(reinterpret_cast<char *>(&compressed.algorithm), 4);
            stream->read(reinterpret_cast<char *>(&compressed.uncompressed_size), 8);

            // Store the offset, make intepreter do their work
            compressed.offset = stream->tellg();

            if (compressed.algorithm != sis_compressed_algorithm::deflated) {
                if (!no_extract) {
                    compressed.uncompressed_data.resize(compressed.uncompressed_size);
                    stream->read(reinterpret_cast<char *>(compressed.uncompressed_data.data()),
                        compressed.uncompressed_size);
                }
            } else {
                uint64_t us = (compressed.len_low | ((uint64_t)compressed.len_high << 32)) - 12;

                if (!no_extract) {
                    compressed.compressed_data.resize(us);
                    stream->read(reinterpret_cast<char *>(compressed.compressed_data.data()), us);

                    mz_stream stream;

                    stream.avail_in = 0;
                    stream.next_in = 0;
                    stream.zalloc = nullptr;
                    stream.zfree = nullptr;

                    inflateInit(&stream);

                    compressed.uncompressed_data.resize(compressed.uncompressed_size);

                    stream.avail_in = static_cast<std::uint32_t>(us);
                    stream.next_in = compressed.compressed_data.data();

                    stream.avail_out = static_cast<std::uint32_t>(compressed.uncompressed_size);
                    stream.next_out = compressed.uncompressed_data.data();

                    inflate(&stream, Z_NO_FLUSH);
                    inflateEnd(&stream);

                    LOG_INFO("Inflating chunk, size: {}", us);
                }
            }

            auto lenseg = (compressed.len_low) | (compressed.len_high << 31);

            if (no_extract) {
                // Must emit the data
                stream->seekg(lenseg - 12, std::ios_base::cur);
            }

            valid_offset();

            return compressed;
        }

        void sis_parser::switch_istrstream(char *buf, size_t size) {
            set_alternative_stream(std::make_shared<std::ifstream>("inflatedController.mt", std::ios::binary));
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

            FILE *ftemp = fopen("inflatedController.mt", "wb");
            fwrite(compress_data.uncompressed_data.data(), 1, compress_data.uncompressed_size, ftemp);
            fclose(ftemp);

            switch_istrstream(reinterpret_cast<char *>(compress_data.uncompressed_data.data()),
                compress_data.uncompressed_size);

            contents.controller = parse_controller();

            LOG_INFO("Current stream position: {}, compressed data size: {}", stream->tellg(), compress_data.uncompressed_size);
            assert((uint64_t)stream->tellg() == compress_data.uncompressed_size);

            switch_stream();

            LOG_INFO("Pos after reading controller: {}", stream->tellg());

            contents.data = parse_data();
            valid_offset();

            return contents;
        }

        sis_controller sis_parser::parse_controller(bool no_type) {
            sis_controller controller;

            parse_field_child(&controller, no_type);

            controller.info = parse_info();
            controller.options = parse_supported_options();
            controller.langs = parse_supported_langs();
            LOG_INFO("Prequisites read position: {}", stream->tellg());
            controller.prequisites = parse_prerequisites();
            controller.properties = parse_properties();

            int logo_avail;

            peek(&logo_avail, 1, 4, stream.get());

            if (logo_avail == (int)sis_field_type::SISLogo) {
                controller.logo = parse_logo();
            }

            controller.install_block = parse_install_block();

            int sigcert_avail;

            peek(&sigcert_avail, 1, 4, stream.get());

            while (sigcert_avail == (int)sis_field_type::SISSignatureCertChain) {
                controller.sigcert_chains.push_back(parse_sig_cert_chain());
                peek(&sigcert_avail, 1, 4, stream.get());
            }

            controller.idx = parse_data_index();

            valid_offset();

            return controller;
        }

        sis_data_index sis_parser::parse_data_index() {
            sis_data_index idx;

            parse_field_child(&idx);
            stream->read(reinterpret_cast<char *>(&idx.data_index), 4);

            valid_offset();

            return idx;
        }

        sis_string sis_parser::parse_string(bool no_arr) {
            sis_string str;
            parse_field_child(&str, no_arr);

            str.unicode_string.resize(((str.len_low) | ((uint64_t)str.len_high << 32)) / 2);

            stream->read(reinterpret_cast<char *>(&str.unicode_string[0]),
                str.unicode_string.size() * 2);

            valid_offset();

            return str;
        }

        sis_version_range sis_parser::parse_version_range() {
            sis_version_range range;

            parse_field_child(&range);
            range.from_ver = parse_version();

            int nkiadik = 0;
            loader::peek(&nkiadik, 1, 4, stream.get());

            if (nkiadik == (int)sis_field_type::SISVersion) {
                range.to_ver = parse_version();
            }

            valid_offset();

            return range;
        }

        sis_prerequisites sis_parser::parse_prerequisites() {
            sis_prerequisites pre;

            parse_field_child(&pre);
            pre.target_devices = parse_array();

            LOG_INFO("Position after reading targets pres: {}", stream->tellg());
            pre.dependencies = parse_array();

            LOG_INFO("Position after reading all pres: {}", stream->tellg());

            valid_offset();

            return pre;
        }

        sis_property sis_parser::parse_property(bool no_type) {
            sis_property pr;

            parse_field_child(&pr, no_type);

            stream->read(reinterpret_cast<char *>(&pr.key), 4);
            stream->read(reinterpret_cast<char *>(&pr.val), 4);

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

            LOG_INFO("Properties read position: {}", stream->tellg());
            parse_field_child(&properties);
            properties.properties = parse_array();

            valid_offset();

            return properties;
        }

        sis_uid sis_parser::parse_uid() {
            sis_uid uid;
            parse_field_child(&uid);
            stream->read(reinterpret_cast<char *>(&uid.uid), 4);

            valid_offset();

            return uid;
        }

        sis_hash sis_parser::parse_hash() {
            sis_hash hash;
            parse_field_child(&hash);

            stream->read(reinterpret_cast<char *>(&hash.hash_method), sizeof(sis_hash_algorithm));

            hash.hash_data = parse_blob();
            valid_offset();

            return hash;
        }

        sis_blob sis_parser::parse_blob() {
            sis_blob blob;
            parse_field_child(&blob);

            blob.raw_data.resize(blob.len_low | ((uint64_t)blob.len_high << 32));

            stream->read(blob.raw_data.data(), blob.raw_data.size());
            valid_offset();
            return blob;
        }

        sis_capabilities sis_parser::parse_caps() {
            sis_capabilities blob;
            parse_field_child(&blob);

            blob.raw_data.resize(blob.len_low | ((uint64_t)blob.len_high << 32));

            stream->read(blob.raw_data.data(), blob.raw_data.size());
            valid_offset();
            return blob;
        }

        sis_logo sis_parser::parse_logo() {
            sis_logo logo;
            parse_field_child(&logo);

            logo.logo = parse_file_description();
            valid_offset();
            return logo;
        }

        sis_file_des sis_parser::parse_file_description(bool no_type) {
            sis_file_des des;
            parse_field_child(&des, no_type);

            des.target = parse_string();
            des.mime_type = parse_string();

            int caps_avail;

            peek(&caps_avail, 1, 4, stream.get());

            if (caps_avail == (int)sis_field_type::SISCapabilites) {
                des.caps = parse_caps();
            }

            des.hash = parse_hash();

            std::string filename_ascii;
            filename_ascii.resize(des.target.unicode_string.size());

            for (int i = 0; i < des.target.unicode_string.size(); i++) {
                filename_ascii[i] = (char)(des.target.unicode_string[i]);
            }

            LOG_INFO("File detected: {}", filename_ascii);

            stream->read(reinterpret_cast<char *>(&des.op), 4);
            stream->read(reinterpret_cast<char *>(&des.op_op), 4);
            stream->read(reinterpret_cast<char *>(&des.len), 8);
            stream->read(reinterpret_cast<char *>(&des.uncompressed_len), 8);
            stream->read(reinterpret_cast<char *>(&des.idx), 4);

            valid_offset();
            return des;
        }

        sis_data sis_parser::parse_data() {
            sis_data dt;

            parse_field_child(&dt);
            dt.data_units = parse_array();

            valid_offset();

            return dt;
        }

        sis_data_unit sis_parser::parse_data_unit(bool no_type) {
            sis_data_unit du;

            parse_field_child(&du, no_type);
            du.data_unit = parse_array();

            valid_offset();

            return du;
        }

        sis_file_data sis_parser::parse_file_data(bool no_type) {
            sis_file_data fd;

            parse_field_child(&fd, no_type);
            fd.raw_data = parse_compressed(true);

            valid_offset();

            return fd;
        }

        sis_if sis_parser::parse_if(bool no_type) {
            sis_if ifexpr;

            parse_field_child(&ifexpr, no_type);
            ifexpr.expr = parse_expression();
            ifexpr.install_block = parse_install_block();
            ifexpr.else_if = parse_array();

            valid_offset();

            return ifexpr;
        }

        sis_else_if sis_parser::parse_if_else(bool no_type) {
            sis_else_if ei;

            parse_field_child(&ei, no_type);
            ei.expr = parse_expression();
            ei.install_block = parse_install_block();

            valid_offset();

            return ei;
        }

        sis_expression sis_parser::parse_expression(bool no_type) {
            sis_expression expr;
            parse_field_child(&expr, no_type);

            stream->read(reinterpret_cast<char *>(&expr.op), 4);

            // Only notice this field with EPrimTypeNumber, EPrimTypeVariable, EPrimTypeOption
            stream->read(reinterpret_cast<char *>(&expr.int_val), 4);

            if (expr.op == ss_expr_op::EFuncExists || expr.op == ss_expr_op::EPrimTypeString) {
                expr.val = parse_string();
            }

            if ((static_cast<int>(expr.op) >= static_cast<int>(ss_expr_op::EBinOpEqual)
                    && (static_cast<int>(expr.op) <= static_cast<int>(ss_expr_op::ELogOpOr)))
                || (expr.op == ss_expr_op::EFuncAppProperties)) {
                expr.left_expr = std::make_shared<sis_expression>(parse_expression());
            }

            if ((expr.op != ss_expr_op::EPrimTypeString) && (expr.op != ss_expr_op::EPrimTypeOption) && (expr.op != ss_expr_op::EPrimTypeVariable) && (expr.op != ss_expr_op::EPrimTypeNumber) && (expr.op != ss_expr_op::EFuncExists)) {
                expr.right_expr = std::make_shared<sis_expression>(parse_expression());
            }

            valid_offset();

            return expr;
        }

        sis_install_block sis_parser::parse_install_block(bool no_type) {
            sis_install_block ib;

            parse_field_child(&ib, no_type);

            ib.files = parse_array();
            ib.controllers = parse_array();
            ib.if_blocks = parse_array();

            valid_offset();

            return ib;
        }

        sis_sig_algorithm sis_parser::parse_signature_algorithm(bool no_type) {
            sis_sig_algorithm algo;

            parse_field_child(&algo, no_type);
            algo.algorithm = parse_string();

            valid_offset();

            return algo;
        }

        sis_sig sis_parser::parse_signature(bool no_type) {
            sis_sig sig;

            parse_field_child(&sig, no_type);
            sig.algorithm = parse_signature_algorithm();
            sig.signature_data = parse_blob();

            valid_offset();

            return sig;
        }

        sis_certificate_chain sis_parser::parse_cert_chain(bool no_type) {
            sis_certificate_chain cert_chain;

            parse_field_child(&cert_chain, no_type);
            cert_chain.cert_data = parse_blob();

            valid_offset();

            return cert_chain;
        }

        sis_sig_cert_chain sis_parser::parse_sig_cert_chain(bool no_type) {
            sis_sig_cert_chain sc;

            parse_field_child(&sc, no_type);
            sc.sigs = parse_array();
            sc.cert_chain = parse_cert_chain();

            valid_offset();

            return sc;
        }

        void sis_parser::jump_t(uint32_t off) {
            stream->seekg(off, std::ios::cur);
        }

        void sis_parser::valid_offset() {
            size_t crr_pos = stream->tellg();

            if (crr_pos % 4 != 0) {
                jump_t(4 - crr_pos % 4);
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
