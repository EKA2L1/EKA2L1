#include <string>

// Symbian separators is \\
// This is mostly same as Windows

namespace eka2l1 {
     bool is_absolute(std::string str, std::string current_dir, bool symbian_use= false);
     std::string absolute_path(std::string str, std::string current_dir, bool symbian_use= false);

     bool is_relative(std::string str, bool symbian_use = false);
     std::string relative_path(std::string str, bool symbian_use = false);

     std::string add_path(const std::string& path1, const std::string& path2, bool symbian_use = false);

     bool has_root_name(std::string path, bool symbian_use= false);
     std::string root_name(std::string path, bool symbian_use= false);

     bool has_root_dir(std::string path, bool symbian_use= false);
     std::string root_dir(std::string path, bool symbian_use= false);

     bool has_root_path(std::string path, bool symbian_use= false);
     std::string root_path(std::string path, bool symbian_use= false);
}
