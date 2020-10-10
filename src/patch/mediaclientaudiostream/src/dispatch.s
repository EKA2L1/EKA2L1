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

.include "../../../pcommon/inc/sv.S"

.global EAudioDspOutStreamCreate
.global EAudioDspOutStreamWrite
.global EAudioDspStreamSetProperties
.global EAudioDspStreamStart
.global EAudioDspStreamStop
.global EAudioDspStreamGetSupportedFormats
.global EAudioDspStreamSetFormat
.global EAudioDspOutStreamSetVolume
.global EAudioDspOutStreamMaxVolume
.global EAudioDspStreamNotifyBufferSentToDriver
.global EAudioDspStreamNotifyEnd
.global EAudioDspStreamDestroy
.global EAudioDspOutStreamGetVolume
.global EAudioDspStreamSetBalance
.global EAudioDspStreamGetBalance
.global EAudioDspStreamBytesRendered
.global EAudioDspStreamPosition
.global EAudioDspStreamGetFormat
.global EAudioDspStreamCancelNotifyBufferSentToDriver
.global EAudioDspStreamResetStat

EAudioDspOutStreamCreate:
    CallHleDispatch 0x40

EAudioDspOutStreamWrite:
    CallHleDispatch 0x41

EAudioDspStreamSetProperties:
    CallHleDispatch 0x42

EAudioDspStreamStart:
    CallHleDispatch 0x43

EAudioDspStreamStop:
    CallHleDispatch 0x44

EAudioDspStreamGetSupportedFormats:
    CallHleDispatch 0x45

EAudioDspStreamSetFormat:
    CallHleDispatch 0x46

EAudioDspOutStreamSetVolume:
    CallHleDispatch 0x47

EAudioDspOutStreamMaxVolume:
    CallHleDispatch 0x48

EAudioDspStreamNotifyBufferSentToDriver:
    CallHleDispatch 0x49

EAudioDspStreamNotifyEnd:
    CallHleDispatch 0x4A

EAudioDspStreamDestroy:
    CallHleDispatch 0x4B

EAudioDspOutStreamGetVolume:
    CallHleDispatch 0x4C

EAudioDspStreamSetBalance:
    CallHleDispatch 0x4D

EAudioDspStreamGetBalance:
    CallHleDispatch 0x4E

EAudioDspStreamBytesRendered:
    CallHleDispatch 0x4F

EAudioDspStreamPosition:
    CallHleDispatch 0x50

EAudioDspStreamGetFormat:
    CallHleDispatch 0x51

EAudioDspStreamCancelNotifyBufferSentToDriver:
    CallHleDispatch 0x52

EAudioDspStreamResetStat:
    CallHleDispatch 0x53