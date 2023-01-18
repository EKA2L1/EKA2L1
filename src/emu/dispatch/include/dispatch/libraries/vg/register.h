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

#include <cstdint>
#include <dispatch/libraries/register.h>

namespace eka2l1::dispatch {
    static const patch_info LIBOPENVG11_PATCH_INFOS[] = {
        // Dispatch number, Ordinal number
        { 0x143B, 2 },
        { 0x143C, 3 },
        { 0x1421, 4 },
        { 0x141B, 5 },
        { 0x141E, 6 },
        { 0x1438, 7 },
        { 0x1424, 10 },
        { 0x1429, 11 },
        { 0x141C, 12 },
        { 0x1456, 13 },
        { 0x1436, 14 },
        { 0x141D, 15 },
        { 0x142A, 16 },
        { 0x1437, 17 },
        { 0x1423, 18 },
        { 0x1458, 19 },
        { 0x1400, 22 },
        { 0x142E, 23 },
        { 0x1415, 26 },
        { 0x142C, 27 },
        { 0x140F, 29 },
        { 0x1412, 30 },
        { 0x1410, 31 },
        { 0x1413, 32 },
        { 0x1422, 33 },
        { 0x143A, 34 },
        { 0x1426, 35 },
        { 0x144C, 36 },
        { 0x1407, 37 },
        { 0x1405, 38 },
        { 0x1408, 39 },
        { 0x1406, 40 },
        { 0x1409, 41 },
        { 0x141f, 43 },
        { 0x1440, 44 },
        { 0x1457, 45 },
        { 0x1414, 46 },
        { 0x1434, 49 },
        { 0x143D, 50 },
        { 0x1416, 51 },
        { 0x142F, 52 },
        { 0x1443, 53 },
        { 0x1441, 54 },
        { 0x1444, 55 },
        { 0x1442, 56 },
        { 0x1428, 57 },
        { 0x1439, 58 },
        { 0x141A, 59 },
        { 0x1418, 60 },
        { 0x142D, 62 },
        { 0x142B, 63 },
        { 0x140A, 64 },
        { 0x140C, 65 },
        { 0x140B, 66 },
        { 0x140E, 67 },
        { 0x1425, 68 },
        { 0x1401, 69 },
        { 0x1403, 70 },
        { 0x1402, 71 },
        { 0x1404, 72 },
        { 0x1419, 73 },
        { 0x143F, 74 },
        { 0x1417, 75 },
        { 0x1427, 76 },
        { 0x1449, 77 },
        { 0x1433, 78 },
        { 0x1445, 79 },
        { 0x1430, 80 },
        { 0x1446, 81 },
        { 0x1431, 82 },
        { 0x144A, 83 },
        { 0x144B, 84 },
        { 0x1432, 85 },
        { 0x1435, 86 },
        { 0x1448, 87 },
        { 0x1447, 88 }
    };

    static const patch_info LIBOPENVGU11_PATCH_INFOS[] = {
        // Dispatch number, Ordinal number
        { 0x1452, 1 },
        { 0x1455, 2 },
        { 0x1453, 3 },
        { 0x1454, 4 },
        { 0x1451, 5 },
        { 0x144d, 6 },
        { 0x144e, 7 },
        { 0x144f, 8 },
        { 0x1450, 9 }
    };

    static const std::uint32_t LIBOPENVG11_PATCH_COUNT = sizeof(LIBOPENVG11_PATCH_INFOS) / sizeof(patch_info);
    static const std::uint32_t LIBOPENVGU11_PATCH_COUNT = sizeof(LIBOPENVGU11_PATCH_INFOS) / sizeof(patch_info);
}