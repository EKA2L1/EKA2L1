#pragma once

#include <vector>
#include <memory>

namespace eka2l1 {
    struct data_dumper_interface {
        virtual void draw() = 0;
        virtual void set_name(const std::string&) = 0;
        virtual void set_data(std::vector<uint8_t>&) = 0;
    };

    void data_dump_setup(std::shared_ptr<data_dumper_interface> dd);
    void dump_data(const std::string& name, std::vector<uint8_t> data);
}
