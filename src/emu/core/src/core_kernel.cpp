#include <atomic>
#include <thread>
#include <queue>

#include <core_kernel.h>
#include <kernel/thread.h>
#include <ptr.h>

namespace eka2l1 {
    namespace core_kernel {
        std::atomic<kernel::uid> crr_uid;
        std::priority_queue<kernel::thread_ptr> threads;

        class thread_scheduler {
            std::vector<kernel::thread*> waiting_threads;
            std::vector<kernel::thread*> ready_threads;
            std::vector<kernel::thread*> running_threads;

        protected:

            void thread_move(kernel::thread* thr, kernel::thread_state new_state) {
                if (thr->current_state() == new_state) {
                    // Supposed i don't want
                    return;
                }

                auto cmpb_thread = [&](const kernel::thread* rhs) {
                    return *thr == *rhs;
                };

                auto is_in_waiting = std::find_if(waiting_threads.begin(),
                                                  waiting_threads.end(),
                                                  cmpb_thread);

                auto is_in_ready = std::find_if(ready_threads.begin(),
                                                  ready_threads.end(),
                                                  cmpb_thread);

                if (is_in_waiting != waiting_threads.end()) {
                    waiting_threads.erase(is_in_waiting);
                }
            }

            kernel::thread* current_running_threads() const {
                return running_threads.back();
            }

            void insert_to_ready(kernel::thread* thread) {
                if (current_running_threads()) {}
            }

        public:
            void schedule(kernel::thread* thread) {
                waiting_threads.push_back(thread);

                if (waiting_threads.back() != waiting_threads[waiting_threads.size() - 1]) {
                    std::sort_heap(waiting_threads.begin(), waiting_threads.end(),
                                   [](auto lhs, auto rhs) {
                        return *lhs > *rhs;
                    });
                }
            }

            bool run(const kernel::thread* thread, size_t arg_len, ptr<void> args) {
                auto ready_thread_ite = std::find_if(waiting_threads.begin(), waiting_threads.end(), [](auto rhs) {
                        return *thread == *rhs;
                });

                if (ready_thread_ite != ready_threads.end()) {
                    // The thread is in the waity, user must schedule it first
                    return false;
                }

                // Now we don't need a ready check, because the thread is already
                // This isnt run ignore, but use it anyway, since we REALLY want to run
                // de thread
                thread->run_ignore(arg_len, args);

                return true;
            }

            void run_all() {
                for (auto& ready_thread: ready_threads) {
                    ready_thread->run();
                }
            }
        };

        std::mutex mut;

        void init() {
            // Intialize the uid with zero
            crr_uid.store(0);
        }

        kernel::uid next_uid() {
            crr_uid++;
            return crr_uid.load();
        }

        void add_thread(kernel::thread* thr) {
            const std::lock_guard<std::mutex> guard(mut);
            threads.emplace(thr);
        }
    }
}
