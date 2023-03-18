#include <uvlooper/uvlooper.h>

namespace libuv {
    task::task(std::function<void()> callback)
        : callback_(callback)
        , callback_invoked_(false) {
    }

    task::~task() {
        for (auto looper_and_async: assigned_) {
            looper_and_async.first->detach_from_cleanup(this);
        }
    }

    looper::looper(uv_loop_t *loop_name, std::function<void()> loop_thread_prepare_callback)
        : target_loop_(loop_name)
        , stop_async_(nullptr)
        , task_post_async_(nullptr)
        , cleanup_async_(nullptr)
        , cleanup_urgent_regular_async_(nullptr)
        , running_thread_(nullptr)
        , loop_thread_prepare_callback_(loop_thread_prepare_callback)
        , stopped_(false)
        , in_tasks_handling_(false) {
        initialize_critical_asyncs();
    }

    looper::~looper() {
        if (!running_thread_) {
            task_cleanup_final_function();

            while (uv_run(target_loop_, UV_RUN_ONCE) != 0) {}
        } else {
            uv_async_send(cleanup_async_);
            stopped_ = true;

            running_thread_->join();
        }
    }

    void looper::start() {
        if (running_thread_) {
            stop();
        }

        stopped_ = false;
        running_thread_ = std::make_unique<std::thread>([](looper *arg) {
            arg->loop_thread_function();
        }, this);
    }

    void looper::stop() {
        if (!running_thread_) {
            return;
        }

        stopped_ = true;
        uv_async_send(stop_async_);

        running_thread_->join();
    }

    inline void close_and_delete_async(uv_async_t *async) {
        uv_close(reinterpret_cast<uv_handle_t *>(async), [](uv_handle_t *handle) {
            delete handle;
        });
    }

    inline uv_async_t *create_raw_async(uv_loop_t *loop, void *data, uv_async_cb async_cb) {
        uv_async_t *result = new uv_async_t;
        result->data = data;

        uv_async_init(loop, result, async_cb);
        return result;
    }

    void looper::loop_thread_function() {
        if (loop_thread_prepare_callback_ != nullptr) {
            loop_thread_prepare_callback_();
        }

        while ((uv_run(target_loop_, UV_RUN_DEFAULT) == 0) && (!stopped_)) {
            std::this_thread::sleep_for(std::chrono::microseconds(5));
        }
    }

    void looper::initialize_critical_asyncs() {
        stop_async_ = create_raw_async(target_loop_, this, [](uv_async_t *callback) {
            reinterpret_cast<looper *>(callback->data)->stop_function();
        });

        task_post_async_ = create_raw_async(target_loop_, this, [](uv_async_t *callback) {
            reinterpret_cast<looper *>(callback->data)->tasks_handle_function();
        });

        cleanup_async_ = create_raw_async(target_loop_, this, [](uv_async_t *callback) {
            reinterpret_cast<looper *>(callback->data)->task_cleanup_final_function();
        });

        cleanup_urgent_regular_async_ = create_raw_async(target_loop_, this, [](uv_async_t *callback) {
            reinterpret_cast<looper *>(callback->data)->task_cleanup_urgent_regularly_function();
        });
    }

    void looper::stop_function() {
        uv_stop(target_loop_);
    }

    void looper::tasks_handle_function() {
        const std::lock_guard<std::mutex> guard(looper_mutex_);
        in_tasks_handling_ = true;

        while (!tasks_.empty()) {
            std::shared_ptr<task> task_to_run = std::move(tasks_.front());
            tasks_.pop();

            // Make a reference to get the task not to destroyed before the callback is invoked
            uv_async_t *new_task = new uv_async_t;
            new_task->data = new std::shared_ptr<task>(task_to_run);

            uv_async_init(target_loop_, new_task, [](uv_async_t *async) {
                std::shared_ptr<task> *task_to_run = reinterpret_cast<std::shared_ptr<task> *>(async->data);
                std::shared_ptr<task> task_to_run_moved = std::move(*task_to_run);

                task_to_run_moved->callback()();
            });

            task_to_run->assigned_[this] = new_task;
            cleanup_instanced_tasks_.emplace(task_to_run.get());

            uv_async_send(new_task);
        }

        in_tasks_handling_ = false;
    }

    static void destroy_looper_async(uv_async_t *async) {
        std::shared_ptr<libuv::task> *task_ptr = reinterpret_cast<std::shared_ptr<libuv::task> *>(async->data);

        delete task_ptr;
        close_and_delete_async(async);
    }

    void looper::task_cleanup_final_function() {
        // Cleanup handles
        const std::lock_guard<std::mutex> guard(looper_mutex_);
        for (auto task: cleanup_instanced_tasks_) {
            auto ite = task->assigned_.find(this);
            if (ite != task->assigned_.end()) {
                destroy_looper_async(ite->second);
                task->assigned_.erase(ite);
            }
        }

        cleanup_instanced_tasks_.clear();

        close_and_delete_async(stop_async_);
        close_and_delete_async(task_post_async_);
        close_and_delete_async(cleanup_async_);
        close_and_delete_async(cleanup_urgent_regular_async_);

        uv_stop(target_loop_);
    }
    
    void looper::task_cleanup_urgent_regularly_function() {
        // Cleanup handles
        const std::lock_guard<std::mutex> guard(looper_mutex_);
        for (auto task: urgent_cleanup_instanced_tasks_) {
            destroy_looper_async(task);
        }

        urgent_cleanup_instanced_tasks_.clear();
    }

    void looper::detach_from_cleanup(task *target) {
       std::unique_lock<std::mutex> lock;
        
        if (!in_tasks_handling_) {
            lock = std::unique_lock<std::mutex>(looper_mutex_);
        }

        if (cleanup_instanced_tasks_.find(target) != cleanup_instanced_tasks_.end()) {
            auto found_assigned_ite = target->assigned_.find(this);
            if (found_assigned_ite != target->assigned_.end()) {
                urgent_cleanup_instanced_tasks_.emplace(found_assigned_ite->second);
            }

            cleanup_instanced_tasks_.erase(target);

            // Cleanup if exceed a max
            if (urgent_cleanup_instanced_tasks_.size() >= CLEANUP_URGENT_INTERVAL) {
                uv_async_send(cleanup_urgent_regular_async_);
            }
        }
    }

    void looper::post_task(std::shared_ptr<task> task_to_run) {
        const std::lock_guard<std::mutex> guard(looper_mutex_);
        const std::lock_guard<std::mutex> guard2(task_to_run->lock_);

        auto task_with_this_loop = task_to_run->assigned_.find(this);

        if (task_with_this_loop != task_to_run->assigned_.end()) {
            std::shared_ptr<task> *task_ptr = reinterpret_cast<std::shared_ptr<task>*>(task_with_this_loop->second->data);
            *task_ptr = std::move(task_to_run);

            uv_async_send(task_with_this_loop->second);
        } else {
            // Push task to create a libuv instance for it...
            tasks_.push(std::move(task_to_run));
            uv_async_send(task_post_async_);
        }
    }

    extern std::shared_ptr<looper> default_looper = std::make_shared<looper>(uv_default_loop());
}