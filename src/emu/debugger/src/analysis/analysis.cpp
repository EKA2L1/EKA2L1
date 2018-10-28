#include <debugger/analysis/analysis.h>

#include <core/core_mem.h>
#include <core/disasm/disasm.h>

#include <common/log.h>

#include <optional>

namespace eka2l1::analysis {
    enum class inst_reg {
        r0,
        r1,
        r2, 
        r3,
        r4, 
        r5,
        r6,
        r7,
        r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15,
        number = 0x900000,
        unknown = 0x1000000
    };
    
    #define REG_TO_STRING_CASE(r) case inst_reg::r: \
                                    return #r;

    std::string reg_to_string(const inst_reg reg) {
        switch (reg) {
        REG_TO_STRING_CASE(r0)
        REG_TO_STRING_CASE(r1)
        REG_TO_STRING_CASE(r2)
        REG_TO_STRING_CASE(r3)
        REG_TO_STRING_CASE(r4)
        REG_TO_STRING_CASE(r5)
        REG_TO_STRING_CASE(r6)
        REG_TO_STRING_CASE(r7)
        REG_TO_STRING_CASE(r8)
        REG_TO_STRING_CASE(r9)
        REG_TO_STRING_CASE(r10)
        REG_TO_STRING_CASE(r11)
        case inst_reg::r12: {
            return "ip";
        }
        case inst_reg::r13: {
            return "sp";
        }
        case inst_reg::r14: {
            return "lr";
        }
        case inst_reg::r15: {
            return "pc";
        }

        default:
            break;
        }

        return "";
    }

    inst_reg string_to_reg(const std::string &s) {
        if (s[0] == 'r') {
            std::string num = s.substr(1, s.length() - 2);
            return static_cast<inst_reg>(std::atoi(num.c_str()));
        }

        if (s == "ip") {
            return inst_reg::r12;
        }

        if (s == "sp") {
            return inst_reg::r13;
        }

        if (s == "lr") {
            return inst_reg::r14;
        }

        if (s == "pc") {
            return inst_reg::r15;
        }

        return inst_reg::unknown;
    }

    std::uint32_t get_capstone_constant_as_number(std::string n) {
        if (n.length() >= 4 && n.substr(1, 2) == "0x") {
            return std::stoul(n.substr(3), 0, 16);
        }

        auto v = n.substr(1);
        return std::atol(v.c_str());
    

        return 0;
    }

    std::string expand_side(std::string target) {
        std::string result;

        if (target[0] == '{') {
            target.erase(0, 1);
            target.erase(target.length() - 1, 1);

            std::size_t colon_pos = target.find_first_of(',');

            if (colon_pos == std::string::npos) {
                colon_pos = target.length() - 1;
            }

            while (colon_pos != std::string::npos) {
                std::string v = target.substr(0, colon_pos);
                std::size_t separate_pos = v.find('-');

                if (separate_pos != std::string::npos) {
                    inst_reg beg = string_to_reg(v.substr(0, separate_pos - 2));
                    inst_reg end = string_to_reg(v.substr(separate_pos + 2, colon_pos - 1));

                    for (inst_reg i = beg; i < end; i = static_cast<inst_reg>(static_cast<int>(i) + 1)) {
                        result += reg_to_string(i) + ", ";
                    }
                } else {
                    result += v + ", ";
                }

                target.erase(0, colon_pos + 2);

                colon_pos = target.find_first_of(',');
            }

            if (target.length() > 0) {
                result += target;
            }
        } else {
            result = target;
        }

        return result;
    }

    std::vector<inst_reg> breakup_to_regs(std::string target) {
        std::vector<inst_reg> regs;

        if (target[0] == '[') {
            target.erase(0, 1);
            target.erase(target.length() - 1, 1);
        }

        std::size_t colon_pos = target.find_first_of(',');

        if (colon_pos == std::string::npos) {
            colon_pos = target.length() - 1;
        }

        while (colon_pos != std::string::npos) {
            std::string v = target.substr(0, colon_pos);

            if (v[0] == '#') {
                regs.push_back(static_cast<inst_reg>(get_capstone_constant_as_number(v) |
                    static_cast<int>(inst_reg::number)));
            } else {
                regs.push_back(string_to_reg(v));
            }

            target.erase(0, colon_pos + 2);

            colon_pos = target.find_first_of(',');
        }

        if (target[0] == '#') {
            regs.push_back(static_cast<inst_reg>(get_capstone_constant_as_number(target) |
                static_cast<int>(inst_reg::number)));
        } else {
            regs.push_back(string_to_reg(target));
        }

        return regs;
    }

    struct inst {
        std::string op;
        
        std::optional<std::string> left;
        std::optional<std::string> right;

        bool is_left_constant() {
            return left && (*left)[0] == '#';
        }

        bool is_right_constant() {
            return right && (*right)[0] == '#';
        }

        std::uint32_t get_left_as_constant() {
            if (is_left_constant()) {
                return get_capstone_constant_as_number(*left);
            }

            return 0;
        }

        std::uint32_t get_right_as_constant() {
            if (is_right_constant()) {
                return get_capstone_constant_as_number(*right);
            }

            return 0;
        }

        bool is_branch_inst() {
            return op[0] == 'b';
        }

        bool is_mov_inst() {
            return op.length() >= 3 && op.substr(0, 3) == "mov";
        }

        bool is_mov_reg(const inst_reg reg) {
            return is_mov_inst() && is_reg_in_left(reg);
        }

        bool is_ldm_reg(const inst_reg r) {
            return (op.substr(0, 3) == "ldm" && is_reg_in_right(r));
        }

        bool is_load_inst() {
            return op.length() >= 2 && op.substr(0, 2) == "ld";
        }

        bool is_load_reg(const inst_reg reg) {
            return is_load_inst() && is_reg_in_left(reg);
        }

        bool is_branch_unconditional() {
            return (op == "b" || op == "bx" || 
                op == "blx" || op == "blj");
        }

        bool is_branch_exchange_inst() {
            return (is_branch_inst() && op.find('x') != std::string::npos);
        }

        bool is_reg_in_left(const inst_reg reg) {
            return left && left->find(reg_to_string(reg)) != std::string::npos;
        }

        bool is_reg_in_right(const inst_reg reg) {
            return right && right->find(reg_to_string(reg)) != std::string::npos;
        }
    };

    inst breakup_disasm_string(std::string s) {
        inst i {};
        
        std::size_t op_end_pos = s.find_first_of(" ");

        if (op_end_pos == std::string::npos) {
            i.op = s;
            return i;
        } 

        i.op = s.substr(0, op_end_pos);
        s.erase(0, op_end_pos + 1);

        i.left = std::string{};
        
        if (s[0] == '{') {
            auto e = s.find_first_of('}');
            i.left = expand_side(s.substr(0, e + 1));

            s.erase(0, e + 1);

            if (s.substr(0, 2) == ", ") {
                s.erase(0, 2);
            }
        } else {
            std::size_t seperator = s.find_first_of(',');

            if (seperator == std::string::npos) {
                i.left = expand_side(s);
                return i;
            }

            i.left = s.substr(0, seperator);
            s.erase(0, seperator + 2);

            i.left = expand_side(*i.left);
        }

        if (s.length() > 0) {
            i.right = expand_side(s);
        }

        return i;
    }

    bool is_subroutine_already_analysis(std::vector<subroutine> &subs, const std::uint32_t addr) {
        return (std::find_if(subs.begin(), subs.end(), [addr](const subroutine &sub) {
            return sub.start == addr;
        }) != subs.end());
    }

    bool calculate_side(std::string s, std::uint32_t *ip, memory_system *mem, std::uint32_t addr, int opc, bool thumb) {
        std::vector<inst_reg> regs = breakup_to_regs(s);
        bool calc_value_in_expression_point_tp = (s[0] == '[');

        std::uint32_t old_ip = *ip;

        for (auto &reg: regs) {
            if (static_cast<int>(reg) <= 15 && reg != inst_reg::r12 && reg != inst_reg::r15) {
                return false;
            }

            std::uint32_t val = 0;
            
            if (reg == inst_reg::r12) {
                val = old_ip;
            } else if (reg == inst_reg::r15) {
                val = addr;
            } else {
                val = static_cast<int>(reg) & ~static_cast<int>(inst_reg::number);
            }

            if (opc == 0) {
                *ip += val;
            } else {
                *ip -= val;
            }
        }

        if (calc_value_in_expression_point_tp) {
            std::uint32_t t = (*ip);
            t += thumb ? 4: 8;
            mem->read(t, ip, thumb ? 2 : 4);
        }

        return true;
    }

    bool interpret_r12(inst &i, uint32_t *ip, memory_system *mem, uint32_t addr, bool thumb) {
        if (i.is_mov_inst() || i.is_load_inst()) {
            *ip = 0;

            if (calculate_side((*i.right), ip, mem, addr, 0, thumb)) {
                return true;
            }
        } else {
            int opc = -1;

            if (i.op.length() >= 3) {
                auto v = i.op.substr(0, 3);

                if (v == "add") {
                    opc = 0;
                } else if (v == "sub") {
                    opc = 1;
                }
            }

            if (opc != -1) {
                if (calculate_side((*i.right), ip, mem, addr, opc, thumb)) {
                    return true;
                }
            }

            return false;
        }

        return false;
    }

    void run_through_subroutine(std::vector<subroutine> &subroutines, disasm *asmdis, 
        memory_system *mem, std::uint32_t start) {
        if (!mem->get_real_pointer(start)) {
            LOG_WARN("A branch or PC jumped here with invalid address, cut this");
            return;
        }

        LOG_TRACE("Detect subroutine 0x{:x}", start);

        subroutine sub;

        sub.thumb = false;
        sub.start = start;

        if (start % 2 == 1) {
            sub.thumb = true;
            sub.start -= 1;
            start -= 1;
        }

        if (is_subroutine_already_analysis(subroutines, sub.start)) {
            return;
        }

        bool should_cont = true;
        bool ip_caculate = true;

        std::uint32_t ip;

        while (should_cont) {
            std::string dis = asmdis->disassemble(
                reinterpret_cast<std::uint8_t*>(mem->get_real_pointer(start)),
                sub.thumb ? 2 : 4, start, sub.thumb);
            
            inst i = breakup_disasm_string(dis);

            if (ip_caculate && i.is_reg_in_left(inst_reg::r12)) {
                ip_caculate = interpret_r12(i, &ip, mem, start, sub.thumb);
            }

            if (i.is_branch_unconditional() || i.is_ldm_reg(inst_reg::r15) 
                || i.is_mov_reg(inst_reg::r15) || i.is_load_reg(inst_reg::r15)) {
                if (i.op == "blx") {
                    sub.thumb = !sub.thumb;
                } else {
                    should_cont = false;
                }

                // PC is definitely moving
                if (i.is_branch_unconditional()) {
                    if (i.is_left_constant()) {
                        std::uint32_t addr = i.get_left_as_constant();

                        if (addr != 0) {
                            run_through_subroutine(subroutines, asmdis, mem,
                                addr);
                        } else {
                            LOG_TRACE("Nullopt for switching mode, not jumping");
                        }
                    } else if (i.is_reg_in_left(inst_reg::r12)) {
                        run_through_subroutine(subroutines, asmdis, mem, 
                            ip);
                    }
                }
            } else {
                if (i.is_branch_inst() && i.is_left_constant()) {
                    if (i.op == "blx") {
                        sub.thumb = !sub.thumb;
                    }

                    std::uint32_t addr = i.get_left_as_constant();

                    if (addr != 0) {
                        run_through_subroutine(subroutines, asmdis, mem,
                            addr);
                    } else {
                        LOG_TRACE("Nullopt for switching mode, not jumping");
                    }
                }
            }

            start += sub.thumb ? 2 : 4;
        }

        sub.end = start;
        subroutines.push_back(sub);
    }

    std::vector<subroutine> analysis_image(const loader::eka2img &img, disasm *asmdis,
        memory_system *mem) {
        std::vector<subroutine> subroutines;

        // Check the entry 
        run_through_subroutine(subroutines, asmdis, mem, img.rt_code_addr + img.header.entry_point);
        
        for (const auto &sym: img.ed.syms) {
            run_through_subroutine(subroutines, asmdis, mem, img.rt_code_addr + sym - img.header.code_base);
        }

        return subroutines;
    }

    std::vector<subroutine> analysis_image(const loader::romimg &img, disasm *asmdis
        , memory_system *mem) {
        std::vector<subroutine> subroutines;

        // Check the entry 
        run_through_subroutine(subroutines, asmdis, mem, img.header.entry_point);
        
        for (const auto &sym: img.exports) {
            run_through_subroutine(subroutines, asmdis, mem, sym);
        }

        return subroutines;
    }
}