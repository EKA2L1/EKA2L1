#pragma once

#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace eka2l1 {
    /*! \brief A modified queue from std::priority_queue.
	 *
	 * This queue can be modified, resort and remove manually.
	*/
    template <typename T, class compare = std::less<typename std::vector<T>::value_type>>
    class cp_queue : public std::priority_queue<T, std::vector<T>, compare> {
    public:
        using iterator = typename std::vector<T>::iterator;
        using const_iterator = typename std::vector<T>::const_iterator;

        /*! \brief Remove a value from the queue.
		 *
		 * \param val The value to remove.
		 * \returns true if remove successes.
		*/
        bool remove(const T &val) {
            auto it = std::find(this->c.begin(), this->c.end(), val);
            if (it != this->c.end()) {
                this->c.erase(it);
                std::make_heap(this->c.begin(), this->c.end(), this->comp);
                return true;
            } else {
                return false;
            }
        }

        iterator begin() {
            return this->c.begin();
        }

        iterator end() {
            return this->c.end();
        }

        bool empty() {
            return this->c.empty();
        }

        /*! \brief Resort the queue.
		*/
        void resort() {
            std::make_heap(this->c.begin(), this->c.end(), this->comp);
        }
    };

    template <typename T, typename Container = std::deque<T>>
    class cn_queue : public std::queue<T, Container> {
    public:
        typedef typename Container::iterator iterator;
        typedef typename Container::const_iterator const_iterator;

        iterator begin() {
            return this->c.begin();
        }

        iterator end() {
            return this->c.end();
        }

        const_iterator begin() const {
            return this->c.begin();
        }

        const_iterator end() const {
            return this->c.end();
        }
    };

    template <typename T, typename Container = std::deque<T>>
    class threadsafe_cn_queue {
        cn_queue<T, Container> queue;

    public:
        std::mutex lock;

        void push_unsafe(const T &val) {
            queue.push(val);
        }

        void push(const T &val) {
            const std::lock_guard<std::mutex> guard(lock);
            queue.push(val);
        }

        std::optional<T> pop() {
            const std::lock_guard<std::mutex> guard(lock);

            if (queue.empty()) {
                return std::nullopt;
            }

            T val = std::move(queue.front());
            queue.pop();

            return val;
        }

        typedef typename Container::iterator iterator;
        typedef typename Container::const_iterator const_iterator;

        iterator begin() {
            return queue.begin();
        }

        iterator end() {
            return queue.end();
        }

        const_iterator begin() const {
            return queue.begin();
        }

        const_iterator end() const {
            return queue.end();
        }

        T &front() const {
            return queue.front();
        }

        T &back() const {
            return queue.back();
        }
    };

    /**
     * \brief A queue that is optimized for taking request on thread acts as a driver
     *
     * When the queue reached a certain amount of defined elements, when a new thread pushed requests into the queue,
     * that thread will be put into wait state, until the queue is not full again.
     *
     * Similar thing happens if the queue is empty. The driver thread that pop this queue will be put in wait state
     * until the queue is not empty again.
     */
    template <typename T>
    class request_queue {
        // Took this from Vita3k mostly
        std::queue<T> queue_;
        std::condition_variable queue_empty_cond_;
        std::condition_variable queue_cond_;
        std::mutex queue_mut_;
        std::atomic<bool> abort_;

    public:
        std::uint32_t max_pending_count_;

        void push(const T &item) {
            {
                std::unique_lock<std::mutex> ulock(queue_mut_);

                while (!abort_ && queue_.size() == max_pending_count_) {
                    queue_cond_.wait(ulock);
                }

                if (abort_) {
                    return;
                }

                queue_.push(item);
            }

            queue_empty_cond_.notify_one();
        }

        std::optional<T> pop(const int ms = 0) {
            T item{ T() };

            {
                std::unique_lock<std::mutex> ulock(queue_mut_);

                if (ms == 0) {
                    while (!abort_ && queue_.empty()) {
                        queue_empty_cond_.wait(ulock);
                    }
                } else {
                    if (queue_.empty()) {
                        queue_empty_cond_.wait_for(ulock, std::chrono::microseconds(ms));
                    }
                }

                if (abort_ || queue_.empty()) {
                    return std::nullopt;
                }

                item = queue_.front();
                queue_.pop();
            }

            queue_cond_.notify_one();
            return item;
        }

        void abort() {
            abort_ = true;
            queue_cond_.notify_all();
            queue_empty_cond_.notify_all();
        }

        void reset() {
            queue_.clear();
            abort_ = false;
        }
    };
}
