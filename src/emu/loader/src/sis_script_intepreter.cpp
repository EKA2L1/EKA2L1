#include <loader/sis_script_intepreter.h>
#include <cassert>

namespace eka2l1 {
    namespace loader {
        ss_inst::ss_inst(ss_op op, uint32_t option):
            raw_op(op), option(option) {
        }

        void ss_inst::add_arg(std::string str) {
            args.push_back(str);
        }

        ss_inst build_inst(ss_op op, uint32_t option, std::string arg1) {
            ss_inst new_inst(op, option);
            new_inst.add_arg(arg1);

            return new_inst;
        }

        ss_inst build_inst(ss_op op, uint32_t option, std::string arg1, std::string arg2) {
            ss_inst new_inst(op, option);
            new_inst.add_arg(arg1);
            new_inst.add_arg(arg2);

            return new_inst;
        }

        ss_inst build_inst(ss_op op, uint32_t option, std::string arg1, std::string arg2, std::string arg3) {
            ss_inst new_inst(op, option);
            new_inst.add_arg(arg1);
            new_inst.add_arg(arg2);
            new_inst.add_arg(arg3);

            return new_inst;
        }

        void ss_script::add_inst(ss_inst script_inst) {
            insts.push_back(script_inst);
        }

        ss_inst ss_script::get_inst(const uint32_t idx) {
            assert(idx >= insts.size());

            return insts[idx];
        }

        ss_intepreter::ss_intepreter(ss_script& script)
            : script(script) {}
    }
}
