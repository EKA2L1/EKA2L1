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

#include <discord.h>
#include <common/log.h>

#include <QSettings>

namespace eka2l1::qt {
    constexpr static const std::uint32_t UPDATE_INVOKE_PERIOD = 100;
    constexpr static const discord::ClientId EKA2L1_CLIENT_ID = 434248613174968320;

    discord_rpc::discord_rpc(QObject *parent)
        : QObject(parent)
        , core_(nullptr)
        , update_timer_(new QTimer(this)) {
        QSettings settings;
        if (settings.value(ENABLE_DISCORD_RICH_PRESENCE_SETTING_NAME, true).toBool()) {
            if (discord::Core::Create(EKA2L1_CLIENT_ID, DiscordCreateFlags_NoRequireDiscord, &core_) != discord::Result::Ok) {
                LOG_ERROR(FRONTEND_UI, "Failed to initialize Discord RPC");
                return;
            }

            connect(update_timer_, &QTimer::timeout, this, &discord_rpc::on_update_timer_hit);
            update_timer_->start(1000);
        }
    }

    discord_rpc::~discord_rpc() {
        delete core_;
        delete update_timer_;
    }
    
    void discord_rpc::on_update_timer_hit() {
        core_->RunCallbacks();
    }

    void discord_rpc::update(const std::string &state, const std::string &detail, bool should_reset_timer) {
        if (!core_) {
            return;
        }

        discord::Activity target_update_activity{};
        target_update_activity.SetType(discord::ActivityType::Playing);
        target_update_activity.SetDetails(detail.c_str());
        target_update_activity.SetState(state.c_str());

        discord::ActivityAssets &assets = target_update_activity.GetAssets();
        std::string large_text_std = tr("A Symbian/N-Gage emulator, available on PC and Android.").toStdString();

        assets.SetLargeText(large_text_std.c_str());
        assets.SetLargeImage("eka2l1_logo");

        if (should_reset_timer) {
            target_update_activity.GetTimestamps().SetStart(time(nullptr));
        }
        
        core_->ActivityManager().UpdateActivity(target_update_activity, [](discord::Result result) {
            if (result != discord::Result::Ok) {
                LOG_ERROR(FRONTEND_UI, "Error updating Discord Rich Presence, err_code: {}", static_cast<int>(result));
            }
        });
    }
}
#endif