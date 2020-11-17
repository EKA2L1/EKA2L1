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

#include <mda/common/audio.h>
#include <mda/common/resource.h>

TInt ConvertFreqEnumToNumber(const TInt caps) {
    switch (caps) {
    case TMdaAudioDataSettings::ESampleRate8000Hz:
        return 8000;
    
    case TMdaAudioDataSettings::ESampleRate11025Hz:
        return 11025;

    case TMdaAudioDataSettings::ESampleRate12000Hz:
        return 12000;

    case TMdaAudioDataSettings::ESampleRate16000Hz:
        return 16000;

    case TMdaAudioDataSettings::ESampleRate22050Hz:
        return 22050;

    case TMdaAudioDataSettings::ESampleRate24000Hz:
        return 24000;

    case TMdaAudioDataSettings::ESampleRate32000Hz:
        return 32000;

    case TMdaAudioDataSettings::ESampleRate44100Hz:
        return 44100;

    case TMdaAudioDataSettings::ESampleRate48000Hz:
        return 48000;

    case TMdaAudioDataSettings::ESampleRate96000Hz:
        return 96000;

    case TMdaAudioDataSettings::ESampleRate64000Hz:
        return 64000;

    default:
        break;
    }

    return -1;
}

TInt ConvertChannelEnumToNum(const TInt caps) {
    switch (caps) {
    case TMdaAudioDataSettings::EChannelsMono:
        return 1;

    case TMdaAudioDataSettings::EChannelsStereo:
        return 2;

    default:
        break;
    }

    return -1;
}
