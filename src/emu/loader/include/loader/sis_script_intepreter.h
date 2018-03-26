#pragma once

#include <vector>
#include <string>

namespace eka2l1 {
    namespace loader {
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
