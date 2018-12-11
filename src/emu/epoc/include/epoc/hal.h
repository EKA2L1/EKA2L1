/*
* Copyright (c) 2018 EKA2L1 Team / 2009 Nokia Corporation
*
* This file is part of EKA2L1 project / 2009 Nokia Corporation
* (see bentokun.github.com/EKA2L1).
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

#include <common/vecx.h>

#include <functional>
#include <unordered_map>

namespace eka2l1 {
    class system;
}

namespace eka2l1::epoc {
    /*! \brief All the function opcode of the kernel HAL cagetory. */
    enum TKernelHalFunction {
        EKernelHalMemoryInfo,
        EKernelHalRomInfo,
        EKernelHalStartupReason,
        EKernelHalFaultReason,
        EKernelHalExceptionId,
        EKernelHalExceptionInfo,
        EKernelHalCpuInfo,
        EKernelHalPageSizeInBytes,
        EKernelHalTickPeriod,
        EKernelHalMemModelInfo,
        EKernelHalFastCounterFrequency,
        EKernelHalNTickPeriod,
        EKernelHalHardwareFloatingPoint,
        EKernelHalGetNonsecureClockOffset,
        EKernelHalSetNonsecureClockOffset,
        EKernelHalSmpSupported,
        EKernelHalNumLogicalCpus,
        EKernelHalSupervisorBarrier,
        EKernelHalFloatingPointSystemId,
        EKernelHalLockThreadToCpu,
        EKernelHalConfigFlags,
        EKernelHalCpuStates,
        EKernelHalSetNumberOfCpus,
    };

    enum TVariantHalFunction {
        EVariantHalVariantInfo,
        EVariantHalDebugPortSet,
        EVariantHalDebugPortGet,
        EVariantHalLedMaskSet,
        EVariantHalLedMaskGet,
        EVariantHalSwitches,
        EVariantHalCustomRestart,
        EVariantHalCustomRestartReason,
        EVariantHalCaseState, // Hmmmm
        EVariantHalCurrentNumberOfScreens,
        EVariantHalPersistStartupMode,
        EVariantHalGetPersistedStartupMode,
        EVariantHalGetMaximumCustomRestartReasons,
        EVariantHalGetMaximumRestartStartupModes,
        EVariantHalTimeoutExpansion,
        EVariantHalSerialNumber,
        EVariantHalProfilingDefaultInterruptBase
    };

    enum TDisplayHalFunction {
        EDisplayHalScreenInfo,
        EDisplayHalWsRegisterSwitchOnScreenHandling,
        EDisplayHalWsSwitchOnScreen,
        EDisplayHalMaxDisplayContrast,
        EDisplayHalSetDisplayContrast,
        EDisplayHalDisplayContrast,
        EDisplayHalSetBacklightBehavior,
        EDisplayHalBacklightBehavior,
        EDisplayHalSetBacklightOnTime,
        EDisplayHalBacklightOnTime,
        EDisplayHalSetBacklightOn,
        EDisplayHalBacklightOn,
        EDisplayHalMaxDisplayBrightness,
        EDisplayHalSetDisplayBrightness,
        EDisplayHalDisplayBrightness,
        EDisplayHalModeCount,
        EDisplayHalSetMode,
        EDisplayHalMode,
        EDisplayHalSetPaletteEntry,
        EDisplayHalPaletteEntry,
        EDisplayHalSetState,
        EDisplayHalState,
        EDisplayHalColors,
        EDisplayHalCurrentModeInfo,
        EDisplayHalSpecifiedModeInfo,
        //	EDisplaySwitchOffScreen,  - This call can be used in lower version of Symbian ?
        EDisplayHalBlockFill,
        EDisplayHalBlockCopy,
        EDisplayHalSecure,
        EDisplayHalSetSecure,
        EDisplayHalGetDisplayMemoryAddress,
        EDisplayHalGetDisplayMemoryHandle,
        EDisplayHalNumberOfResolutions,
        EDisplayHalSpecificScreenInfo,
        EDisplayHalCurrentScreenInfo,
        EDisplayHalSetDisplayState,
        EDisplayHalGetStateSpinner,
    };

    enum TDisplayConnectState {
        ENormalResolution,
        ENoResolution,
        EDisconnect,
        ESingleResolution,
        EDisplayStateTooHigh
    };

    enum TDigitiserHalFunction {
        EDigitiserHalSetXYInputCalibration,
        EDigitiserHalCalibrationPoints,
        EDigitiserHalSaveXYInputCalibration,
        EDigitiserHalRestoreXYInputCalibration,
        EDigitiserHalXYInfo,
        EDigitiserHalXYState,
        EDigitiserHalSetXYState,
        EDigitiserHal3DPointer,
        EDigitiserHalSetZRange,
        EDigitiserHalSetNumberOfPointers,
        EDigitiserHal3DInfo,
        EDigitiserOrientation
    };

    struct TMemoryInfoV1 {
        int iTotalRamInBytes;
        int iTotalRomInBytes;
        int iMaxFreeRamInBytes;
        int iFreeRamInBytes;
        int iInternalDiskRamInBytes;
        bool iRomIsReprogrammable;
    };

    struct TVariantInfoV1 {
        uint8_t iMajor;
        uint8_t iMinor;
        uint16_t iBuild;

        uint64_t iMachineUid;
        uint32_t iLedCaps;
        uint32_t iProessorClockInMhz;
        uint32_t iSpeedFactor;
    };

    class TVideoInfoV01 {
    public:
        vec2 iSizeInPixels; /**< The visible width/height of the display in pixels. */
        vec2 iSizeInTwips; /**< The visible width/height of the display in twips. */
        bool iIsMono; /**< True if display is monochrome; false otherwise. */
        bool iIsPalettized; /**< True if display is palettized (in current display mode); false otherwise. */
        int iBitsPerPixel; /**< The number of bits in one pixel. */
        int iVideoAddress; /**< The virtual address of screen memory. */
        int iOffsetToFirstPixel; /**< Number of bytes from iVideoAddress for the first displayed pixel. */
        int iOffsetBetweenLines; /**< Number of bytes between start of one line and start of next. */
        bool iIsPixelOrderRGB; /**< The orientation of sub pixels on the screen; True if RBG, False if BGR. */
        bool iIsPixelOrderLandscape; /**< True if display pixels are landscape. */
        int iDisplayMode; /**< The current display mode. */
    };

    /*! \brief A HAL function. Each function has minimum of 0 arg and maximum of 2 args. */
    using hal_func = std::function<int(int *, int *)>;
    using hal_cagetory_funcs = std::unordered_map<uint32_t, hal_func>;

    /*! \brief Base class for all High Level Abstraction Layer Cagetory. */
    struct hal {
        hal_cagetory_funcs funcs;
        eka2l1::system *sys;

    public:
        hal(eka2l1::system *sys);

        /*! \brief Execute an HAL function. 
         *
         * \param func Function opcode.
         * \returns HAL operation result.
         */
        int do_hal(uint32_t func, int *a1, int *a2);
    };

    /*! \brief Initalize HAL. 
     *
     * This adds all HAL cagetory to the system. 
    */
    void init_hal(eka2l1::system *sys);

    /*! \brief Do a HAL function with the specified cagetory and key. 
     *
     * \param cage The HAL cagetory.
     * \param func The HAL function opcode.
     *
     * \returns Operation result.
    */
    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2);
}