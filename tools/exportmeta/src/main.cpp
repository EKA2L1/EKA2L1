#include <common/algorithm.h>
#include <common/log.h>
#include <common/hash.h>
#include <common/cvt.h>
#include <yaml-cpp/yaml.h>

#include <clang-c/Index.h>

#include <algorithm>
#include <experimental/filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
#include <thread>
#include <tuple>

YAML::Emitter emitter;

using sid = uint32_t;

struct entry {
    sid id;
    std::string name;
    std::string type;
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
std::mutex mut;

uint32_t thread_count = 1;

std::string fully_qualified(CXCursor cursor) {
   if (clang_getCursorKind(cursor) == CXCursor_TranslationUnit) {
       return "";
   }

   std::string res = fully_qualified(clang_getCursorSemanticParent(cursor));

   if (res != "") {
       return res + "::" + reinterpret_cast<const char*>(clang_getCursorSpelling(cursor).data);
   }

   return reinterpret_cast<const char*>(clang_getCursorSpelling(cursor).data);
}

size_t find_nth(std::string targ, std::string str, size_t idx, size_t pos = 0) {
    size_t found_pos = targ.find(str, pos);

    if (1 == idx || found_pos == std::string::npos) {
        return found_pos;
    }

    return find_nth(targ, str, idx - 1, found_pos + 1);
}

std::string resolve_name(std::string dec1, CXCursor cursor) {
    std::string name = reinterpret_cast<const char*>(clang_getCursorSpelling(cursor).data);

    size_t weird_class = name.find("class");
    size_t weird_struct = name.find("struct");

    if (weird_class != std::string::npos) {
        name = name.substr(5);
    } else if (weird_struct != std::string::npos) {
        name = name.substr(6);
    }

    auto count = [](std::string inp) -> size_t {
        size_t res = 0;
        size_t pos = 0;

        do {
            pos = inp.find("::", pos);

            if (pos == std::string::npos) {
                break;
            } else {
                pos++;
                res++;
                inp.substr(pos);
            }
        } while (true);

        return res;
    };

    size_t n1 = count(dec1);
    size_t n2 = count(name);

    if (n2 >= n1) {
        return name;
    } else {
        size_t pos = find_nth(dec1, "::", n1 - n2);
        return dec1.substr(0, pos) + "::" + name;
    }

    return "";
}

std::string normalize_for_hash(std::string org) {
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

    std::string sub = org.substr(beg, end);

    remove(sub, "class");
    remove(sub, "const");
    remove(sub, "struct");

    auto res =  org.substr(0, beg) + sub + org.substr(end + 1);

    LOG_INFO("Help: {}", res);

    return res;
}

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
            new_entry.name = info->name + "::" + new_entry.name;

            auto arg = clang_Cursor_getNumArguments(cursor);

            new_entry.name += "(";

            // Forgive my laziness
            for (decltype(arg) i = 0; i < arg; i++) {
                CXCursor carg = clang_Cursor_getArgument(cursor, i);
                new_entry.name += clang_getCString(clang_getTypeSpelling(clang_getCursorType(carg)));

                if (i < (arg - 1)) {
                    new_entry.name += ", ";
                }
            }

            new_entry.name += ")";

            if (clang_CXXMethod_isConst(cursor)) {
                new_entry.name += " const";
            }

            new_entry.id = common::hash(
                       normalize_for_hash(new_entry.name));

            new_entry.attribute = "none";
            new_entry.type = clang_getCString(clang_getTypeSpelling(clang_getResultType(clang_getCursorType(cursor))));

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

            if (clang_CXXMethod_isStatic(cursor)) {
                new_entry.attribute = "static";
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
            auto resolved = resolve_name(info->info.name, cursor);
            LOG_INFO("{}", resolved);
            info->base.push_back(common::hash(resolved));

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
        if (!clang_equalCursors(clang_getCursorDefinition(cursor),
                               clang_getNullCursor())) {
            list->emplace_back(cursor);
        }
    }

    return CXChildVisit_Recurse;
}

std::optional<class_infos> gather_classinfos(const std::string& path) {
    LOG_INFO("Gather info of: {}", path);

    const char* const args[] = { "-xc++", "--include-directory", global_include_path.c_str(), "-DIMPORT_C= "};

    CXIndex idx = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        idx,
        path.c_str(), args, 4,
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

        info->name = reinterpret_cast<const char*>(clang_getCursorSpelling(all_classes[i]).data);

        info->id = common::hash(info->name);
        info->info.name = fully_qualified(all_classes[i]);

        LOG_INFO("Found class/struct: {}, mangled: {}", info->name, info->info.name);

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

    std::lock_guard<std::mutex> guard(mut);

    for (auto& info: real_info) {
        emitter << YAML::Key << info.info.name << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "id" << YAML::Value << std::string("0x") + common::to_string(info.id, std::hex);

                emitter << YAML::Key << "base" << YAML::Value << YAML::BeginSeq;

                    for (auto& minbase: info.base) {
                        emitter << std::string("0x") + common::to_string(minbase, std::hex);
                    }

                emitter << YAML::EndSeq;

                emitter << YAML::Key << "entries" << YAML::Value << YAML::BeginMap;

                    for (auto& entry: info.func_entries) {
                        emitter << YAML::Key << entry.name << YAML::Value << YAML::BeginMap;
                            emitter << YAML::Key << "id" << YAML::Value << std::string("0x") + common::to_string(entry.id, std::hex);
                            emitter << YAML::Key << "type" << YAML::Value << entry.type;
                            emitter << YAML::Key << "attrib" << YAML::Value << entry.attribute;
                        emitter << YAML::EndMap;
                    }

                emitter << YAML::EndMap;

        emitter << YAML::EndMap;
    }
}

void launch_gen_metadata(int start_off, int end_off) {
    for (uint32_t i = start_off; i < end_off; i++) {
        yaml_meta_emit(headers[i].string());
    }
}

std::vector<std::thread> setup_threads(int total) {
    if (total <= 0) {
        return std::vector<std::thread>{};
    }

    uint32_t total_thread = total;
    uint32_t header_for_work, start_off = 0;
    uint32_t end_off = 0;

    std::vector<std::thread> threads;
    threads.resize(total_thread);

    for (uint32_t i = 0; i < total_thread - 1; i++) {
        header_for_work = headers.size() / total_thread;
        end_off += header_for_work;

        threads[i] = std::thread(launch_gen_metadata, start_off, end_off);
        start_off += header_for_work;
    }

    header_for_work = headers.size() / total_thread + headers.size() % total_thread;
    end_off += header_for_work;
    threads[total_thread-1] = std::thread(launch_gen_metadata, start_off, end_off);

    return threads;
}

void parse_args(int argc, char** argv) {
    if (argc == 1) {
        global_include_path = ".";
    } else {
        global_include_path = argv[argc-1];
    }

    for (auto i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-j") == 0) {
            if (i + 1 < argc - 1) {
                thread_count = common::min(10, std::atoi(argv[++i]));
            }
        }
    }
}

void write_meta_yml() {
    std::ofstream meta("meta.yml");
    meta << emitter.c_str();
}

int main(int argc, char** argv) {
    parse_args(argc, argv);

    log::setup_log(nullptr);
    headers = query_headers(global_include_path);

    emitter << YAML::BeginMap;
    emitter << YAML::Key << "classes" << YAML::Value << YAML::BeginMap;

    auto threads = setup_threads(thread_count);

    for (auto& thr: threads) {
        thr.join();
    }

    emitter << YAML::EndMap;
    emitter << YAML::EndMap;

    write_meta_yml();

    return 0;
}
