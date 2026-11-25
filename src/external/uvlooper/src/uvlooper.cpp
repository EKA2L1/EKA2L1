#include <uvlooper/uvlooper.h>

namespace libuv {
    task::task(std::function<void()> callback)
        : callback_(std::move(callback)) {
    }

    looper::looper(uv_loop_t *loop_name, std::function<void()> loop_thread_prepare_callback)
        : target_loop_(loop_name)
        , stop_async_(nullptr)
        , task_post_async_(nullptr)
        , cleanup_async_(nullptr)
        , running_thread_(nullptr)
        , loop_thread_prepare_callback_(loop_thread_prepare_callback)
        , stopped_(false) {
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
            delete reinterpret_cast<uv_async_t *>(handle);
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
    }

    void looper::stop_function() {
        uv_stop(target_loop_);
    }

    void looper::tasks_handle_function() {
        std::unique_lock<std::mutex> lock(looper_mutex_);
        // A callback may repost itself. Leave new posts for the next loop iteration.
        for (auto remaining = tasks_.size(); remaining > 0; --remaining) {
            auto current = std::move(tasks_.front());
            tasks_.pop();
            pending_.erase(current.get());
            lock.unlock();
            current->callback()();
            current.reset();
            lock.lock();
        }
    }

    void looper::task_cleanup_final_function() {
        close_and_delete_async(stop_async_);
        close_and_delete_async(task_post_async_);
        close_and_delete_async(cleanup_async_);

        uv_stop(target_loop_);
    }
    
    void looper::post_task(std::shared_ptr<task> task_to_run) {
        const std::lock_guard<std::mutex> guard(looper_mutex_);
        if (pending_.insert(task_to_run.get()).second) {
            tasks_.push(std::move(task_to_run));
            uv_async_send(task_post_async_);
        }
    }

    extern std::shared_ptr<looper> default_looper = std::make_shared<looper>(uv_default_loop());
}