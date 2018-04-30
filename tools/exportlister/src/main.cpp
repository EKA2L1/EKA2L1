#include <loader/eka2img.h>
#include <loader/romimage.h>
#include <common/log.h>

#include <iostream>
#include <string>
#include <fstream>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
using namespace eka2l1::loader;

std::vector<fs::path> libs;

void dump_to_syms(const std::string& org_path, eka2img& img) {
    std::string lib = fs::path(org_path).filename().replace_extension(fs::path("")).string();
    std::ofstream of(lib + ".nsd");

    of << "library: " << lib << std::endl;

    for (uint32_t i = 0; i < img.ed.syms.size(); i++) {
        of << "\t" << i << ": " << img.ed.syms[i] << std::endl;
    }
}

void dump_to_syms2(const std::string& org_path, romimg& img) {
    std::string lib = fs::path(org_path).filename().replace_extension(fs::path("")).string();
    std::ofstream of(lib + ".nsd");

    of << "library: " << lib << std::endl;

    for (uint32_t i = 0; i < img.exports.size(); i++) {
        of << "\t" << i << ": " << img.exports[i] << std::endl;
    }
}

void list_all_libs() {
    for (auto& f: fs::directory_iterator(".")) {
        fs::path fdir = fs::path(f);

        if (fs::is_regular_file(fdir) && fdir.extension().string() == ".dll") {
            libs.push_back(fs::path(f));
        }
    }
}

int main(int argc, char** argv) {
    eka2l1::log::setup_log(nullptr);

    if (argc == 1) {
        list_all_libs();
    } else {
        for (auto i = 1; i < argc; i++) {
            libs.push_back(fs::path(argv[i]));
        }
    }

    for (auto& path: libs) {
        auto img = parse_eka2img(path.string(), false);

        if (img) {
            dump_to_syms(path.string(), img.value());
        } else {
            auto rom = parse_romimg(path.string());

            if (rom) {
                dump_to_syms2(path.string(), rom.value());
            } else {
                LOG_ERROR("Can't dump: not romimg or e32img!");
            }
        }
    }

    return 0;
}
