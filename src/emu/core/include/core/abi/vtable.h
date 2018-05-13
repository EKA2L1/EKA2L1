#pragma once

#include <algorithm>
#include <cstring>
#include <vector>

#include <ptr.h>

namespace eka2l1 {
	using sid = uint32_t;

	namespace abi {
		struct stub {
			ptr<void> pos;
			sid id;

			stub(ptr<void> pos, const sid id)
				: pos(pos), id(id) {}
		};

		struct deexport {
			stub* st;
			std::string des;
			std::string name;

			enum attrib {
				pure_virt = 0,
				deleted_virt = 0,
				virt = 1,
				none = 2,
				ride = 3
			} at;

			bool is_destructor;
			vtable* parent;

			deexport(stub& tst, const std::string des, const std::string name, attrib at,
				bool is_des)
				: st(&tst), des(des), name(name), at(at), is_destructor(is_des) {}

			sid get_sid() const {
				return st->id;
			}
		};

		struct vtable_indices {
			std::vector<deexport> indices;
			vtable* parent;

			void add_stub(deexport& st) {
				indices.push_back(st);
			}

			void remove_stub(sid id) {
				std::remove_if(indices.begin(),
					indices.end(), [id](auto index) {
					return index.st->id == id;
				});
			}
		};

		struct vtable {
			std::vector<vtable*> vtabs;
			std::vector<vtable_indices> indices;

			vtable_indices this_indices;

			void init();

		public:
			void resolve_once();
			void resolve_multiple();
		};
	}
}