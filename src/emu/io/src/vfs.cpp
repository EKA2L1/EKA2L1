#include <io/vfs.h>

#include <map>
#include <mutex>

#include <filesystem>

namespace eka2l1 {
	namespace vfs {
		std::string _pref_path;
		std::string _current_dir;

		std::map<std::string, std::string> _mount_map;

		std::mutex _gl_mut;

		std::string pref_path() {
			return _pref_path;
		}

		void pref_path(const std::string &new_pref_path) {
			std::lock_guard<std::mutex> guard(_gl_mut);
			_pref_path = new_pref_path;
		}

		std::string current_dir() {
			return _current_dir;
		}

		void current_dir(const std::string &new_dir) {
			std::lock_guard<std::mutex> guard(_gl_mut);
			_current_dir = new_dir;
		}

		void mount(const std::string &dvc, const std::string &real_path) {
			auto find_res = _mount_map.find(dvc);

			if (find_res != _mount_map.end()) {

			}

			std::lock_guard<std::mutex> guard(_gl_mut);
			_mount_map.insert(dvc, real_path);
		}

		void unmount(const std::string &dvc) {
			std::lock_guard<std::mutex> guard(_gl_mut);
			_mount_map.erase(dvc);
		}

		// TODO: Replace this with boost until its functional
		// Please compile with GCC on MacOS (XCode Clang wont work)
		namespace fs = std::experimental::filesystem;

		std::string get(std::string &vir_path) {
			// Firstly, absolute the path. The current dir is proven to be always absolute
			fs::path _path = fs::absolute(vir_path, vfs::get(_current_dir)).string();

			// If path has root name like C:/ or Z:/, replace them with pref path
			if (_path.has_root_name()) {
				std::string root_name = _path.root_name().string();
				std::string new_vir_path = _path.string().erase(0, root_name.size());

				new_vir_path = _pref_path + new_vir_path;

				return new_vir_path;
			}

			// So finally, from a path like ../Private with the current path is C:/Sys for example,
			// and preference path is E:/SymbianEmu, we first get: C:/Private, then with some
			// processing, we got: E:/SymbainEmu/Private

			return _path.string();
		}
	}
}
