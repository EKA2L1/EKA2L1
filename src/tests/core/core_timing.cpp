#include <core/core_timing.h>
#include <gtest/gtest.h>

#include <cstdint>

using namespace eka2l1;

void timed_io_callback(uint64_t time_delay) {
    FILE* f = fopen("test_register.txt", "w");
    const char* fo = "ur mom nice";

    fwrite(fo, 1, 12, f);
    fclose(f);
}

void timed_nop_callback(uint64_t time_delay) {
    // NOP
}

void advance_and_check(uint32_t ticks) {
    core_timing::add_ticks(core_timing::get_downcount());
    core_timing::advance();

    ASSERT_EQ(core_timing::get_downcount(), ticks);
}

struct scope_guard {
    scope_guard() {
        core_timing::init();
    }

    ~scope_guard() {
        core_timing::shutdown();
    }
};

TEST(coreTimingTest, basicScheduling) {
    scope_guard guard;

    auto ioevt = core_timing::register_event("testIOEvent", std::bind(timed_io_callback, std::placeholders::_1));
    auto nopevt = core_timing::register_event("testNonEvent", std::bind(timed_nop_callback, std::placeholders::_1));

    core_timing::advance();

    // Schedule: make sure those are executed correctly
    // No order, because the core timing of EKA2L1 doesn't use
    // a priority queue
    core_timing::schedule_event(400, ioevt);
    core_timing::schedule_event(300, nopevt);

    advance_and_check(400);
    advance_and_check(300);
}
