#include <disasm/disasm.h>
#include <common/log.h>
#include <capstone.h>

#include <memory>
#include <functional>
#include <sstream>

namespace eka2l1 {
    namespace disasm {
        using insn_ptr = std::unique_ptr<cs_insn, std::function<void(cs_insn *)>>;
        using csh_ptr = std::unique_ptr<csh, std::function<void(csh*)>>;

        csh cp_handle;
        insn_ptr cp_insn;

        void shutdown_insn(cs_insn* insn) {
            if (cp_insn) {
                cs_free(insn, 1);
            }
        }

        void init() {
            cs_err err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &cp_handle);

            if (err != CS_ERR_OK) {
                LOG_ERROR("Capstone open errored! Error code: {}", err);
                return;
            }

            cp_insn = insn_ptr(cs_malloc(cp_handle), shutdown_insn);
            cs_option(cp_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

            if (!cp_insn) {
                LOG_ERROR("Capstone INSN allocation failed!");
                return;
            }
        }

        void shutdown() {
            cp_insn.reset();
            cs_close(&cp_handle);
        }

        std::string disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb) {
            cs_err err = cs_option(cp_handle, CS_OPT_MODE, thumb ? CS_MODE_THUMB : CS_MODE_ARM);

            if (err != CS_ERR_OK) {
                LOG_ERROR("Unable to set disassemble option! Error: {}", err);
                return "";
            }

            bool success = cs_disasm_iter(cp_handle, &code, &size, &address, cp_insn.get());

            std::ostringstream out;
            out << cp_insn->mnemonic << " " << cp_insn->op_str;

            if (!success) {
                cs_err err = cs_errno(cp_handle);
                out << " (" << cs_strerror(err) << ")";
            }

            return out.str();
        }
    }
}
