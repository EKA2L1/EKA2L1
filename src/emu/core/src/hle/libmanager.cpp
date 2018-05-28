#include <hle/libmanager.h>
#include <common/log.h>
#include <vfs.h>

namespace eka2l1 {
    namespace hle {
        void lib_manager::load_all_sids(const epocver ver) {
			std::vector<sid> tids;
			std::string lib_name;

			#define LIB(x) lib_name = #x;
			#define EXPORT(x, y) tids.push_back(y); func_names.insert(std::make_pair(y, x)); 
			#define ENDLIB() ids.insert(std::make_pair(lib_name, tids)); tids.clear(); 

			if (ver == epocver::epoc6) {
				#include <hle/epoc6_n.h>
			} else {
				#include <hle/epoc9_n.h>
			}

			#undef LIB
			#undef EXPORT
			#undef ENLIB
        }

        std::optional<sids> lib_manager::get_sids(const std::string& lib_name) {
            auto res = ids.find(lib_name);

            if (res == ids.end()) {
                return std::optional<sids>{};
            }

            return res->second;
        }

        std::optional<exportaddrs> lib_manager::get_export_addrs(const std::u16string& lib_name) {
            auto res = exports.find(lib_name);

            if (res == exports.end()) {
                return std::optional<exportaddrs>{};
            }

            return res->second;
        }

        lib_manager::lib_manager(const epocver ver) {
            load_all_sids(ver);
        }

        bool lib_manager::register_exports(const std::u16string& lib_name, exportaddrs& addrs) {           
            auto libidsop = get_sids(lib_name);

            if (!libidsop) {
                return false;
            }

            exports.insert(std::make_pair(lib_name, addrs));

            sids libids = libidsop.value();

            for (uint32_t i = 0; i < addrs.size(); i++) {
                addr_map.insert(std::make_pair(addrs[i], libids[i]));
            }

            return true;
        }

        std::optional<sid> lib_manager::get_sid(exportaddr addr) {
            auto res = addr_map.find(addr);

            if (res == addr_map.end()) {
                return std::optional<sid>{};
            }

            return res->second;
        }

		// Images are searched in
		// C:\\sys\bin, E:\\sys\\bin and Z:\\sys\\bin
		loader::e32img_ptr lib_manager::load_e32img(const std::u16string& img_name) {
			symfile img = io->open_file(img_name, READ_MODE);
			bool xip = false;
			std::u16string path;

			// I'm so sorry
			if (!img) {
				img = io->open_file( u"C:\\sys\\bin\\" + img_name, READ_MODE);
				
				if (!img) {
					img = io->open_file(u"E:\\sys\\bin\\" + img_name, READ_MODE);

					if (!img) {
						img = io->open_file(u"Z:\\sys\\bin\\" + img_name, READ_MODE);

						if (!img) {
							return loader::e32img_ptr(nullptr);
						} else {
							xip = true;
						}
					}
				}
			}

			loader::e32img_ptr pimg = std::make_shared<loader::eka2img>(loader::parse_eka2img(img));
			
			if (e32imgs_cache.find(pimg->header.check) != e32imgs_cache.end()) {
				img->close();
				return e32imgs_cache[pimg->header.check].img;
			}

			e32img_inf info;

			info.img = pimg;
			info.is_xip = xip;

			uint32_t check = info.img->header.check;

			e32imgs_cache.insert(std::make_pair(info.img->header.check, std::move(info)));
			img->close();

			return e32imgs_cache[check].img;
		}

		loader::romimg_ptr lib_manager::load_romimg(const std::u16string& rom_name) {
			symfile romimgf = io->open_file(u"Z:\\sys\\bin\\" + rom_name, READ_MODE);

			if (!romimgf) {
				romimgf = io->open_file(rom_name, READ_MODE);

				if (!romimgf) {
					return loader::romimg_ptr(nullptr);
				}
			}
		}
    }
}