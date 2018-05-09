#include <abi/eabi.h>

#include <vector>
#include <mutex>
#include <sstream>

namespace eka2l1 {
    // libcxxabi doesnt work with msvc
    namespace eabi {
        // Some functions leave.
        struct leave_handler {
            std::string leave_msg;
            uint32_t leave_id;
        };

        std::vector<leave_handler> leaves;
        std::mutex mut;

        void leave() {}
        void trap_leave(uint32_t id) {}

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

        std::string demangle_arg(std::stringstream& sstream) {
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
                char letter;
                sstream >> letter;

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
                            sstream >> next;
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

                            arg.resize(len);

                            sstream.read(arg.data(), arg.length());
                        }

                        break;
                    }
                }
            }

            if (has_template) {
                char letter;
                sstream >> letter;

                if (letter == '1') {
                    return "";
                }

                sstream >> letter;

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

                c1.resize(num);

                sstream.read(c1.data(), c1.size());

                sstream >> num;
                c2.resize(num);

                sstream.read(c2.data(), c2.size());

                arg = c1 + "::" + c2;
            }

            if (is_func) {
                std::vector<std::string> ptrs;
                std::vector<std::string> func_args;
                std::string temp = "";
                std::string tempall = "";

                while (!sstream.eof()) {
                    char letter;
                    sstream >> letter;

                    if (letter != '_') {
                        temp += letter;
                    } else {
                        tempall += temp;
                        temp = "";
                    }
                }

                std::stringstream tempallstream(tempall);

                while (!tempallstream.eof()) {
                    func_args.push_back(demangle_arg(tempallstream));
                }

                arg = "(*)(";

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
            std::stringstream argstream(mangled_args);
            std::vector<std::string> args;

            while (!argstream.eof()) {
                args.push_back(demangle_arg(argstream));
            }

            return "";
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

                    return "";
                }
            }
        }

		std::string mangle(std::string target) {

        }
    }
}