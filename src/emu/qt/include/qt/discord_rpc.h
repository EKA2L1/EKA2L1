/*
 * Copyright (c) 2023 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <common/configure.h>

#if ENABLE_DISCORD_RICH_PRESENCE
#include <QObject>
#include <QTimer>
#include <memory>

namespace discord {
    class Core;
}

namespace eka2l1::qt {
    class discord_rpc : public QObject {
        Q_OBJECT

    private:
        discord::Core *core_;
        QTimer *update_timer_;

    private slots:
        void on_update_timer_hit();

    public:
        explicit discord_rpc(QObject *parent);
        ~discord_rpc();

        void update(const std::string &state, const std::string &detail, bool should_reset_timer = true);
    };
}
#else
#include <QObject>

namespace eka2l1::qt {
    class discord_rpc {
    public:
        explicit discord_rpc(QObject *parent) {}
        ~discord_rpc() = default;

        void update(const std::string &state, const std::string &detail, bool should_reset_timer = true) {}
    };
}
#endif