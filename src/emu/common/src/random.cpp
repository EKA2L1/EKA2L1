#include <common/random.h>

namespace eka2l1 {
    uint32_t random() {
        return random_range(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
    }

    uint32_t random_range(uint32_t beg, uint32_t end) {
        std::mt19937 rng;
        rng.seed(std::random_device()());
        std::uniform_int_distribution<std::mt19937::result_type> dist(beg, end);

        return dist(rng);
    }
}