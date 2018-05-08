#include <loader/stub.h>
#include <yaml-cpp/yaml.h>
#include <common/algorithm.h>
#include <common/hash.h>
#include <core_mem.h>

//#include <cxxabi.h>
#include <cstring>
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
		std::map<uint32_t, ordinial_stub> ordinial_cache;

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
			int32_t* addr = reinterpret_cast<int32_t*>(pos.get());
			
			for (uint32_t i = 0; i < mini_stubs.size(); i++) {
				auto as_ordinial = dynamic_cast<ordinial_stub*>(mini_stubs[i]);
				
				if (as_ordinial != nullptr) {
					addr[i] = as_ordinial->ord;
					continue;
				}

				addr[i] = static_cast<int32_t>(mini_stubs[i]->pos.ptr_address());
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

		void init_vtable(vtable_stub& vstub) {
			// I don't think a class have more than 100 methods
			vstub.pos = core_mem::alloc_ime(600);
		} 

		void init_new_class_stub(module_stub& mstub, const std::string& thatclass) {
			class_stub* cstub = mstub.new_class_stub();
			cstub->des = thatclass;
			cstub->is_template = thatclass.find("<") != std::string::npos;
			cstub->id = common::hash(
				common::normalize_for_hash(cstub->des));

			cstub->module = &mstub;

			init_vtable(cstub->vtab_stub);
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

		bool vtable_cache_exists(const class_stub& stub) {
			auto res = vtable_cache.find(stub.des);

			if (res == vtable_cache.end()) {
				return false;
			}

			return true;
		}

		class_stub* find_class_stub(module_stub& mstub, const std::string& str) {
			auto res = std::find_if(mstub.class_stubs.begin(), mstub.class_stubs.end(), 
				[str](auto cstub) {
					return cstub.des == str;
				});

			if (res == mstub.class_stubs.end()) {
				return nullptr;
			}

			return &(*res);
		}

		auto find_function_stub(std::vector<function_stub>& fstubs, uint32_t id) {
			auto res = std::find_if(fstubs.begin(), fstubs.end(), 
				[id](auto fstub) {
					return fstub.id == id;
				});

			return res;
		}

		// Given a SID, find the function stub. 
		function_stub* find_function_stub(module_stub& mstub, uint32_t id) {
			auto res = find_function_stub(mstub.pub_func_stubs, id);

			if (res == mstub.pub_func_stubs.end()) {
				return nullptr;
			}

			return &(*res);
		}

		ordinial_stub* get_ordinial_stub(uint32_t ord) {
			auto res = ordinial_cache.find(ord);

			if (res == ordinial_cache.end()) {
				ordinial_stub new_stub;
				new_stub.ord = ord;

				ordinial_cache.insert(std::make_pair(ord, new_stub));
				return &ordinial_cache[ord];
			}

			return &res->second;
		}

		// Implying that the function name follows: a::b::c(a,a,a)
		std::string get_class_function_name(const std::string des) {
			size_t last_access = des.find_last_of("::");
			size_t first_bracket = des.find_first_of("(");

			return des.substr(last_access + 2, first_bracket - last_access - 2);
		}

		function_stub* find_same_name(function_stub* src, class_stub::class_base& base) {
			if (src == nullptr) {
				return nullptr;
			}

			for (auto fn: base.stub->fn_stubs) {
				const std::string fn_name = get_class_function_name(fn.des);
				const std::string src_fn_name = get_class_function_name(src->des);
				
				if (fn_name == src_fn_name) {
					return &fn;
				}
			}

			return nullptr;
		}

		// Easy situation: only one class is based on another
		void resolve_base_only_one(class_stub& stub) {
			class_stub::class_base& base = stub.bases[0];

			ordinial_stub* zero_top_offset = get_ordinial_stub(0);
			stub.vtab_stub.mini_stubs.push_back(zero_top_offset);

			auto fn_stubs_base_copy = base.stub->fn_stubs;

			for (uint32_t i = 0; i < stub.fn_stubs.size(); i++) {
				if (stub.fn_stubs[i].attrib == func_attrib::none) {
					continue;
				}

				function_stub* base_stub = find_same_name(&stub.fn_stubs[i], base);

				if (base_stub == nullptr) {
					stub.vtab_stub.mini_stubs.push_back(&stub.fn_stubs[i]);
				} else {
					fn_stubs_base_copy.erase(find_function_stub(stub.fn_stubs, base_stub->id));

					// If two functions have the same attribute, choose the lastest
					if (base_stub->attrib == stub.fn_stubs[i].attrib) {
						stub.vtab_stub.mini_stubs.push_back(&stub.fn_stubs[i]);
					} else {
						if (stub.fn_stubs[i].attrib == func_attrib::ovride) {
							stub.vtab_stub.mini_stubs.push_back(&stub.fn_stubs[i]);
						} 
					}
				}
			}

			// Fill the rest of the vtable
			for (auto base_left: fn_stubs_base_copy) {
				stub.vtab_stub.mini_stubs.push_back(&base_left);
			}
		}

		// Fill a vtable
		void fill_vtable(class_stub& stub) {
			if (vtable_cache_exists(stub)) {
				return;
			}

			auto meta_inf = meta_node[stub.des.c_str()];

			if (meta_inf.IsMap()) {
				auto all_meta_bases = meta_inf["bases"];

				if (all_meta_bases.IsMap()) {
					for (auto meta_base: all_meta_bases) {	
						if (meta_base.IsMap()) {
							bool is_virt_relationship = 
								meta_base.second["attribute"].as<std::string>() == "virtual";

							class_stub::class_base cbase_inf;
							std::string base_class_name = meta_base.first.as<std::string>();
							
							class_stub* class_stub_find_res = find_class_stub(*stub.module, base_class_name);

							if (class_stub_find_res == nullptr) {
								// Throw an error here
							}

							cbase_inf.stub = class_stub_find_res;
							cbase_inf.virt_base = is_virt_relationship;

							stub.bases.push_back(cbase_inf);
						}
					}
				}
			}

			auto all_entries = meta_inf["entries"];

			if (stub.bases.size() > 0) {
				// We need to fill the vtable of those before progressing
				for (auto base_stub: stub.bases) {
					fill_vtable(*base_stub.stub);
				}
			} else {
				// Act freely, 'cause there is no parenty thing here
				ordinial_stub* zero_top_offset = get_ordinial_stub(0);
				vtable_stub* vtab = &stub.vtab_stub;

				vtab->add_stub(*zero_top_offset);

				for (auto ent: all_entries) {
					function_stub* fstub = find_function_stub(*stub.module, ent.second["sid"].as<uint32_t>());
					
					if (fstub == nullptr) {
						// Log critical here too
					}

					const std::string attrib_raw = ent.second["attrib"].as<std::string>();
					
					if (attrib_raw == "none") {
						fstub->attrib = func_attrib::none;
						continue;
					} else if (attrib_raw == "override") {
						fstub->attrib = func_attrib::ovride;
					} else if (attrib_raw == "pure_virtual") {
						fstub->attrib = func_attrib::pure_virt;
					} else {
						fstub->attrib = func_attrib::virt;
					}

					vtab->add_stub(*fstub);
				}

				return;
			}

			if (stub.bases.size() == 1) {
				// If the class is only based on one class, 
				// do things as normal. In this case, we can
				// ignore the virtual base
			}
		}

        // Extract the library function from the database, then write the stub into memory
        module_stub write_module_stub(std::string name, const std::string &db_path) {
            db_node = YAML::LoadFile(db_path + "/db.yml")["modules"];
            meta_node = YAML::LoadFile(db_path + "/meta.yml")["classes"];

			auto all_stubs = db_node[name.c_str()];
			module_stub mstub;

			if (all_stubs.IsMap()) {
				auto all_exports = all_stubs["exports"];

				if (all_exports.IsMap()) {
					for (auto exp : all_exports) {
						std::string des = exp.first.as<std::string>();

						find_reference_classes(des, mstub);

						function_stub* last_stub = mstub.new_function_stub();
						last_stub->pos = core_mem::alloc_ime(24);
						last_stub->id = exp.second.as<uint32_t>();
						last_stub->des = des;
					}
				}
			}

			for (auto cstub: mstub.class_stubs) {
				if (vtable_cache_exists(cstub)) {
					continue;
				} 

				auto cstub_meta_node = meta_node["cache"];
			}

			return mstub;
        }
    }
}
