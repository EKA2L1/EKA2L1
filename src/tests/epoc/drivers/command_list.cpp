// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#include <catch2/catch.hpp>
#include <drivers/itc.h>

using namespace eka2l1::drivers;

static command_list make_full_batch(graphics_command_builder &builder) {
    for (std::size_t i = 0; i < MAX_CAP_COMMAND_COUNT; ++i) {
        builder.create_next_command();
    }
    return builder.retrieve_command_list();
}

TEST_CASE("Graphics command batches grow on demand and keep the merge budget", "[drivers][command-list]") {
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

    source.create_next_command();
    auto hinted = source.retrieve_command_list();
    REQUIRE(hinted.allocated_ == 257);
    delete[] hinted.base_;

    auto full = make_full_batch(source);
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

    auto second_full = make_full_batch(source);
    destination.create_next_command();
    REQUIRE_FALSE(destination.merge(second_full));
    delete[] second_full.base_;
}
