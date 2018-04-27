#include <common/log.h>
#include <yaml-cpp/yaml.h>

#include <experimental/filesystem>
#include <fstream>

// You could tell that I'm depressed
#include <cxxabi.h>

using namespace eka2l1::log;
namespace fs = std::experimental::filesystem;

std::vector<fs::path> libs;
YAML::Emitter emitter;

std::string current_lib;

using function_name = std::string;

void init_log() {
    setup_log(nullptr);
}

void list_all_libs() {
    for (auto& f: fs::directory_iterator(".")) {
        fs::path fdir = fs::path(f);

        if (fs::is_regular_file(fdir) && fdir.extension().string() == ".nsd") {
            libs.push_back(fs::path(f));
        }
    }
}

std::vector<uint32_t> read_nsd(const fs::path& path) {

}

// Based on mrRosset work
// See: https://github.com/mrRosset/Engemu/blob/master/Scripts/Export_Converter.py
std::string extract_num(std::string ot) {

}

function_name demangle_name(function_name fn) {
    if (fn.length() == 0) {
        return fn;
    }

    if (fn[0] == '.') {
        // Destructors, kill them
        if (fn[1] != '_') {
            LOG_ERROR("Unknown destructor format: {}", fn);
            return fn;
        }

        auto left = fn.substr(2);
        std::string cnst = "";

        if (left[0] == 'C') {
            left = fn.substr(1);
            cnst = "const";
        } else if (left[0] == 't') {
            left = fn.substr(1);

        }


    }

    return fn;
}

std::vector<function_name> read_idt(const fs::path& path) {
    std::ifstream idt(path.string());
    std::vector<function_name> fts;

    uint32_t line = 1;

    while (idt) {
        std::string line = std::getline(idt);

        if (line[0] == ";" || line == "") {
            continue;
        }

        auto where = line.find("Name=");

        if (where == std::string::npos) {
            LOG_ERROR("Unable to interpret IDT line: {}", line);
        }

        function_name raw_sauce = line.substr(where + 5);
        function_name cooked_sauce;

        cooked_sauce.resize(256);
        int result = 0;

#ifndef _MSC_VER
        abi::__cxa_demangle(raw_sauce.c_str(), cooked_sauce.data(), raw_sauce.length(), &result);
#else
        cooked_sauce = demangle_name(raw_sauce);
#endif

        fts.push_back(cooked_sauce);

        ++line;
    }
}

void yml_link(const fs::path& path) {
    fs::path idt_path = path.filename().replace_extension(".idt");

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

    return 0;
}
