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

#include <common/configure.h>

#if ENABLE_DISCORD_RICH_PRESENCE
#include <qt/discord_rpc.h>
#include <qt/utils.h>

#include <common/log.h>
#include <discord_rpc.h>

#include <QSettings>

#include <ctime>

namespace eka2l1::qt {
    constexpr static const char *EKA2L1_CLIENT_ID = "434248613174968320";

    discord_rpc::discord_rpc(QObject *parent)
        : QObject(parent)
        , initialised_(false)
        , start_time_(0)
        , update_timer_(new QTimer(this)) {
        QSettings settings;
        if (!settings.value(ENABLE_DISCORD_RICH_PRESENCE_SETTING_NAME, true).toBool()) {
            return;
        }

        DiscordEventHandlers handlers{};
        Discord_Initialize(EKA2L1_CLIENT_ID, &handlers, 0, nullptr);

        initialised_ = true;
        start_time_ = static_cast<std::int64_t>(time(nullptr));

        connect(update_timer_, &QTimer::timeout, this, &discord_rpc::on_update_timer_hit);
        update_timer_->start(1000);
    }

    discord_rpc::~discord_rpc() {
        if (initialised_) {
            Discord_ClearPresence();
            Discord_Shutdown();
        }
    }

    void discord_rpc::on_update_timer_hit() {
        Discord_RunCallbacks();
    }

    void discord_rpc::update(const std::string &state, const std::string &detail, bool should_reset_timer) {
        if (!initialised_) {
            return;
        }

        if (should_reset_timer) {
            start_time_ = static_cast<std::int64_t>(time(nullptr));
        }

        const std::string large_text = tr("A Symbian/N-Gage emulator, available on PC and Android.").toStdString();

        DiscordRichPresence presence{};
        presence.details = detail.c_str();
        presence.state = state.c_str();
        presence.largeImageKey = "eka2l1_logo";
        presence.largeImageText = large_text.c_str();
        presence.startTimestamp = start_time_;

        Discord_UpdatePresence(&presence);
    }
}
#endif
