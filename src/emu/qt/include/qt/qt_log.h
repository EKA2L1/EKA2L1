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

#pragma once

namespace eka2l1::desktop {
    /**
     * \brief Send Qt's own diagnostics to the emulator log.
     *
     * Without this Qt writes them to standard error and nowhere else, so nothing it
     * reports about the platform plugin, the window system or the GL context ever
     * reaches EKA2L1.log.
     *
     * Call before constructing QApplication: the platform plugin is chosen there, and
     * failing to load one is reported and then fatal. Messages arriving before the
     * logger exists are held back and written by drain_early_qt_messages().
     */
    void install_qt_message_handler();

    /**
     * \brief Write out the Qt messages that arrived before the logger existed.
     *
     * Call once the log is set up. Does nothing if there were none.
     */
    void drain_early_qt_messages();
}
