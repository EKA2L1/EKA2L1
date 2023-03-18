#pragma once

extern "C" {
    #include <uv.h>
}

#include <atomic>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>

#include <unordered_map>
#include <unordered_set>

namespace libuv {
    struct looper;

    /**
     * @brief A task to be ran on a loop.
     * 
     * This task callback can only be assigned once, but it is reusable. The task can be scheduled and reuseable
     * again after the callback has been called (even when in the callback).
     * 
     * To instantiate a task, it's best to use the create_task method, which will return a shared pointer.
     */
    struct task {
    private:
        friend struct looper;

        std::function<void()> callback_;
        std::unordered_map<looper*, uv_async_t*> assigned_;
        bool callback_invoked_;

        std::mutex lock_;

    public:
        explicit task(std::function<void()> callback);
        ~task();

        std::function<void()> callback() {
            return callback_;
        }
    };

    inline std::shared_ptr<task> create_task(std::function<void()> callback) {
        return std::make_shared<task>(callback);
    }

    /**
     * @brief A looper object running a libuv's loop.
     * 
     * A new thread will be created exclusively for running this. Client using this class must ensure the loop
     * used for this looper is not running elsewhere.
     */
    struct looper {
    private:
        static constexpr std::size_t CLEANUP_URGENT_INTERVAL = 10;

        friend struct task;

        uv_loop_t *target_loop_;
        uv_async_t *stop_async_;
        uv_async_t *task_post_async_;
        uv_async_t *cleanup_async_;
        uv_async_t *cleanup_urgent_regular_async_;

        std::unique_ptr<std::thread> running_thread_;
        std::function<void()> loop_thread_prepare_callback_;
        std::queue<std::shared_ptr<task>> tasks_;
        std::unordered_set<task *> cleanup_instanced_tasks_;
        std::unordered_set<uv_async_t *> urgent_cleanup_instanced_tasks_;
        std::mutex looper_mutex_;

        bool stopped_;
        bool in_tasks_handling_;

    private:
        void initialize_critical_asyncs();
        void create_neccessary_handles();
        void detach_from_cleanup(task *target);

    public:
        explicit looper(uv_loop_t *loop_name, std::function<void()> loop_thread_prepare_callback = nullptr);
        ~looper();

        void start();
        void stop();
        void post_task(std::shared_ptr<task> task_to_run);

        void one_shot(std::function<void()> task) {
            post_task(create_task(task));
        }

        void tasks_handle_function();
        void task_cleanup_final_function();
        void task_cleanup_urgent_regularly_function();
        void loop_thread_function();
        void stop_function();

        uv_loop_t *raw_loop() const {
            return target_loop_;
        }

        bool started() const {
            return (running_thread_ != nullptr);
        }

        void set_loop_thread_prepare_callback(std::function<void()> callback) {
            loop_thread_prepare_callback_ = callback;
        }
    };

    extern std::shared_ptr<looper> default_looper;
}