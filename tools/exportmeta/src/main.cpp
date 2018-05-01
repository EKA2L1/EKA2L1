#include <clang-c/Index.h>

#include <vector>
#include <string>
#include <tuple>
#include <optional>

using sid = uint32_t;
using entry = std::pair<sid, std::string>;
using entries = std::vector<entry>;
using typeinfo = std::tuple<sid, std::string, std::string>;
using typeinfos = std::vector<typeinfo>;

std::optional<typeinfos> gather_typeinfos(const std::string path) {
    CXIndex idx = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index,
        path.c_str(), nullptr, 0,
        nullptr, 0,
        CXTranslationUnit_None);

    if (!unit) {
        return std::optional<typeinfos>{};
    }

    CXCursor cursor = clang_getTranslationUnitCursor(unit);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(idx);
}

int main(int argc, char** argv) {

}
