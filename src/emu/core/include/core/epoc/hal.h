#pragma once

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

    enum TVariantHalFunction
	{
	    EVariantHalVariantInfo,
	    EVariantHalDebugPortSet,
    	EVariantHalDebugPortGet,
	    EVariantHalLedMaskSet,
        EVariantHalLedMaskGet,
        EVariantHalSwitches,
        EVariantHalCustomRestart,
        EVariantHalCustomRestartReason,
        EVariantHalCaseState,     // Hmmmm
        EVariantHalCurrentNumberOfScreens,
        EVariantHalPersistStartupMode,
        EVariantHalGetPersistedStartupMode,
        EVariantHalGetMaximumCustomRestartReasons,
        EVariantHalGetMaximumRestartStartupModes,
        EVariantHalTimeoutExpansion,
        EVariantHalSerialNumber,
        EVariantHalProfilingDefaultInterruptBase
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