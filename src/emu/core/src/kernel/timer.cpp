#include <kernel/timer.h>

namespace eka2l1 {
    namespace kernel {
        timer::timer(timing_system *sys, std::string name, reset_type rt)
            : timing(sys)
            , name(name)
            , rt(rt)
            , signaled(false)
            , init_delay(0)
            , interval_delay(0) {
            callback_type = timing->get_register_event("TimerCallback");

            if (callback_type == -1) {
                callback_type = timing->register_event("TimerCallback",
                    timer_callback);
            }
        }

        timer::~timer() {}

        void timer::set(int64_t init, int64_t interval) {
            cancel();

            init_delay = init;
            interval_delay = interval;

            if (init_delay == 0) {
                signal(0);
            } else {
                timing->schedule_event(
                    timing->ns_to_cycles(init),
                    callback_type,
                    this);
            }
        }

        void timer::cancel() {
            timing->unschedule_event(timer_callback, (uint64_t)this);
        }

        void timer::signal(int cycles_late) {
            signaled = true;

            wake_up_waiting_threads();

            if (interval_delay != 0) {
                timing->schedule_event(timing->ns_to_cycles(interval_delay) - cycles_late,
                    callback_type, (uint64_t)this);
            }
        }

        void timer::clear() {
            signaled = false;
        }

        void timer::wake_up_waiting_threads() {
            wait_obj::wake_up_waiting_threads();

            if (rt == reset_type::again) {
                signaled = false;
            }
        }

        bool timer::should_wait(kernel::uid id) {
            return !signaled;
        }

        bool timer::acquire(const kernel::uid thr_id) {
            if (rt == reset_type::oneshot) {
                signaled = false;
            }
        }

        void timer_callback(uint64_t user, int cycles_late) {
            timer *thandle = reinterpret_cast<timer *>(user);

            if (!thandle) {
                return;
            }

            thandle->signal(cycles_late);
        }
    }
}