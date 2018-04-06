#include <core/vfs.h>
#include <common/path.h>

#include <iostream>

#include <map>
#include <mutex>
#include <thread>

namespace eka2l1 {
	namespace vfs {
		std::string _pref_path;
		std::string _current_dir;

		std::map<std::string, std::string> _mount_map;

        std::mutex _gl_mut;

		std::string current_dir() {
			return _current_dir;
		}

		void current_dir(const std::string &new_dir) {
			std::lock_guard<std::mutex> guard(_gl_mut);
			_current_dir = new_dir;
		}

		void mount(const std::string &dvc, const std::string &real_path) {
			auto find_res = _mount_map.find(dvc);

            if (find_res == _mount_map.end()) {
                // Warn
			}

			std::lock_guard<std::mutex> guard(_gl_mut);
            _mount_map.insert(std::make_pair(dvc, real_path));

            _current_dir = dvc;
        }

		void unmount(const std::string &dvc) {
			std::lock_guard<std::mutex> guard(_gl_mut);
			_mount_map.erase(dvc);
		}

        std::string get(std::string vir_path) {
            std::string abs_path = "";

            std::string current_dir =
                    _current_dir;

            // Current directory is always an absolute path
            std::string partition = current_dir.substr(0, 2);

            auto res = _mount_map.find(partition);

            if (res == _mount_map.end()) {
                //log_error("Could not find partition");
                return "";
            }

            current_dir = res->second + current_dir.substr(2);

            if (!is_absolute(vir_path, current_dir)) {
                abs_path = absolute_path(vir_path, current_dir);
            } else {
                abs_path = add_path(res->second, vir_path.substr(1));
            }

            return abs_path;
		}
	}
}
