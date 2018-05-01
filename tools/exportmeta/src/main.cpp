#include <common/log.h>
#include <common/hash.h>
#include <yaml-cpp/yaml.h>

#include <clang-c/Index.h>

#include <experimental/filesystem>
#include <vector>
#include <string>
#include <tuple>
#include <optional>

YAML::Emitter emitter;

using sid = uint32_t;

struct entry {
    sid id;
    std::string name;
    std::string attribute;
};

using entries = std::vector<entry>;

struct typeinfo {
    std::string name;
};

using typeinfos = std::vector<typeinfo>;

struct class_info {
    sid id;

    typeinfo info;
    entries func_entries;

    std::vector<sid> parents;

    bool sinful = false;
};

using class_infos = std::vector<class_info>;

using namespace eka2l1;
namespace fs = std::experimental::filesystem;

CXChildVisitResult visit_class(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    class_info* info = reinterpret_cast<class_info*>(client_data);

    if (info == nullptr) {
        LOG_ERROR("Client data is invalid!");
        return CXChildVisit_Break;
    }

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_CXXMethod:
        {
            entry new_entry;

            new_entry.name = reinterpret_cast<const char*>(clang_getCursorSpelling(cursor).data);
            new_entry.id = common::hash(
                        std::string(reinterpret_cast<const char*>(clang_Cursor_getMangling(cursor).data)));

            if (clang_CXXMethod_isPureVirtual(cursor)) {
                new_entry.attribute = "pure_virtual";
                info->sinful = true;
            } else if (clang_CXXMethod_isVirtual(cursor)) {
                new_entry.attribute = "virtual";
                info->sinful = true;
            }

            CXCursor* overriden_cursor;
            unsigned int overriden_num = 0;

            clang_getOverriddenCursors(cursor, &overriden_cursor, &overriden_num);

            if (overriden_num > 0) {
                new_entry.attribute = "override";
                info->sinful = true;
                clang_disposeOverriddenCursors(overriden_cursor);
            } else {
                new_entry.attribute = "none";
            }

            info->func_entries.push_back(new_entry);

            break;
        }

        case CXCursor_CXXBaseSpecifier:
        {
            std::string name = reinterpret_cast<const char*>(clang_getCursorSpelling(cursor).data);
            size_t class_reside = name.find("class ");
            size_t struct_reside = name.find("struct ");

            std::string parent1;

            if (class_reside != std::string::npos) {
                parent1 = name.substr(class_reside);
            } else {
                parent1 = name.substr(struct_reside);
            }

            info->parents.push_back(common::hash(name));

            break;
        }

        default:
            break;
    }

    return CXChildVisit_Continue;
}

CXChildVisitResult visit_big_guy(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    using cursor_list = std::vector<CXCursor>;
    cursor_list* list = reinterpret_cast<cursor_list*>(client_data);

    if (list == nullptr) {
        LOG_ERROR("Client data is invalid!");
        return CXChildVisit_Break;
    }

    if (clang_getCursorKind(cursor) == CXCursor_ClassDecl
            || clang_getCursorKind(cursor) == CXCursor_StructDecl) {
        list->emplace_back(cursor);
    }

    return CXChildVisit_Continue;
}

std::optional<class_infos> gather_classinfos(const std::string& path) {
    CXIndex idx = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        idx,
        path.c_str(), nullptr, 0,
        nullptr, 0,
        CXTranslationUnit_None);

    if (!unit) {
        return std::optional<class_infos>{};
    }

    CXCursor cursor = clang_getTranslationUnitCursor(unit);

    std::vector<CXCursor> all_classes;

    clang_visitChildren(cursor, visit_big_guy, &all_classes);

    class_infos infos;

    infos.resize(all_classes.size());

    for (auto i = 0; i < all_classes.size(); i++) {
        class_info* info = &infos[i];

        std::string name = reinterpret_cast<const char*>(clang_getCursorSpelling(all_classes[i]).data);

        info->id = common::hash(name);
        info->info.name = reinterpret_cast<const char*>(clang_Cursor_getMangling(all_classes[i]).data);

        clang_visitChildren(cursor, visit_class, info);
    }

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(idx);

    return infos;
}

std::vector<fs::path> query_headers(const std::string& where) {
    std::vector<fs::path> header_paths;

    for (auto &f : fs::directory_iterator(where)) {
        fs::path fdir = fs::path(f);

        if (fs::is_regular_file(fdir)) {
            auto ext = fdir.extension().string();

            if (ext == ".hpp" || ext == ".h" || ext == ".cpp" || ext == ".inc") {
                header_paths.push_back(fs::path(f));
            }
        }
    }

    return header_paths;
}

void yaml_meta_emit(const std::string& path) {
}

int main(int argc, char** argv) {

}
