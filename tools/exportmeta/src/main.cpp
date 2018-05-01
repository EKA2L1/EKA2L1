#include <common/log.h>
#include <common/hash.h>
#include <yaml-cpp/yaml.h>

#include <clang-c/Index.h>

#include <experimental/filesystem>
#include <vector>
#include <string>
#include <thread>
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
    std::string name;

    typeinfo info;
    entries func_entries;

    std::vector<sid> base;

    bool sinful = false;
};

using class_infos = std::vector<class_info>;

using namespace eka2l1;
namespace fs = std::experimental::filesystem;

std::string global_include_path;
std::vector<fs::path> headers;

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

            new_entry.attribute = "none";

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
            }

            LOG_TRACE("Find method: {}, Attribute: {}", new_entry.name, new_entry.attribute);

            info->func_entries.push_back(new_entry);

            break;
        }

        case CXCursor_ClassDecl: case CXCursor_StructDecl: {
            return CXChildVisit_Continue;
        }

        case CXCursor_CXXBaseSpecifier:
        {
            std::string name = reinterpret_cast<const char*>(clang_getCursorSpelling(cursor).data);
            size_t class_reside = name.find("class ");
            std::string parent1;

            if (class_reside != std::string::npos) {
                parent1 = name.substr(7);
            } else {
                parent1 = name.substr(8);
            }

            info->base.push_back(common::hash(name));

            break;
        }

        default:
            break;
    }

    return CXChildVisit_Recurse;
}

CXChildVisitResult visit_big_guy(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    using cursor_list = std::vector<CXCursor>;
    cursor_list* list = reinterpret_cast<cursor_list*>(client_data);

    if (list == nullptr) {
        LOG_ERROR("Client data is invalid!");
        return CXChildVisit_Break;
    }

    if (!clang_Location_isFromMainFile(clang_getCursorLocation (cursor))) {
        return CXChildVisit_Continue;
    }

    if (clang_getCursorKind(cursor) == CXCursor_ClassDecl
            || clang_getCursorKind(cursor) == CXCursor_StructDecl) {
        list->emplace_back(cursor);
    }

    return CXChildVisit_Recurse;
}

std::optional<class_infos> gather_classinfos(const std::string& path) {
    LOG_INFO("Gather info of: {}", path);

    const char* const args[] = { "-x", "c++", "--include-directory", global_include_path.c_str() };

    CXIndex idx = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        idx,
        path.c_str(), args, 2,
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

        LOG_INFO("Found class/struct: {}", name);

        info->id = common::hash(name);
        info->name = name;
        info->info.name = reinterpret_cast<const char*>(clang_Cursor_getMangling(all_classes[i]).data);

        clang_visitChildren(all_classes[i], visit_class, info);
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
    auto inf = gather_classinfos(path);

    if (!inf) {
        return;
    }

    auto real_info = inf.value();
}

void launch_gen_metadata(int start_off, int end_off) {
    for (uint32_t i = start_off; i < end_off; i++) {
        yaml_meta_emit(headers[i].string());
    }
}

int main(int argc, char** argv) {
    if (argc == 1) {
        global_include_path = ".";
    } else {
        global_include_path = argv[argc-1];
    }

    log::setup_log(nullptr);
    headers = query_headers(global_include_path);

    uint32_t total_thread = 4;
    uint32_t header_for_work, start_off = 0;
    uint32_t end_off = 0;

    for (uint32_t i = 0; i < total_thread - 1; i++) {
        header_for_work = headers.size() / total_thread;
        end_off += header_for_work;

        std::thread t1(launch_gen_metadata, start_off, end_off);
        t1.join();

        start_off += header_for_work;
    }

    header_for_work = headers.size() / total_thread + headers.size() % total_thread;
    end_off += header_for_work;

    std::thread t2(launch_gen_metadata, start_off, end_off);
    t2.join();

    for (auto& header: headers) {
        yaml_meta_emit(header.string());
    }

    return 0;
}
