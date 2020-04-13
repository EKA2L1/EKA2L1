/*
 * Copyright (c) 2018 EKA2L1 Team / 2009 Nokia Corporation
 *
 * This file is part of EKA2L1 project / 2009 Nokia Corporation
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

    class video_info_v1 {
    public:
        vec2 size_in_pixels_;
        vec2 size_in_twips_;
        std::int32_t is_mono_;
        std::int32_t is_palettelized_;
        std::int32_t bits_per_pixel_;
        std::uint32_t video_address_;
        std::int32_t offset_to_first_pixel_;
        std::int32_t offset_between_lines_;
        std::int32_t is_pixel_order_rgb_;
        std::int32_t is_pixel_order_landspace_;
        std::int32_t display_mode_;
    };

    static_assert(sizeof(video_info_v1) == 52);

    /*! \brief A HAL function. Each function has minimum of 0 arg and maximum of 2 args. */
    using hal_func = std::function<int(int *, int *, std::uint16_t)>;
    using hal_cagetory_funcs = std::unordered_map<uint32_t, hal_func>;

    /**
     * \brief Base class for all High Level Abstraction Layer Cagetory.
     */
    struct hal {
        hal_cagetory_funcs funcs;
        eka2l1::system *sys;

    public:
        explicit hal(eka2l1::system *sys);

        /*
         * \brief Execute an HAL function. 
         *
         * \param func Function opcode.
         * \returns HAL operation result.
         */
        int do_hal(uint32_t func, int *a1, int *a2, const std::uint16_t device_number);
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