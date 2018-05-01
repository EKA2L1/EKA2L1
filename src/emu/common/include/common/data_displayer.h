#pragma once

#include <memory>
#include <vector>

namespace eka2l1 {
    struct data_dumper_interface {
        virtual void draw() = 0;
        virtual void set_name(const std::string &) = 0;
        virtual void set_data(std::vector<uint8_t> &) = 0;
        virtual void set_data_map(uint8_t *ptr, size_t size) = 0;
        virtual void set_start_off(size_t stoff) = 0;
    };

    void data_dump_setup(std::shared_ptr<data_dumper_interface> dd);
    void dump_data(const std::string &name, std::vector<uint8_t> &data, size_t start_off = 0);
    void dump_data_map(const std::string &name, uint8_t *start, size_t size, size_t start_off = 0);
}
