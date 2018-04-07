#include <common/data_displayer.h>

namespace eka2l1 {
    std::shared_ptr<data_dumper_interface> data_dumper;

    void data_dump_setup(std::shared_ptr<data_dumper_interface> dd) {
        data_dumper = dd;
    }

    void dump_data(const std::string& name, std::vector<uint8_t> data) {
        data_dumper->set_name(name);
        data_dumper->set_data(data);
    }
}
