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

.global ECamGetNumberOfCameras
.global ECamCreate
.global ECamClaim
.global ECamRelease
.global ECamPowerOn
.global ECamPowerOff
.global ECamSetParameter
.global ECamQueryCameraInfo
.global ECamQueryStillImageSize
.global ECamTakeImage
.global ECamCancelTakeImage
.global ECamReceiveImage
.global ECamQueryVideoFrameDimension
.global ECamQueryVideoFrameRate
.global ECamTakeVideo
.global ECamReceiveVideoBuffer
.global ECamCancelTakeVideo
.global ECamDuplicate

ECamGetNumberOfCameras:
    CallHleDispatch 0x50

ECamCreate:
    CallHleDispatch 0x51

ECamClaim:
    CallHleDispatch 0x52

ECamRelease:
    CallHleDispatch 0x53

ECamPowerOn:
    CallHleDispatch 0x54

ECamPowerOff:
    CallHleDispatch 0x55

ECamSetParameter:
    CallHleDispatch 0x56

ECamQueryCameraInfo:
    CallHleDispatch 0x57

ECamQueryStillImageSize:
    CallHleDispatch 0x58

ECamTakeImage:
    CallHleDispatch 0x59

ECamCancelTakeImage:
    CallHleDispatch 0x5A

ECamReceiveImage:
    CallHleDispatch 0x5B
    
ECamQueryVideoFrameDimension:
    CallHleDispatch 0x5C

ECamQueryVideoFrameRate:
    CallHleDispatch 0x5D

ECamTakeVideo:
    CallHleDispatch 0x5E

ECamReceiveVideoBuffer:
    CallHleDispatch 0x5F

ECamCancelTakeVideo:
    CallHleDispatch 0x60

ECamDuplicate:
    CallHleDispatch 0x61
