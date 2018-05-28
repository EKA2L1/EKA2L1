#pragma once

#include <functional>
#include <string>
#include <vector>

#include <loader/sis_fields.h>

#define CHUNK_SIZE 0x2000
#define CHUNK_MAX_INFLATED_SIZE 0x20000

namespace eka2l1 {
	class io_system;

	namespace manager {
		class package_manager;
	}

    namespace loader {
        // An interpreter that runs SIS install script
        class ss_interpreter {
            sis_install_block install_block;
            sis_data install_data;
            sis_drive install_drive;

            std::shared_ptr<std::istream> data_stream;
            std::function<bool(std::vector<uint8_t>)> show_text_func;

            io_system* io;
			manager::package_manager* mngr;

			bool appprop(sis_uid uid, sis_property prop);
			bool package(sis_uid uid);

        public:
            std::vector<uint8_t> get_small_file_buf(uint32_t data_idx, uint16_t crr_blck_idx);
            void extract_file(const std::string &path, const uint32_t idx, uint16_t crr_blck_idx);

            explicit ss_interpreter();
            ss_interpreter(std::shared_ptr<std::istream> stream,
                io_system* io,
				manager::package_manager* pkgmngr,
                sis_install_block inst_blck,
                sis_data inst_data,
                sis_drive install_drv);

            bool interpret(sis_install_block install_block, uint16_t crr_blck_idx = 0);
            bool interpret() { return interpret(install_block); }
        };
    }
}
