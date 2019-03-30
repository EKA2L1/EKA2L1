#pragma once

#include <algorithm>
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
}
