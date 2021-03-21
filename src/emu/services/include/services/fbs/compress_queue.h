/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <common/queue.h>
#include <utils/reqsts.h>

#include <mutex>

namespace eka2l1 {
    struct fbsbitmap;
    class fbs_server;

    /**
     * \brief Queue that handle bitmap compression.
     * 
     * This queue runs on separate thread.
     */
    class compress_queue {
        request_queue<fbsbitmap *> queue_;
        fbs_server *serv_;

        std::vector<epoc::notify_info> notifies_;
        std::mutex notify_mutex_;

    public:
        explicit compress_queue(fbs_server *serv);

        // Exposed this, but should be standalone...
        void actual_compress(fbsbitmap *bmp);

        void notify(epoc::notify_info &nof);

        /**
         * \brief Cancel a notify request.
         * 
         * \param nof   Target notification.
         * 
         * \returns True on success.
         */
        bool cancel(epoc::notify_info &nof);

        /**
         * \brief Compress a bitmap.
         * 
         * If the compress queue is full. This function stalls until the queue has slot for this bitmap.
         * 
         * 
         * \param bmp   The bitmap to compress.
         */
        void compress(fbsbitmap *bmp);

        /**
         * \brief Run the compression queue.
         */
        void run();

        /**
         * \brief Abort the compression run.
         */
        void abort();
    };
}