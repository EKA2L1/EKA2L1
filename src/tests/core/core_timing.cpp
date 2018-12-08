#include <epoc/core_timing.h>
#include <gtest/gtest.h>

#include <cstdint>

using namespace eka2l1;

void timed_io_callback(uint64_t time_delay) {
    FILE *f = fopen("test_register.txt", "w");
    const char *fo = "ur mom nice";

    fwrite(fo, 1, 12, f);
    fclose(f);
}

void timed_nop_callback(uint64_t time_delay) {
    // NOP
}

void advance_and_check(eka2l1::timing_system &timing, uint32_t ticks) {
    timing.add_ticks(timing.get_downcount());
    timing.advance();

    ASSERT_EQ(timing.get_downcount(), ticks);
}

struct scope_guard {
    eka2l1::timing_system *timing;

    scope_guard(eka2l1::timing_system &timing_sys)
        : timing(&timing_sys){
        timing->init();
    }

    ~scope_guard() {
        timing->shutdown();
    }
};

TEST(timing_test, basic_scheduling) {
    eka2l1::timing_system timing;
    scope_guard guard(timing);

    auto ioevt = timing.register_event("testIOEvent", std::bind(timed_io_callback, std::placeholders::_1));
    auto nopevt = timing.register_event("testNonEvent", std::bind(timed_nop_callback, std::placeholders::_1));

    // Schedule: make sure those are executed correctly
    // No order, because the core timing of EKA2L1 doesn't use
    // a priority queue
    timing.schedule_event(25000, ioevt);
    timing.schedule_event(300, nopevt);

    advance_and_check(timing, 5000);
    advance_and_check(timing, 20000);
}