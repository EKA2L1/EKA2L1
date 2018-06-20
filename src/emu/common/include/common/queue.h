#pragma once

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
        bool remove(const T& val) {
            auto it = std::find(c.begin(), c.end(), val);
            if (it != c.end()) {
                c.erase(it);
                std::make_heap(c.begin(), c.end(), comp);
                return true;
            }
            else {
                return false;
            }
        }

        iterator begin()  {
            return c.begin();
        }

        iterator end() {
            return c.end();
        }

		/*! \brief Resort the queue.
		*/
        void resort() {
            std::make_heap(c.begin(), c.end(), comp);
        }
    };
}