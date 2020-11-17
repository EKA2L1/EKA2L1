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

.include "../../../priv/inc/sv.S"

.global EAudioPlayerNewInstance
.global EAudioPlayerNotifyAnyDone
.global EAudioPlayerSupplyUrl
.global EAudioPlayerSupplyData
.global EAudioPlayerSetVolume
.global EAudioPlayerGetVolume
.global EAudioPlayerMaxVolume
.global EAudioPlayerPlay
.global EAudioPlayerStop
.global EAudioPlayerPause
.global EAudioPlayerCancelNotifyAnyDone
.global EAudioPlayerGetCurrentPlayPos
.global EAudioPlayerSetCurrentPlayPos
.global EAudioPlayerGetCurrentBitRate
.global EAudioPlayerSetBalance
.global EAudioPlayerGetBalance
.global EAudioPlayerSetRepeats
.global EAudioPlayerDestroy
.global EAudioPlayerGetDestinationSampleRate
.global EAudioPlayerSetDestinationSampleRate
.global EAudioPlayerGetDestinationChannelCount
.global EAudioPlayerSetDestinationChannelCount
.global EAudioPlayerSetDestinationEncoding
.global EAudioPlayerGetDestinationEncoding
.global EAudioPlayerSetContainerFormat

EAudioPlayerNewInstance:
    CallHleDispatch 0x20

EAudioPlayerNotifyAnyDone:
    CallHleDispatch 0x21

EAudioPlayerSupplyUrl:
    CallHleDispatch 0x22

EAudioPlayerSupplyData:
    CallHleDispatch 0x23

EAudioPlayerSetVolume:
    CallHleDispatch 0x24

EAudioPlayerGetVolume:
    CallHleDispatch 0x25

EAudioPlayerMaxVolume:
    CallHleDispatch 0x26

EAudioPlayerPlay:
    CallHleDispatch 0x27

EAudioPlayerStop:
    CallHleDispatch 0x28

EAudioPlayerPause:
    CallHleDispatch 0x29

EAudioPlayerCancelNotifyAnyDone:
    CallHleDispatch 0x2A

EAudioPlayerGetCurrentPlayPos:
    CallHleDispatch 0x2B

EAudioPlayerSetCurrentPlayPos:
    CallHleDispatch 0x2C

EAudioPlayerGetCurrentBitRate:
    CallHleDispatch 0x2D

EAudioPlayerSetBalance:
    CallHleDispatch 0x2E

EAudioPlayerGetBalance:
    CallHleDispatch 0x2F

EAudioPlayerSetRepeats:
    CallHleDispatch 0x30

EAudioPlayerDestroy:
    CallHleDispatch 0x31

EAudioPlayerGetDestinationSampleRate:
    CallHleDispatch 0x32

EAudioPlayerSetDestinationSampleRate:
    CallHleDispatch 0x33

EAudioPlayerGetDestinationChannelCount:
    CallHleDispatch 0x34

EAudioPlayerSetDestinationChannelCount:
    CallHleDispatch 0x35

EAudioPlayerSetDestinationEncoding:
    CallHleDispatch 0x36

EAudioPlayerGetDestinationEncoding:
    CallHleDispatch 0x37

EAudioPlayerSetContainerFormat:
    CallHleDispatch 0x38
