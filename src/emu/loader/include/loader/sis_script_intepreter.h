#pragma once

#include <vector>
#include <string>

#include <loader/sis_fields.h>

namespace eka2l1 {
    namespace loader {
        class ss_inst {
            std::vector<std::string> args;

            ss_op raw_op;
            uint32_t option;

        public:
            ss_inst(ss_op op, uint32_t option);
            void add_arg(std::string str);

            ss_op get_raw_op() {
                return raw_op;
            }

            size_t get_arg_count() {
                return args.size();
            }

            std::string* get_args() {
                return args.data();
            }
        };

        ss_inst build_inst(ss_op op, uint32_t option, std::string arg1);
        ss_inst build_inst(ss_op op, uint32_t option, std::string arg1, std::string arg2);
        ss_inst build_inst(ss_op op, uint32_t option, std::string arg1, std::string arg2);

        class ss_script {
            std::vector<ss_inst> insts;

        public:
             ss_script() {}

             void add_inst(ss_inst script_inst);
             ss_inst get_inst(const uint32_t idx);
        };

        class ss_intepreter {
            friend class ss_script;
            ss_script script;

        public:
            ss_intepreter(ss_script& script);

            bool intepret();
        };
    }
}
