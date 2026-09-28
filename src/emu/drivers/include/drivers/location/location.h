/*
 * Copyright (c) 2026 EKA2L1 Team
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>
#include <memory>

namespace eka2l1::drivers {
    /**
     * @brief A WGS84 position from the host. Unknown values are NaN.
     */
    struct location_fix {
        double latitude_;
        double longitude_;
        float altitude_; ///< Metres above sea level.
        float horizontal_accuracy_; ///< Metres.
        float vertical_accuracy_; ///< Metres.
        float speed_; ///< Metres per second.
        float speed_accuracy_;
        float course_; ///< Degrees clockwise from true north.
        float course_accuracy_;
    };

    using location_update_callback = std::function<void(const location_fix &fix)>;

    class location_driver {
    public:
        virtual ~location_driver() = default;

        /**
         * @brief Start delivering host fixes to the callback, which may run on any host thread.
         *
         * The host may ask the user for permission; until it is granted no fix arrives.
         */
        virtual void start(location_update_callback callback) = 0;

        /**
         * @brief Stop updates. The callback is not invoked once this returns.
         */
        virtual void stop() = 0;
    };

    /**
     * @brief Create the host location driver, or nullptr when the host has none.
     */
    std::unique_ptr<location_driver> make_location_driver();
}
