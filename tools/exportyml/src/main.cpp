// Unfinished

#include <common/cvt.h>
#include <common/hash.h>
#include <common/log.h>

#include <yaml-cpp/yaml.h>

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

// You could tell that I'm depressed
#include <cxxabi.h>

using namespace eka2l1::log;
using namespace eka2l1;
namespace fs = std::experimental::filesystem;

std::vector<fs::path> libs;
std::ofstream ostr("modules.h");
YAML::Emitter emitter;

std::string current_lib;

using function_name = std::string;
using vtable_name = std::string;
using typeinfo_name = std::string;
using function_raw_args = std::string;
using function_args = std::string;

struct function {
	function_name dename;
	uint32_t id;

	function(function_name name, uint32_t sid)
		: dename(name), id(sid) {}
};

void init_log() {
    setup_log(nullptr);
}

void list_all_libs() {
    for (auto &f : fs::directory_iterator(".")) {
        fs::path fdir = fs::path(f);

        if (fs::is_regular_file(fdir) && fdir.extension().string() == ".idt") {
            libs.push_back(fs::path(f));
        }
    }
}

std::vector<function> read_idt(const fs::path &path) {
    std::ifstream idt(path.string());
    std::vector<function> fts;

    uint32_t lc = 1;

    while (idt) {
        std::string line;
        std::getline(idt, line);

        if (line[0] == ';' || line == "") {
            continue;
        }

        auto where = line.find("Name=");

        if (where == std::string::npos) {
            LOG_ERROR("Unable to interpret IDT line: {}", lc);
        }

        function_name raw_sauce = line.substr(where + 5);

        auto pos = raw_sauce.find("Pascal");

        if (pos != std::string::npos) {
            continue;
        }

        char *cooked_sauce = reinterpret_cast<char *>(malloc(512));
        int result = 0;

        size_t len = raw_sauce.length();
        char *cooked_output = abi::__cxa_demangle(raw_sauce.c_str(), cooked_sauce, &len, &result);

		std::string cooked;

		if (cooked_output) {
			cooked = cooked_output;
			std::string temp(raw_sauce);

			if (temp.find("C1") != std::string::npos) {
				cooked += " (complete object constructor)";
			}
			else if (temp.find("C2") != std::string::npos) {
				cooked += " (base object constructor)";
			}
			else if (temp.find("C3") != std::string::npos) {
				cooked += " (complete object allocating constructor)";
			}
			else if (temp.find("D1") != std::string::npos) {
				cooked += " (complete object destructor)";
			}
			else if (temp.find("D2") != std::string::npos) {
				cooked += " (base object destructor)";
			}
		}

        if (!result) {
            fts.push_back(function(cooked, common::hash(raw_sauce)));
            LOG_INFO("{}", cooked);
        } else {
            fts.push_back(function(raw_sauce, common::hash(raw_sauce)));
			LOG_INFO("{}", raw_sauce);
            free(cooked_sauce);
        }

        ++lc;
    }

    return fts;
}

std::string normalize_for_hash(std::string org) {
    if (org.find("vtable for") != std::string::npos) {
        return org;
    }

    if (org.find("typeinfo for") != std::string::npos) {
        return org;
    }

    auto remove = [](std::string& inp, std::string to_remove) {
        size_t pos = 0;

        do {
            pos = inp.find(to_remove, pos);

            if (pos == std::string::npos) {
                break;
            } else {
                inp.erase(pos, to_remove.length());
            }
        } while (true);
    };

    for (auto& c: org) {
        c = std::tolower(c);
    }

    remove(org, " ");
    // Remove class in arg

    std::size_t beg = org.find("(");
    std::size_t end = org.find(")");

    if (beg == std::string::npos ||
            end == std::string::npos) {
        return org;
    }

    std::string sub = org.substr(beg, end);

    remove(sub, "class");
    remove(sub, "const");
    remove(sub, "struct");

    auto res =  org.substr(0, beg) + sub + org.substr(end+1);

    return res;
}

void yml_link(std::vector<function>& funcs, const fs::path &path) {
    auto lib = path.filename().replace_extension("").string();

    emitter << YAML::Key << lib << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "sid" << YAML::Value << "0x" + common::to_string(common::hash(lib), std::hex);
        emitter << YAML::Key << "exports";
        emitter << YAML::Value << YAML::BeginMap;

            for (auto &func : funcs) {
                emitter << YAML::Key << func.dename<< YAML::Value << "0x" + common::to_string(func.id, std::hex);
            }

        emitter << YAML::EndMap;
    emitter << YAML::EndMap;
}

void header_link(std::vector<function>& funcs, const fs::path& path) {
	auto lib = path.filename().replace_extension("").string();

	ostr << "LIB(" << lib << ")" << std::endl;

	for (auto &func : funcs) {
		ostr << "EXPORT(" << '"' << func.dename << '"' << ", " << func.id << ")" << std::endl;
	}

	ostr << "ENDLIB()" << std::endl;
}

int main(int argc, char **argv) {
    init_log();

    if (argc == 1) {
        list_all_libs();
    } else {
        for (auto i = 1; i < argc; i++) {
            libs.push_back(fs::path(argv[i]));
        }
    }

    emitter << YAML::BeginMap;
    emitter << YAML::Key << "modules";
    emitter << YAML::Value << YAML::BeginMap;

    for (auto lib : libs) {
		LOG_INFO("Lib: {}", lib.string());

		auto funcs = read_idt(lib);

        yml_link(funcs, lib);
		header_link(funcs, lib);
    }

    emitter << YAML::EndMap;
    emitter << YAML::EndMap;

    std::ofstream out("db.yml");
    out << emitter.c_str();

    return 0;
}
