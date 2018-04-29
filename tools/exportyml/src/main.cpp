#include <common/log.h>
#include <yaml-cpp/yaml.h>

#include <experimental/filesystem>
#include <fstream>
#include <sstream>

// You could tell that I'm depressed
#include <cxxabi.h>

using namespace eka2l1::log;
namespace fs = std::experimental::filesystem;

std::vector<fs::path> libs;
YAML::Emitter emitter;

std::string current_lib;

using function_name = std::string;
using function_raw_args = std::string;
using function_args = std::string;

void init_log() {
    setup_log(nullptr);
}

void list_all_libs() {
    for (auto& f: fs::directory_iterator(".")) {
        fs::path fdir = fs::path(f);

        if (fs::is_regular_file(fdir) && fdir.extension().string() == ".idt") {
            libs.push_back(fs::path(f));
        }
    }
}

std::vector<uint32_t> read_nsd(const fs::path& path) {
    YAML::Node node = YAML::LoadFile(path.string());
    std::vector<uint32_t> exp_nodes;

    current_lib = node["library"].as<std::string>();

    uint64_t i = 0;

    while (true) {
        auto nn = node[std::to_string(i)];

        if (!nn) {
            break;
        }

        exp_nodes.push_back(nn.as<uint32_t>());

        ++i;
    }

    return exp_nodes;
}

std::vector<function_name> read_idt(const fs::path& path) {
    std::ifstream idt(path.string());
    std::vector<function_name> fts;

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
        function_name cooked_sauce;

        cooked_sauce.resize(256);
        int result = 0;

        size_t len = raw_sauce.length();

        abi::__cxa_demangle(raw_sauce.c_str(), cooked_sauce.data(), &len, &result);

        fts.push_back(cooked_sauce);

        ++lc;
    }

    return fts;
}

void yml_link(const fs::path& path) {
    fs::path dll_path = path.filename().replace_extension(".nsd");

    auto exports = read_nsd(dll_path);
    auto func_names = read_idt(path);

}

int main(int argc, char** argv) {
    init_log();

    if (argc == 1) {
        list_all_libs();
    } else {
        for (auto i = 1; i < argc; i++) {
            libs.push_back(fs::path(argv[i]));
        }
    }

    for (auto lib: libs) {
        yml_link(lib);
    }

    return 0;
}
