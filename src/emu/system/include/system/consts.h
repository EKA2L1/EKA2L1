/*
 * Copyright (c) 2020 EKA2L1 Team.
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

namespace eka2l1::preset {
    static const char *ROM_FOLDER_PATH = "roms//";
    static const char *DRIVE_FOLDER_PATH = "drives//";
    static const char *ROM_FILENAME = "SYM.ROM";

    enum system_cpu_hz {
        SYSTEM_CPU_HZ_S60V1 = 104000000,
        SYSTEM_CPU_HZ_S60V2 = 220000000,
        SYSTEM_CPU_HZ_S60V3 = 369000000,
        SYSTEM_CPU_HZ_S60V5 = 434000000,
        SYSTEM_CPU_HZ_S3 = 680000000,
        SYSTEM_CPU_HZ_BELLE = 1200000000
    };
}