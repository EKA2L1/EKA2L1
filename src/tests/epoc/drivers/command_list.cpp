// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#include <catch2/catch.hpp>
#include <drivers/itc.h>

using namespace eka2l1::drivers;

TEST_CASE("Graphics command batches grow and merge without changing the flush budget", "[drivers][command-list]") {
    graphics_command_builder source, destination;
    for (unsigned i = 0; i < 257; ++i) {
        auto *cmd = source.create_next_command();
        REQUIRE(cmd->data_[0] == 0);
        cmd->data_[0] = i;
    }
    auto first = source.retrieve_command_list();
    auto *original = first.base_;
    REQUIRE(destination.merge(first));
    REQUIRE(first.empty());
    auto adopted = destination.retrieve_command_list();
    REQUIRE(adopted.base_ == original);
    destination.create_next_command()->data_[0] = 999;
    REQUIRE(destination.merge(adopted));
    REQUIRE(adopted.empty());
    auto batch = destination.retrieve_command_list();
    REQUIRE(batch.size_ == 258);
    REQUIRE(batch.base_[0].data_[0] == 999);
    for (unsigned i = 0; i < 257; ++i) {
        REQUIRE(batch.base_[i + 1].data_[0] == i);
    }
    delete[] batch.base_;

    for (std::size_t i = 0; i < MAX_CAP_COMMAND_COUNT; ++i) {
        source.create_next_command();
    }
    auto full = source.retrieve_command_list();
    destination.create_next_command();
    REQUIRE_FALSE(destination.merge(full));
    REQUIRE(full.size_ == MAX_CAP_COMMAND_COUNT);
    destination.reset_list();
    REQUIRE(destination.merge(full));
    REQUIRE(destination.need_flush());
    destination.create_next_command()->data_[0] = 1234;
    auto overflow = destination.retrieve_command_list();
    REQUIRE(overflow.size_ == MAX_CAP_COMMAND_COUNT + 1);
    REQUIRE(overflow.base_[MAX_CAP_COMMAND_COUNT].data_[0] == 1234);
    delete[] overflow.base_;
}
