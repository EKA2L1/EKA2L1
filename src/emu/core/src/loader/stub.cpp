#include <loader/stub.h>
#include <yaml-cpp/yaml.h>
#include <common/algorithm.h>
#include <common/hash.h>

#include <mutex>

#ifdef _MSC_VER
#define __attribute__(x) 
#endif

#include <cxxabi.h>

namespace eka2l1 {
	namespace loader {
		YAML::Node db_node;
		YAML::Node meta_node;

		std::mutex mut;

		std::map<std::string, vtable_stub> vtable_cache;
		std::map<std::string, typeinfo_stub> typeinfo_cache;

		void function_stub::write_stub() {
			uint32_t* wh = reinterpret_cast<uint32_t*>(pos.get());

			wh[0] = 0xef000000; // svc #0
			wh[1] = 0xe1a0f00e;
			wh[2] = id;
		}

		void typeinfo_stub::write_stub() {
			uint32_t* wh = reinterpret_cast<uint32_t*>(pos.get());
			uint8_t* wh8 = reinterpret_cast<uint8_t*>(pos.get());

			wh[0] = helper.ptr_address();
			wh8 += 4;

			// I will provide a fake abi class, and there is nothing to hide, so ...
			memcpy(wh8, info_des.data(), info_des.length());

			++wh8 = '\0';
			wh = reinterpret_cast<uint32_t*>(wh8);

			wh[0] = parent_info.ptr_address();
		}

		void write_non_virtual_thunk(uint32_t* pos, const uint32_t virt_virt_func, uint32_t top_off) {
			pos[0] = 0x4883ef00 | top_off; // sub #top_off, %?
			pos[1] = 0xeb000000; // jmp virt_virt_func
			pos[2] = virt_virt_func;
		}

		void vtable_stub::write_stub() {
			for (auto& st : mini_stubs) {
				st->write_stub();
			}
		}

		bool check_class_exists(module_stub& mstub, const std::string& cname) {
			auto res = std::find_if(mstub.class_stubs.begin(), mstub.class_stubs.end(),
				[cname](auto ms) {
					return cname == ms.des;
				});

			if (res == mstub.class_stubs.end()) {
				return false;
			}

			return true;
		}

		void init_new_class_stub(module_stub& mstub, const std::string& thatclass) {
			class_stub* cstub = mstub.new_class_stub();
			cstub->des = thatclass;
			cstub->is_template = thatclass.find("<") != std::string::npos;
			cstub->id = common::hash(
				common::normalize_for_hash(cstub->des));
		}

		// Given a description, find all referenced class to get in meta.yml
		void find_reference_classes(std::string des, module_stub& mstub) {
			size_t pos1 = des.find("typeinfo for");
			size_t pos2 = des.find("vtable for");

			if (pos1 != std::string::npos) {
				std::string thatclass = des.substr(pos1 + 1);

				if (check_class_exists(mstub, thatclass)) {
					return;
				}

				init_new_class_stub(mstub, thatclass);

				return;
			}

			if (pos2 != std::string::npos) {
				std::string thatclass = des.substr(pos2 + 1);

				if (check_class_exists(mstub, thatclass)) {
					return;
				}

				init_new_class_stub(mstub, thatclass);

				return;
			}

			size_t first_in = des.find_first_of("::");

			if (first_in != std::string::npos) {
				std::string cname = des.substr(0, first_in);

				if (!check_class_exists(mstub, cname)) {
					init_new_class_stub(mstub, cname);
				}
			}

			common::remove(des, "const");
			common::remove(des, "&");
			common::remove(des, "*");

			size_t start_tokenizing = des.find_first_of("(");

			if (start_tokenizing != std::string::npos) {
				std::string temp = "";
				
				for (uint32_t i = start_tokenizing; i < des.length(); i++) {
					if (des[i] != ',' && des[i] != ')') {
						temp += des[i];
					} else {
						if (!check_class_exists(mstub, temp)) {
							init_new_class_stub(mstub, temp);
						}

						temp = "";
					}
				}
			}
		}

        // Extract the library function from the database, then write the stub into memory
        module_stub write_module_stub(std::string name, const std::string &db_path) {
            db_node = YAML::LoadFile(db_path + "/db.yml");
            meta_node = YAML::LoadFile(db_path + "/meta.yml");

			auto all_stubs = db_node[name.c_str()];
			module_stub mstub;

			if (all_stubs.IsMap()) {
				auto all_exports = all_stubs["exports"];

				if (all_exports.IsMap()) {
					for (auto exp : all_exports) {
						std::string des = exp.first.as<std::string>();

						find_reference_classes(des, mstub);

						mstub.pub_func_stubs.push_back(eka2l1::loader::function_stub{});
						
						function_stub* last_stub = &mstub.pub_func_stubs.back();
						last_stub->pos = core_mem::alloc_ime(24);
						last_stub->id = exp.second.as<uint32_t>();
						last_stub->des = des;
					}
				}
			}

			return mstub;
        }
    }
}
