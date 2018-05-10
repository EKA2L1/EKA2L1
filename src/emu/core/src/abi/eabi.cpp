#include <abi/eabi.h>
#include <common/advstream.h>
#include <common/log.h>

#include <vector>
#include <mutex>
#include <sstream>

namespace eka2l1 {
    // libcxxabi doesnt work with msvc
    // tends to use this to replace libcxxabi
    namespace eabi {
        // Some functions leave.
        struct leave_handler {
            std::string leave_msg;
            uint32_t leave_id;
        };

        std::vector<leave_handler> leaves;
        std::mutex mut;

        void leave(uint32_t id, const std::string& msg)
        {
            std::lock_guard<std::mutex> guard(mut);
            leave_handler handler;

            handler.leave_id = id;
            handler.leave_msg = msg;

            leaves.push_back(handler);

            LOG_CRITICAL("Process leaved! ID: {}, Message: {}", id, msg);
        }

        void trap_leave(uint32_t id) {
            auto res = std::find_if(leaves.begin(), leaves.end(),
                [id](auto lh) { return lh.leave_id == lh });

            if (res != leaves.end()) {
                std::lock_guard<std::mutex> guard(mut);
                leaves.erase(res);

                LOG_INFO("Leave ID {} has been trapped", id);
            }
        }

        void fake_eabi_pure_virtual_call() {
            LOG_CRITICAL("WHAT DID YOU DO TO MY PRECIOUS VTABLE!");
        };

        void fake_eabi_deleted_virtual_call() {
            LOG_CRITICAL("WHAT DID YOU DO TO MY PRECIOUS VTABLE!");
        }

        std::string extract_mangled_string(std::string target, uint32_t* after_pos = nullptr) {
            std::string lenstr = "";
            uint32_t crr_pos = 0;

            while (std::isdigit(target[crr_pos])) {
                ++crr_pos;
                lenstr += target[crr_pos];
            }

            if (after_pos != nullptr) {
                *after_pos = lenstr.length() + std::atoi(lenstr.data());
            }

            return target.substr(crr_pos, std::atoi(lenstr.data()));
        }

        std::string demangle_arg(common::advstringstream& sstream) {
            bool is_func = false;
            bool has_template = false;
            bool has_namespace = false;

            std::string arg = "";

            std::vector<std::string> pointers;
            decltype(pointers) refs;
            decltype(pointers) unsigneds;
            decltype(pointers) signeds;
            decltype(pointers) consts;

            std::string templatez = "";

            while (arg.empty()) {
                char letter = sstream.get();

                switch (letter) {
                    case 'P': {
                        pointers.push_back("*");
                        break;
                    }

                    case 'R': {
                        refs.push_back("&");
                        break;
                    }

                    case 'S': {
                        signeds.push_back("signed");
                        break;
                    }

                    case 'U': {
                        unsigneds.push_back("unsigned");
                        break;
                    }

                    case 'C': {
                        consts.push_back("const");
                        break;
                    }

                    case 'F': {
                        is_func = true;
                        arg = "(*)";
                        
                        break;
                    }

                    case 't': {
                        has_template = true;
                        break;
                    }

                    case 'Q': {
                        char next = sstream.peek();

                        if (next == '2') {
                            next = sstream.get();
                            has_namespace = true;
                            arg = "::";
                        }

                        break;
                    }

                    case 'i': {
                        arg = "int";
                        break;
                    }

                    case 'd': {
                        arg = "double";
                        break;
                    }

                    case 'v': {
                        arg = "void";
                        break;
                    }

                    case 'c': {
                        arg = "char";
                        break;
                    }

                    case 's': {
                        arg = "short";
                        break;
                    }

                    case 'l': {
                        arg = "long";
                        break;
                    }

                    case 'f': {
                        arg = "float";
                        break;
                    }

                    case 'G': {
                        break;
                    }

                    default: {
                        if (std::isdigit(letter)) {
                            uint32_t len;
                            sstream >> len;

                            arg = sstream.getstr(len);
                        }

                        break;
                    }
                }
            }

            if (has_template) {
                char letter = sstream.get();

                if (letter == '1') {
                    return "";
                }

                letter = sstream.get();

                if (letter == 'Z') {
                    templatez = demangle_arg(sstream);
                } else if (letter == 'Z') {
                    uint32_t constant_int_template = 0;
                    sstream >> constant_int_template;

                    templatez = std::to_string(constant_int_template);
                } else {
                    return "";
                }
            }

            if (has_namespace) {
                uint32_t num;
                sstream >> num;

                std::string c1 = ""; 
                std::string c2 = "";

                c1 = sstream.getstr(num);

                sstream >> num;

                c2 = sstream.getstr(num);

                arg = c1 + "::" + c2;
            }

            if (is_func) {
                std::vector<std::string> ptrs;
                std::vector<std::string> func_args;
                std::string temp = "";
                std::string tempall = sstream.peekstr(sstream.length());

                for (uint32_t i = 0; i < tempall.size(); i++) {
                    if (tempall[i] == ' ') {
                        tempall.erase(i, 1);
                    }
                }

                common::advstringstream tempallstream(tempall);

                sstream.getstr(tempallstream.length() + 1);

                while (!tempallstream.eof()) {
                    func_args.push_back(demangle_arg(tempallstream));
                }

                arg = demangle_arg(sstream);
                arg += "(*)(";

                for (auto arg2: func_args) {
                    arg += "," + arg2;
                }                

                arg += ")";
            } 

            std::string res = "";

            for (auto uw: unsigneds) {
                res += uw;
            }

            res += " ";

            for (auto sw: signeds) {
                res += sw;
            }

            res += " " + arg;

            if (has_template) {
                res += "<" + templatez + ">";
            }

            if (consts.size() > 0) {
                res += " ";

                for (auto cw: consts) {
                    res += cw;
                }
            }

            if (pointers.size() > 0) {
                res +=  " ";

                for (auto ptr: pointers) {
                    res += ptr;
                }
            }

            if (refs.size() > 0) {
                res += " ";

                for (auto ref: refs) {
                    res += ref;
                }
            }

            return res;
        }

        std::string demangle_args(std::string mangled_args) {
            common::advstringstream argstream(mangled_args);
            std::vector<std::string> args;
            std::string res;

            while (!argstream.eof()) {
                args.push_back(demangle_arg(argstream));

                if (argstream.length() >= 3 &&
                    argstream.peekstr(3) == "N13") {
                    argstream.getstr(3);
                    
                    for (auto i = 0; i < 3; i++) args.push_back(args.back());
                }

                if (argstream.length() >= 3 &&
                    (argstream.peekstr(3) == "N22" ||
                     argstream.peekstr(3) == "N21")) {
                    argstream.getstr(3);
                    for (auto i = 0; i < 3; i++) args.push_back(args.back());
                }

                if (argstream.length() > 1 &&
                    argstream.peek() == 'T') {
                    argstream.get();

                    auto rep = argstream.get();

                    if (rep != 1 && rep != 2 && rep != 3) {
                        return "";
                    }

                    args.push_back(args.back());
                }

                if (argstream.length() == 1 && 
                    argstream.peek() == 'e') {
                    args.push_back("...");
                }
            }

            res = "(";

            for (auto arg: args) {
                res += arg + ", ";
            };

            res += ")";

            return res;
        }

        std::vector<std::string> get_pairs(common::advstringstream& sstream) {
            std::vector<std::string> pairs;

            while (!sstream.eof()) {
                uint32_t len;
                sstream >> len;

                pairs.push_back(sstream.getstr(len));
            }
            
            return pairs;
        }

        std::string demangle_itanium_abi(std::string target) {
            common::advstringstream streamstr(target);

            if (target[0] != '_' || target[1] != 'Z') {
                return target;
            }

            std::string res;
            
            if (target[2] == 'N') {
                streamstr.getstr(3);
                auto nspcs = get_pairs(streamstr);

                for (uint32_t i = 0; i < nspcs.size(); i++) {
                    res += nspcs[i] + "::";
                }

                res += nspcs.back();
            } else {
                streamstr.getstr(2);
                auto name = get_pairs(streamstr);

                res += name[0];
            }

            auto args = demangle_args(streamstr.getstr(streamstr.length()));
            res += args;

            return res;
        }

		// Given a GCC-mangled string, demangle it into
        // a full real name
        // A C++ port of mrRosset's Engemu demangler
        std::string demangle(std::string target) {
            std::stringstream tstream(target);

            if (target[0] == '.') {
                if (target[1] == '_') {
                    return target;
                }

                std::string cnst = ""; 
                std::string rest = target.substr(2);

                if (rest[0] == 'C') {
                    rest = rest.substr(1);
                    cnst = "const";
                }

                if (rest[0] == 't') {
                    rest = rest.substr(1);

                    uint32_t nsafter = 0;

                    const std::string namespace_name = extract_mangled_string(rest, &nsafter);
                    const std::string args_all = extract_mangled_string(rest.substr(nsafter));

                    return namespace_name + "::" + namespace_name + demangle_args(args_all) + cnst;
                } else {
                    uint32_t nsafter = 0;

                    const std::string namespace_name = extract_mangled_string(rest, &nsafter);
                    return namespace_name + "::~" + namespace_name + "()" + cnst;
                }
            } else if (target[0] == '_') {
                // GCC 2.9x constructor
                if (std::isdigit(target[1])) {
                    std::string cnst = ""; 
                    std::string rest = target.substr(2);

                    if (rest[0] == 'C') {
                        rest = rest.substr(1);
                        cnst = "const";
                    }

                    if (rest[0] == 't') {
                        rest = rest.substr(1);

                        uint32_t nsafter = 0;

                        const std::string namespace_name = extract_mangled_string(rest, &nsafter);
                        const std::string args_all = extract_mangled_string(rest.substr(nsafter));

                        return namespace_name + "::" + namespace_name + demangle_args(args_all) + cnst;
                    }
                } else if (target[1] == 'Z') {
                    // GCC 3.x and upper mangling, for compability between version
                    return demangle_itanium_abi(target);   
                }
            }

            return "";
        }

		std::string mangle(std::string target) {
            return "";
        }
    }
}