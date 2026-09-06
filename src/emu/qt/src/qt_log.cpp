/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <qt/qt_log.h>

#include <common/log.h>

#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace eka2l1::desktop {
    namespace {
        struct early_message {
            QtMsgType type_;
            std::string text_;
        };

        // Qt talks before stage one builds the logger: the platform plugin is loaded
        // while QApplication is constructed, and failing to find one is reported and
        // then fatal. Hold those until there is somewhere to put them. The cap keeps a
        // component that fails in a loop from growing this without bound.
        constexpr std::size_t MAX_EARLY_MESSAGES = 128;

        std::mutex early_mutex;
        std::vector<early_message> early_messages;
        bool early_messages_dropped = false;

        std::string describe(const QMessageLogContext &context, const QString &message) {
            std::string described;

            // Everything uncategorised reports the category as "default", which says
            // nothing; a real one ("qt.qpa.wayland") is most of the value of the line.
            if (context.category && (std::strcmp(context.category, "default") != 0)) {
                described.append(context.category).append(": ");
            }

            described.append(message.toUtf8().constData());

            // Only filled in when Qt itself was built with QT_MESSAGELOGCONTEXT, which
            // release builds are not. Keep the line usable when it is absent.
            if (context.file && context.file[0]) {
                described.append(" (").append(context.file).append(":")
                    .append(std::to_string(context.line)).append(")");
            }

            return described;
        }

        void write_to_log(const QtMsgType type, const std::string &text) {
            switch (type) {
            case QtDebugMsg:
                LOG_DEBUG(FRONTEND_UI, "Qt: {}", text);
                break;

            case QtInfoMsg:
                LOG_INFO(FRONTEND_UI, "Qt: {}", text);
                break;

            case QtWarningMsg:
                LOG_WARN(FRONTEND_UI, "Qt: {}", text);
                break;

            case QtCriticalMsg:
                LOG_ERROR(FRONTEND_UI, "Qt: {}", text);
                break;

            case QtFatalMsg:
                LOG_CRITICAL(FRONTEND_UI, "Qt: {}", text);
                break;
            }
        }

        void message_handler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
            const std::string text = describe(context, message);

            if (already_setup) {
                write_to_log(type, text);
            } else {
                {
                    const std::lock_guard<std::mutex> guard(early_mutex);

                    if (early_messages.size() < MAX_EARLY_MESSAGES) {
                        early_messages.push_back({ type, text });
                    } else {
                        early_messages_dropped = true;
                    }
                }

                // There is no log file yet, and a fatal message never lives to be
                // replayed, so this is the only copy that survives.
                std::cerr << "Qt: " << text << std::endl;
            }

            if (type == QtFatalMsg) {
                // Qt aborts the moment this returns, well inside the one second the
                // periodic flush is allowed to sit on the message.
                if (log::spd_logger) {
                    log::spd_logger->flush();
                }
            }
        }
    }

    void install_qt_message_handler() {
        qInstallMessageHandler(message_handler);
    }

    void drain_early_qt_messages() {
        std::vector<early_message> pending;
        bool dropped = false;

        {
            const std::lock_guard<std::mutex> guard(early_mutex);

            pending.swap(early_messages);
            dropped = early_messages_dropped;
        }

        for (const early_message &message : pending) {
            write_to_log(message.type_, message.text_);
        }

        if (dropped) {
            LOG_WARN(FRONTEND_UI, "More than {} Qt messages arrived before logging started; "
                                  "the remainder went only to standard error", MAX_EARLY_MESSAGES);
        }
    }
}
