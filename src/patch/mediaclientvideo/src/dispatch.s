/*
 * Copyright (c) 2022 EKA2L1 Team.
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

.global EVideoPlayerCreate
.global EVideoPlayerRegisterWindow
.global EVideoPlayerSetClipRect
.global EVideoPlayerOpenUrl
.global EVideoPlayerOpenDes
.global EVideoPlayerPlay
.global EVideoPlayerPause
.global EVideoPlayerStop
.global EVideoPlayerDestroy
.global EVideoPlayerGetVideoFps
.global EVideoPlayerSetVideoFps
.global EVideoPlayerGetBitrate
.global EVideoPlayerSetPlayDoneNotification
.global EVideoPlayerCancelPlayDoneNotification
.global EVideoPlayerClose
.global EVideoPlayerSetPosition
.global EVideoPlayerGetPosition
.global EVideoPlayerGetDuration
.global EVideoPlayerMaxVolume
.global EVideoPlayerCurrentVolume
.global EVideoPlayerSetVolume
.global EVideoPlayerSetRotation
.global EVideoPlayerUnregisterWindow

EVideoPlayerCreate:
    CallHleDispatch 0x80

EVideoPlayerRegisterWindow:
    CallHleDispatch 0x81

EVideoPlayerSetClipRect:
    CallHleDispatch 0x82
    
EVideoPlayerOpenUrl:
    CallHleDispatch 0x83
    
EVideoPlayerOpenDes:
	CallHleDispatch 0x84

EVideoPlayerPlay:
	CallHleDispatch 0x85
	
EVideoPlayerPause:
	CallHleDispatch 0x86

EVideoPlayerStop:
	CallHleDispatch 0x87

EVideoPlayerDestroy:
	CallHleDispatch 0x88

EVideoPlayerGetVideoFps:
	CallHleDispatch 0x89

EVideoPlayerSetVideoFps:
	CallHleDispatch 0x8A

EVideoPlayerGetBitrate:
	CallHleDispatch 0x8B
	
EVideoPlayerSetPlayDoneNotification:
	CallHleDispatch 0x8C

EVideoPlayerCancelPlayDoneNotification:
	CallHleDispatch 0x8D

EVideoPlayerClose:
	CallHleDispatch 0x8E

EVideoPlayerSetPosition:
	CallHleDispatch 0x8F

EVideoPlayerGetPosition:
	CallHleDispatch 0x90
	
EVideoPlayerGetDuration:
	CallHleDispatch 0x91
	
EVideoPlayerMaxVolume:
	CallHleDispatch 0x92

EVideoPlayerCurrentVolume:
	CallHleDispatch 0x93
	
EVideoPlayerSetVolume:
	CallHleDispatch 0x94

EVideoPlayerSetRotation:
	CallHleDispatch 0x95

EVideoPlayerUnregisterWindow:
	CallHleDispatch 0x96