#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/types.h>
#include <loader/sis_script_interpreter.h>

#include <vfs.h>

#include <miniz.h>

namespace eka2l1 {
    namespace loader {
        const char drive_name(sis_drive drv) {
            if (drv == sis_drive::drive_c) {
                return 'c';
            }

            if (drv == sis_drive::drive_e) {
                return 'e';
            }

            if (drv == sis_drive::drive_z) {
                return 'z';
            }
        }

        bool exists(const utf16_str &str) {
            FILE *temp = fopen(common::ucs2_to_utf8(str).c_str(), "rb");
            return temp;
        }

        std::string get_install_path(const std::u16string &pseudo_path, sis_drive drv) {
            std::u16string raw_path = pseudo_path;

            if (raw_path.substr(0, 2) == u"!:") {
                raw_path[0] = (char16_t)drive_name(drv);
            }

            return common::ucs2_to_utf8(raw_path);
        }

        bool appprop(sis_uid uid, sis_property prop) {
            return false;
        }

        bool package(sis_uid uid) {
            // package_manager& manager = get_package_manager();
            // if (manager.installed(uid.uid)) {
            //    return true;
            // }
            //
            // return false;
            return false;
        }

        ss_interpreter::ss_interpreter() {
            data_stream.reset();
        }

        ss_interpreter::ss_interpreter(std::shared_ptr<std::istream> stream,
            io_system* io,
            sis_install_block inst_blck,
            sis_data inst_data,
            sis_drive inst_drv)
            : data_stream(stream)
            , io(io)
            , install_block(inst_blck)
            , install_data(inst_data)
            , install_drive(inst_drv) {}

        bool inflate_data(mz_stream *stream, void *in, void *out, uint32_t in_size, uint32_t *out_size = nullptr) {
            stream->avail_in = in_size;
            stream->next_in = static_cast<const unsigned char *>(in);
            stream->next_out = static_cast<unsigned char *>(out);
            stream->avail_out = CHUNK_MAX_INFLATED_SIZE;

            auto res = inflate(stream, Z_NO_FLUSH);

            if (res != MZ_OK) {
                if (res == MZ_STREAM_END) {
                    if (out_size)
                        *out_size = CHUNK_MAX_INFLATED_SIZE - stream->avail_out;

                    return true;
                }

                LOG_ERROR("Inflate failed description: {}", mz_error(res));
                *out_size = CHUNK_MAX_INFLATED_SIZE - stream->avail_out;

                return false;
            };

            *out_size = CHUNK_MAX_INFLATED_SIZE - stream->avail_out;
            return true;
        }

        std::vector<uint8_t> ss_interpreter::get_small_file_buf(uint32_t data_idx, uint16_t crr_blck_idx) {
            sis_file_data *data = reinterpret_cast<sis_file_data *>(
                reinterpret_cast<sis_data_unit *>(install_data.data_units.fields[crr_blck_idx].get())->data_unit.fields[data_idx].get());
            sis_compressed compressed = data->raw_data;

            uint32_t us = ((compressed.len_low) | (compressed.len_high << 32)) - 4;

            compressed.compressed_data.resize(us);

            data_stream->seekg(compressed.offset);
            data_stream->read(reinterpret_cast<char *>(compressed.compressed_data.data()), us);

            if (compressed.algorithm == sis_compressed_algorithm::none) {
                return compressed.compressed_data;
            }

            compressed.uncompressed_data.resize(compressed.uncompressed_size);
            mz_stream stream;

            stream.zalloc = nullptr;
            stream.zfree = nullptr;

            if (inflateInit(&stream) != MZ_OK) {
                LOG_ERROR("Can not intialize inflate stream");
            }

            inflate_data(&stream, compressed.compressed_data.data(), compressed.uncompressed_data.data(), us);
            inflateEnd(&stream);

            return compressed.uncompressed_data;
        }

        // Assuming this file is small since it's stored in std::vector
        // Directly write this
        void extract_file_with_buf(const std::string &path, std::vector<uint8_t> &data) {
            std::string rp = eka2l1::file_directory(path);
            eka2l1::create_directories(rp);

            LOG_INFO("Write to: {}", path);

            FILE *temp = fopen(path.c_str(), "wb");
            fwrite(data.data(), 1, data.size(), temp);

            fclose(temp);
        }

        void ss_interpreter::extract_file(const std::string &path, const uint32_t idx, uint16_t crr_blck_idx) {
            std::string rp = eka2l1::file_directory(path);
            eka2l1::create_directories(rp);

            FILE *file = fopen(path.c_str(), "wb");

            sis_data_unit *data_unit = reinterpret_cast<sis_data_unit *>(install_data.data_units.fields[crr_blck_idx].get());
            sis_file_data *data = reinterpret_cast<sis_file_data *>(data_unit->data_unit.fields[idx].get());

            sis_compressed compressed = data->raw_data;

            uint32_t us = ((compressed.len_low) | (compressed.len_high << 31)) - 4;
            compressed.compressed_data.resize(us);

            data_stream->seekg(compressed.offset);

            std::vector<unsigned char> temp_chunk;
            temp_chunk.resize(CHUNK_SIZE);

            std::vector<unsigned char> temp_inflated_chunk;
            temp_inflated_chunk.resize(CHUNK_MAX_INFLATED_SIZE);

            long long left = us;
            mz_stream stream;

            stream.zalloc = nullptr;
            stream.zfree = nullptr;

            if (compressed.algorithm == sis_compressed_algorithm::deflated) {
                if (inflateInit(&stream) != MZ_OK) {
                    LOG_ERROR("Can not intialize inflate stream");
                }
            }

            while (left > 0) {
                std::fill(temp_chunk.begin(), temp_chunk.end(), 0);

                int grab = left < CHUNK_SIZE ? left : CHUNK_SIZE;

                data_stream->read(reinterpret_cast<char *>(temp_chunk.data()), grab);

                if (compressed.algorithm == sis_compressed_algorithm::deflated) {
                    uint32_t inflated_size = 0;

                    auto res = inflate_data(&stream, temp_chunk.data(), temp_inflated_chunk.data(), grab, &inflated_size);

                    if (!res) {
                        LOG_ERROR("Uncompress failed!");
                    }

                    fwrite(temp_inflated_chunk.data(), 1, inflated_size, file);
                } else {
                    fwrite(temp_chunk.data(), 1, grab, file);
                }

                left -= grab;
            }

            if (compressed.algorithm == sis_compressed_algorithm::deflated) {
                inflateEnd(&stream);
            }

            fclose(file);
        }

        bool operator==(const sis_expression &lhs, const sis_expression &rhs) {
            if (lhs.op != rhs.op) {
                return false;
            }

            if ((lhs.op == ss_expr_op::EPrimTypeNumber)
                || (lhs.op == ss_expr_op::EPrimTypeOption)
                || (lhs.op == ss_expr_op::EPrimTypeVariable)) {
                return lhs.int_val == rhs.int_val;
            }

            if (lhs.op == ss_expr_op::EPrimTypeString) {
                return lhs.val.unicode_string == rhs.val.unicode_string;
            }

            return false;
        }

        bool operator>(const sis_expression &lhs, const sis_expression &rhs) {
            if (lhs.op != rhs.op) {
                return false;
            }

            if ((lhs.op == ss_expr_op::EPrimTypeNumber)
                || (lhs.op == ss_expr_op::EPrimTypeOption)
                || (lhs.op == ss_expr_op::EPrimTypeVariable)) {
                return lhs.int_val > rhs.int_val;
            }

            if (lhs.op == ss_expr_op::EPrimTypeString) {
                return lhs.val.unicode_string > rhs.val.unicode_string;
            }

            return false;
        }

        bool operator<(const sis_expression &lhs, const sis_expression &rhs) {
            if (lhs.op != rhs.op) {
                return false;
            }

            if ((lhs.op == ss_expr_op::EPrimTypeNumber)
                || (lhs.op == ss_expr_op::EPrimTypeOption)
                || (lhs.op == ss_expr_op::EPrimTypeVariable)) {
                return lhs.int_val < rhs.int_val;
            }

            if (lhs.op == ss_expr_op::EPrimTypeString) {
                return lhs.val.unicode_string < rhs.val.unicode_string;
            }

            return false;
        }

        // Take two expression, return if logical and is bigger than 0 or not
        bool operator&(const sis_expression &lhs, const sis_expression &rhs) {
            if (lhs.op != rhs.op) {
                return false;
            }

            if ((lhs.op == ss_expr_op::EPrimTypeNumber)
                || (lhs.op == ss_expr_op::EPrimTypeOption)
                || (lhs.op == ss_expr_op::EPrimTypeVariable)) {
                return lhs.int_val & rhs.int_val;
            }

            return false;
        }

        // Take two expression, return if logical and is bigger than 0 or not
        bool operator|(const sis_expression &lhs, const sis_expression &rhs) {
            if (lhs.op != rhs.op) {
                return false;
            }

            if ((lhs.op == ss_expr_op::EPrimTypeNumber)
                || (lhs.op == ss_expr_op::EPrimTypeOption)
                || (lhs.op == ss_expr_op::EPrimTypeVariable)) {
                return lhs.int_val | rhs.int_val;
            }

            return false;
        }

        bool operator!=(const sis_expression &lhs, const sis_expression &rhs) {
            return !(lhs == rhs);
        }

        bool operator>=(const sis_expression &lhs, const sis_expression &rhs) {
            return (lhs > rhs) || (lhs == rhs);
        }

        bool operator<=(const sis_expression &lhs, const sis_expression &rhs) {
            return (lhs < rhs) || (lhs == rhs);
        }

        bool ss_interpreter::interpret(sis_install_block install_block, uint16_t crr_blck_idx) {
            // Process file
            auto install_file = [&](sis_install_block inst_blck, uint16_t crr_blck_idx) {
                for (auto &wrap_file : inst_blck.files.fields) {
                    sis_file_des *file = (sis_file_des *)(wrap_file.get());
                    std::string raw_path = io->get(get_install_path(file->target.unicode_string, install_drive));

                    if (file->op == ss_op::EOpText) {
                        auto buf = get_small_file_buf(file->idx, crr_blck_idx);
                        //extract_file_with_buf(raw_path, buf);
                        //show_text_func(buf);

                        LOG_INFO("EOpText");
                    } else if (file->op == ss_op::EOpRun) {
                        // Doesn't do anything yet.
                        LOG_INFO("EOpRun {}", raw_path);
                    } else if (file->op == ss_op::EOpInstall) {
                        extract_file(raw_path, file->idx, crr_blck_idx);
                        LOG_INFO("EOpInstall {}", raw_path);
                    } else {
                        LOG_INFO("EOpNull");
                    }
                }
            };

            install_file(install_block, crr_blck_idx);

            auto can_pass = [&](sis_field *wrap_if_stmt) -> bool {
                sis_if *if_stmt = (sis_if *)(wrap_if_stmt);
                ss_expr_op stmt_type = if_stmt->expr.op;

                bool pass = false;

                if (stmt_type == ss_expr_op::EBinOpEqual) {
                    pass = (*if_stmt->expr.left_expr == *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpNotEqual) {
                    pass = (*if_stmt->expr.left_expr != *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpGreaterThan) {
                    pass = (*if_stmt->expr.left_expr > *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpGreaterThanOrEqual) {
                    pass = (*if_stmt->expr.left_expr >= *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpLessThan) {
                    pass = (*if_stmt->expr.left_expr < *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpLessOrEqual) {
                    pass = (*if_stmt->expr.left_expr <= *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::ELogOpAnd) {
                    pass = (*if_stmt->expr.left_expr & *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::ELogOpOr) {
                    pass = (*if_stmt->expr.left_expr | *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EFuncExists) {
                    pass = exists(if_stmt->expr.val.unicode_string);
                } else if (stmt_type == ss_expr_op::EFuncAppProperties) {
                    pass = /*appprop(if_stmt->expr.left_expr, );*/ false;
                } else if (stmt_type == ss_expr_op::EFuncDevProperties) {
                    pass = false;
                }

                return pass;
            };

            auto can_pass_else = [&](sis_field *wrap_if_stmt) -> bool {
                sis_else_if *if_stmt = (sis_else_if *)(wrap_if_stmt);
                ss_expr_op stmt_type = if_stmt->expr.op;

                bool pass = false;

                if (stmt_type == ss_expr_op::EBinOpEqual) {
                    pass = (*if_stmt->expr.left_expr == *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpNotEqual) {
                    pass = (*if_stmt->expr.left_expr != *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpGreaterThan) {
                    pass = (*if_stmt->expr.left_expr > *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpGreaterThanOrEqual) {
                    pass = (*if_stmt->expr.left_expr >= *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpLessThan) {
                    pass = (*if_stmt->expr.left_expr < *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EBinOpLessOrEqual) {
                    pass = (*if_stmt->expr.left_expr <= *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::ELogOpAnd) {
                    pass = (*if_stmt->expr.left_expr & *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::ELogOpOr) {
                    pass = (*if_stmt->expr.left_expr | *if_stmt->expr.right_expr);
                } else if (stmt_type == ss_expr_op::EFuncExists) {
                    pass = exists(if_stmt->expr.val.unicode_string);
                } else if (stmt_type == ss_expr_op::EFuncAppProperties) {
                    pass = /*appprop(if_stmt->expr.left_expr, );*/ false;
                } else if (stmt_type == ss_expr_op::EFuncDevProperties) {
                    pass = false;
                }

                return pass;
            };

            // Parse if blocks
            for (auto &wrap_if_statement : install_block.if_blocks.fields) {
                bool pass = can_pass(wrap_if_statement.get());
                sis_if *if_stmt = (sis_if *)(wrap_if_statement.get());

                if (pass) {
                    interpret(if_stmt->install_block, ++crr_blck_idx);
                } else {
                    for (auto &wrap_else_brnch : if_stmt->else_if.fields) {
                        pass = can_pass_else(wrap_else_brnch.get());
                        sis_else_if *if_stmt = (sis_else_if *)(wrap_if_statement.get());

                        if (pass) {
                            interpret(if_stmt->install_block, ++crr_blck_idx);
                        }
                    }
                }
            }

            for (auto &wrap_mini_pkg : install_block.controllers.fields) {
                sis_controller *ctrl = (sis_controller *)(wrap_mini_pkg.get());
                interpret(ctrl->install_block, ++crr_blck_idx);
            }

            return true;
        }
    }
}
